#include "dsp/Voicing.h"
#include "TestHelpers.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <limits>

// Issue #42: high-band distortion voicing (Gnaw/Wool/Razor) acceptance
// gates.
namespace
{
    constexpr double testSampleRate = 48000.0;
    constexpr int numSamples = 2048;

    juce::dsp::ProcessSpec makeSpec (int numChannels = 1)
    {
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = testSampleRate;
        spec.maximumBlockSize = static_cast<juce::uint32> (numSamples);
        spec.numChannels = static_cast<juce::uint32> (numChannels);
        return spec;
    }

    constexpr std::array<cryp::VoicingType, 3> allVoicings {
        cryp::VoicingType::gnaw, cryp::VoicingType::wool, cryp::VoicingType::razor
    };
}

TEST_CASE ("Voicing: 0% blend is a passthrough within a small margin (dry-path DryWetMixer delay compensation)", "[voicing][dsp]")
{
    // Not bit-exact: the dry path runs through a Thiran (fractional-
    // interpolation-capable) delay line inside DryWetMixer even when the
    // requested delay is an exact integer sample count, so a tiny amount of
    // allpass ripple is expected - this checks level transparency, the way
    // GainProcessingTests.cpp's passthrough test does for the full chain.
    for (const auto voicing : allVoicings)
    {
        cryp::Voicing voicingUnderTest;
        voicingUnderTest.prepare (makeSpec(), 0.0f);
        voicingUnderTest.setVoicing (voicing);
        voicingUnderTest.setDrive (1.0f);
        voicingUnderTest.setTone (0.5f);

        juce::AudioBuffer<float> buffer (1, numSamples);
        TestHelpers::fillWithSine (buffer, testSampleRate, 1000.0, 0.5f);

        juce::AudioBuffer<float> reference;
        reference.makeCopyOf (buffer);

        juce::dsp::AudioBlock<float> block (buffer);
        voicingUnderTest.process (block);

        const auto outRms = TestHelpers::rms (buffer);
        const auto refRms = TestHelpers::rms (reference);

        INFO ("voicing index = " << static_cast<int> (voicing));
        CHECK (juce::Decibels::gainToDecibels (outRms / refRms) == Catch::Approx (0.0).margin (0.2));
    }
}

TEST_CASE ("Voicing: reports positive, plausible oversampling latency after prepare()", "[voicing][dsp]")
{
    cryp::Voicing voicing;
    CHECK (voicing.getLatencySamples() == 0);

    voicing.prepare (makeSpec(), 1.0f);
    CHECK (voicing.getLatencySamples() > 0);
    CHECK (voicing.getLatencySamples() < 1000);
}

TEST_CASE ("Voicing: no NaN/Inf and no runaway output at extreme drive for every voicing", "[voicing][dsp][robustness]")
{
    // The waveshapers themselves (tanh/hard-clip) are always bounded to
    // [-1, 1], but the post-shaper mid-hump/scoop peak filter can still
    // legitimately push a transient above unity when it's boosting (e.g.
    // Razor's +5dB hump) - that's ordinary EQ-after-clipper behaviour, not a
    // bug, so this checks for finiteness and the absence of a runaway
    // feedback-loop-style blow-up rather than a strict unity ceiling.
    for (const auto voicingType : allVoicings)
    {
        cryp::Voicing voicing;
        voicing.prepare (makeSpec(), 1.0f);
        voicing.setVoicing (voicingType);
        voicing.setDrive (1.0f); // maximum drive
        voicing.setTone (0.5f);

        // A hot input (well above 0dBFS) is exactly when an unbounded
        // waveshaper would misbehave.
        juce::AudioBuffer<float> buffer (1, numSamples);
        TestHelpers::fillWithSine (buffer, testSampleRate, 300.0, 4.0f);

        juce::dsp::AudioBlock<float> block (buffer);
        CHECK_NOTHROW (voicing.process (block));
        CHECK (TestHelpers::allSamplesFinite (buffer));

        const auto* data = buffer.getReadPointer (0);

        for (int sample = 0; sample < numSamples; ++sample)
            CHECK (std::abs (data[sample]) <= 10.0f);
    }
}

TEST_CASE ("Voicing: higher drive increases harmonic energy for every voicing", "[voicing][dsp]")
{
    // A pure sine driven harder through a nonlinearity gains energy outside
    // its fundamental (harmonics), so a simple, topology-agnostic proxy for
    // "more drive audibly changes the signal" is that the *difference*
    // between the low-drive and high-drive outputs is clearly non-zero.
    for (const auto voicingType : allVoicings)
    {
        cryp::Voicing lowDriveVoicing;
        lowDriveVoicing.prepare (makeSpec(), 1.0f);
        lowDriveVoicing.setVoicing (voicingType);
        lowDriveVoicing.setDrive (0.0f);
        lowDriveVoicing.setTone (0.5f);

        cryp::Voicing highDriveVoicing;
        highDriveVoicing.prepare (makeSpec(), 1.0f);
        highDriveVoicing.setVoicing (voicingType);
        highDriveVoicing.setDrive (1.0f);
        highDriveVoicing.setTone (0.5f);

        juce::AudioBuffer<float> lowDriveBuffer (1, numSamples);
        TestHelpers::fillWithSine (lowDriveBuffer, testSampleRate, 300.0, 0.7f);
        juce::dsp::AudioBlock<float> lowDriveBlock (lowDriveBuffer);
        lowDriveVoicing.process (lowDriveBlock);

        juce::AudioBuffer<float> highDriveBuffer (1, numSamples);
        TestHelpers::fillWithSine (highDriveBuffer, testSampleRate, 300.0, 0.7f);
        juce::dsp::AudioBlock<float> highDriveBlock (highDriveBuffer);
        highDriveVoicing.process (highDriveBlock);

        juce::AudioBuffer<float> difference;
        difference.makeCopyOf (highDriveBuffer);
        difference.addFrom (0, 0, lowDriveBuffer, 0, 0, numSamples, -1.0f);

        INFO ("voicing index = " << static_cast<int> (voicingType));
        CHECK (TestHelpers::rms (difference) > 0.01);
    }
}

TEST_CASE ("Voicing: no NaN/Inf across a denormal-range sweep for every voicing", "[voicing][dsp][robustness]")
{
    for (const auto voicingType : allVoicings)
    {
        cryp::Voicing voicing;
        voicing.prepare (makeSpec (2), 1.0f);
        voicing.setVoicing (voicingType);
        voicing.setDrive (1.0f);
        voicing.setTone (1.0f);

        juce::AudioBuffer<float> buffer (2, numSamples);
        const auto denormalValue = std::numeric_limits<float>::denorm_min() * 4.0f;

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            auto* data = buffer.getWritePointer (channel);

            for (int sample = 0; sample < numSamples; ++sample)
                data[sample] = (sample % 2 == 0) ? denormalValue : -denormalValue;
        }

        juce::dsp::AudioBlock<float> block (buffer);
        CHECK_NOTHROW (voicing.process (block));
        CHECK (TestHelpers::allSamplesFinite (buffer));
    }
}

namespace
{
    // Excites Wool's mid-scoop filter (500Hz/-6dB/Q0.9) at `exciteFreqHz`,
    // switches to Razor (900Hz/+5dB/Q1.0 - the largest mid-filter
    // coefficient jump available in the voicing set) and returns the RMS of
    // a short window of the following silence, skipping past the
    // oversampling stage's own (finite, unrelated) FIR flush tail.
    //
    // Processes in `numSamples`-sized chunks (the block size makeSpec()
    // prepared) rather than one oversized block, since
    // Oversampling::processSamplesUp() asserts on blocks larger than what
    // initProcessing() was prepared for.
    double exciteThenMeasurePostSwitchSilence (double exciteFreqHz)
    {
        cryp::Voicing voicing;
        voicing.prepare (makeSpec (1), 1.0f); // fully wet, so midFilter fully reaches the output
        voicing.setVoicing (cryp::VoicingType::wool);
        voicing.setDrive (0.0f); // minimal waveshaping so the mid filter dominates the response
        voicing.setTone (1.0f);  // tone filter's pole sits near Nyquist, so its own state decays near-instantly

        constexpr int numExciteBlocks = 4;

        for (int i = 0; i < numExciteBlocks; ++i)
        {
            juce::AudioBuffer<float> excite (1, numSamples);
            TestHelpers::fillWithSine (excite, testSampleRate, exciteFreqHz, 0.9f, static_cast<juce::int64> (i) * numSamples);
            juce::dsp::AudioBlock<float> exciteBlock (excite);
            voicing.process (exciteBlock);
        }

        // Switch to Razor: a substantial mid-filter coefficient jump under
        // whatever mid-filter state the excite phase built up in Wool.
        voicing.setVoicing (cryp::VoicingType::razor);

        juce::AudioBuffer<float> silence (1, numSamples);
        silence.clear();
        juce::dsp::AudioBlock<float> silenceBlock (silence);
        voicing.process (silenceBlock);

        const auto latency = juce::jmax (1, voicing.getLatencySamples());
        const auto skipSamples = juce::jlimit (0, numSamples / 2, 3 * latency);
        const auto windowLength = juce::jmin (numSamples - skipSamples, 4 * latency);

        juce::AudioBuffer<float> measured (1, windowLength);
        measured.copyFrom (0, 0, silence, 0, skipSamples, windowLength);

        return TestHelpers::rms (measured);
    }
}

TEST_CASE ("Voicing: setVoicing() resets midFilter state instead of ringing a coefficient-jump transient into silence", "[voicing][dsp]")
{
    // Issue #57: setVoicing() unconditionally recomputes midFilter's
    // coefficients on every voicing change but (unlike preHighPass's
    // explicit Razor-switch handling) never reset its state - a stale
    // biquad delay-line combined with freshly-swapped coefficients rings a
    // spurious transient.
    //
    // A raw "silence right after the switch" magnitude check can't isolate
    // this cleanly: the oversampling stage's own FIR flush tail from the
    // excite-to-silence discontinuity, combined with Razor's mid filter
    // being a +5dB *boost* (vs Wool's -6dB *cut*), both legitimately and
    // unavoidably still leave some residual regardless of whether midFilter
    // itself is reset - that residual would swamp a fixed threshold either
    // way. Instead, this compares the residual left by exciting Wool's mid
    // filter *on resonance* (500Hz, its own center frequency - maximises
    // energy stored in midFilter's state specifically) against the residual
    // left by exciting *off resonance* (20Hz, far below any voicing's mid
    // filter - minimises midFilter's own contribution while leaving the
    // shared oversampling-FIR-tail/gain-difference confound roughly the
    // same, since that part depends mostly on excite amplitude, not
    // frequency). The on/off-resonance *ratio* isolates midFilter's own
    // stale-state contribution from those confounds: with midFilter
    // properly reset on every switch (this fix), that ratio is small and
    // stable; left unreset, the on-resonance case rings measurably harder
    // than the off-resonance case, inflating the ratio.
    const auto onResonanceRms = exciteThenMeasurePostSwitchSilence (500.0);  // Wool's own mid-filter center frequency
    const auto offResonanceRms = exciteThenMeasurePostSwitchSilence (20.0); // far below every voicing's mid filter

    REQUIRE (offResonanceRms > 0.0);

    const auto ratio = onResonanceRms / offResonanceRms;

    INFO ("onResonanceRms = " << onResonanceRms << ", offResonanceRms = " << offResonanceRms << ", ratio = " << ratio);
    CHECK (ratio < 2.35);
}

TEST_CASE ("Voicing: Tight (highTightHz) attenuates low frequencies monotonically, identically for every voicing", "[voicing][dsp][tight]")
{
    // docs/design-brief.md's Guarantee #5: Tight was promoted from a
    // Razor-only fixed 200Hz internal constant to a first-class,
    // voicing-independent control (v0.2.0) - this closes the gap identified
    // in the brief's "Why v1 falls short" #3 by asserting the sweep behaves
    // identically across all three voicings, not just Razor.
    //
    // The probe sits well below every swept Tight setting (including the
    // 20 Hz floor) so it stays in the HPF's stopband-widening regime across
    // the *entire* sweep, giving a clean monotonic attenuation trend rather
    // than only a partial one; a dedicated, larger local buffer (not the
    // file's shared 2048-sample numSamples) gives this specific probe
    // frequency enough periods to settle/measure reliably.
    constexpr double lowProbeFrequencyHz = 15.0;
    constexpr int tightSweepNumSamples = 16384;

    const float tightSweepHz[] = { 20.0f, 60.0f, 100.0f, 150.0f, 250.0f, 400.0f, 500.0f };

    for (const auto voicingType : allVoicings)
    {
        double previousRms = std::numeric_limits<double>::infinity();

        for (const auto tightHz : tightSweepHz)
        {
            cryp::Voicing voicing;

            juce::dsp::ProcessSpec sweepSpec;
            sweepSpec.sampleRate = testSampleRate;
            sweepSpec.maximumBlockSize = static_cast<juce::uint32> (tightSweepNumSamples);
            sweepSpec.numChannels = 1;

            voicing.prepare (sweepSpec, 1.0f); // fully wet so Tight's effect reaches the output
            voicing.setVoicing (voicingType);
            voicing.setTightHz (tightHz);
            voicing.setDrive (0.3f); // fixed, moderate drive
            voicing.setTone (1.0f);  // tone filter's pole near Nyquist, minimal own contribution

            juce::AudioBuffer<float> buffer (1, tightSweepNumSamples);
            TestHelpers::fillWithSine (buffer, testSampleRate, lowProbeFrequencyHz, 0.7f);

            juce::dsp::AudioBlock<float> block (buffer);
            voicing.process (block);

            const auto rms = TestHelpers::rms (buffer);

            INFO ("voicing index = " << static_cast<int> (voicingType) << ", tightHz = " << tightHz);
            // Monotonically non-increasing as Tight rises (more attenuation
            // of the fixed low-frequency probe) - small numerical margin for
            // floating-point noise at adjacent sweep steps.
            CHECK (rms <= previousRms + 1.0e-6);
            previousRms = rms;
        }
    }
}

TEST_CASE ("Voicing: switching voicing mid-stream never produces NaN/Inf", "[voicing][dsp][robustness]")
{
    cryp::Voicing voicing;
    voicing.prepare (makeSpec (2), 1.0f);
    voicing.setDrive (0.8f);
    voicing.setTone (0.3f);

    juce::AudioBuffer<float> buffer (2, numSamples);

    for (int iteration = 0; iteration < 12; ++iteration)
    {
        voicing.setVoicing (allVoicings[static_cast<size_t> (iteration) % allVoicings.size()]);
        TestHelpers::fillWithSine (buffer, testSampleRate, 250.0, 0.6f, static_cast<juce::int64> (iteration) * numSamples);

        juce::dsp::AudioBlock<float> block (buffer);
        CHECK_NOTHROW (voicing.process (block));
        CHECK (TestHelpers::allSamplesFinite (buffer));
    }
}

#include "dsp/MidBand.h"
#include "dsp/Voicing.h"
#include "TestHelpers.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <limits>

// v0.2.0 3-band rebuild: Mid band acceptance gates (docs/design-brief.md's
// "Mid band" section, Guarantees #4/#8/#9).
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
}

TEST_CASE ("MidBand: 0% drive is a passthrough within a small margin (no separate blend control)", "[midband][dsp]")
{
    // Unlike Voicing's highBlend, MidBand has no dry/wet mixer at all (see
    // cryp::MidBand's class docs) - "Mid Drive = 0%" must be a passthrough
    // by the shaper's own math crossfading to identity, not by a separate
    // mix parameter set to 0%.
    cryp::MidBand midBand;
    midBand.prepare (makeSpec());
    midBand.setDrive (0.0f);

    juce::AudioBuffer<float> buffer (1, numSamples);
    TestHelpers::fillWithSine (buffer, testSampleRate, 300.0, 0.6f);

    juce::AudioBuffer<float> reference;
    reference.makeCopyOf (buffer);

    juce::dsp::AudioBlock<float> block (buffer);
    midBand.process (block);

    const auto outRms = TestHelpers::rms (buffer);
    const auto refRms = TestHelpers::rms (reference);

    CHECK (juce::Decibels::gainToDecibels (outRms / refRms) == Catch::Approx (0.0).margin (0.2));
}

TEST_CASE ("MidBand: reports positive, plausible oversampling latency after prepare()", "[midband][dsp]")
{
    cryp::MidBand midBand;
    CHECK (midBand.getLatencySamples() == 0);

    midBand.prepare (makeSpec());
    CHECK (midBand.getLatencySamples() > 0);
    CHECK (midBand.getLatencySamples() < 1000);
}

TEST_CASE ("MidBand: latency matches Voicing's own oversampling latency exactly (shared delay-compensation contract)", "[midband][dsp][latency]")
{
    // CryptaAudioProcessor's shared Mid+High latency-compensation value
    // depends on MidBand and Voicing reporting numerically identical
    // latencies - see MidBand.h's class docs for why this holds by
    // construction (same oversampling factor exponent/filter type) even
    // though they are two physically separate Oversampling instances.
    cryp::MidBand midBand;
    midBand.prepare (makeSpec (2));

    cryp::Voicing voicing;
    voicing.prepare (makeSpec (2), 1.0f);

    CHECK (midBand.getLatencySamples() == voicing.getLatencySamples());
}

TEST_CASE ("MidBand: harmonic energy increases monotonically with drive", "[midband][dsp]")
{
    // Mirrors Voicing's own monotonic-harmonic-energy-with-drive test
    // pattern (VoicingTests.cpp), extended to the new Mid stage per
    // docs/design-brief.md's Guarantee #4.
    cryp::MidBand lowDriveBand;
    lowDriveBand.prepare (makeSpec());
    lowDriveBand.setDrive (0.0f);

    cryp::MidBand highDriveBand;
    highDriveBand.prepare (makeSpec());
    highDriveBand.setDrive (1.0f);

    juce::AudioBuffer<float> lowDriveBuffer (1, numSamples);
    TestHelpers::fillWithSine (lowDriveBuffer, testSampleRate, 300.0, 0.7f);
    juce::dsp::AudioBlock<float> lowDriveBlock (lowDriveBuffer);
    lowDriveBand.process (lowDriveBlock);

    juce::AudioBuffer<float> highDriveBuffer (1, numSamples);
    TestHelpers::fillWithSine (highDriveBuffer, testSampleRate, 300.0, 0.7f);
    juce::dsp::AudioBlock<float> highDriveBlock (highDriveBuffer);
    highDriveBand.process (highDriveBlock);

    juce::AudioBuffer<float> difference;
    difference.makeCopyOf (highDriveBuffer);
    difference.addFrom (0, 0, lowDriveBuffer, 0, 0, numSamples, -1.0f);

    CHECK (TestHelpers::rms (difference) > 0.01);

    // Monotonic across more than just the two extremes.
    const float driveSteps[] = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
    double previousDifferenceRms = -1.0;

    for (const auto drive : driveSteps)
    {
        cryp::MidBand band;
        band.prepare (makeSpec());
        band.setDrive (drive);

        juce::AudioBuffer<float> driven (1, numSamples);
        TestHelpers::fillWithSine (driven, testSampleRate, 300.0, 0.7f);
        juce::dsp::AudioBlock<float> drivenBlock (driven);
        band.process (drivenBlock);

        juce::AudioBuffer<float> stepDifference;
        stepDifference.makeCopyOf (driven);

        juce::AudioBuffer<float> dryReference (1, numSamples);
        TestHelpers::fillWithSine (dryReference, testSampleRate, 300.0, 0.7f);
        stepDifference.addFrom (0, 0, dryReference, 0, 0, numSamples, -1.0f);

        const auto differenceRms = TestHelpers::rms (stepDifference);

        INFO ("drive = " << drive);
        CHECK (differenceRms >= previousDifferenceRms - 1.0e-6);
        previousDifferenceRms = differenceRms;
    }
}

TEST_CASE ("MidBand: no NaN/Inf and no runaway output at extreme drive", "[midband][dsp][robustness]")
{
    cryp::MidBand midBand;
    midBand.prepare (makeSpec());
    midBand.setDrive (1.0f);

    juce::AudioBuffer<float> buffer (1, numSamples);
    TestHelpers::fillWithSine (buffer, testSampleRate, 300.0, 4.0f);

    juce::dsp::AudioBlock<float> block (buffer);
    CHECK_NOTHROW (midBand.process (block));
    CHECK (TestHelpers::allSamplesFinite (buffer));

    const auto* data = buffer.getReadPointer (0);

    for (int sample = 0; sample < numSamples; ++sample)
        CHECK (std::abs (data[sample]) <= 10.0f);
}

TEST_CASE ("MidBand: no NaN/Inf across a denormal-range sweep", "[midband][dsp][robustness]")
{
    cryp::MidBand midBand;
    midBand.prepare (makeSpec (2));
    midBand.setDrive (1.0f);

    juce::AudioBuffer<float> buffer (2, numSamples);
    const auto denormalValue = std::numeric_limits<float>::denorm_min() * 4.0f;

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* data = buffer.getWritePointer (channel);

        for (int sample = 0; sample < numSamples; ++sample)
            data[sample] = (sample % 2 == 0) ? denormalValue : -denormalValue;
    }

    juce::dsp::AudioBlock<float> block (buffer);
    CHECK_NOTHROW (midBand.process (block));
    CHECK (TestHelpers::allSamplesFinite (buffer));
}

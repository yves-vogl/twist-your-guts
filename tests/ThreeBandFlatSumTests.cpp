#include "dsp/Crossover.h"
#include "dsp/PhaseAlignFilter.h"
#include "dsp/SplitGap.h"
#include "TestHelpers.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

// v0.2.0 3-band rebuild (docs/design-brief.md's Guarantee #1): extends
// CrossoverTests.cpp's 2-band flat-sum property to the cascaded 3-band
// topology - two cryp::Crossover instances in series (Low, then Mid/High)
// must still reconstruct the original signal within +-0.1 dB across the
// band, for a swept combination of splitLowHz/splitHighHz settings,
// including at the minimum-gap clamp's own boundary.
//
// A naive cascade of two independent 2-way LR4 crossovers does NOT
// flat-sum on its own (confirmed empirically while building this test file:
// deviations up to -10 dB at close splitLowHz/splitHighHz ratios) - the
// harness below applies the same cryp::PhaseAlignFilter compensation
// PluginProcessor.cpp's processChunk() applies to the Low band, exactly
// mirroring the real production signal path. See PhaseAlignFilter.h's
// class-level docs for the algebraic proof of why this specific
// compensation (an allpass matching midHighSplit's own cutoff, applied to
// the Low band before the final sum) makes the three-way sum exactly flat.
namespace
{
    constexpr double testSampleRate = 48000.0;
    // Generous enough to settle three cascaded 4th-order IIR stages
    // (lowSplit + midHighSplit + the lowBandPhaseAlign compensation, each
    // its own transient) even for the swept-combination test's lowest probe
    // frequencies (as low as splitLowHz * 0.5, down to 30 Hz - one period is
    // 1600 samples at 48kHz, so settleSamples below covers >10 periods).
    constexpr int numSamples = 32768;
    constexpr int settleSamples = 16384;

    // Runs a steady-state probe tone through the cascaded Low -> Mid/High
    // split and returns the level difference, in dB, between the
    // low+mid+high sum and the original input signal, measured over the
    // settled (post-transient) tail.
    double measureThreeBandFlatSumDeviationDb (double probeFrequencyHz, float splitLowHz, float requestedSplitHighHz)
    {
        cryp::Crossover lowSplit;
        cryp::Crossover midHighSplit;
        cryp::PhaseAlignFilter lowBandPhaseAlign;

        juce::dsp::ProcessSpec spec;
        spec.sampleRate = testSampleRate;
        spec.maximumBlockSize = static_cast<juce::uint32> (numSamples);
        spec.numChannels = 1;

        lowSplit.prepare (spec);
        lowSplit.setCutoffFrequency (splitLowHz);

        const auto effectiveSplitHighHz = cryp::clampSplitHighHz (splitLowHz, requestedSplitHighHz);

        midHighSplit.prepare (spec);
        midHighSplit.setCutoffFrequency (effectiveSplitHighHz);

        lowBandPhaseAlign.prepare (spec);
        lowBandPhaseAlign.setCutoffFrequency (effectiveSplitHighHz);

        juce::AudioBuffer<float> input (1, numSamples);
        TestHelpers::fillWithSine (input, testSampleRate, probeFrequencyHz, 1.0f);

        juce::AudioBuffer<float> low (1, numSamples);
        juce::AudioBuffer<float> remainder (1, numSamples);
        juce::AudioBuffer<float> mid (1, numSamples);
        juce::AudioBuffer<float> high (1, numSamples);

        const juce::dsp::AudioBlock<const float> inputBlock (input);
        juce::dsp::AudioBlock<float> lowBlock (low);
        juce::dsp::AudioBlock<float> remainderBlock (remainder);
        juce::dsp::AudioBlock<float> midBlock (mid);
        juce::dsp::AudioBlock<float> highBlock (high);

        lowSplit.process (inputBlock, lowBlock, remainderBlock);
        midHighSplit.process (juce::dsp::AudioBlock<const float> (remainder), midBlock, highBlock);
        lowBandPhaseAlign.process (lowBlock);

        double sumOfSquaresInput = 0.0;
        double sumOfSquaresSum = 0.0;
        int countedSamples = 0;

        const auto* inData = input.getReadPointer (0);
        const auto* lowData = low.getReadPointer (0);
        const auto* midData = mid.getReadPointer (0);
        const auto* highData = high.getReadPointer (0);

        for (int i = settleSamples; i < numSamples; ++i)
        {
            const auto summed = lowData[i] + midData[i] + highData[i];
            sumOfSquaresInput += static_cast<double> (inData[i]) * static_cast<double> (inData[i]);
            sumOfSquaresSum += static_cast<double> (summed) * static_cast<double> (summed);
            ++countedSamples;
        }

        REQUIRE (countedSamples > 0);

        const auto inputRms = std::sqrt (sumOfSquaresInput / static_cast<double> (countedSamples));
        const auto sumRms = std::sqrt (sumOfSquaresSum / static_cast<double> (countedSamples));

        REQUIRE (inputRms > 0.0);

        return juce::Decibels::gainToDecibels (sumRms / inputRms);
    }
}

TEST_CASE ("Three-band cascaded LR4: low+mid+high sum reconstructs the input within +-0.1 dB at the v0.2.0 defaults", "[crossover][dsp][three-band]")
{
    constexpr float splitLowHz = 120.0f;
    constexpr float splitHighHz = 600.0f;

    // Spans well below Split Low, at Split Low, inside the Mid band, at
    // Split High (the hardest case for either crossover stage), and well
    // above Split High.
    const double probeFrequenciesHz[] = {
        30.0, 60.0, 100.0,
        120.0, // exactly at Split Low
        200.0, 400.0,
        600.0, // exactly at Split High
        1000.0, 2000.0, 5000.0, 10000.0, 15000.0, 19000.0
    };

    for (const auto probeFrequencyHz : probeFrequenciesHz)
    {
        INFO ("probe frequency = " << probeFrequencyHz << " Hz");
        const auto deviationDb = measureThreeBandFlatSumDeviationDb (probeFrequencyHz, splitLowHz, splitHighHz);
        CHECK (deviationDb == Catch::Approx (0.0).margin (0.1));
    }
}

TEST_CASE ("Three-band cascaded LR4: flat sum holds across a swept combination of splitLowHz/splitHighHz settings", "[crossover][dsp][three-band]")
{
    struct SplitCombo
    {
        float splitLowHz;
        float splitHighHz;
    };

    const SplitCombo combos[] = {
        { 60.0f, 300.0f },   // both at their range floors
        { 90.0f, 500.0f },   // Sub Lock preset
        { 180.0f, 900.0f },  // Cut Through preset
        { 400.0f, 2000.0f }, // both at their range ceilings
    };

    for (const auto& combo : combos)
    {
        const double probeFrequenciesHz[] = {
            combo.splitLowHz * 0.5, combo.splitLowHz,
            (combo.splitLowHz + combo.splitHighHz) * 0.5,
            combo.splitHighHz, combo.splitHighHz * 2.0
        };

        for (const auto probeFrequencyHz : probeFrequenciesHz)
        {
            INFO ("splitLowHz = " << combo.splitLowHz << ", splitHighHz = " << combo.splitHighHz
                                   << ", probe = " << probeFrequencyHz << " Hz");
            const auto deviationDb = measureThreeBandFlatSumDeviationDb (probeFrequencyHz, combo.splitLowHz, combo.splitHighHz);
            CHECK (deviationDb == Catch::Approx (0.0).margin (0.1));
        }
    }
}

TEST_CASE ("Three-band cascaded LR4: flat sum holds exactly at the minimum-gap clamp's own boundary", "[crossover][dsp][three-band][splitgap]")
{
    // Requests splitHighHz pinned to splitLowHz itself (a degenerate,
    // unclamped zero-width Mid band) - clampSplitHighHz() raises the
    // *effective* splitHighHz fed to midHighSplit up to the 1/3-octave
    // floor (see SplitGapTests.cpp), and the flat-sum property must still
    // hold at that clamped boundary, not just away from it.
    constexpr float splitLowHz = 250.0f;
    const auto effectiveSplitHighHz = cryp::clampSplitHighHz (splitLowHz, splitLowHz);

    const double probeFrequenciesHz[] = {
        splitLowHz * 0.5, splitLowHz,
        effectiveSplitHighHz, // exactly at the clamped boundary
        effectiveSplitHighHz * 2.0
    };

    for (const auto probeFrequencyHz : probeFrequenciesHz)
    {
        INFO ("probe frequency = " << probeFrequencyHz << " Hz");
        // Pass the raw (unclamped) requested splitHighHz value
        // (splitLowHz itself) - measureThreeBandFlatSumDeviationDb()
        // applies clampSplitHighHz() internally, matching production code.
        const auto deviationDb = measureThreeBandFlatSumDeviationDb (probeFrequencyHz, splitLowHz, splitLowHz);
        CHECK (deviationDb == Catch::Approx (0.0).margin (0.1));
    }
}

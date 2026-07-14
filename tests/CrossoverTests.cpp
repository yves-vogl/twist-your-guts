#include "dsp/Crossover.h"
#include "TestHelpers.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>

// Issue #8: the LR4 (Linkwitz-Riley 4th order) crossover's defining property
// is that summing its low and high outputs at unity reconstructs the input's
// magnitude spectrum flat (no notch or bump at the crossover point), unlike
// a naive pair of same-order Butterworth filters. This file is the flat-sum
// acceptance gate: it probes many frequencies spanning well below, at, and
// well above the crossover point and asserts the reconstructed level stays
// within +-0.1 dB of the original at every one of them.
namespace
{
    constexpr double testSampleRate = 48000.0;
    constexpr float crossoverHz = 250.0f;
    constexpr int numSamples = 8192;

    // Skip the first ~43ms (2048 samples @ 48kHz) of each probe so the LR4
    // filter's transient response has settled before RMS is measured -
    // otherwise the filter's turn-on transient would pollute the level
    // measurement of a steady-state tone.
    constexpr int settleSamples = 2048;

    // Runs one steady-state probe tone through the crossover and returns the
    // level difference, in dB, between the low+high sum and the original
    // input signal, measured over the settled (post-transient) tail.
    double measureFlatSumDeviationDb (double probeFrequencyHz)
    {
        cryp::Crossover crossover;

        juce::dsp::ProcessSpec spec;
        spec.sampleRate = testSampleRate;
        spec.maximumBlockSize = static_cast<juce::uint32> (numSamples);
        spec.numChannels = 1;

        crossover.prepare (spec);
        crossover.setCutoffFrequency (crossoverHz);

        juce::AudioBuffer<float> input (1, numSamples);
        TestHelpers::fillWithSine (input, testSampleRate, probeFrequencyHz, 1.0f);

        juce::AudioBuffer<float> low (1, numSamples);
        juce::AudioBuffer<float> high (1, numSamples);

        const juce::dsp::AudioBlock<const float> inputBlock (input);
        juce::dsp::AudioBlock<float> lowBlock (low);
        juce::dsp::AudioBlock<float> highBlock (high);

        crossover.process (inputBlock, lowBlock, highBlock);

        double sumOfSquaresInput = 0.0;
        double sumOfSquaresSum = 0.0;
        int countedSamples = 0;

        const auto* inData = input.getReadPointer (0);
        const auto* lowData = low.getReadPointer (0);
        const auto* highData = high.getReadPointer (0);

        for (int i = settleSamples; i < numSamples; ++i)
        {
            const auto summed = lowData[i] + highData[i];
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

TEST_CASE ("LR4 crossover: low+high sum reconstructs the input within +-0.1 dB across the band", "[crossover][dsp]")
{
    // Deliberately spans: well below the 250 Hz crossover, the low end and
    // high end of the crossoverFreq parameter's own 60 Hz-1000 Hz range,
    // exactly at the crossover point (the hardest case for any crossover
    // topology to keep flat), and on up through the rest of the audible
    // band. Not a single lucky frequency.
    const double probeFrequenciesHz[] = {
        30.0, 60.0, 100.0, 150.0, 200.0,
        250.0, // exactly at the crossover point
        300.0, 400.0, 600.0, 1000.0, 2000.0,
        5000.0, 10000.0, 15000.0, 19000.0
    };

    for (const auto probeFrequencyHz : probeFrequenciesHz)
    {
        INFO ("probe frequency = " << probeFrequencyHz << " Hz");
        const auto deviationDb = measureFlatSumDeviationDb (probeFrequencyHz);
        CHECK (deviationDb == Catch::Approx (0.0).margin (0.1));
    }
}

TEST_CASE ("LR4 crossover: low+high sum stays flat when the crossover frequency is retuned", "[crossover][dsp]")
{
    // Same flat-sum property, but checked at a non-default crossover point
    // to guard against a regression that only happens to look flat at the
    // 250 Hz default (e.g. a hardcoded coefficient).
    cryp::Crossover crossover;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = testSampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (numSamples);
    spec.numChannels = 1;

    crossover.prepare (spec);
    crossover.setCutoffFrequency (600.0f);

    CHECK (crossover.getCutoffFrequency() == Catch::Approx (600.0f));

    const double probeFrequenciesHz[] = { 100.0, 400.0, 600.0, 900.0, 4000.0, 12000.0 };

    for (const auto probeFrequencyHz : probeFrequenciesHz)
    {
        juce::AudioBuffer<float> input (1, numSamples);
        TestHelpers::fillWithSine (input, testSampleRate, probeFrequencyHz, 1.0f);

        juce::AudioBuffer<float> low (1, numSamples);
        juce::AudioBuffer<float> high (1, numSamples);

        const juce::dsp::AudioBlock<const float> inputBlock (input);
        juce::dsp::AudioBlock<float> lowBlock (low);
        juce::dsp::AudioBlock<float> highBlock (high);

        crossover.process (inputBlock, lowBlock, highBlock);

        double sumOfSquaresInput = 0.0;
        double sumOfSquaresSum = 0.0;
        int countedSamples = 0;

        const auto* inData = input.getReadPointer (0);
        const auto* lowData = low.getReadPointer (0);
        const auto* highData = high.getReadPointer (0);

        for (int i = settleSamples; i < numSamples; ++i)
        {
            const auto summed = lowData[i] + highData[i];
            sumOfSquaresInput += static_cast<double> (inData[i]) * static_cast<double> (inData[i]);
            sumOfSquaresSum += static_cast<double> (summed) * static_cast<double> (summed);
            ++countedSamples;
        }

        const auto inputRms = std::sqrt (sumOfSquaresInput / static_cast<double> (countedSamples));
        const auto sumRms = std::sqrt (sumOfSquaresSum / static_cast<double> (countedSamples));

        INFO ("probe frequency = " << probeFrequencyHz << " Hz");
        CHECK (juce::Decibels::gainToDecibels (sumRms / inputRms) == Catch::Approx (0.0).margin (0.1));
    }
}

TEST_CASE ("LR4 crossover: no NaN/Inf across a denormal-range sweep", "[crossover][dsp][robustness]")
{
    cryp::Crossover crossover;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = testSampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (numSamples);
    spec.numChannels = 2;

    crossover.prepare (spec);
    crossover.setCutoffFrequency (crossoverHz);

    juce::AudioBuffer<float> input (2, numSamples);
    const auto denormalValue = std::numeric_limits<float>::denorm_min() * 4.0f;

    for (int channel = 0; channel < input.getNumChannels(); ++channel)
    {
        auto* data = input.getWritePointer (channel);

        for (int sample = 0; sample < numSamples; ++sample)
            data[sample] = (sample % 2 == 0) ? denormalValue : -denormalValue;
    }

    juce::AudioBuffer<float> low (2, numSamples);
    juce::AudioBuffer<float> high (2, numSamples);

    const juce::dsp::AudioBlock<const float> inputBlock (input);
    juce::dsp::AudioBlock<float> lowBlock (low);
    juce::dsp::AudioBlock<float> highBlock (high);

    CHECK_NOTHROW (crossover.process (inputBlock, lowBlock, highBlock));
    CHECK (TestHelpers::allSamplesFinite (low));
    CHECK (TestHelpers::allSamplesFinite (high));
}

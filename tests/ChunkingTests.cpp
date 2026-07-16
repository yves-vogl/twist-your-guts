#include "PluginProcessor.h"
#include "params/ParameterIds.h"
#include "TestHelpers.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

// Issue #59: processBlock()'s chunking logic (PluginProcessor.cpp) splits a
// host block into sub-blocks of at most `preparedBlockSize` samples
// specifically to avoid overrunning lowBandBuffer/highBandBuffer and the
// fixed-size internal buffers of juce::dsp::Oversampling/DryWetMixer inside
// Voicing, all sized via prepare(spec.maximumBlockSize). Every other test in
// this suite only ever calls processBlock() with a buffer sized at or below
// what prepareToPlay() promised, so the multi-iteration chunking branch
// (executing more than once per processBlock() call) was never exercised by
// CI. This test deliberately hands processBlock() a single host block larger
// than promised - and not a whole multiple of preparedBlockSize, so the
// final short remainder chunk is exercised too - and checks that the result
// is sample-identical (within floating-point tolerance) to feeding the exact
// same signal through in properly host-sized sub-blocks, which is the
// documented/already-covered path. This is a Release-safe path (no
// jassert-only branching in the code under test), so the comparison is valid
// in both Debug and Release builds.
namespace
{
    constexpr double testSampleRate = 48000.0;
    constexpr int preparedBlockSize = 256;

    // Not a whole multiple of preparedBlockSize, and spans more than two
    // full chunks (256 + 256 + 256 + 37), so the chunking loop executes
    // multiple times including one final short remainder chunk.
    constexpr int oversizedBlockSamples = preparedBlockSize * 3 + 37;

    void enableEveryStage (CryptaAudioProcessor& processor)
    {
        auto setParam = [&] (const char* id, float value01)
        {
            auto* parameter = processor.apvts.getParameter (id);
            REQUIRE (parameter != nullptr);
            parameter->setValueNotifyingHost (value01);
        };

        // Full chain (gate, crossover, low/high bands, EQ, IR loader) all
        // participating, not just the always-on stages.
        setParam (ParamIDs::gateEnabled, 1.0f);
        setParam (ParamIDs::eqEnabled, 1.0f);
        setParam (ParamIDs::irEnabled, 1.0f);
    }
}

TEST_CASE ("processBlock(): a host block larger than prepareToPlay's samplesPerBlock chunks identically to feeding the same signal in properly-sized sub-blocks", "[processblock][chunking]")
{
    CryptaAudioProcessor chunkedProcessor;
    chunkedProcessor.prepareToPlay (testSampleRate, preparedBlockSize);
    enableEveryStage (chunkedProcessor);

    CryptaAudioProcessor referenceProcessor;
    referenceProcessor.prepareToPlay (testSampleRate, preparedBlockSize);
    enableEveryStage (referenceProcessor);

    juce::MidiBuffer midi;

    // Reference: the exact same continuous signal (phase-continuous across
    // sub-block boundaries via the absolute sample offset), fed through in
    // separate processBlock() calls each sized at or below
    // preparedBlockSize - the host-behaviour-as-documented path every other
    // test in this suite already covers.
    juce::AudioBuffer<float> referenceOutput (2, oversizedBlockSamples);

    for (int offset = 0; offset < oversizedBlockSamples; offset += preparedBlockSize)
    {
        const auto chunkLength = juce::jmin (preparedBlockSize, oversizedBlockSamples - offset);
        juce::AudioBuffer<float> chunk (2, chunkLength);
        TestHelpers::fillWithSine (chunk, testSampleRate, 220.0, 0.6f, offset);

        referenceProcessor.processBlock (chunk, midi);

        for (int channel = 0; channel < 2; ++channel)
            referenceOutput.copyFrom (channel, offset, chunk, channel, 0, chunkLength);
    }

    // Under test: the exact same signal, handed to processBlock() as a
    // single oversized call - only safe/correct if the internal chunking
    // loop (processBlock()/processChunk() in PluginProcessor.cpp) does its
    // job.
    juce::AudioBuffer<float> chunkedOutput (2, oversizedBlockSamples);
    TestHelpers::fillWithSine (chunkedOutput, testSampleRate, 220.0, 0.6f);

    CHECK_NOTHROW (chunkedProcessor.processBlock (chunkedOutput, midi));
    CHECK (TestHelpers::allSamplesFinite (chunkedOutput));

    for (int channel = 0; channel < 2; ++channel)
    {
        const auto* referenceData = referenceOutput.getReadPointer (channel);
        const auto* chunkedData = chunkedOutput.getReadPointer (channel);

        for (int sample = 0; sample < oversizedBlockSamples; ++sample)
        {
            INFO ("channel = " << channel << ", sample = " << sample);
            CHECK (chunkedData[sample] == Catch::Approx (referenceData[sample]).margin (1.0e-5f));
        }
    }
}

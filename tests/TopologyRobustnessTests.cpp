#include "PluginProcessor.h"
#include "params/ParameterIds.h"
#include "TestHelpers.h"

#include <catch2/catch_test_macros.hpp>

// v0.2.0 3-band rebuild (docs/design-brief.md's Guarantee #8): extends the
// existing robustness-sweep pattern (RobustnessTests.cpp,
// SampleRateAndRobustnessTests.cpp) to every new/changed control -
// splitLowHz, splitHighHz (including their minimum-gap clamp boundary),
// midDrive, and highTightHz - combined with extreme Drive/Level/EQ settings,
// asserting no NaN/Inf ever propagates.
namespace
{
    constexpr double testSampleRate = 48000.0;
    constexpr int testBlockSize = 512;

    void setParam (CryptaAudioProcessor& processor, const char* id, float plainValue)
    {
        auto* parameter = processor.apvts.getParameter (id);
        REQUIRE (parameter != nullptr);
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (plainValue));
    }

    void setBoolParam (CryptaAudioProcessor& processor, const char* id, bool value)
    {
        auto* parameter = processor.apvts.getParameter (id);
        REQUIRE (parameter != nullptr);
        parameter->setValueNotifyingHost (value ? 1.0f : 0.0f);
    }
}

TEST_CASE ("Topology robustness: extreme splitLowHz/splitHighHz combinations (incl. the minimum-gap boundary) never produce NaN/Inf", "[robustness][topology]")
{
    struct SplitCombo
    {
        float splitLowHz;
        float splitHighHz;
    };

    const SplitCombo combos[] = {
        { 60.0f, 300.0f },   // both range floors
        { 400.0f, 2000.0f }, // both range ceilings
        { 400.0f, 300.0f },  // inverted - exercises the minimum-gap clamp hard
        { 250.0f, 250.0f },  // pinned equal - degenerate zero-gap request
        { 60.0f, 2000.0f },  // widest possible Mid band
    };

    for (const auto& combo : combos)
    {
        CryptaAudioProcessor processor;
        processor.prepareToPlay (testSampleRate, testBlockSize);

        setParam (processor, ParamIDs::splitLowHz, combo.splitLowHz);
        setParam (processor, ParamIDs::splitHighHz, combo.splitHighHz);

        // Extreme Drive/Level/EQ settings stacked on top, per the brief's
        // Guarantee #8 wording.
        setParam (processor, ParamIDs::midDrive, 100.0f);
        setParam (processor, ParamIDs::highTightHz, 500.0f);
        setParam (processor, ParamIDs::highDrive, 100.0f);
        setParam (processor, ParamIDs::lowLevel, 12.0f);
        setParam (processor, ParamIDs::midLevel, 12.0f);
        setParam (processor, ParamIDs::highLevel, 12.0f);
        setBoolParam (processor, ParamIDs::eqEnabled, true);
        setParam (processor, ParamIDs::eqLowShelfGain, 18.0f);
        setParam (processor, ParamIDs::eqPeak1Gain, -18.0f);
        setParam (processor, ParamIDs::eqPeak2Gain, 18.0f);
        setParam (processor, ParamIDs::eqHighShelfGain, -18.0f);

        juce::AudioBuffer<float> buffer (2, testBlockSize);
        juce::MidiBuffer midi;

        for (int i = 0; i < 8; ++i)
        {
            TestHelpers::fillWithSine (buffer, testSampleRate, 220.0, 0.9f, static_cast<juce::int64> (i) * testBlockSize);

            INFO ("splitLowHz = " << combo.splitLowHz << ", splitHighHz = " << combo.splitHighHz << ", block = " << i);
            CHECK_NOTHROW (processor.processBlock (buffer, midi));
            CHECK (TestHelpers::allSamplesFinite (buffer));
        }
    }
}

TEST_CASE ("Topology robustness: extreme midDrive/highTightHz sweep at extreme splits never produces NaN/Inf", "[robustness][topology]")
{
    const float midDriveValues[] = { 0.0f, 50.0f, 100.0f };
    const float highTightHzValues[] = { 20.0f, 100.0f, 500.0f };

    for (const auto midDrive : midDriveValues)
    {
        for (const auto highTightHz : highTightHzValues)
        {
            CryptaAudioProcessor processor;
            processor.prepareToPlay (testSampleRate, testBlockSize);

            setParam (processor, ParamIDs::splitLowHz, 400.0f);  // range ceiling
            setParam (processor, ParamIDs::splitHighHz, 300.0f); // range floor (below splitLowHz - exercises clamp)
            setParam (processor, ParamIDs::midDrive, midDrive);
            setParam (processor, ParamIDs::highTightHz, highTightHz);
            setParam (processor, ParamIDs::highDrive, 100.0f);

            juce::AudioBuffer<float> buffer (2, testBlockSize);
            juce::MidiBuffer midi;

            TestHelpers::fillWithSine (buffer, testSampleRate, 150.0, 0.9f);

            INFO ("midDrive = " << midDrive << ", highTightHz = " << highTightHz);
            CHECK_NOTHROW (processor.processBlock (buffer, midi));
            CHECK (TestHelpers::allSamplesFinite (buffer));
        }
    }
}

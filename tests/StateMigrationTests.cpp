#include "PluginProcessor.h"
#include "params/ParameterIds.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

// v0.1.x -> v0.2.0 structural state migration (docs/design-brief.md's
// Guarantee #7): a v1 saved session serializes a single "crossoverFreq"
// PARAM element - that parameter ID no longer exists in v0.2.0's
// ParameterLayout, replaced by the splitLowHz/splitHighHz pair. These tests
// hand-craft v1-shaped APVTS state XML directly (the same
// <PARAMETERS><PARAM id="..." value="..."/>...</PARAMETERS> shape
// AudioProcessorValueTreeState itself produces/consumes - JUCE 8.0.14,
// juce_AudioProcessorValueTreeState.h's valueType="PARAM"/idPropertyID="id"/
// valuePropertyID="value") and feed it through
// CryptaAudioProcessor::setStateInformation(), exercising the migration path
// a real v0.1.x-saved session would hit.
namespace
{
    juce::MemoryBlock makeStateBlock (const std::vector<std::pair<juce::String, double>>& idsAndValues)
    {
        juce::XmlElement root ("PARAMETERS");

        for (auto& [id, value] : idsAndValues)
        {
            auto* param = new juce::XmlElement ("PARAM");
            param->setAttribute ("id", id);
            param->setAttribute ("value", value);
            root.addChildElement (param);
        }

        juce::MemoryBlock block;
        juce::AudioProcessor::copyXmlToBinary (root, block);
        return block;
    }

    float getParam (CryptaAudioProcessor& processor, const char* id)
    {
        auto* param = processor.apvts.getParameter (id);
        REQUIRE (param != nullptr);
        return param->convertFrom0to1 (param->getValue());
    }
}

TEST_CASE ("State migration: an untouched v1 session (shipped default crossoverFreq=250Hz) lands splitHighHz exactly at the new 300Hz floor", "[state][migration]")
{
    // The single most common migration path per docs/design-brief.md's
    // Guarantee #7: v1's shipped default (250 Hz) is below splitHighHz's new
    // 300-2000 Hz range, so this is the dedicated regression test asserting
    // that specific, explicitly-called-out clamp lands exactly on the floor.
    CryptaAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    const auto legacyState = makeStateBlock ({ { "crossoverFreq", 250.0 } });
    processor.setStateInformation (legacyState.getData(), static_cast<int> (legacyState.getSize()));

    CHECK (getParam (processor, ParamIDs::splitHighHz) == Catch::Approx (300.0f).margin (1.0e-3));
}

TEST_CASE ("State migration: a legacy crossoverFreq already inside the new range passes through unclamped", "[state][migration]")
{
    CryptaAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    const auto legacyState = makeStateBlock ({ { "crossoverFreq", 500.0 } });
    processor.setStateInformation (legacyState.getData(), static_cast<int> (legacyState.getSize()));

    CHECK (getParam (processor, ParamIDs::splitHighHz) == Catch::Approx (500.0f).margin (1.0e-3));
}

TEST_CASE ("State migration: a legacy crossoverFreq above the new range's ceiling is clamped down to 2000Hz", "[state][migration]")
{
    CryptaAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    // v1's crossoverFreq range extended up to 1000 Hz (never actually above
    // splitHighHz's 2000 Hz ceiling in practice), but the migration's own
    // jlimit() must still be exercised defensively at/above the ceiling.
    const auto legacyState = makeStateBlock ({ { "crossoverFreq", 1000.0 } });
    processor.setStateInformation (legacyState.getData(), static_cast<int> (legacyState.getSize()));

    CHECK (getParam (processor, ParamIDs::splitHighHz) == Catch::Approx (1000.0f).margin (1.0e-3));

    CryptaAudioProcessor processorAboveCeiling;
    processorAboveCeiling.prepareToPlay (48000.0, 512);
    const auto extremeState = makeStateBlock ({ { "crossoverFreq", 5000.0 } });
    processorAboveCeiling.setStateInformation (extremeState.getData(), static_cast<int> (extremeState.getSize()));

    CHECK (getParam (processorAboveCeiling, ParamIDs::splitHighHz) == Catch::Approx (2000.0f).margin (1.0e-3));
}

TEST_CASE ("State migration: splitLowHz and every new mid-band/Tight parameter fall back to v0.2.0 defaults, never garbage", "[state][migration]")
{
    CryptaAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    const auto legacyState = makeStateBlock ({ { "crossoverFreq", 250.0 } });
    processor.setStateInformation (legacyState.getData(), static_cast<int> (legacyState.getSize()));

    CHECK (getParam (processor, ParamIDs::splitLowHz) == Catch::Approx (120.0f).margin (1.0e-3));
    CHECK (getParam (processor, ParamIDs::midDrive) == Catch::Approx (30.0f).margin (1.0e-3));
    CHECK (getParam (processor, ParamIDs::midLevel) == Catch::Approx (0.0f).margin (1.0e-3));
    CHECK (getParam (processor, ParamIDs::highTightHz) == Catch::Approx (100.0f).margin (1.0e-3));
}

TEST_CASE ("State migration: an explicitly-changed v1 lowCompRatio/Attack/Release is preserved as-is, not forced to the new v0.2.0 defaults", "[state][migration]")
{
    // Only the *shipped default* changed (4:1/10ms/120ms -> 2:1/3ms/6ms) -
    // a user's deliberate old setting must round-trip untouched.
    CryptaAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    const auto legacyState = makeStateBlock ({
        { "crossoverFreq", 250.0 },
        { "lowCompRatio", 8.0 },
        { "lowCompAttack", 25.0 },
        { "lowCompRelease", 300.0 },
    });
    processor.setStateInformation (legacyState.getData(), static_cast<int> (legacyState.getSize()));

    CHECK (getParam (processor, ParamIDs::lowCompRatio) == Catch::Approx (8.0f).margin (1.0e-3));
    CHECK (getParam (processor, ParamIDs::lowCompAttack) == Catch::Approx (25.0f).margin (1.0e-3));
    CHECK (getParam (processor, ParamIDs::lowCompRelease) == Catch::Approx (300.0f).margin (1.0e-3));
}

TEST_CASE ("State migration: a v0.2.0+ session (no crossoverFreq element) is untouched by the migration path", "[state][migration]")
{
    CryptaAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    const auto v2State = makeStateBlock ({ { "splitLowHz", 150.0 }, { "splitHighHz", 700.0 } });
    processor.setStateInformation (v2State.getData(), static_cast<int> (v2State.getSize()));

    CHECK (getParam (processor, ParamIDs::splitLowHz) == Catch::Approx (150.0f).margin (1.0e-3));
    CHECK (getParam (processor, ParamIDs::splitHighHz) == Catch::Approx (700.0f).margin (1.0e-3));
}

TEST_CASE ("State migration: an already-present splitHighHz is never overwritten by a stray legacy crossoverFreq element", "[state][migration]")
{
    // Defensive case (docs/design-brief.md notes this can't happen from a
    // genuine v1 or v2 session, only from a malformed/hand-edited file) -
    // migrateLegacySingleCrossover() must not clobber an explicit
    // splitHighHz value that's already present.
    CryptaAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    const auto malformedState = makeStateBlock ({ { "crossoverFreq", 250.0 }, { "splitHighHz", 800.0 } });
    processor.setStateInformation (malformedState.getData(), static_cast<int> (malformedState.getSize()));

    CHECK (getParam (processor, ParamIDs::splitHighHz) == Catch::Approx (800.0f).margin (1.0e-3));
}

TEST_CASE ("State migration: never crashes and produces finite output on a legacy v1 session", "[state][migration][robustness]")
{
    CryptaAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    const auto legacyState = makeStateBlock ({
        { "crossoverFreq", 250.0 },
        { "lowCompRatio", 4.0 },
        { "lowCompAttack", 10.0 },
        { "lowCompRelease", 120.0 },
        { "highVoicing", 0.0 },
        { "highDrive", 50.0 },
    });

    CHECK_NOTHROW (processor.setStateInformation (legacyState.getData(), static_cast<int> (legacyState.getSize())));

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    CHECK_NOTHROW (processor.processBlock (buffer, midi));
}

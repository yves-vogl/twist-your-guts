#include "PluginProcessor.h"
#include "params/ParameterIds.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE ("State round-trip preserves non-default parameter values", "[state]")
{
    CryptaAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    auto* inputGainParam = processor.apvts.getParameter (ParamIDs::inputGain);
    auto* outputGainParam = processor.apvts.getParameter (ParamIDs::outputGain);
    auto* bypassParam = processor.apvts.getParameter (ParamIDs::bypass);

    REQUIRE (inputGainParam != nullptr);
    REQUIRE (outputGainParam != nullptr);
    REQUIRE (bypassParam != nullptr);

    inputGainParam->setValueNotifyingHost (inputGainParam->convertTo0to1 (12.0f));
    outputGainParam->setValueNotifyingHost (outputGainParam->convertTo0to1 (-8.5f));
    bypassParam->setValueNotifyingHost (1.0f);

    const auto savedInputValue = inputGainParam->getValue();
    const auto savedOutputValue = outputGainParam->getValue();
    const auto savedBypassValue = bypassParam->getValue();

    juce::MemoryBlock savedState;
    processor.getStateInformation (savedState);
    REQUIRE (savedState.getSize() > 0);

    // Reset every parameter back to its default before restoring, so the
    // round-trip assertion below can't pass by accident.
    inputGainParam->setValueNotifyingHost (inputGainParam->getDefaultValue());
    outputGainParam->setValueNotifyingHost (outputGainParam->getDefaultValue());
    bypassParam->setValueNotifyingHost (bypassParam->getDefaultValue());

    REQUIRE (inputGainParam->getValue() != Catch::Approx (savedInputValue));
    REQUIRE (outputGainParam->getValue() != Catch::Approx (savedOutputValue));
    REQUIRE (bypassParam->getValue() != Catch::Approx (savedBypassValue));

    processor.setStateInformation (savedState.getData(), static_cast<int> (savedState.getSize()));

    CHECK (inputGainParam->getValue() == Catch::Approx (savedInputValue).margin (1e-6));
    CHECK (outputGainParam->getValue() == Catch::Approx (savedOutputValue).margin (1e-6));
    CHECK (bypassParam->getValue() == Catch::Approx (savedBypassValue).margin (1e-6));
}

TEST_CASE ("State round-trip preserves non-default values of the full v1.0 parameter set", "[state][parameters]")
{
    CryptaAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    // Exercise a representative float (log-skewed frequency), a bool, and
    // the AudioParameterChoice - one of each parameter kind declared by the
    // full v1.0 layout, not just the M0 IO parameters.
    auto* crossoverParam = processor.apvts.getParameter (ParamIDs::crossoverFreq);
    auto* gateEnabledParam = processor.apvts.getParameter (ParamIDs::gateEnabled);
    auto* voicingParam = processor.apvts.getParameter (ParamIDs::highVoicing);

    REQUIRE (crossoverParam != nullptr);
    REQUIRE (gateEnabledParam != nullptr);
    REQUIRE (voicingParam != nullptr);

    crossoverParam->setValueNotifyingHost (crossoverParam->convertTo0to1 (400.0f));
    gateEnabledParam->setValueNotifyingHost (1.0f);
    // "Razor" is index 2 of {Gnaw, Wool, Razor}; normalise via the choice
    // parameter's own range so this doesn't hardcode the 0-1 step size.
    voicingParam->setValueNotifyingHost (voicingParam->convertTo0to1 (2.0f));

    const auto savedCrossoverValue = crossoverParam->getValue();
    const auto savedGateEnabledValue = gateEnabledParam->getValue();
    const auto savedVoicingValue = voicingParam->getValue();

    juce::MemoryBlock savedState;
    processor.getStateInformation (savedState);
    REQUIRE (savedState.getSize() > 0);

    // Reset back to defaults before restoring, so the round-trip assertion
    // below can't pass by accident.
    crossoverParam->setValueNotifyingHost (crossoverParam->getDefaultValue());
    gateEnabledParam->setValueNotifyingHost (gateEnabledParam->getDefaultValue());
    voicingParam->setValueNotifyingHost (voicingParam->getDefaultValue());

    REQUIRE (crossoverParam->getValue() != Catch::Approx (savedCrossoverValue));
    REQUIRE (gateEnabledParam->getValue() != Catch::Approx (savedGateEnabledValue));
    REQUIRE (voicingParam->getValue() != Catch::Approx (savedVoicingValue));

    processor.setStateInformation (savedState.getData(), static_cast<int> (savedState.getSize()));

    CHECK (crossoverParam->getValue() == Catch::Approx (savedCrossoverValue).margin (1e-6));
    CHECK (gateEnabledParam->getValue() == Catch::Approx (savedGateEnabledValue).margin (1e-6));
    CHECK (voicingParam->getValue() == Catch::Approx (savedVoicingValue).margin (1e-6));
}

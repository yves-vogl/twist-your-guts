#include "PluginProcessor.h"
#include "params/ParameterIds.h"
#include "TestHelpers.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

// v0.2.0 topology rebuild (docs/design-brief.md's Guarantee #3): "Low band
// never reaches the IR loader" - the IR loader is relocated to process only
// the Mid+High post-sum signal, structurally never the Low band. This is
// directly testable now that the IR loader has moved in the signal path
// (previously it sat post-sum, after both v1 bands were already combined).
//
// A full-processor RMS/level probe test can't isolate this cleanly on its
// own: the LR4 crossover's finite stopband rejection means some low-
// frequency energy always leaks into the Mid/High branch (tens of dB down,
// not below float32 precision), so even an unrelated IR-loader change would
// nudge the *combined* processBlock() output by a measurable, if tiny,
// amount regardless of whether the Low band itself is truly IR-independent.
// CryptaAudioProcessor::setLowBandIsolationCaptureForTests() exists
// specifically to make the *actual* structural claim - the Low band's own
// fully-processed, pre-final-sum output - directly, bit-exactly comparable.
namespace
{
    constexpr double testSampleRate = 48000.0;
    constexpr int testBlockSize = 512;

    juce::AudioBuffer<float> makeAggressiveSyntheticImpulseResponse (int numIrSamples = 256)
    {
        juce::AudioBuffer<float> ir (2, numIrSamples);

        for (int channel = 0; channel < ir.getNumChannels(); ++channel)
        {
            auto* data = ir.getWritePointer (channel);

            for (int sample = 0; sample < numIrSamples; ++sample)
                data[sample] = std::sin (static_cast<float> (sample) * 0.9f) * std::exp (-static_cast<float> (sample) / 20.0f);
        }

        return ir;
    }

    // A broadband-ish signal (fundamental plus a few harmonics) so the Low
    // band's own compressor/level chain has something non-trivial to react
    // to, not just a single pure tone.
    void fillWithBroadbandSignal (juce::AudioBuffer<float>& buffer, juce::int64 startSampleIndex)
    {
        TestHelpers::fillWithSine (buffer, testSampleRate, 90.0, 0.5f, startSampleIndex);

        juce::AudioBuffer<float> harmonic (buffer.getNumChannels(), buffer.getNumSamples());
        TestHelpers::fillWithSine (harmonic, testSampleRate, 1200.0, 0.3f, startSampleIndex);

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            buffer.addFrom (channel, 0, harmonic, channel, 0, buffer.getNumSamples());
    }
}

TEST_CASE ("Low band isolation: the Low band's own output is bit-exact identical whether the IR loader is enabled or not", "[ir][routing][dsp]")
{
    juce::AudioBuffer<float> lowBandWithIrOff (2, testBlockSize);
    juce::AudioBuffer<float> lowBandWithIrOn (2, testBlockSize);

    CryptaAudioProcessor processorIrOff;
    processorIrOff.prepareToPlay (testSampleRate, testBlockSize);
    processorIrOff.setLowBandIsolationCaptureForTests (&lowBandWithIrOff);

    CryptaAudioProcessor processorIrOn;
    processorIrOn.prepareToPlay (testSampleRate, testBlockSize);
    processorIrOn.setLowBandIsolationCaptureForTests (&lowBandWithIrOn);

    auto* irEnabledParam = processorIrOn.apvts.getParameter (ParamIDs::irEnabled);
    auto* irMixParam = processorIrOn.apvts.getParameter (ParamIDs::irMix);
    REQUIRE (irEnabledParam != nullptr);
    REQUIRE (irMixParam != nullptr);
    irEnabledParam->setValueNotifyingHost (1.0f);
    irMixParam->setValueNotifyingHost (irMixParam->convertTo0to1 (100.0f));

    processorIrOn.loadImpulseResponse (makeAggressiveSyntheticImpulseResponse(), testSampleRate);

    juce::MidiBuffer midi;

    // Several blocks so both processors' low-band compressor/level chains
    // reach a comparable settled state, and so the IR loader's own
    // asynchronous engine-install has time to actually become active on the
    // processorIrOn instance (irrelevant to the assertion below, but
    // confirms this test would actually catch a real routing regression).
    for (int i = 0; i < 8; ++i)
    {
        juce::AudioBuffer<float> bufferOff (2, testBlockSize);
        fillWithBroadbandSignal (bufferOff, static_cast<juce::int64> (i) * testBlockSize);
        processorIrOff.processBlock (bufferOff, midi);

        juce::AudioBuffer<float> bufferOn (2, testBlockSize);
        fillWithBroadbandSignal (bufferOn, static_cast<juce::int64> (i) * testBlockSize);
        processorIrOn.processBlock (bufferOn, midi);
    }

    for (int channel = 0; channel < 2; ++channel)
    {
        const auto* offData = lowBandWithIrOff.getReadPointer (channel);
        const auto* onData = lowBandWithIrOn.getReadPointer (channel);

        for (int sample = 0; sample < testBlockSize; ++sample)
        {
            INFO ("channel = " << channel << ", sample = " << sample);
            CHECK (offData[sample] == Catch::Approx (onData[sample]).margin (0.0));
        }
    }
}

TEST_CASE ("Low band isolation: the Low band's own output is unaffected by which IR is loaded", "[ir][routing][dsp]")
{
    juce::AudioBuffer<float> lowBandNoIr (2, testBlockSize);
    juce::AudioBuffer<float> lowBandWithIr (2, testBlockSize);

    CryptaAudioProcessor processorNoIr;
    processorNoIr.prepareToPlay (testSampleRate, testBlockSize);
    processorNoIr.setLowBandIsolationCaptureForTests (&lowBandNoIr);

    auto* irEnabledNoIrParam = processorNoIr.apvts.getParameter (ParamIDs::irEnabled);
    REQUIRE (irEnabledNoIrParam != nullptr);
    irEnabledNoIrParam->setValueNotifyingHost (1.0f); // enabled, but no real IR ever loaded (identity passthrough)

    CryptaAudioProcessor processorWithIr;
    processorWithIr.prepareToPlay (testSampleRate, testBlockSize);
    processorWithIr.setLowBandIsolationCaptureForTests (&lowBandWithIr);

    auto* irEnabledParam = processorWithIr.apvts.getParameter (ParamIDs::irEnabled);
    REQUIRE (irEnabledParam != nullptr);
    irEnabledParam->setValueNotifyingHost (1.0f);
    processorWithIr.loadImpulseResponse (makeAggressiveSyntheticImpulseResponse (128), testSampleRate);

    juce::MidiBuffer midi;

    for (int i = 0; i < 8; ++i)
    {
        juce::AudioBuffer<float> bufferNoIr (2, testBlockSize);
        fillWithBroadbandSignal (bufferNoIr, static_cast<juce::int64> (i) * testBlockSize);
        processorNoIr.processBlock (bufferNoIr, midi);

        juce::AudioBuffer<float> bufferWithIr (2, testBlockSize);
        fillWithBroadbandSignal (bufferWithIr, static_cast<juce::int64> (i) * testBlockSize);
        processorWithIr.processBlock (bufferWithIr, midi);
    }

    for (int channel = 0; channel < 2; ++channel)
    {
        const auto* noIrData = lowBandNoIr.getReadPointer (channel);
        const auto* withIrData = lowBandWithIr.getReadPointer (channel);

        for (int sample = 0; sample < testBlockSize; ++sample)
        {
            INFO ("channel = " << channel << ", sample = " << sample);
            CHECK (noIrData[sample] == Catch::Approx (withIrData[sample]).margin (0.0));
        }
    }
}

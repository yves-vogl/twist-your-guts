#include "ParameterLayout.h"
#include "ParameterIds.h"

namespace
{
    // True logarithmic (base-10) mapping for frequency parameters, so slider/
    // knob travel spends equal space per octave rather than per Hz. Uses
    // juce::mapToLog10/mapFromLog10 rather than NormalisableRange's built-in
    // power-law skew, which only approximates a log curve.
    juce::NormalisableRange<float> makeLogFrequencyRange (float minHz, float maxHz)
    {
        return juce::NormalisableRange<float> (
            minHz,
            maxHz,
            [] (float rangeStart, float rangeEnd, float normalised)
            { return juce::mapToLog10 (normalised, rangeStart, rangeEnd); },
            [] (float rangeStart, float rangeEnd, float value)
            { return juce::mapFromLog10 (value, rangeStart, rangeEnd); });
    }

    // Same idea for time parameters (attack/release) specified with a log
    // skew in the spec: equal slider travel per decade, not per millisecond.
    juce::NormalisableRange<float> makeLogTimeRange (float minMs, float maxMs)
    {
        return juce::NormalisableRange<float> (
            minMs,
            maxMs,
            [] (float rangeStart, float rangeEnd, float normalised)
            { return juce::mapToLog10 (normalised, rangeStart, rangeEnd); },
            [] (float rangeStart, float rangeEnd, float value)
            { return juce::mapFromLog10 (value, rangeStart, rangeEnd); });
    }
}

namespace cryp
{
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        //======================================================================
        // IO / Global
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::inputGain, 1 },
            "Input Gain",
            juce::NormalisableRange<float> (-24.0f, 24.0f, 0.01f),
            0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("dB")));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::outputGain, 1 },
            "Output Gain",
            juce::NormalisableRange<float> (-24.0f, 24.0f, 0.01f),
            0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("dB")));

        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { ParamIDs::bypass, 1 },
            "Bypass",
            false));

        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { ParamIDs::outputClip, 1 },
            "Safety Clip",
            false));

        //======================================================================
        // Noise gate (full-band, mirrors juce::dsp::NoiseGate's parameters)
        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { ParamIDs::gateEnabled, 1 },
            "Gate Enable",
            false));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::gateThreshold, 1 },
            "Gate Threshold",
            juce::NormalisableRange<float> (-80.0f, 0.0f, 0.01f),
            -60.0f,
            juce::AudioParameterFloatAttributes().withLabel ("dB")));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::gateRatio, 1 },
            "Gate Ratio",
            juce::NormalisableRange<float> (1.0f, 20.0f, 0.01f),
            10.0f,
            juce::AudioParameterFloatAttributes().withLabel (":1")));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::gateAttack, 1 },
            "Gate Attack",
            makeLogTimeRange (0.1f, 50.0f),
            1.0f,
            juce::AudioParameterFloatAttributes().withLabel ("ms")));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::gateRelease, 1 },
            "Gate Release",
            makeLogTimeRange (5.0f, 500.0f),
            100.0f,
            juce::AudioParameterFloatAttributes().withLabel ("ms")));

        //======================================================================
        // Crossover
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::crossoverFreq, 1 },
            "Crossover Frequency",
            makeLogFrequencyRange (60.0f, 1000.0f),
            250.0f,
            juce::AudioParameterFloatAttributes().withLabel ("Hz")));

        //======================================================================
        // Low band: compressor + level
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::lowCompThreshold, 1 },
            "Low Comp Threshold",
            juce::NormalisableRange<float> (-60.0f, 0.0f, 0.01f),
            -18.0f,
            juce::AudioParameterFloatAttributes().withLabel ("dB")));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::lowCompRatio, 1 },
            "Low Comp Ratio",
            juce::NormalisableRange<float> (1.0f, 20.0f, 0.01f),
            4.0f,
            juce::AudioParameterFloatAttributes().withLabel (":1")));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::lowCompAttack, 1 },
            "Low Comp Attack",
            makeLogTimeRange (0.1f, 100.0f),
            10.0f,
            juce::AudioParameterFloatAttributes().withLabel ("ms")));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::lowCompRelease, 1 },
            "Low Comp Release",
            makeLogTimeRange (10.0f, 1000.0f),
            120.0f,
            juce::AudioParameterFloatAttributes().withLabel ("ms")));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::lowCompMakeup, 1 },
            "Low Comp Makeup",
            juce::NormalisableRange<float> (-12.0f, 24.0f, 0.01f),
            0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("dB")));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::lowCompMix, 1 },
            "Low Comp Mix",
            juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
            100.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::lowLevel, 1 },
            "Low Level",
            juce::NormalisableRange<float> (-24.0f, 12.0f, 0.01f),
            0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("dB")));

        //======================================================================
        // High band: voicing + drive + tone + blend + level
        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { ParamIDs::highVoicing, 1 },
            "High Voicing",
            juce::StringArray { "Gnaw", "Wool", "Razor" },
            0));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::highDrive, 1 },
            "High Drive",
            juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
            50.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::highTone, 1 },
            "High Tone",
            juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
            50.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::highBlend, 1 },
            "High Blend",
            juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
            100.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::highLevel, 1 },
            "High Level",
            juce::NormalisableRange<float> (-24.0f, 12.0f, 0.01f),
            0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("dB")));

        //======================================================================
        // Post-sum 4-band EQ (LowShelf / Peak / Peak / HighShelf)
        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { ParamIDs::eqEnabled, 1 },
            "EQ Enable",
            false));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::eqLowShelfFreq, 1 },
            "EQ Low Shelf Frequency",
            makeLogFrequencyRange (40.0f, 400.0f),
            100.0f,
            juce::AudioParameterFloatAttributes().withLabel ("Hz")));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::eqLowShelfGain, 1 },
            "EQ Low Shelf Gain",
            juce::NormalisableRange<float> (-18.0f, 18.0f, 0.01f),
            0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("dB")));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::eqPeak1Freq, 1 },
            "EQ Peak 1 Frequency",
            makeLogFrequencyRange (100.0f, 2000.0f),
            500.0f,
            juce::AudioParameterFloatAttributes().withLabel ("Hz")));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::eqPeak1Gain, 1 },
            "EQ Peak 1 Gain",
            juce::NormalisableRange<float> (-18.0f, 18.0f, 0.01f),
            0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("dB")));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::eqPeak1Q, 1 },
            "EQ Peak 1 Q",
            juce::NormalisableRange<float> (0.2f, 5.0f, 0.01f),
            0.7f,
            juce::AudioParameterFloatAttributes()));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::eqPeak2Freq, 1 },
            "EQ Peak 2 Frequency",
            makeLogFrequencyRange (500.0f, 8000.0f),
            2500.0f,
            juce::AudioParameterFloatAttributes().withLabel ("Hz")));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::eqPeak2Gain, 1 },
            "EQ Peak 2 Gain",
            juce::NormalisableRange<float> (-18.0f, 18.0f, 0.01f),
            0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("dB")));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::eqPeak2Q, 1 },
            "EQ Peak 2 Q",
            juce::NormalisableRange<float> (0.2f, 5.0f, 0.01f),
            0.7f,
            juce::AudioParameterFloatAttributes()));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::eqHighShelfFreq, 1 },
            "EQ High Shelf Frequency",
            makeLogFrequencyRange (2000.0f, 16000.0f),
            8000.0f,
            juce::AudioParameterFloatAttributes().withLabel ("Hz")));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::eqHighShelfGain, 1 },
            "EQ High Shelf Gain",
            juce::NormalisableRange<float> (-18.0f, 18.0f, 0.01f),
            0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("dB")));

        //======================================================================
        // IR loader / cab sim (IR file path itself is non-parameter state,
        // handled separately in M4)
        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { ParamIDs::irEnabled, 1 },
            "IR Enable",
            false));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::irMix, 1 },
            "IR Mix",
            juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
            100.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")));

        return layout;
    }
}

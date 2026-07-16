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
        // Crossover: two cascaded LR4 splits (v0.2.0 2-band -> 3-band
        // rebuild - docs/design-brief.md's "Split Low / Split High"
        // section). splitHighHz's floor (300 Hz) is deliberately above
        // splitLowHz's own ceiling-minus-gap, and the two are further
        // clamped at runtime by cryp::clampSplitHighHz() (src/dsp/SplitGap.h)
        // so splitHighHz can never collapse the Mid band to a degenerate
        // near-zero width even at the extremes of both ranges.
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::splitLowHz, 1 },
            "Split Low",
            makeLogFrequencyRange (60.0f, 400.0f),
            120.0f,
            juce::AudioParameterFloatAttributes().withLabel ("Hz")));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::splitHighHz, 1 },
            "Split High",
            makeLogFrequencyRange (300.0f, 2000.0f),
            600.0f,
            juce::AudioParameterFloatAttributes().withLabel ("Hz")));

        //======================================================================
        // Low band: compressor + level. v0.2.0 re-sources the ballistics
        // defaults to the reference class's own fixed "glue" bus-compressor
        // values (ratio 2.0 / attack 3ms / release 6ms - docs/design-brief.md,
        // docs/research-notes.md §3); the release range floor is lowered
        // from 10ms to 5ms (breaking change, acceptable pre-1.0) so the
        // sourced 6ms default is reachable. Threshold/makeup/mix/level
        // ranges and defaults are unchanged from v1 - no source contradicts
        // them.
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
            2.0f,
            juce::AudioParameterFloatAttributes().withLabel (":1")));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::lowCompAttack, 1 },
            "Low Comp Attack",
            makeLogTimeRange (0.1f, 100.0f),
            3.0f,
            juce::AudioParameterFloatAttributes().withLabel ("ms")));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::lowCompRelease, 1 },
            "Low Comp Release",
            makeLogTimeRange (5.0f, 1000.0f),
            6.0f,
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
        // Mid band (NEW in v0.2.0): drive + level only - no filter/tone/
        // blend, matching the reference class's own Mid band exactly
        // (docs/design-brief.md's "Mid band" section).
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::midDrive, 1 },
            "Mid Drive",
            juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
            30.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::midLevel, 1 },
            "Mid Level",
            juce::NormalisableRange<float> (-24.0f, 12.0f, 0.01f),
            0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("dB")));

        //======================================================================
        // High band: Tight (NEW in v0.2.0) + voicing + drive + tone + blend
        // + level
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::highTightHz, 1 },
            "High Tight",
            makeLogFrequencyRange (20.0f, 500.0f),
            100.0f,
            juce::AudioParameterFloatAttributes().withLabel ("Hz")));

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
        // Post-sum 4-band EQ (LowShelf / Peak / Peak / HighShelf). v0.2.0
        // re-anchors the default corner frequencies to the sourced
        // bass-tone-stack frequency set from the same design lineage as the
        // reference class (80 / 500 / 2800 / 5000 Hz - docs/research-notes.md
        // §6); v1's 100/500/2500/8000 Hz defaults were unsourced
        // placeholders. Gain/Q ranges and defaults are unchanged - the EQ
        // ships off by default either way, so this is a dormant-until-engaged
        // anchor change, not an audible v1->v2 difference unless a user (or
        // factory preset) turns the EQ on.
        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { ParamIDs::eqEnabled, 1 },
            "EQ Enable",
            false));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::eqLowShelfFreq, 1 },
            "EQ Low Shelf Frequency",
            makeLogFrequencyRange (40.0f, 400.0f),
            80.0f,
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
            2800.0f,
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
            5000.0f,
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

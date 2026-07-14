#pragma once

#include <juce_dsp/juce_dsp.h>

// Low-band parallel ("New York style") compressor (issue #42): the low band
// is compressed, made up, and blended back with its own uncompressed self
// via lowCompMix, rather than inserted serially - which is what makes it a
// *parallel* compressor rather than a plain dynamics insert. Sits after the
// LR4 split and before lowLevel in the low-band chain.
//
// juce::dsp::Compressor<float> (JUCE 8.0.14,
// juce_dsp/widgets/juce_Compressor.h) is a simple feed-forward VCA-style
// compressor with no lookahead, so it introduces zero sample latency - the
// juce::dsp::DryWetMixer used for the parallel blend is configured
// accordingly (wetLatency stays 0), so this stage never needs to feed into
// the plugin's latency-compensation seam.
namespace tyg
{
    class ParallelCompressor
    {
    public:
        ParallelCompressor() = default;

        // `initialWetMixProportion01` must be the current lowCompMix value
        // (0..1) *before* prepare() runs the mixer's internal reset(): the
        // DryWetMixer primes its smoothed dry/wet volumes from whatever
        // `mix` was set to at the time reset() executes (JUCE 8.0.14
        // gotcha), so passing the real value here - rather than setting it
        // only after prepare() - avoids an audible fade-in glitch on the
        // very first block.
        void prepare (const juce::dsp::ProcessSpec& spec, float initialWetMixProportion01);
        void reset();

        // All real-time safe: Compressor's setters just recompute ballistics
        // coefficients (no allocation), Gain's setter only retargets its
        // SmoothedValue, and DryWetMixer::setWetMixProportion only updates a
        // scalar + recomputes the (already-allocated) dry/wet volume
        // targets.
        void setThresholdDb (float newThresholdDb) noexcept { compressor.setThreshold (newThresholdDb); }
        void setRatio (float newRatio) noexcept { compressor.setRatio (newRatio); }
        void setAttackMs (float newAttackMs) noexcept { compressor.setAttack (newAttackMs); }
        void setReleaseMs (float newReleaseMs) noexcept { compressor.setRelease (newReleaseMs); }
        void setMakeupGainDb (float newMakeupDb) noexcept { makeupGain.setGainDecibels (newMakeupDb); }
        void setWetMixProportion (float newWetMixProportion01) noexcept { mixer.setWetMixProportion (newWetMixProportion01); }

        // In-place parallel compression: mixer.pushDrySamples() captures the
        // pre-compression signal, the compressor + makeup gain run in place,
        // then mixer.mixWetSamples() blends the compressed ("wet") result
        // back with the captured dry signal per lowCompMix.
        void process (juce::dsp::AudioBlock<float>& block) noexcept
        {
            mixer.pushDrySamples (juce::dsp::AudioBlock<const float> (block));

            juce::dsp::ProcessContextReplacing<float> context (block);
            compressor.process (context);
            makeupGain.process (context);

            mixer.mixWetSamples (block);
        }

    private:
        juce::dsp::Compressor<float> compressor;
        juce::dsp::Gain<float> makeupGain;
        juce::dsp::DryWetMixer<float> mixer;
    };
}

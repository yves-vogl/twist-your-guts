#pragma once

#include <juce_dsp/juce_dsp.h>

// Cab-sim IR loader (issue #42), the final stage before output. Wraps
// juce::dsp::Convolution (JUCE 8.0.14, juce_dsp/frequency/juce_Convolution.h)
// configured for zero added latency (the default `Convolution()` /
// `Convolution(Latency{0})` constructor), with an irMix DryWetMixer wrapped
// around it so the loaded IR can be blended rather than fully replacing the
// signal.
//
// Safe-by-default without any bundled factory IR: juce::dsp::Convolution
// with no loadImpulseResponse() call yet made falls back to an internal
// single-sample identity impulse response (JUCE 8.0.14
// ConvolutionEngineFactory::makeImpulseBuffer(), juce_Convolution.cpp).
// *However*, that internal fallback's assumed source sample rate is
// hardcoded to the ProcessSpec default (44100 Hz) and is never updated by
// prepare() - only by an explicit loadImpulseResponse() call - so at any
// *other* session sample rate, Convolution would silently resample that
// single-sample impulse against a mismatched rate, smearing/attenuating it
// (juce_Convolution.cpp's resampleImpulseResponse() only skips resampling
// when the rates match exactly) and quietly colouring the signal even with
// no IR ever loaded. prepare() below closes that gap by explicitly loading
// a correctly-rate-tagged identity impulse response itself, so "no IR
// loaded" is a guaranteed bit-exact passthrough at every session sample
// rate, not only at 44100 Hz.
//
// Bundling license-clean factory cab IRs is its own ear-tuning-gated
// milestone (see docs/manual.md); loadImpulseResponse() here is the DSP-side
// seam a future GUI file browser (or preset system) will call.
//
// Threading: per JUCE's docs, Convolution::loadImpulseResponse() is
// wait-free and safe to call from the audio thread, but this class's
// loadImpulseResponse() is intended to be called from the message thread
// (e.g. in response to a GUI file-picker or preset load) - it takes
// ownership of (moves) the supplied buffer, so the caller must not also
// touch it on the audio thread afterwards.
namespace cryp
{
    class IRLoader
    {
    public:
        IRLoader() = default;

        // `initialWetMixProportion01` must be the current irMix value
        // (0..1) *before* prepare() runs the internal DryWetMixer's
        // reset(): see the same gotcha documented on cryp::Voicing::prepare().
        void prepare (const juce::dsp::ProcessSpec& spec, float initialWetMixProportion01);
        void reset();

        void setWetMixProportion (float wetMixProportion01) noexcept { mixer.setWetMixProportion (wetMixProportion01); }

        // Loads a new impulse response. Not real-time safe by contract of
        // this wrapper (even though the underlying JUCE call is wait-free) -
        // call from the message thread only. `irSampleRate` is the sample
        // rate the buffer's samples were captured/generated at; Convolution
        // resamples internally to match the session's sample rate.
        void loadImpulseResponse (juce::AudioBuffer<float> irBuffer, double irSampleRate);

        // In-place: convolution + irMix dry/wet blend. Callers should skip
        // calling this entirely when irEnabled is off, for a guaranteed
        // bit-exact bypass rather than relying on mix==0.
        void process (juce::dsp::AudioBlock<float>& block) noexcept
        {
            mixer.pushDrySamples (juce::dsp::AudioBlock<const float> (block));

            juce::dsp::ProcessContextReplacing<float> context (block);
            convolution.process (context);

            mixer.mixWetSamples (block);
        }

    private:
        juce::dsp::Convolution convolution;
        juce::dsp::DryWetMixer<float> mixer;
    };
}

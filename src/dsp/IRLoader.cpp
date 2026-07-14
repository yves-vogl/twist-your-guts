#include "IRLoader.h"

namespace
{
    // A single-sample, unity-gain impulse response tagged with the *actual*
    // session sample rate, so juce::dsp::Convolution's internal resampling
    // step (juce_Convolution.cpp's resampleImpulseResponse()) is a no-op
    // (exact rate match) rather than smearing/attenuating the impulse - see
    // the class-level comment in IRLoader.h for why this matters.
    juce::AudioBuffer<float> makeIdentityImpulseResponse()
    {
        juce::AudioBuffer<float> identity (2, 1);
        identity.setSample (0, 0, 1.0f);
        identity.setSample (1, 0, 1.0f);
        return identity;
    }
}

namespace cryp
{
    void IRLoader::prepare (const juce::dsp::ProcessSpec& spec, float initialWetMixProportion01)
    {
        // Explicitly (re)install a correctly-rate-tagged identity IR *before*
        // convolution.prepare(), per JUCE's own recommendation ("it is
        // recommended to call loadImpulseResponse() before prepare() if a
        // specific IR must be active during the first process() call") - so
        // "no IR loaded yet" is a guaranteed bit-exact passthrough at this
        // session's sample rate from the very first block (see class-level
        // comment in IRLoader.h). loadImpulseResponse() itself is wait-free/
        // real-time safe per JUCE's docs, but this call happens here in
        // prepare() (message thread), never from processBlock().
        convolution.loadImpulseResponse (makeIdentityImpulseResponse(),
                                          spec.sampleRate,
                                          juce::dsp::Convolution::Stereo::yes,
                                          juce::dsp::Convolution::Trim::no,
                                          juce::dsp::Convolution::Normalise::no);

        convolution.prepare (spec);

        // Prime the mix *before* prepare() so the mixer's internal reset()
        // (called at the end of DryWetMixer::prepare()) snaps its smoothed
        // dry/wet volumes to the correct starting point instead of a stale
        // default (JUCE 8.0.14 DryWetMixer gotcha).
        mixer.setMixingRule (juce::dsp::DryWetMixingRule::linear);
        mixer.setWetMixProportion (initialWetMixProportion01);
        mixer.prepare (spec);

        // Convolution() defaults to Latency{0} (zero added latency by
        // construction), so the dry path never needs compensating delay.
        mixer.setWetLatency (0.0f);
    }

    void IRLoader::reset()
    {
        convolution.reset();
        mixer.reset();
    }

    void IRLoader::loadImpulseResponse (juce::AudioBuffer<float> irBuffer, double irSampleRate)
    {
        convolution.loadImpulseResponse (std::move (irBuffer),
                                          irSampleRate,
                                          juce::dsp::Convolution::Stereo::yes,
                                          juce::dsp::Convolution::Trim::no,
                                          juce::dsp::Convolution::Normalise::yes);
    }
}

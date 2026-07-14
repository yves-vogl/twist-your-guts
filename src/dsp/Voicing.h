#pragma once

#include "RealtimeCoefficients.h"

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <memory>

// High-band distortion engine (issue #42): selectable voicing (Gnaw/Wool/
// Razor) with drive, tone, and a clean/distorted blend, running the
// nonlinear shaping stage oversampled to keep aliasing under control -
// exactly the "oversampled nonlinearities to control aliasing" pattern this
// suite uses for every distortion stage.
//
// Topology per block, all real-time safe (no allocation once prepare() has
// run):
//   dry tap (pre-voicing high band) --------------------------+
//                                                              |
//   [Razor only] pre-highpass (base rate, tight low end) --+   |
//   oversample up (juce::dsp::Oversampling<float>, JUCE     |   |
//   8.0.14 juce_dsp/processors/juce_Oversampling.h) --------+   |
//   per-sample waveshape (voicing-specific, drive-scaled)       |
//   oversample down                                             |
//   mid filter (peak, voicing-specific hump/scoop)               |
//   tone filter (1st-order lowpass, highTone-controlled)          |
//   ------------------------------------------------------ wet -> DryWetMixer (highBlend)
//
// Latency: only the oversampling stage adds sample latency (all the IIR
// filters here are zero-latency). getLatencySamples() reports it so
// PluginProcessor can feed it into the plugin's overall latency-compensation
// seam (issue #9) and delay the low band to match.
namespace cryp
{
    enum class VoicingType
    {
        gnaw = 0,
        wool = 1,
        razor = 2
    };

    class Voicing
    {
    public:
        Voicing() = default;

        // `initialWetMixProportion01` must be the current highBlend value
        // (0..1) *before* prepare() runs the internal DryWetMixer's
        // reset(): DryWetMixer primes its smoothed dry/wet volumes from
        // whatever `mix` was set to at the moment reset() executes (JUCE
        // 8.0.14 gotcha - see docs/architecture.md), so passing the real
        // value here avoids an audible fade-in glitch on the very first
        // block.
        void prepare (const juce::dsp::ProcessSpec& spec, float initialWetMixProportion01);
        void reset();

        void setVoicing (VoicingType newVoicing) noexcept;
        void setDrive (float drive01) noexcept { driveAmount01 = juce::jlimit (0.0f, 1.0f, drive01); }
        void setTone (float tone01) noexcept { toneAmount01 = juce::jlimit (0.0f, 1.0f, tone01); }
        void setWetMixProportion (float wetMixProportion01) noexcept { blendMixer.setWetMixProportion (wetMixProportion01); }

        // Integer sample latency contributed by the oversampling stage.
        // Zero until prepare() has run.
        int getLatencySamples() const noexcept { return latencySamples; }

        // In-place: processes the high band (already split off by the LR4
        // crossover) through the selected voicing and blends clean/distorted
        // per the current wet mix proportion.
        void process (juce::dsp::AudioBlock<float>& block) noexcept;

    private:
        void updateMidFilterCoefficients() noexcept;
        void updateToneFilterCoefficients() noexcept;
        void updatePreHighPassCoefficients() noexcept;

        // Per-sample nonlinearity for the current voicing, scaled by the
        // current drive amount. Always returns a finite value in [-1, 1]
        // regardless of drive/input magnitude (hard-clip and tanh-based
        // shapers are both inherently bounded), so extreme drive settings
        // can never produce NaN/Inf.
        float shapeSample (float x) const noexcept;

        double sampleRate = 44100.0;
        VoicingType voicing = VoicingType::gnaw;
        float driveAmount01 = 0.5f;
        float toneAmount01 = 0.5f;

        // Constructed in prepare() once the real channel count is known;
        // Oversampling's constructor itself allocates, so it must never be
        // (re)constructed from processBlock().
        std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
        int latencySamples = 0;

        using Duplicator = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>>;

        // Razor-only pre-emphasis highpass, run at the base sample rate
        // before oversampling (linear filter, no aliasing concern, so no
        // need to pay the oversampling cost for it).
        Duplicator preHighPass { new juce::dsp::IIR::Coefficients<float> (1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f) };

        // Voicing-specific character filter (mid hump for Razor, mid scoop
        // for Wool, neutral for Gnaw), applied post-downsampling.
        Duplicator midFilter { new juce::dsp::IIR::Coefficients<float> (1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f) };

        // highTone-controlled 1st-order lowpass, applied post-downsampling.
        Duplicator toneFilter { new juce::dsp::IIR::Coefficients<float> (1.0f, 0.0f, 1.0f, 0.0f) };

        // Generous headroom above any realistic oversampling FIR latency at
        // any factor/sample-rate combination this plugin supports.
        static constexpr int maxWetLatencySamples = 2048;
        juce::dsp::DryWetMixer<float> blendMixer { maxWetLatencySamples };
    };
}

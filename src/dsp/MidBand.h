#pragma once

#include <juce_dsp/juce_dsp.h>

#include <memory>

// Mid band (v0.2.0 3-band rebuild, docs/design-brief.md's "Mid band" section):
// staged/cascaded saturation only - "Mid Drive" - no filter, no tone, and
// (deliberately, matching the reference class exactly) no clean/distorted
// blend control. Runs its own 4x-oversampled shaping stage so its
// nonlinearity gets the same anti-aliasing headroom as the High band's
// Voicing stage.
//
// docs/design-brief.md asks for the Mid band to "share the oversampling
// instance" with the High band's Voicing stage for CPU efficiency. This
// class deliberately does NOT do that literally - see the class-level
// comment in Voicing.h and docs/design-brief.md's implementation note for
// the reasoning: MidBand instead owns its own juce::dsp::Oversampling
// instance, configured identically (same factor exponent, same FIR filter
// type) to Voicing's own. Because juce::dsp::Oversampling's reported latency
// depends only on that configuration (not on any per-instance state or the
// audio data itself), MidBand::getLatencySamples() and
// Voicing::getLatencySamples() are guaranteed numerically equal, which is
// the actual correctness property CryptaAudioProcessor's shared Mid+High
// latency-compensation value depends on - physically sharing one
// Oversampling object would only add CPU savings on top of that, at
// materially higher implementation/testing risk (manual channel
// interleaving through one shared oversampled block) for this pass.
namespace cryp
{
    class MidBand
    {
    public:
        MidBand() = default;

        // Allocates the oversampling stage. Must be called before process()
        // and whenever the channel count, sample rate, or max block size
        // changes.
        void prepare (const juce::dsp::ProcessSpec& spec);

        // Clears the oversampling stage's internal FIR history (e.g. on
        // transport stop) without deallocating.
        void reset();

        // Real-time safe: just retargets a scalar, no allocation.
        void setDrive (float drive01) noexcept { driveAmount01 = juce::jlimit (0.0f, 1.0f, drive01); }

        // Integer sample latency contributed by the oversampling stage. Zero
        // until prepare() has run.
        int getLatencySamples() const noexcept { return latencySamples; }

        // In-place: processes the Mid band (already split off by the second
        // LR4 crossover) through the staged saturation stage. Real-time
        // safe: no allocation once prepare() has run.
        void process (juce::dsp::AudioBlock<float>& block) noexcept;

    private:
        float driveAmount01 = 0.3f;

        // Constructed in prepare() once the real channel count is known;
        // Oversampling's constructor itself allocates, so it must never be
        // (re)constructed from processBlock().
        std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
        int latencySamples = 0;
    };
}

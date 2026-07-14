#pragma once

#include <juce_dsp/juce_dsp.h>

// 4th-order Linkwitz-Riley crossover (issue #8): splits a signal into a low
// band and a high band whose magnitude-flat sum reconstructs the original
// signal (within floating-point precision), which is the defining property
// of an LR4 crossover and the reason it - rather than a plain pair of
// Butterworth filters - is the standard choice for splitting a signal ahead
// of independent per-band processing without introducing a notch or bump at
// the crossover point.
//
// Wraps juce::dsp::LinkwitzRileyFilter<float> (JUCE 8.0.14,
// juce_dsp/processors/juce_LinkwitzRileyFilter.h) using its dual-output
// processSample(channel, input, outputLow, outputHigh) overload. That
// overload runs a single cascaded TPT (topology-preserving transform) state
// per channel and emits matched low/high outputs from that shared state -
// this is what guarantees the flat-magnitude sum. Using two independently
// configured LinkwitzRileyFilter instances (one set to lowpass, one to
// highpass) would also flat-sum in theory, but doubles the per-channel state
// and risks the two cutoffs drifting apart under future automation/preset
// changes; the single-instance dual-output form makes that impossible by
// construction.
namespace cryp
{
    class Crossover
    {
    public:
        Crossover() = default;

        // Allocates per-channel filter state. Must be called before process()
        // and whenever the channel count or sample rate changes.
        void prepare (const juce::dsp::ProcessSpec& spec);

        // Clears filter state (e.g. on transport stop) without deallocating.
        void reset();

        // Real-time safe: just recomputes a handful of filter coefficients
        // from the new cutoff, no allocation.
        void setCutoffFrequency (float newCutoffHz);

        float getCutoffFrequency() const noexcept { return filter.getCutoffFrequency(); }

        // Splits `input` sample-for-sample into `lowOutput` and `highOutput`.
        // All three blocks must share the same channel/sample counts (as
        // established by the most recent prepare() call); lowOutput and
        // highOutput must not alias input or each other. Real-time safe: no
        // allocation, just per-sample filter math.
        void process (const juce::dsp::AudioBlock<const float>& input,
                      juce::dsp::AudioBlock<float>& lowOutput,
                      juce::dsp::AudioBlock<float>& highOutput) noexcept;

    private:
        juce::dsp::LinkwitzRileyFilter<float> filter;
    };
}

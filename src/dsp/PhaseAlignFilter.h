#pragma once

#include <juce_dsp/juce_dsp.h>

// v0.2.0 3-band rebuild: phase-alignment allpass companion to cryp::Crossover,
// required to make the cascaded (Low, then Mid/High) topology actually
// flat-sum.
//
// A single cryp::Crossover's own dual-output Low+High sum is exactly a
// flat-magnitude allpass-filtered version of *that stage's own input*
// (juce::dsp::LinkwitzRileyFilter's documented property - JUCE 8.0.14,
// juce_LinkwitzRileyFilter.h). But cascading two such stages - splitting Low
// off first, then splitting the remainder into Mid/High - does NOT
// automatically make Low+Mid+High flat relative to the *original* signal:
// Mid+High is a flat-magnitude allpass-filtered version of the *remainder*
// (the first stage's own High output), not of the original input, so
// summing it directly against the first stage's untouched Low output
// generally does NOT reconstruct flat magnitude - confirmed empirically in
// this repo (see the PR this class shipped in: a direct 3-band flat-sum test
// without this compensation showed deviations up to -10 dB at close
// splitLowHz/splitHighHz ratios, worst inside the first crossover's own
// transition band).
//
// The fix, proven algebraically (not just empirically): reading JUCE 8.0.14's
// own juce_LinkwitzRileyFilter.cpp source confirms the dual-output
// processSample(channel, input, outputLow, outputHigh)'s outputLow+outputHigh
// sum uses the *exact same formula* (built only from the first internal
// biquad stage's s1/s2 state) as that same class's single-output
// processSample(channel, input) in Type::allpass mode. Since that allpass
// transform is linear, applying the *second* crossover's own allpass
// transform (same cutoff, a physically separate PhaseAlignFilter
// instance/state) to the *first* crossover's Low output before the final sum
// makes the algebra exact:
//
//   Low_compensated + Mid + High
//     = Allpass2(Low) + Allpass2(Remainder)      [Mid+High = Allpass2(Remainder)]
//     = Allpass2(Low + Remainder)                [Allpass2 is linear]
//     = Allpass2(Input)                          [Low + Remainder = Input exactly,
//                                                   the first crossover's own
//                                                   dual-output reconstruction
//                                                   property]
//
// and Allpass2(Input) has flat magnitude relative to Input by definition (an
// allpass filter's magnitude response is unity at every frequency) - so the
// three-way sum is flat, exactly, not just approximately.
//
// See PluginProcessor.cpp's processChunk() for where this is wired into the
// signal path (applied to the Low band, cutoff tied to the same effective
// splitHighHz fed to the Mid/High crossover) and
// tests/ThreeBandFlatSumTests.cpp for the direct-DSP-level proof.
namespace cryp
{
    class PhaseAlignFilter
    {
    public:
        PhaseAlignFilter()
        {
            filter.setType (juce::dsp::LinkwitzRileyFilter<float>::Type::allpass);
        }

        void prepare (const juce::dsp::ProcessSpec& spec) { filter.prepare (spec); }
        void reset() { filter.reset(); }

        // Must always be set to the *same* cutoff frequency as the
        // downstream Mid/High crossover this instance is compensating for
        // (i.e. the effective, already-clamped splitHighHz - see
        // cryp::clampSplitHighHz()), or the algebraic cancellation above
        // does not hold.
        void setCutoffFrequency (float newCutoffHz) { filter.setCutoffFrequency (newCutoffHz); }

        // In-place: real-time safe, no allocation once prepare() has run.
        void process (juce::dsp::AudioBlock<float>& block) noexcept
        {
            const auto numChannels = block.getNumChannels();
            const auto numSamples = block.getNumSamples();

            for (size_t channel = 0; channel < numChannels; ++channel)
            {
                auto* data = block.getChannelPointer (channel);

                for (size_t sample = 0; sample < numSamples; ++sample)
                    data[sample] = filter.processSample (static_cast<int> (channel), data[sample]);
            }
        }

    private:
        juce::dsp::LinkwitzRileyFilter<float> filter;
    };
}

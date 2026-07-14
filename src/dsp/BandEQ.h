#pragma once

#include "RealtimeCoefficients.h"

#include <juce_dsp/juce_dsp.h>

// Post-sum 4-band EQ (issue #42): LowShelf -> Peak1 -> Peak2 -> HighShelf,
// run in series after the low/high bands are summed back together.
//
// Each band is a juce::dsp::ProcessorDuplicator<IIR::Filter<float>,
// IIR::Coefficients<float>> (JUCE 8.0.14, juce_dsp/processors/
// juce_ProcessorDuplicator.h / juce_IIRFilter.h) so a single instance covers
// mono or stereo. Coefficients are *never* replaced wholesale from
// processBlock() (juce::dsp::IIR::Coefficients<float>::makeLowShelf/
// makePeakFilter/makeHighShelf heap-allocate a new Coefficients object every
// call, which is not real-time safe for parameters that can be automated
// continuously). Instead, prepare() allocates each band's Coefficients
// object once (2nd-order form, via the raw 6-argument constructor with
// placeholder identity values), and every subsequent parameter change
// overwrites that same object's raw coefficient storage in place via
// cryp::applyBiquadCoefficients(), using
// juce::dsp::IIR::ArrayCoefficients<float>::makeXxx() (stack-only, zero
// allocation) as the source of the new values. See RealtimeCoefficients.h.
namespace cryp
{
    class BandEQ
    {
    public:
        BandEQ() = default;

        void prepare (const juce::dsp::ProcessSpec& spec);
        void reset();

        // freqHz/gainDb/q are all real-time safe to call every block: they
        // recompute a stack-only coefficient set and copy it in place into
        // already-allocated storage (see class comment above).
        void setLowShelf (float freqHz, float gainDb) noexcept;
        void setPeak1 (float freqHz, float gainDb, float q) noexcept;
        void setPeak2 (float freqHz, float gainDb, float q) noexcept;
        void setHighShelf (float freqHz, float gainDb) noexcept;

        void process (juce::dsp::AudioBlock<float>& block) noexcept
        {
            juce::dsp::ProcessContextReplacing<float> context (block);
            lowShelf.process (context);
            peak1.process (context);
            peak2.process (context);
            highShelf.process (context);
        }

    private:
        using Duplicator = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>>;

        double sampleRate = 44100.0;

        Duplicator lowShelf { new juce::dsp::IIR::Coefficients<float> (1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f) };
        Duplicator peak1 { new juce::dsp::IIR::Coefficients<float> (1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f) };
        Duplicator peak2 { new juce::dsp::IIR::Coefficients<float> (1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f) };
        Duplicator highShelf { new juce::dsp::IIR::Coefficients<float> (1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f) };
    };
}

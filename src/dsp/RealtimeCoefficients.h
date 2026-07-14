#pragma once

#include <juce_dsp/juce_dsp.h>

#include <array>

// Real-time-safe biquad/first-order coefficient updates for juce::dsp::IIR::Filter.
//
// juce::dsp::IIR::Coefficients<float>::makeLowShelf/makePeakFilter/... (the
// usual way to build filter coefficients) heap-allocate a brand new
// Coefficients object on every call - fine in prepareToPlay(), not fine on
// the audio thread if a parameter is being automated every block.
//
// juce::dsp::IIR::ArrayCoefficients<float>::makeXxx returns the same
// coefficients as a std::array (stack storage, zero allocation). This header
// writes that array's values directly into an *already-allocated*
// Coefficients<float> object's raw coefficient storage (normalising by a0
// exactly the way Coefficients' own constructor does), so repeated calls
// during processBlock() never touch the heap.
//
// JUCE 8.0.14, juce_dsp/processors/juce_IIRFilter.h /
// juce_dsp/processors/juce_IIRFilter_Impl.h (Coefficients::assignImpl shows
// the {b0,b1,b2,a1,a2} normalised-by-a0 storage layout this mirrors).
namespace cryp
{
    // Writes a normalised 2nd-order {b0,b1,b2,a1,a2} set (5 raw coefficients)
    // computed from a raw {b0,b1,b2,a0,a1,a2} array (as returned by
    // juce::dsp::IIR::ArrayCoefficients<float>::makeLowShelf/makeHighShelf/
    // makePeakFilter/...) into `target`, which must already hold a 2nd-order
    // filter's coefficient storage (i.e. have been constructed via the 6-
    // argument Coefficients constructor, or a prior makeXxx() call, at least
    // once - typically during prepareToPlay()).
    inline void applyBiquadCoefficients (juce::dsp::IIR::Coefficients<float>& target,
                                          const std::array<float, 6>& raw) noexcept
    {
        jassert (target.getFilterOrder() == 2);

        auto* dest = target.getRawCoefficients();
        const auto invA0 = 1.0f / raw[3];

        dest[0] = raw[0] * invA0; // b0
        dest[1] = raw[1] * invA0; // b1
        dest[2] = raw[2] * invA0; // b2
        dest[3] = raw[4] * invA0; // a1
        dest[4] = raw[5] * invA0; // a2
    }

    // Same idea for a 1st-order {b0,b1,a1} set (3 raw coefficients) computed
    // from a raw {b0,b1,a0,a1} array (as returned by
    // ArrayCoefficients<float>::makeFirstOrderLowPass/HighPass/AllPass).
    inline void applyFirstOrderCoefficients (juce::dsp::IIR::Coefficients<float>& target,
                                              const std::array<float, 4>& raw) noexcept
    {
        jassert (target.getFilterOrder() == 1);

        auto* dest = target.getRawCoefficients();
        const auto invA0 = 1.0f / raw[2];

        dest[0] = raw[0] * invA0; // b0
        dest[1] = raw[1] * invA0; // b1
        dest[2] = raw[3] * invA0; // a1
    }
}

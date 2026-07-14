#include "BandEQ.h"

namespace
{
    // Standard "flat"/Butterworth shelf slope (Q = 1/sqrt(2)), matching the
    // implicit default juce::dsp::IIR::ArrayCoefficients::makeLowShelf/
    // makeHighShelf use when no explicit Q is given.
    constexpr float shelfQ = 0.70710678f;
}

namespace tyg
{
    void BandEQ::prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;

        lowShelf.prepare (spec);
        peak1.prepare (spec);
        peak2.prepare (spec);
        highShelf.prepare (spec);
    }

    void BandEQ::reset()
    {
        lowShelf.reset();
        peak1.reset();
        peak2.reset();
        highShelf.reset();
    }

    void BandEQ::setLowShelf (float freqHz, float gainDb) noexcept
    {
        const auto gainFactor = juce::Decibels::decibelsToGain (gainDb);
        const auto raw = juce::dsp::IIR::ArrayCoefficients<float>::makeLowShelf (sampleRate, freqHz, shelfQ, gainFactor);
        applyBiquadCoefficients (*lowShelf.state, raw);
    }

    void BandEQ::setPeak1 (float freqHz, float gainDb, float q) noexcept
    {
        const auto gainFactor = juce::Decibels::decibelsToGain (gainDb);
        const auto raw = juce::dsp::IIR::ArrayCoefficients<float>::makePeakFilter (sampleRate, freqHz, q, gainFactor);
        applyBiquadCoefficients (*peak1.state, raw);
    }

    void BandEQ::setPeak2 (float freqHz, float gainDb, float q) noexcept
    {
        const auto gainFactor = juce::Decibels::decibelsToGain (gainDb);
        const auto raw = juce::dsp::IIR::ArrayCoefficients<float>::makePeakFilter (sampleRate, freqHz, q, gainFactor);
        applyBiquadCoefficients (*peak2.state, raw);
    }

    void BandEQ::setHighShelf (float freqHz, float gainDb) noexcept
    {
        const auto gainFactor = juce::Decibels::decibelsToGain (gainDb);
        const auto raw = juce::dsp::IIR::ArrayCoefficients<float>::makeHighShelf (sampleRate, freqHz, shelfQ, gainFactor);
        applyBiquadCoefficients (*highShelf.state, raw);
    }
}

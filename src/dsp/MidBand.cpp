#include "MidBand.h"

#include <cmath>

namespace
{
    // Cascaded two-stage tanh, structurally mirroring Voicing's Wool
    // voicing (the reference class's own "multiple tube gain stages...
    // designed for the Mid and Treble bands separately" - see
    // docs/design-brief.md's Mid band section for the sourcing). Deliberately
    // more modest ceilings than Wool's own 12x/6x: the Mid band is a single
    // always-on stage with no Blend control, so its 0-100% Drive range alone
    // has to cover everything from "barely there" to "dominant", not just
    // add character on top of a separately-adjustable blend the way Voicing's
    // highBlend does.
    constexpr float midMaxDriveGain1 = 8.0f;
    constexpr float midMaxDriveGain2 = 4.0f;

    // 4x oversampling (factor exponent 2), matching Voicing's High band
    // factor - see MidBand.h's class-level comment for why this is a
    // separate, identically-configured instance rather than one physically
    // shared with Voicing.
    constexpr size_t oversamplingFactorExponent = 2;
}

namespace cryp
{
    void MidBand::prepare (const juce::dsp::ProcessSpec& spec)
    {
        oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
            static_cast<size_t> (spec.numChannels),
            oversamplingFactorExponent,
            juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple,
            true,
            true);
        oversampling->initProcessing (static_cast<size_t> (spec.maximumBlockSize));
        oversampling->reset();
        latencySamples = static_cast<int> (std::lround (oversampling->getLatencyInSamples()));
    }

    void MidBand::reset()
    {
        if (oversampling != nullptr)
            oversampling->reset();
    }

    void MidBand::process (juce::dsp::AudioBlock<float>& block) noexcept
    {
        jassert (oversampling != nullptr);

        auto upBlock = oversampling->processSamplesUp (juce::dsp::AudioBlock<const float> (block));

        const auto numChannels = upBlock.getNumChannels();
        const auto numSamples = upBlock.getNumSamples();

        const auto g1 = 1.0f + driveAmount01 * (midMaxDriveGain1 - 1.0f);
        const auto g2 = 1.0f + driveAmount01 * (midMaxDriveGain2 - 1.0f);

        for (size_t channel = 0; channel < numChannels; ++channel)
        {
            auto* data = upBlock.getChannelPointer (channel);

            for (size_t sample = 0; sample < numSamples; ++sample)
            {
                const auto x = data[sample];
                const auto driven = std::tanh (g2 * std::tanh (g1 * x));

                // No separate blend control on this band (see class docs) -
                // driveAmount01 itself crossfades toward an exact
                // passthrough as it approaches 0, so "Mid Drive = 0%" is a
                // guaranteed passthrough rather than "0% of a fixed,
                // already-non-unity nonlinearity".
                data[sample] = x + driveAmount01 * (driven - x);
            }
        }

        oversampling->processSamplesDown (block);
    }
}

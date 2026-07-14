#include "Voicing.h"

#include <cmath>

namespace
{
    // Voicing-specific drive-gain ceilings (issue #42's per-voicing
    // character): Gnaw pushes hard into a symmetric hard clip ("op-amp hard
    // clip"), Wool cascades two moderate-drive asymmetric soft-clip stages
    // ("cascaded soft-clip fuzz"), Razor stays comparatively mild ("tight
    // overdrive"). Starting points only - final voicing character is an
    // ear-tuning decision (see docs/manual.md and the PR description), not
    // something math alone should finalise.
    constexpr float gnawMaxDriveGain = 40.0f;
    constexpr float woolMaxDriveGain1 = 12.0f;
    constexpr float woolMaxDriveGain2 = 6.0f;
    constexpr float woolAsymmetryBias = 0.15f;
    constexpr float razorMaxDriveGain = 8.0f;

    // Voicing-specific mid-band character filter.
    constexpr float gnawMidFreqHz = 1000.0f, gnawMidGainDb = 0.0f, gnawMidQ = 0.7f;
    constexpr float woolMidFreqHz = 500.0f, woolMidGainDb = -6.0f, woolMidQ = 0.9f;   // mid scoop
    constexpr float razorMidFreqHz = 900.0f, razorMidGainDb = 5.0f, razorMidQ = 1.0f; // mid hump

    // Razor's pre-clip highpass: keeps the fundamental out of the shaper so
    // the low end stays tight instead of flabby.
    constexpr float razorPreHighPassHz = 200.0f;
    constexpr float razorPreHighPassQ = 0.7071f;

    // highTone sweep range.
    constexpr float minToneHz = 700.0f;
    constexpr float maxToneHz = 15000.0f;

    // 4x oversampling (factor exponent 2 -> 2^2): enough headroom to push
    // the aliasing products from a hard-clip/tanh nonlinearity well above
    // the audible band for a bass-range source, at a latency/CPU cost that
    // stays modest. FIR (not IIR) filtering trades latency for linear phase
    // and cleaner stopband rejection, appropriate for a distortion stage
    // that's also blended with a clean dry signal (highBlend) where phase
    // mismatches between wet and dry would be audible as comb filtering.
    constexpr size_t oversamplingFactorExponent = 2;
}

namespace cryp
{
    void Voicing::prepare (const juce::dsp::ProcessSpec& spec, float initialWetMixProportion01)
    {
        sampleRate = spec.sampleRate;

        oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
            static_cast<size_t> (spec.numChannels),
            oversamplingFactorExponent,
            juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple,
            true,
            true);
        oversampling->initProcessing (static_cast<size_t> (spec.maximumBlockSize));
        oversampling->reset();
        latencySamples = static_cast<int> (std::lround (oversampling->getLatencyInSamples()));

        preHighPass.prepare (spec);
        midFilter.prepare (spec);
        toneFilter.prepare (spec);

        // Prime the mix *before* prepare() so the mixer's internal reset()
        // (called at the end of DryWetMixer::prepare()) snaps its smoothed
        // dry/wet volumes to the correct starting point instead of a stale
        // default (JUCE 8.0.14 DryWetMixer gotcha).
        blendMixer.setMixingRule (juce::dsp::DryWetMixingRule::linear);
        blendMixer.setWetMixProportion (initialWetMixProportion01);
        blendMixer.prepare (spec);
        blendMixer.setWetLatency (static_cast<float> (latencySamples));

        updatePreHighPassCoefficients();
        updateMidFilterCoefficients();
        updateToneFilterCoefficients();

        reset();
    }

    void Voicing::reset()
    {
        if (oversampling != nullptr)
            oversampling->reset();

        preHighPass.reset();
        midFilter.reset();
        toneFilter.reset();
        blendMixer.reset();
    }

    void Voicing::setVoicing (VoicingType newVoicing) noexcept
    {
        if (newVoicing == voicing)
            return;

        voicing = newVoicing;

        updateMidFilterCoefficients();

        if (voicing == VoicingType::razor)
        {
            updatePreHighPassCoefficients();
            // Avoid carrying over stale filter state from a previous Razor
            // session (or silence) into the newly (re)activated pre-HPF.
            preHighPass.reset();
        }
    }

    void Voicing::process (juce::dsp::AudioBlock<float>& block) noexcept
    {
        jassert (oversampling != nullptr);

        // Control-rate refresh: highTone is read fresh every host block by
        // the processor and forwarded via setTone(), so recomputing the
        // tone filter's (zero-allocation, see RealtimeCoefficients.h)
        // coefficients once per block keeps it tracking automation without
        // per-sample coefficient-recompute cost.
        updateToneFilterCoefficients();

        blendMixer.pushDrySamples (juce::dsp::AudioBlock<const float> (block));

        if (voicing == VoicingType::razor)
            preHighPass.process (juce::dsp::ProcessContextReplacing<float> (block));

        auto upBlock = oversampling->processSamplesUp (juce::dsp::AudioBlock<const float> (block));

        const auto numChannels = upBlock.getNumChannels();
        const auto numSamples = upBlock.getNumSamples();

        for (size_t channel = 0; channel < numChannels; ++channel)
        {
            auto* data = upBlock.getChannelPointer (channel);

            for (size_t sample = 0; sample < numSamples; ++sample)
                data[sample] = shapeSample (data[sample]);
        }

        oversampling->processSamplesDown (block);

        juce::dsp::ProcessContextReplacing<float> context (block);
        midFilter.process (context);
        toneFilter.process (context);

        blendMixer.mixWetSamples (block);
    }

    float Voicing::shapeSample (float x) const noexcept
    {
        switch (voicing)
        {
            case VoicingType::gnaw:
            {
                // Op-amp-style hard clip: symmetric, unforgiving.
                const auto driveGain = 1.0f + driveAmount01 * (gnawMaxDriveGain - 1.0f);
                return juce::jlimit (-1.0f, 1.0f, driveGain * x);
            }

            case VoicingType::wool:
            {
                // Two cascaded tanh soft-clip stages; a small DC bias ahead
                // of the second stage (removed again afterwards, so no
                // steady-state DC offset reaches the output) breaks the
                // symmetry for a grittier, more fuzz-like harmonic series.
                const auto g1 = 1.0f + driveAmount01 * (woolMaxDriveGain1 - 1.0f);
                const auto g2 = 1.0f + driveAmount01 * (woolMaxDriveGain2 - 1.0f);
                const auto stage1 = std::tanh (g1 * x);
                return std::tanh (g2 * (stage1 + woolAsymmetryBias)) - std::tanh (g2 * woolAsymmetryBias);
            }

            case VoicingType::razor:
            default:
            {
                // Comparatively mild tanh soft-clip; the "tight overdrive"
                // character comes mostly from the pre-clip highpass and the
                // mid-hump filter, not from the shaper itself.
                const auto driveGain = 1.0f + driveAmount01 * (razorMaxDriveGain - 1.0f);
                return std::tanh (driveGain * x);
            }
        }
    }

    void Voicing::updateMidFilterCoefficients() noexcept
    {
        float freqHz = gnawMidFreqHz, gainDb = gnawMidGainDb, q = gnawMidQ;

        if (voicing == VoicingType::wool)
        {
            freqHz = woolMidFreqHz;
            gainDb = woolMidGainDb;
            q = woolMidQ;
        }
        else if (voicing == VoicingType::razor)
        {
            freqHz = razorMidFreqHz;
            gainDb = razorMidGainDb;
            q = razorMidQ;
        }

        const auto gainFactor = juce::Decibels::decibelsToGain (gainDb);
        const auto raw = juce::dsp::IIR::ArrayCoefficients<float>::makePeakFilter (sampleRate, freqHz, q, gainFactor);
        applyBiquadCoefficients (*midFilter.state, raw);
    }

    void Voicing::updateToneFilterCoefficients() noexcept
    {
        const auto cutoffHz = juce::mapToLog10 (toneAmount01, minToneHz, maxToneHz);
        const auto clampedHz = juce::jlimit (20.0f, static_cast<float> (sampleRate * 0.49), cutoffHz);
        const auto raw = juce::dsp::IIR::ArrayCoefficients<float>::makeFirstOrderLowPass (sampleRate, clampedHz);
        applyFirstOrderCoefficients (*toneFilter.state, raw);
    }

    void Voicing::updatePreHighPassCoefficients() noexcept
    {
        const auto raw = juce::dsp::IIR::ArrayCoefficients<float>::makeHighPass (sampleRate, razorPreHighPassHz, razorPreHighPassQ);
        applyBiquadCoefficients (*preHighPass.state, raw);
    }
}

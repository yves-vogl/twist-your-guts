#pragma once

#include <juce_dsp/juce_dsp.h>

// Full-band input noise gate (issue #42), sitting between input trim and the
// LR4 crossover split in the signal chain. Thin wrapper around
// juce::dsp::NoiseGate<float> (JUCE 8.0.14,
// juce_dsp/widgets/juce_NoiseGate.h) so the processor and the test suite
// share one real-time-safe seam, matching the pattern already established by
// cryp::Crossover.
//
// juce::dsp::NoiseGate's ballistics filters carry their own per-channel
// state internally (via BallisticsFilter, indexed by the `channel` argument
// passed to processSample()), so a single instance handles mono/stereo
// without any extra per-channel bookkeeping here.
namespace cryp
{
    class NoiseGateStage
    {
    public:
        NoiseGateStage() = default;

        void prepare (const juce::dsp::ProcessSpec& spec);
        void reset();

        void setEnabled (bool shouldBeEnabled) noexcept { enabled = shouldBeEnabled; }
        bool isEnabled() const noexcept { return enabled; }

        // Real-time safe: NoiseGate::setThreshold/setRatio/setAttack/
        // setRelease just recompute ballistics coefficients, no allocation.
        void setThresholdDb (float newThresholdDb) noexcept { gate.setThreshold (newThresholdDb); }
        void setRatio (float newRatio) noexcept { gate.setRatio (newRatio); }
        void setAttackMs (float newAttackMs) noexcept { gate.setAttack (newAttackMs); }
        void setReleaseMs (float newReleaseMs) noexcept { gate.setRelease (newReleaseMs); }

        // In-place gate. When disabled, this is a deliberate no-op (not just
        // a unity-gain pass through the gate's own math) so the disabled
        // state is bit-exact transparent and costs nothing on the audio
        // thread.
        void process (juce::dsp::AudioBlock<float>& block) noexcept
        {
            if (! enabled)
                return;

            gate.process (juce::dsp::ProcessContextReplacing<float> (block));
        }

    private:
        juce::dsp::NoiseGate<float> gate;
        bool enabled = false;
    };
}

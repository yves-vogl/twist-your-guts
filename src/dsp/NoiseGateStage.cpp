#include "NoiseGateStage.h"

namespace cryp
{
    void NoiseGateStage::prepare (const juce::dsp::ProcessSpec& spec)
    {
        gate.prepare (spec);
    }

    void NoiseGateStage::reset()
    {
        gate.reset();
    }
}

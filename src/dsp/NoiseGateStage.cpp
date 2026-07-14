#include "NoiseGateStage.h"

namespace tyg
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

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Builds the complete v1.0 AudioProcessorValueTreeState parameter layout.
// Extracted from the processor into its own translation unit so it can be
// unit-tested in isolation (SharedCode target) without instantiating the
// full AudioProcessor. See ParameterIds.h for the frozen-ID contract this
// function must honour: IDs never change, ranges/defaults may be tuned.
namespace cryp
{
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
}

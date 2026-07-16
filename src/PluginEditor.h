#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "presets/PresetBar.h"

class CryptaAudioProcessor;

// Minimal editor: a M2 preset bar (src/presets/PresetBar.h) docked at the
// top, wrapping JUCE's GenericAudioProcessorEditor below it so every APVTS
// parameter still gets a working control for free. A custom GUI replaces
// this in a later milestone (M3).
class CryptaAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit CryptaAudioProcessorEditor (CryptaAudioProcessor& processorToEdit);
    ~CryptaAudioProcessorEditor() override;

    void resized() override;

private:
    // M2 preset system (src/presets/PresetBar.h) - a horizontal strip
    // docked at the top of the editor. Constructed after the localisation
    // frame is installed (see the constructor) so its TRANS()'d strings (and
    // any of its own dialogs opened later) pick up the right language from
    // the very first paint.
    basilica::presets::PresetBar presetBar;

    juce::GenericAudioProcessorEditor genericEditor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CryptaAudioProcessorEditor)
};

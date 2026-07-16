#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "presets/Localisation.h"

#include <BinaryData.h>

namespace
{
    constexpr int presetBarHeight = 28;
    constexpr int margin = 4;

    // M2 i18n frame (.scaffold/specs/preset-system-m2.md): selects German
    // (resources/i18n/de.txt) or falls through to English, once, at editor
    // construction - see Localisation.h's docs. `presetBar` is a member
    // initialised via the constructor's initialiser list, and its own
    // constructor already calls TRANS() on every button label - member
    // initialisers run in declaration order regardless of the order they're
    // written in, so this helper (called from presetBar's own initialiser
    // expression below) is what actually guarantees installLocalisation()
    // runs before presetBar exists, not a installLocalisation() call in the
    // constructor *body*, which would run too late.
    basilica::presets::PresetManager& initLocalisationThenGetPresetManager (CryptaAudioProcessor& processor)
    {
        basilica::presets::installLocalisation (BinaryData::de_txt, BinaryData::de_txtSize);
        return processor.presetManager;
    }
}

CryptaAudioProcessorEditor::CryptaAudioProcessorEditor (CryptaAudioProcessor& processorToEdit)
    : juce::AudioProcessorEditor (&processorToEdit),
      presetBar (initLocalisationThenGetPresetManager (processorToEdit)),
      genericEditor (processorToEdit)
{
    addAndMakeVisible (presetBar);
    addAndMakeVisible (genericEditor);

    setResizable (true, true);
    setSize (genericEditor.getWidth(), presetBarHeight + margin + genericEditor.getHeight());
}

CryptaAudioProcessorEditor::~CryptaAudioProcessorEditor() = default;

void CryptaAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    presetBar.setBounds (bounds.removeFromTop (presetBarHeight));
    bounds.removeFromTop (margin);

    genericEditor.setBounds (bounds);
}

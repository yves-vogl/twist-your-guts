#include "PluginEditor.h"
#include "PluginProcessor.h"

CryptaAudioProcessorEditor::CryptaAudioProcessorEditor (CryptaAudioProcessor& processorToEdit)
    : juce::AudioProcessorEditor (&processorToEdit),
      genericEditor (processorToEdit)
{
    addAndMakeVisible (genericEditor);
    setResizable (true, true);
    setSize (genericEditor.getWidth(), genericEditor.getHeight());
}

CryptaAudioProcessorEditor::~CryptaAudioProcessorEditor() = default;

void CryptaAudioProcessorEditor::resized()
{
    genericEditor.setBounds (getLocalBounds());
}

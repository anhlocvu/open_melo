#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class OpenMeloAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     public juce::AudioProcessorEditorARAExtension
{
public:
    explicit OpenMeloAudioProcessorEditor(OpenMeloAudioProcessor&);
    ~OpenMeloAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    bool keyPressed(const juce::KeyPress& key) override;
    void focusGained(FocusChangeType cause) override;

private:
    OpenMeloAudioProcessor& audioProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenMeloAudioProcessorEditor)
};

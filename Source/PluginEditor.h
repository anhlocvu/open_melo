#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class OpenMeloAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     public juce::FileDragAndDropTarget
{
public:
    explicit OpenMeloAudioProcessorEditor(OpenMeloAudioProcessor&);
    ~OpenMeloAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    bool keyPressed(const juce::KeyPress& key) override;
    void focusGained(FocusChangeType cause) override;

    // File Drag and Drop for Instant Track Scanning
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

private:
    OpenMeloAudioProcessor& audioProcessor;
    std::unique_ptr<juce::FileChooser> fileChooser;

    void openAudioFileDialog();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenMeloAudioProcessorEditor)
};

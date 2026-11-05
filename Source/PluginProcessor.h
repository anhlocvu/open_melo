#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "OpenMeloARA.h"
#include "NoteManager.h"
#include "AuditionVoice.h"

class OpenMeloAudioProcessor : public juce::AudioProcessor,
                               public juce::AudioProcessorARAExtension
{
public:
    OpenMeloAudioProcessor();
    ~OpenMeloAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    NoteManager& getNoteManager();
    AuditionVoice& getAuditionVoice();

    bool isBoundToARAMode() const { return isBoundToARA(); }

protected:
    void didBindToARA() noexcept override;

private:
    NoteManager fallbackNoteManager;
    AuditionVoice auditionVoice;
    OpenMeloDocumentController* documentController = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenMeloAudioProcessor)
};

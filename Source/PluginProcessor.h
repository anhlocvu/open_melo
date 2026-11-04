#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "NoteManager.h"
#include "AuditionVoice.h"

class OpenMeloAudioProcessor : public juce::AudioProcessor
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

    NoteManager& getNoteManager() { return noteManager; }
    AuditionVoice& getAuditionVoice() { return auditionVoice; }

private:
    NoteManager noteManager;
    AuditionVoice auditionVoice;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenMeloAudioProcessor)
};

#pragma once

#include "NoteModel.h"
#include "PitchShiftEngine.h"
#include <vector>
#include <functional>

class NoteManager
{
public:
    NoteManager();
    ~NoteManager();

    void setSampleRate(double newSampleRate);
    void clearNotes();

    // Continuous real-time ingestion from DAW track aligned with exact timeline
    void processIncomingTrackAudio(const juce::AudioBuffer<float>& buffer, int64_t hostPlayheadSample, int numSamples);

    // Sample-accurate playback replacement when DAW host is playing
    void renderTrackPlayback(juce::AudioBuffer<float>& buffer, int64_t hostPlayheadSample, int numSamples);

    // Fast Offline Scan: Scan entire audio buffer or file at once (instant analysis)
    bool scanAudioBuffer(const juce::AudioBuffer<float>& buffer, double fileSampleRate, int64_t startTimelineSample = 0);
    bool scanAudioFile(const juce::File& file, int64_t startTimelineSample = 0);

    // Check if an existing note covers the given timeline position
    bool isTimelineCovered(int64_t samplePos) const;

    // State Persistence (Saving / Loading with DAW project)
    void serialize(juce::MemoryOutputStream& stream) const;
    void deserialize(juce::MemoryInputStream& stream);

    // Navigation
    void navigateLeft();
    void navigateRight();
    void selectNoteIndex(int index);

    // Audio Audition & Info
    void playCurrentNote();

    // Pitch Adjustments
    void transposeCurrentNote(int deltaSemitones);
    void tuneCentsCurrentNote(int deltaCents);
    void snapCurrentNoteToZero();

    // Note Slicing / Splitting
    bool splitCurrentNote();
    int sliceCurrentNoteIntoPieces(int targetSegmentMs = 60);

    // Getters
    int getSelectedNoteIndex() const { return selectedNoteIndex; }
    int getNumNotes() const { return (int)notes.size(); }
    const OpenMeloNote* getSelectedNote() const;
    const std::vector<OpenMeloNote>& getAllNotes() const { return notes; }

    // Pre-renders pitch-shifted and formant-preserved audio for a note
    void updateNoteRendering(OpenMeloNote& note);

    // Callbacks
    std::function<void(const juce::AudioBuffer<float>&)> onPlayAudition;
    std::function<void()> onNotesChanged;

private:
    double sampleRate = 44100.0;
    int selectedNoteIndex = 0;
    std::vector<OpenMeloNote> notes;
    PitchShiftEngine pitchEngine;

    // Continuous timeline capture tracking
    std::vector<float> liveTrackAccumulatorLeft;
    std::vector<float> liveTrackAccumulatorRight;
    std::vector<float> currentNoteAudioLeft;
    std::vector<float> currentNoteAudioRight;
    double currentNotePitchSum = 0.0;
    int currentNotePitchCount = 0;
    int silenceFrameCount = 0;
    int64_t currentNoteTimelineStart = 0;
    int64_t lastProcessedHostSample = -1;

    double detectPitchYin(const float* buffer, int length);
    void finalizeCurrentTrackNote(int64_t currentEndSample);
    void sortNotes();

    void renderAndAuditionNote(OpenMeloNote& note);
    void triggerSpeech(const juce::String& text);
};

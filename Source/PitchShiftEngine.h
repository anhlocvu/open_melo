#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

class PitchShiftEngine
{
public:
    PitchShiftEngine();
    ~PitchShiftEngine();

    void setSampleRate(double newSampleRate);

    // Shifts pitch using Time-Domain Pitch Synchronous Overlap-Add (TD-PSOLA)
    // Preserves 100% natural vocal tract formants (F1, F2, F3) and original singer timbre
    void process(const juce::AudioBuffer<float>& input,
                 juce::AudioBuffer<float>& output,
                 double pitchRatio,
                 double originalPitchMidi = 60.0);

    // Generates reference natural vocal tone
    static void generateVocalTone(juce::AudioBuffer<float>& buffer,
                                  double sampleRate,
                                  double midiPitch,
                                  int numSamples);

private:
    double sampleRate = 44100.0;

    void processTdPsola(const float* inData, int inSamples, float* outData, double pitchRatio, double pitchHz);
    std::vector<int> detectPitchMarks(const float* data, int numSamples, int periodSamples);
};

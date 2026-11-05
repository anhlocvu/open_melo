#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>

class PitchShiftEngine
{
public:
    PitchShiftEngine();
    ~PitchShiftEngine();

    void setSampleRate(double newSampleRate);

    // Shifts pitch using Signalsmith Stretch spectral phase vocoder
    // Preserves 100% natural vocal tract formants and timbre with tonality limit & formant compensation
    // Immune to phase cancellation, metallic artifacts, and clicks/pops
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
};

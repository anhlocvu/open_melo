#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>

class PitchShiftEngine
{
public:
    PitchShiftEngine();
    ~PitchShiftEngine();

    void setSampleRate(double newSampleRate);

    // Shifts pitch using WSOLA (Waveform Similarity Overlap-Add) with Spectral Formant Preservation
    // Preserves 100% natural vocal tract formants (F1, F2, F3), zero clicks, and exact timing/speed
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

    void processWsolaPitchShift(const float* inData, int inSamples, float* outData, double pitchRatio);
    void applyFormantPreservation(const float* origData, float* shiftedData, int inSamples, double pitchRatio);
    bool isUnvoicedSegment(const float* data, int numSamples);
};

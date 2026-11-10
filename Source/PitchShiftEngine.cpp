#include "PitchShiftEngine.h"
#include "signalsmith-stretch.h"
#include <cmath>
#include <algorithm>
#include <vector>

PitchShiftEngine::PitchShiftEngine()
{
}

PitchShiftEngine::~PitchShiftEngine()
{
}

void PitchShiftEngine::setSampleRate(double newSampleRate)
{
    sampleRate = (newSampleRate > 0.0) ? newSampleRate : 44100.0;
}

void PitchShiftEngine::process(const juce::AudioBuffer<float>& input,
                               juce::AudioBuffer<float>& output,
                               double pitchRatio,
                               double originalPitchMidi)
{
    int numChannels = input.getNumChannels();
    int inSamples = input.getNumSamples();

    if (inSamples <= 0 || numChannels <= 0 || pitchRatio <= 0.0)
    {
        output.setSize(numChannels, 0);
        return;
    }

    output.setSize(numChannels, inSamples);

    // Identity bypass when pitch is not shifted
    if (std::abs(pitchRatio - 1.0) < 0.0005)
    {
        output.makeCopyOf(input);
        return;
    }

    double semitones = 12.0 * std::log2(pitchRatio);
    double clampedMidi = juce::jlimit(36.0, 96.0, originalPitchMidi);
    double pitchHz = 440.0 * std::pow(2.0, (clampedMidi - 69.0) / 12.0);

    // Vocal-optimized STFT configuration (~46ms window / ~11.5ms hop -> 2048 FFT @ 44.1/48kHz)
    // Eliminates the 120ms window phase-smearing and robotic tail of presetDefault
    int blockSamples = juce::jmax(512, (int)std::round(sampleRate * 0.046));
    int intervalSamples = juce::jmax(128, blockSamples / 4);

    signalsmith::stretch::SignalsmithStretch<float> stretch;
    stretch.configure(numChannels, blockSamples, intervalSamples, false);

    // 1. Tonality limit preserves natural vocal breath/sibilants above 8kHz
    stretch.setTransposeSemitones((float)semitones, 8000.0f / (float)sampleRate);

    // 2. Formant preservation: compensate pitch shift so vocal tract formants stay at original frequency
    stretch.setFormantSemitones(0.0f, true);
    stretch.setFormantBase((float)(pitchHz / sampleRate));

    // Always pad both ends by seekLen so 100% of the vocal note is processed in steady-state
    // and never contaminated by outputSeek startup or flush(zeros) phase-inversion tail.
    const int seekLen = juce::jmax(blockSamples + intervalSamples, stretch.outputSeekLength(1.0f));
    const int pad = seekLen;
    const int paddedLen = inSamples + 2 * pad;

    juce::AudioBuffer<float> paddedInput(numChannels, paddedLen);
    juce::AudioBuffer<float> paddedOutput(numChannels, paddedLen);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        const float* src = input.getReadPointer(ch);
        float* dest = paddedInput.getWritePointer(ch);

        // Copy main note into the center
        std::copy(src, src + inSamples, dest + pad);

        // Smoothly tapered mirrored padding on the left and right
        for (int i = 0; i < pad; ++i)
        {
            float taper = 0.5f * (1.0f + std::cos(juce::MathConstants<float>::pi * (float)i / (float)pad));
            dest[pad - 1 - i] = src[i % inSamples] * taper;
            dest[pad + inSamples + i] = src[inSamples - 1 - (i % inSamples)] * taper;
        }
    }

    std::vector<const float*> inPtrs(numChannels);
    std::vector<float*> outPtrs(numChannels);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        inPtrs[ch] = paddedInput.getReadPointer(ch);
        outPtrs[ch] = paddedOutput.getWritePointer(ch);
    }

    bool success = stretch.exact(inPtrs.data(), paddedLen, outPtrs.data(), paddedLen);
    if (success)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            output.copyFrom(ch, 0, paddedOutput, ch, pad, inSamples);
        }

        // Apply subtle 32-sample micro-fade at note boundaries to ensure click-free auditioning
        const int fadeLen = juce::jmin(32, inSamples / 4);
        if (fadeLen > 1)
        {
            output.applyGainRamp(0, fadeLen, 0.0f, 1.0f);
            output.applyGainRamp(inSamples - fadeLen, fadeLen, 1.0f, 0.0f);
        }
    }
    else
    {
        output.makeCopyOf(input);
    }
}

void PitchShiftEngine::generateVocalTone(juce::AudioBuffer<float>& buffer,
                                         double sRate,
                                         double midiPitch,
                                         int numSamples)
{
    buffer.setSize(2, numSamples);
    buffer.clear();

    if (numSamples <= 0 || sRate <= 0.0)
        return;

    double f0 = 440.0 * std::pow(2.0, (midiPitch - 69.0) / 12.0);
    double phase = 0.0;
    double phaseInc = f0 / sRate;

    float* l = buffer.getWritePointer(0);
    float* r = buffer.getWritePointer(1);

    // Natural formant filters for human vowel /a/
    double f1 = 800.0, bw1 = 80.0;
    double f2 = 1200.0, bw2 = 90.0;
    double f3 = 2500.0, bw3 = 120.0;

    auto calcFormantGain = [&](double freq, double fc, double bw) -> double {
        double diff = (freq - fc) / (bw * 0.5);
        return std::exp(-0.5 * diff * diff);
    };

    for (int i = 0; i < numSamples; ++i)
    {
        float val = 0.0f;
        int maxHarmonics = juce::jmin(40, (int)(sRate * 0.45 / f0));

        for (int h = 1; h <= maxHarmonics; ++h)
        {
            double harmonicFreq = f0 * h;
            double sourceAmp = 1.0 / (double)h;

            double formantAmp = 0.25
                              + calcFormantGain(harmonicFreq, f1, bw1) * 1.0
                              + calcFormantGain(harmonicFreq, f2, bw2) * 0.6
                              + calcFormantGain(harmonicFreq, f3, bw3) * 0.3;

            val += (float)(sourceAmp * formantAmp * std::sin(2.0 * juce::MathConstants<double>::pi * phase * h));
        }

        // Envelope
        float env = 1.0f;
        int attack = (int)(sRate * 0.015);
        int release = (int)(sRate * 0.025);

        if (i < attack)
            env = (float)i / (float)attack;
        else if (i > numSamples - release)
            env = (float)(numSamples - i) / (float)release;

        float sampleOut = val * env * 0.25f;
        l[i] = sampleOut;
        r[i] = sampleOut;

        phase += phaseInc;
        if (phase >= 1.0) phase -= 1.0;
    }
}

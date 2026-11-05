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
    double pitchHz = 440.0 * std::pow(2.0, (originalPitchMidi - 69.0) / 12.0);

    signalsmith::stretch::SignalsmithStretch<float> stretch;
    stretch.presetDefault(numChannels, (float)sampleRate);

    // 1. Tonality limit preserves vocal timbre above 8kHz
    stretch.setTransposeSemitones((float)semitones, 8000.0f / (float)sampleRate);

    // 2. Formant preservation: compensate pitch shift so vocal tract formants stay at original frequency
    stretch.setFormantSemitones(0.0f, true);
    stretch.setFormantBase((float)(pitchHz / sampleRate));

    auto seekLen = stretch.outputSeekLength(1.0f);

    if (inSamples >= seekLen)
    {
        std::vector<const float*> inPtrs(numChannels);
        std::vector<float*> outPtrs(numChannels);
        for (int ch = 0; ch < numChannels; ++ch)
        {
            inPtrs[ch] = input.getReadPointer(ch);
            outPtrs[ch] = output.getWritePointer(ch);
        }

        bool success = stretch.exact(inPtrs.data(), inSamples, outPtrs.data(), inSamples);
        if (!success)
        {
            output.makeCopyOf(input);
        }
    }
    else
    {
        // For short segments (< seekLen), pad with mirrored/reflected audio at both ends
        int pad = seekLen;
        int paddedLen = inSamples + 2 * pad;

        juce::AudioBuffer<float> paddedInput(numChannels, paddedLen);
        juce::AudioBuffer<float> paddedOutput(numChannels, paddedLen);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float* src = input.getReadPointer(ch);
            float* dest = paddedInput.getWritePointer(ch);

            // Copy middle
            std::copy(src, src + inSamples, dest + pad);

            // Reflect left
            for (int i = 0; i < pad; ++i)
                dest[pad - 1 - i] = src[i % inSamples];

            // Reflect right
            for (int i = 0; i < pad; ++i)
                dest[pad + inSamples + i] = src[(inSamples - 1 - (i % inSamples))];
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
        }
        else
        {
            output.makeCopyOf(input);
        }
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

            double formantAmp = calcFormantGain(harmonicFreq, f1, bw1) * 1.0
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

#include "PitchShiftEngine.h"
#include <cmath>
#include <algorithm>

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

std::vector<int> PitchShiftEngine::detectPitchMarks(const float* data, int numSamples, int periodSamples)
{
    std::vector<int> marks;
    if (numSamples <= 0 || periodSamples <= 2)
        return marks;

    // 1. Find the first local maximum within the first period
    int searchLen = juce::jmin(numSamples, periodSamples * 2);
    int bestIdx = 0;
    float bestVal = -1e9f;

    for (int i = 0; i < searchLen; ++i)
    {
        if (data[i] > bestVal)
        {
            bestVal = data[i];
            bestIdx = i;
        }
    }

    marks.push_back(bestIdx);

    // 2. Iteratively search for the next glottal pulse peak in the range [curr + 0.8 * T0, curr + 1.2 * T0]
    int curr = bestIdx;
    int minStep = juce::jmax(2, (int)(periodSamples * 0.75f));
    int maxStep = (int)(periodSamples * 1.25f);

    while (curr + minStep < numSamples)
    {
        int winStart = curr + minStep;
        int winEnd = juce::jmin(numSamples - 1, curr + maxStep);

        int peakIdx = winStart;
        float peakVal = -1e9f;

        for (int i = winStart; i <= winEnd; ++i)
        {
            if (data[i] > peakVal)
            {
                peakVal = data[i];
                peakIdx = i;
            }
        }

        marks.push_back(peakIdx);
        curr = peakIdx;
    }

    return marks;
}

void PitchShiftEngine::processTdPsola(const float* inData, int inSamples, float* outData, double pitchRatio, double pitchHz)
{
    if (inSamples <= 0)
        return;

    // If no shift, bit-transparent bypass
    if (std::abs(pitchRatio - 1.0) < 0.001)
    {
        std::copy(inData, inData + inSamples, outData);
        return;
    }

    // Calculate fundamental period T0 in samples
    double safePitchHz = juce::jlimit(50.0, 1500.0, pitchHz);
    int periodSamples = juce::jlimit(16, 1024, (int)std::round(sampleRate / safePitchHz));

    // Pitch marks for glottal pulse epochs
    std::vector<int> marks = detectPitchMarks(inData, inSamples, periodSamples);

    if (marks.size() < 2)
    {
        // Fallback: regular spacing if no distinct marks found
        marks.clear();
        for (int p = 0; p < inSamples; p += periodSamples)
            marks.push_back(p);
    }

    // Synthesis period
    double targetPeriod = (double)periodSamples / pitchRatio;
    targetPeriod = juce::jlimit(8.0, 2048.0, targetPeriod);

    std::vector<float> accum(inSamples, 0.0f);
    std::vector<float> norm(inSamples, 0.0f);

    // Synthesis time pointer
    double synthTime = (double)marks[0];
    int halfWin = periodSamples;
    int winLen = halfWin * 2;

    // Precompute Hann window of length 2 * T0
    std::vector<float> window(winLen);
    for (int i = 0; i < winLen; ++i)
    {
        window[i] = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * (float)i / (float)(winLen - 1)));
    }

    while (synthTime < (double)inSamples)
    {
        int synthCenter = (int)std::round(synthTime);

        // Find closest analysis mark to the current synthesis time
        int closestMark = marks[0];
        int minDiff = std::abs(marks[0] - synthCenter);

        for (size_t i = 1; i < marks.size(); ++i)
        {
            int diff = std::abs(marks[i] - synthCenter);
            if (diff < minDiff)
            {
                minDiff = diff;
                closestMark = marks[i];
            }
            else if (diff > minDiff)
            {
                // Marks are monotonically increasing
                break;
            }
        }

        // Overlap-add windowed period from analysis point into synthesis position
        for (int i = 0; i < winLen; ++i)
        {
            int inIdx = closestMark - halfWin + i;
            int outIdx = synthCenter - halfWin + i;

            if (outIdx >= 0 && outIdx < inSamples)
            {
                float sampleVal = 0.0f;
                if (inIdx >= 0 && inIdx < inSamples)
                    sampleVal = inData[inIdx];

                float w = window[i];
                accum[outIdx] += sampleVal * w;
                norm[outIdx] += w;
            }
        }

        synthTime += targetPeriod;
    }

    // Normalize output and crossfade smoothly
    for (int i = 0; i < inSamples; ++i)
    {
        if (norm[i] > 0.02f)
        {
            outData[i] = accum[i] / norm[i];
        }
        else
        {
            outData[i] = inData[i];
        }
    }
}

void PitchShiftEngine::process(const juce::AudioBuffer<float>& input,
                               juce::AudioBuffer<float>& output,
                               double pitchRatio,
                               double originalPitchMidi)
{
    int numChannels = input.getNumChannels();
    int inSamples = input.getNumSamples();

    if (inSamples == 0 || pitchRatio <= 0.0)
    {
        output.setSize(numChannels, 0);
        return;
    }

    output.setSize(numChannels, inSamples);
    output.clear();

    if (std::abs(pitchRatio - 1.0) < 0.001)
    {
        output.makeCopyOf(input);
        return;
    }

    double pitchHz = 440.0 * std::pow(2.0, (originalPitchMidi - 69.0) / 12.0);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        processTdPsola(input.getReadPointer(ch),
                       inSamples,
                       output.getWritePointer(ch),
                       pitchRatio,
                       pitchHz);
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

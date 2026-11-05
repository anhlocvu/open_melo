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

bool PitchShiftEngine::isUnvoicedSegment(const float* data, int numSamples)
{
    if (numSamples < 128)
        return false;

    int zeroCrossings = 0;
    float energy = 0.0f;

    for (int i = 0; i < numSamples - 1; ++i)
    {
        energy += data[i] * data[i];
        if ((data[i] >= 0.0f && data[i + 1] < 0.0f) || (data[i] < 0.0f && data[i + 1] >= 0.0f))
            zeroCrossings++;
    }

    float zcr = (float)zeroCrossings / (float)(numSamples - 1);
    float rms = std::sqrt(energy / (float)numSamples);

    // High zero-crossing rate with moderate/low energy = sibilant / unvoiced fricative ('s', 'sh', 't', breath)
    return (zcr > 0.30f && rms < 0.20f);
}

void PitchShiftEngine::processWsolaPitchShift(const float* inData, int inSamples, float* outData, double pitchRatio)
{
    if (inSamples <= 0)
        return;

    // Bit-transparent bypass if no transposition
    if (std::abs(pitchRatio - 1.0) < 0.0005)
    {
        std::copy(inData, inData + inSamples, outData);
        return;
    }

    // 1. Resample inData by factor alpha = pitchRatio (Cubic Hermite Interpolation)
    int resampledLen = juce::jmax(1, (int)std::round((double)inSamples / pitchRatio));
    std::vector<float> resampled((size_t)resampledLen);

    for (int i = 0; i < resampledLen; ++i)
    {
        double pos = (double)i * pitchRatio;
        int idx = (int)pos;
        float frac = (float)(pos - (double)idx);

        float y0 = (idx > 0) ? inData[idx - 1] : inData[0];
        float y1 = (idx < inSamples) ? inData[idx] : 0.0f;
        float y2 = (idx + 1 < inSamples) ? inData[idx + 1] : y1;
        float y3 = (idx + 2 < inSamples) ? inData[idx + 2] : y2;

        float c0 = y1;
        float c1 = 0.5f * (y2 - y0);
        float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);

        resampled[(size_t)i] = ((c3 * frac + c2) * frac + c1) * frac + c0;
    }

    // 2. WSOLA time stretch from resampledLen back to inSamples (Preserving 100% exact note length & tempo)
    int winLen = juce::jlimit(512, 2048, (int)(sampleRate * 0.032));
    if (winLen % 2 != 0)
        winLen++;

    int hopS = winLen / 4; // 75% overlap for ultra-smooth crossfade
    double hopA = (double)hopS * ((double)resampledLen / (double)inSamples);
    int maxDelta = hopS / 2;

    std::vector<float> hann((size_t)winLen);
    for (int i = 0; i < winLen; ++i)
    {
        hann[(size_t)i] = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * (float)i / (float)(winLen - 1)));
    }

    std::vector<float> accum((size_t)inSamples, 0.0f);
    std::vector<float> norm((size_t)inSamples, 0.0f);

    int synPos = 0;
    double anPos = 0.0;

    while (synPos + winLen <= inSamples && (int)anPos + winLen <= resampledLen)
    {
        int nomAn = (int)anPos;
        int bestAn = nomAn;

        if (synPos > 0)
        {
            float bestCorr = -1e9f;
            int deltaMin = juce::jmax(0, nomAn - maxDelta);
            int deltaMax = juce::jmin(resampledLen - winLen, nomAn + maxDelta);

            for (int cand = deltaMin; cand <= deltaMax; ++cand)
            {
                float corr = 0.0f;
                for (int j = 0; j < winLen; j += 4)
                {
                    corr += resampled[(size_t)(cand + j)] * accum[(size_t)(synPos + j)];
                }
                if (corr > bestCorr)
                {
                    bestCorr = corr;
                    bestAn = cand;
                }
            }
        }

        // Overlap-add windowed grain
        for (int j = 0; j < winLen; ++j)
        {
            float w = hann[(size_t)j];
            accum[(size_t)(synPos + j)] += resampled[(size_t)(bestAn + j)] * w;
            norm[(size_t)(synPos + j)] += w;
        }

        synPos += hopS;
        anPos += hopA;
    }

    // 3. Normalization (Smooth without clicks or divide-by-zero)
    for (int i = 0; i < inSamples; ++i)
    {
        if (norm[(size_t)i] > 1e-4f)
        {
            outData[i] = accum[(size_t)i] / norm[(size_t)i];
        }
        else
        {
            outData[i] = inData[i];
        }
    }
}

void PitchShiftEngine::applyFormantPreservation(const float* origData, float* shiftedData, int inSamples, double pitchRatio)
{
    if (inSamples <= 512 || std::abs(pitchRatio - 1.0) < 0.02)
        return;

    const int fftOrder = 11;
    const int fftSize = 1 << fftOrder; // 2048 points
    const int hopSize = fftSize / 2;    // 50% overlap

    juce::dsp::FFT fft(fftOrder);

    std::vector<float> hann((size_t)fftSize);
    for (int i = 0; i < fftSize; ++i)
    {
        hann[(size_t)i] = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * (float)i / (float)(fftSize - 1)));
    }

    std::vector<float> origFftBuffer((size_t)(fftSize * 2), 0.0f);
    std::vector<float> shiftFftBuffer((size_t)(fftSize * 2), 0.0f);

    std::vector<float> origMag((size_t)(fftSize / 2 + 1));
    std::vector<float> shiftMag((size_t)(fftSize / 2 + 1));
    std::vector<float> origEnv((size_t)(fftSize / 2 + 1));
    std::vector<float> shiftEnv((size_t)(fftSize / 2 + 1));

    std::vector<float> outputAccum((size_t)inSamples, 0.0f);
    std::vector<float> outputNorm((size_t)inSamples, 0.0f);

    const int filterRadius = 14; // Smoothing filter width (~300 Hz)

    for (int pos = 0; pos + fftSize <= inSamples; pos += hopSize)
    {
        // Window input frames
        std::fill(origFftBuffer.begin(), origFftBuffer.end(), 0.0f);
        std::fill(shiftFftBuffer.begin(), shiftFftBuffer.end(), 0.0f);

        for (int i = 0; i < fftSize; ++i)
        {
            origFftBuffer[(size_t)i] = origData[pos + i] * hann[(size_t)i];
            shiftFftBuffer[(size_t)i] = shiftedData[pos + i] * hann[(size_t)i];
        }

        // Forward FFT
        fft.performRealOnlyForwardTransform(origFftBuffer.data());
        fft.performRealOnlyForwardTransform(shiftFftBuffer.data());

        // Extract magnitudes
        for (int k = 0; k <= fftSize / 2; ++k)
        {
            float rO = origFftBuffer[(size_t)(2 * k)];
            float iO = origFftBuffer[(size_t)(2 * k + 1)];
            origMag[(size_t)k] = std::sqrt(rO * rO + iO * iO);

            float rS = shiftFftBuffer[(size_t)(2 * k)];
            float iS = shiftFftBuffer[(size_t)(2 * k + 1)];
            shiftMag[(size_t)k] = std::sqrt(rS * rS + iS * iS);
        }

        // Smooth spectral envelopes (moving average)
        for (int k = 0; k <= fftSize / 2; ++k)
        {
            float sumO = 0.0f, sumS = 0.0f;
            int count = 0;
            int kMin = juce::jmax(0, k - filterRadius);
            int kMax = juce::jmin(fftSize / 2, k + filterRadius);

            for (int j = kMin; j <= kMax; ++j)
            {
                sumO += origMag[(size_t)j];
                sumS += shiftMag[(size_t)j];
                count++;
            }

            origEnv[(size_t)k] = sumO / (float)count;
            shiftEnv[(size_t)k] = sumS / (float)count;
        }

        // Formant compensation gain
        for (int k = 0; k <= fftSize / 2; ++k)
        {
            float gain = (origEnv[(size_t)k] + 1e-3f) / (shiftEnv[(size_t)k] + 1e-3f);
            gain = juce::jlimit(0.35f, 2.8f, gain);

            shiftFftBuffer[(size_t)(2 * k)] *= gain;
            shiftFftBuffer[(size_t)(2 * k + 1)] *= gain;
        }

        // Inverse FFT
        fft.performRealOnlyInverseTransform(shiftFftBuffer.data());

        // Overlap-add back into accumulator
        for (int i = 0; i < fftSize; ++i)
        {
            float w = hann[(size_t)i];
            outputAccum[(size_t)(pos + i)] += shiftFftBuffer[(size_t)i] * w;
            outputNorm[(size_t)(pos + i)] += w * w;
        }
    }

    // Blend restored formant audio back into shiftedData
    for (int i = 0; i < inSamples; ++i)
    {
        if (outputNorm[(size_t)i] > 1e-4f)
        {
            shiftedData[i] = outputAccum[(size_t)i] / outputNorm[(size_t)i];
        }
    }
}

void PitchShiftEngine::process(const juce::AudioBuffer<float>& input,
                               juce::AudioBuffer<float>& output,
                               double pitchRatio,
                               double originalPitchMidi)
{
    juce::ignoreUnused(originalPitchMidi);

    int numChannels = input.getNumChannels();
    int inSamples = input.getNumSamples();

    if (inSamples == 0 || pitchRatio <= 0.0)
    {
        output.setSize(numChannels, 0);
        return;
    }

    output.setSize(numChannels, inSamples);
    output.clear();

    if (std::abs(pitchRatio - 1.0) < 0.0005)
    {
        output.makeCopyOf(input);
        return;
    }

    for (int ch = 0; ch < numChannels; ++ch)
    {
        const float* inPtr = input.getReadPointer(ch);
        float* outPtr = output.getWritePointer(ch);

        // Melodyne Sibilant Protection: Unvoiced sections (fricatives, breath, 's', 't') bypass pitch shifting
        if (isUnvoicedSegment(inPtr, inSamples))
        {
            std::copy(inPtr, inPtr + inSamples, outPtr);
            continue;
        }

        // 1. Artifact-free WSOLA Pitch Shifting
        processWsolaPitchShift(inPtr, inSamples, outPtr, pitchRatio);

        // 2. Anti-Chipmunk Formant Envelope Preservation
        applyFormantPreservation(inPtr, outPtr, inSamples, pitchRatio);

        // 3. Smooth 64-sample boundary crossfade to eliminate edge clicks
        int fadeLen = juce::jmin(64, inSamples / 2);
        for (int i = 0; i < fadeLen; ++i)
        {
            float gain = (float)i / (float)fadeLen;
            outPtr[i] *= gain;
            outPtr[inSamples - 1 - i] *= gain;
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

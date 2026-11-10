#pragma once

#include <cmath>
#include <vector>
#include <algorithm>
#include <juce_core/juce_core.h>

class PitchDetector
{
public:
    // McLeod Pitch Method (MPM) - Normalized Square Difference Function (NSDF) with Key Maxima & Parabolic Interpolation
    // Accurately extracts fundamental frequency (F0) and MIDI pitch for vocal audio
    // Immune to formant-induced harmonic octave jumps and subharmonic errors
    static double detectPitch(const float* buffer, int length, double sampleRate, float clarityThreshold = 0.50f)
    {
        if (length <= 256 || sampleRate <= 0.0 || buffer == nullptr)
            return 0.0;

        int maxTau = length / 2;
        int minLag = juce::jmax(2, (int)(sampleRate / 1100.0));   // up to ~1100 Hz (C6)
        int maxLag = juce::jmin(maxTau, (int)(sampleRate / 60.0)); // down to ~60 Hz (B1)

        if (maxLag <= minLag)
            return 0.0;

        // 1. Compute Normalized Square Difference Function (NSDF)
        // Start from tau = 1 so we accurately track the initial zero-crossing from the main lobe at tau = 0
        std::vector<float> nsdf(maxLag + 1, 0.0f);
        nsdf[0] = 1.0f;

        for (int tau = 1; tau <= maxLag; ++tau)
        {
            float r = 0.0f;
            float m = 0.0f;
            int count = length - tau;

            for (int j = 0; j < count; ++j)
            {
                float x1 = buffer[j];
                float x2 = buffer[j + tau];
                r += x1 * x2;
                m += x1 * x1 + x2 * x2;
            }

            if (m > 1e-6f)
                nsdf[tau] = (2.0f * r) / m;
            else
                nsdf[tau] = 0.0f;
        }

        // 2. Find MPM "Key Maxima": exactly ONE highest peak per positive-valued region after the first zero-crossing
        struct KeyPeak
        {
            int tau;
            float value;
        };
        std::vector<KeyPeak> keyPeaks;
        float globalMaxPeak = -1.0f;

        // Advance past the initial positive lobe starting at tau = 0
        int t = 1;
        while (t <= maxLag && nsdf[t] > 0.0f)
            ++t;

        t = juce::jmax(t, minLag);

        while (t < maxLag)
        {
            // Skip negative region until a positive zero-crossing
            while (t < maxLag && nsdf[t] <= 0.0f)
                ++t;

            if (t >= maxLag)
                break;

            // Within this positive lobe, find the single highest local peak
            int bestTauInLobe = -1;
            float bestValInLobe = 0.0f;

            while (t < maxLag && nsdf[t] > 0.0f)
            {
                if (t >= minLag && nsdf[t] >= nsdf[t - 1] && nsdf[t] >= nsdf[t + 1])
                {
                    if (nsdf[t] > bestValInLobe)
                    {
                        bestValInLobe = nsdf[t];
                        bestTauInLobe = t;
                    }
                }
                ++t;
            }

            if (bestTauInLobe >= minLag && bestValInLobe > 0.0f)
            {
                keyPeaks.push_back({ bestTauInLobe, bestValInLobe });
                if (bestValInLobe > globalMaxPeak)
                    globalMaxPeak = bestValInLobe;
            }
        }

        // Unvoiced or noise
        if (keyPeaks.empty() || globalMaxPeak < clarityThreshold)
            return 0.0;

        // 3. MPM Peak selection: select the FIRST Key Maximum >= k * globalMaxPeak (McLeod standard k = 0.93)
        const float threshold = globalMaxPeak * 0.93f;
        double chosenTau = 0.0;

        for (const auto& p : keyPeaks)
        {
            if (p.value >= threshold)
            {
                // 4. Sub-sample parabolic interpolation
                int tauIdx = p.tau;
                float y1 = nsdf[tauIdx - 1];
                float y2 = nsdf[tauIdx];
                float y3 = (tauIdx + 1 <= maxLag) ? nsdf[tauIdx + 1] : y2;

                float denominator = 2.0f * (2.0f * y2 - y1 - y3);
                float delta = 0.0f;
                if (std::abs(denominator) > 1e-7f)
                {
                    delta = (y3 - y1) / denominator;
                    delta = juce::jlimit(-0.5f, 0.5f, delta);
                }

                chosenTau = (double)tauIdx + (double)delta;
                break;
            }
        }

        if (chosenTau <= 0.0)
            return 0.0;

        double freq = sampleRate / chosenTau;
        if (freq < 55.0 || freq > 1150.0)
            return 0.0;

        double midi = 69.0 + 12.0 * std::log2(freq / 440.0);
        return midi;
    }
};

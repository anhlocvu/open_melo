#pragma once

#include <cmath>
#include <vector>
#include <algorithm>
#include <juce_core/juce_core.h>

class PitchDetector
{
public:
    // McLeod Pitch Method (MPM) - Normalized Square Difference Function (NSDF) with parabolic interpolation
    // Accurately extracts fundamental frequency (F0) and MIDI pitch for vocal audio
    // Prevents octave jumps, subharmonic errors, and eliminates semitone detuning
    static double detectPitch(const float* buffer, int length, double sampleRate, float clarityThreshold = 0.45f)
    {
        if (length <= 256 || sampleRate <= 0.0 || buffer == nullptr)
            return 0.0;

        int maxTau = length / 2;
        int minLag = juce::jmax(2, (int)(sampleRate / 1200.0));    // up to ~1200 Hz
        int maxLag = juce::jmin(maxTau, (int)(sampleRate / 55.0)); // down to ~55 Hz (A1)

        if (maxLag <= minLag)
            return 0.0;

        // 1. Compute Normalized Square Difference Function (NSDF)
        std::vector<float> nsdf(maxLag + 1, 0.0f);

        for (int tau = minLag; tau <= maxLag; ++tau)
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

        // 2. Find local maxima (peaks) of NSDF after first zero crossing
        struct Peak
        {
            int tau;
            float value;
        };
        std::vector<Peak> peaks;
        float globalMaxPeak = -1.0f;

        int tauIdx = minLag;
        while (tauIdx <= maxLag && nsdf[tauIdx] > 0.0f)
            ++tauIdx;
        while (tauIdx <= maxLag && nsdf[tauIdx] <= 0.0f)
            ++tauIdx;

        for (int t = tauIdx; t < maxLag; ++t)
        {
            if (nsdf[t] > nsdf[t - 1] && nsdf[t] >= nsdf[t + 1] && nsdf[t] > 0.0f)
            {
                if (nsdf[t] > globalMaxPeak)
                    globalMaxPeak = nsdf[t];
                peaks.push_back({ t, nsdf[t] });
            }
        }

        // Unvoiced or noise
        if (peaks.empty() || globalMaxPeak < clarityThreshold)
            return 0.0;

        // 3. Peak selection: select the FIRST peak >= 0.85 * globalMaxPeak
        float threshold = globalMaxPeak * 0.85f;
        double chosenTau = 0.0;

        for (const auto& p : peaks)
        {
            if (p.value >= threshold)
            {
                // 4. Sub-sample parabolic interpolation
                int t = p.tau;
                float y1 = nsdf[t - 1];
                float y2 = nsdf[t];
                float y3 = (t + 1 <= maxLag) ? nsdf[t + 1] : y2;

                float denominator = 2.0f * (2.0f * y2 - y1 - y3);
                float delta = 0.0f;
                if (std::abs(denominator) > 1e-7f)
                {
                    delta = (y3 - y1) / denominator;
                    delta = juce::jlimit(-0.5f, 0.5f, delta);
                }

                chosenTau = (double)t + (double)delta;
                break;
            }
        }

        if (chosenTau <= 0.0)
            return 0.0;

        double freq = sampleRate / chosenTau;
        if (freq < 50.0 || freq > 1500.0)
            return 0.0;

        double midi = 69.0 + 12.0 * std::log2(freq / 440.0);
        return midi;
    }
};

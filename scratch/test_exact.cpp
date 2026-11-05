#include <iostream>
#include <vector>
#include <cmath>
#include "signalsmith-stretch.h"

int main() {
    signalsmith::stretch::SignalsmithStretch<float> stretch;
    float sampleRate = 48000.0f;
    int channels = 2;
    stretch.presetDefault(channels, sampleRate);

    float semitones = 3.0f;
    stretch.setTransposeSemitones(semitones, 8000.0f / sampleRate);
    stretch.setFormantSemitones(0.0f, true); // Preserve vocal formants!
    stretch.setFormantBase(440.0f / sampleRate);

    int nSamples = 48000; // 1 second
    std::vector<float> inL(nSamples), inR(nSamples);
    for (int i = 0; i < nSamples; ++i) {
        float s = std::sin(2.0f * 3.14159265f * 440.0f * i / sampleRate);
        inL[i] = s;
        inR[i] = s;
    }

    std::vector<float> outL(nSamples), outR(nSamples);
    float* inBuffers[2] = { inL.data(), inR.data() };
    float* outBuffers[2] = { outL.data(), outR.data() };

    bool ok = stretch.exact(inBuffers, nSamples, outBuffers, nSamples);
    std::cout << "exact() returned: " << (ok ? "TRUE" : "FALSE") << std::endl;
    std::cout << "Output sample 24000: " << outL[24000] << std::endl;
    return 0;
}

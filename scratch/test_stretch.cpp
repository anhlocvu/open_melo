#include <iostream>
#include <vector>
#include <cmath>
#include "signalsmith-stretch.h"

int main() {
    signalsmith::stretch::SignalsmithStretch<float> stretch;
    float sampleRate = 48000.0f;
    stretch.presetDefault(1, sampleRate);
    stretch.setTransposeSemitones(2.0f, 8000.0f / sampleRate);

    int nSamples = 4800; // 0.1s
    std::vector<float> inData(nSamples);
    for (int i = 0; i < nSamples; ++i) {
        inData[i] = std::sin(2.0f * 3.14159265f * 440.0f * i / sampleRate);
    }
    std::vector<float> outData(nSamples, 0.0f);

    float* inPtrs[1] = { inData.data() };
    float* outPtrs[1] = { outData.data() };

    // With outputSeek:
    // Need seek length input:
    int seekLen = stretch.outputSeekLength(1.0f);
    std::vector<float> seekBuffer(seekLen, 0.0f);
    float* seekPtrs[1] = { seekBuffer.data() };
    stretch.outputSeek(seekPtrs, seekLen);

    stretch.process(inPtrs, nSamples, outPtrs, nSamples);

    std::cout << "First 5 out samples: ";
    for (int i = 0; i < 5; ++i) std::cout << outData[i] << " ";
    std::cout << "\nMid 5 out samples: ";
    for (int i = nSamples/2; i < nSamples/2 + 5; ++i) std::cout << outData[i] << " ";
    std::cout << std::endl;
    return 0;
}

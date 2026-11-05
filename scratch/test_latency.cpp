#include <iostream>
#include <vector>
#include <cmath>
#include "signalsmith-stretch.h"

int main() {
    signalsmith::stretch::SignalsmithStretch<float> stretch;
    float sampleRate = 48000.0f;
    stretch.presetDefault(1, sampleRate);
    // 0 semitones shift (identity)
    stretch.setTransposeSemitones(0.0f);

    int inLat = stretch.inputLatency();
    int outLat = stretch.outputLatency();
    int totalLat = inLat + outLat;

    int n = 1000;
    std::vector<float> inputPadded(inLat + n + outLat, 0.0f);
    // Put an impulse at index inLat
    inputPadded[inLat] = 1.0f;

    std::vector<float> outputPadded(inLat + n + outLat, 0.0f);

    float* inPtr[1] = { inputPadded.data() };
    float* outPtr[1] = { outputPadded.data() };

    stretch.reset();
    stretch.process(inPtr, inLat + n + outLat, outPtr, inLat + n + outLat);

    // Find where the peak appears in output
    int maxIdx = 0;
    float maxVal = 0.0f;
    for (int i = 0; i < inLat + n + outLat; ++i) {
        if (std::abs(outputPadded[i]) > maxVal) {
            maxVal = std::abs(outputPadded[i]);
            maxIdx = i;
        }
    }

    std::cout << "Impulse was at index: " << inLat << " (which corresponds to time 0 of actual signal)\n";
    std::cout << "Peak in output is at index: " << maxIdx << " with value " << maxVal << "\n";
    std::cout << "Delay from start of output buffer: " << maxIdx << "\n";
    std::cout << "Hence, actual sound in output starts at index: " << (maxIdx - 0) << " = " << (totalLat) << "? Let's check diff: " << (maxIdx - inLat) << std::endl;
    return 0;
}

#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>

class AuditionVoice
{
public:
    AuditionVoice()
    {
        playbackBuffer.setSize(2, 0);
    }

    void setSampleRate(double newSampleRate)
    {
        sampleRate = newSampleRate;
    }

    // Triggered from UI / Message Thread
    void startPlayback(const juce::AudioBuffer<float>& bufferToPlay)
    {
        const juce::SpinLock::ScopedLockType sl(lock);

        int numCh = bufferToPlay.getNumChannels();
        int numSamples = bufferToPlay.getNumSamples();

        if (numSamples <= 0)
        {
            isPlaying.store(false);
            return;
        }

        playbackBuffer.setSize(numCh, numSamples, false, false, true);
        for (int ch = 0; ch < numCh; ++ch)
        {
            playbackBuffer.copyFrom(ch, 0, bufferToPlay, ch, 0, numSamples);
        }

        playheadPosition.store(0);
        isPlaying.store(true);
    }

    void stopPlayback()
    {
        isPlaying.store(false);
        playheadPosition.store(0);
    }

    bool isPlayingNow() const
    {
        return isPlaying.load();
    }

    // Called on Audio Realtime Thread
    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
    {
        if (!isPlaying.load())
            return;

        const juce::SpinLock::ScopedTryLockType sl(lock);
        if (!sl.isLocked())
            return;

        int totalSamples = playbackBuffer.getNumSamples();
        int currentPos = playheadPosition.load();

        if (currentPos >= totalSamples)
        {
            isPlaying.store(false);
            return;
        }

        int samplesToCopy = juce::jmin(numSamples, totalSamples - currentPos);
        int outChans = outputBuffer.getNumChannels();
        int playChans = playbackBuffer.getNumChannels();

        for (int ch = 0; ch < outChans; ++ch)
        {
            int srcCh = ch % playChans;
            outputBuffer.addFrom(ch, startSample, playbackBuffer, srcCh, currentPos, samplesToCopy);
        }

        currentPos += samplesToCopy;
        playheadPosition.store(currentPos);

        if (currentPos >= totalSamples)
        {
            isPlaying.store(false);
        }
    }

private:
    double sampleRate = 44100.0;
    juce::SpinLock lock;
    juce::AudioBuffer<float> playbackBuffer;
    std::atomic<int> playheadPosition { 0 };
    std::atomic<bool> isPlaying { false };
};

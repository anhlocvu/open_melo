#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

struct OpenMeloNote
{
    int id = 0;
    int64_t startSample = 0;
    int64_t lengthSamples = 0;

    double originalPitchMidi = 60.0; // Middle C (C4)
    int semitoneOffset = 0;          // Transposition delta (+/- semitones)
    int centsOffset = 0;             // Fine tuning delta (+/- cents)

    juce::AudioBuffer<float> audioData;      // Original raw vocal audio
    juce::AudioBuffer<float> renderedBuffer;  // Pre-rendered pitch & formant shifted audio for instant DAW playback

    int getNominalMidiNote() const
    {
        return juce::roundToInt(originalPitchMidi) + semitoneOffset;
    }

    int getCurrentTotalCents() const
    {
        // Calculate cents deviation relative to nominal MIDI note
        double fractionalPart = originalPitchMidi - std::round(originalPitchMidi);
        int baseCents = juce::roundToInt(fractionalPart * 100.0);
        return baseCents + centsOffset;
    }

    double getEffectivePitchMidi() const
    {
        return originalPitchMidi + semitoneOffset + (centsOffset / 100.0);
    }

    double getPitchRatio() const
    {
        // Frequency multiplier relative to original pitch
        double totalSemitonesShift = semitoneOffset + (centsOffset / 100.0);
        return std::pow(2.0, totalSemitonesShift / 12.0);
    }

    juce::String getNoteName() const
    {
        static const char* noteNames[] = {
            "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
        };

        int midi = getNominalMidiNote();
        int noteIndex = (midi % 12 + 12) % 12;
        int octave = (midi / 12) - 1;

        return juce::String(noteNames[noteIndex]) + juce::String(octave);
    }

    juce::String getCentsString() const
    {
        int cents = getCurrentTotalCents();
        if (cents > 0)
            return "+" + juce::String(cents) + " ct";
        return juce::String(cents) + " ct";
    }

    juce::String getSpeechDescription() const
    {
        return getNoteName() + ", " + getCentsString();
    }
};

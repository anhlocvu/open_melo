#include "NoteManager.h"
#include "NvdaBridge.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <cmath>
#include <algorithm>

NoteManager::NoteManager()
{
    pitchEngine.setSampleRate(sampleRate);
    clearNotes();
}

NoteManager::~NoteManager()
{
}

void NoteManager::clearNotes()
{
    notes.clear();
    selectedNoteIndex = 0;
    liveTrackAccumulatorLeft.clear();
    liveTrackAccumulatorRight.clear();
    currentNoteAudioLeft.clear();
    currentNoteAudioRight.clear();
    currentNotePitchSum = 0.0;
    currentNotePitchCount = 0;
    silenceFrameCount = 0;
    currentNoteTimelineStart = 0;
    lastProcessedHostSample = -1;

    if (onNotesChanged)
        onNotesChanged();
}

void NoteManager::setSampleRate(double newSampleRate)
{
    sampleRate = (newSampleRate > 0.0) ? newSampleRate : 44100.0;
    pitchEngine.setSampleRate(sampleRate);

    for (auto& n : notes)
    {
        updateNoteRendering(n);
    }
}

const OpenMeloNote* NoteManager::getSelectedNote() const
{
    if (selectedNoteIndex >= 0 && selectedNoteIndex < (int)notes.size())
        return &notes[selectedNoteIndex];
    return nullptr;
}

void NoteManager::updateNoteRendering(OpenMeloNote& note)
{
    if (note.audioData.getNumSamples() <= 0)
    {
        note.renderedBuffer.setSize(2, 0);
        return;
    }

    double ratio = note.getPitchRatio();
    if (std::abs(ratio - 1.0) < 0.001)
    {
        note.renderedBuffer.makeCopyOf(note.audioData);
    }
    else
    {
        // TD-PSOLA pitch and formant preserved rendering
        pitchEngine.process(note.audioData, note.renderedBuffer, ratio, note.originalPitchMidi);
    }
}

bool NoteManager::isTimelineCovered(int64_t samplePos) const
{
    for (const auto& note : notes)
    {
        if (samplePos >= note.startSample && samplePos < note.startSample + note.lengthSamples)
            return true;
    }
    return false;
}

void NoteManager::sortNotes()
{
    std::sort(notes.begin(), notes.end(), [](const OpenMeloNote& a, const OpenMeloNote& b) {
        return a.startSample < b.startSample;
    });

    for (size_t i = 0; i < notes.size(); ++i)
    {
        notes[i].id = (int)i + 1;
    }
}

void NoteManager::renderTrackPlayback(juce::AudioBuffer<float>& buffer, int64_t hostPlayheadSample, int numSamples)
{
    if (notes.empty() || numSamples <= 0)
        return;

    int64_t blockStart = hostPlayheadSample;
    int64_t blockEnd = hostPlayheadSample + numSamples;
    int numChannels = buffer.getNumChannels();

    for (auto& note : notes)
    {
        int64_t noteStart = note.startSample;
        int64_t noteEnd = note.startSample + note.lengthSamples;

        // Check if note overlaps with current audio buffer block
        int64_t overlapStart = juce::jmax(noteStart, blockStart);
        int64_t overlapEnd = juce::jmin(noteEnd, blockEnd);

        if (overlapEnd > overlapStart)
        {
            // Only replace track audio if note has been tuned or transposed
            if (note.semitoneOffset != 0 || note.centsOffset != 0)
            {
                int bufferOffset = (int)(overlapStart - blockStart);
                int noteOffset = (int)(overlapStart - noteStart);
                int copyLen = (int)(overlapEnd - overlapStart);

                int rendChans = note.renderedBuffer.getNumChannels();
                int rendSamples = note.renderedBuffer.getNumSamples();

                if (rendSamples <= 0)
                    continue;

                for (int ch = 0; ch < numChannels; ++ch)
                {
                    int srcCh = ch % juce::jmax(1, rendChans);
                    float* destPtr = buffer.getWritePointer(ch, bufferOffset);
                    const float* srcPtr = note.renderedBuffer.getReadPointer(srcCh);

                    int actualCopy = juce::jmin(copyLen, rendSamples - noteOffset);
                    if (actualCopy > 0 && noteOffset >= 0 && noteOffset < rendSamples)
                    {
                        for (int i = 0; i < actualCopy; ++i)
                        {
                            int sampleInNote = noteOffset + i;
                            float gain = 1.0f;

                            // 64-sample crossfade at the boundaries to prevent clicks
                            if (sampleInNote < 64)
                                gain = (float)sampleInNote / 64.0f;
                            else if (sampleInNote > note.lengthSamples - 64)
                                gain = (float)(note.lengthSamples - sampleInNote) / 64.0f;

                            destPtr[i] = srcPtr[sampleInNote] * gain;
                        }
                    }
                }
            }
        }
    }
}

void NoteManager::processIncomingTrackAudio(const juce::AudioBuffer<float>& buffer, int64_t hostPlayheadSample, int numSamples)
{
    if (numSamples <= 0)
        return;

    // Detect transport jump or seek
    if (lastProcessedHostSample >= 0 && std::abs(hostPlayheadSample - (lastProcessedHostSample + numSamples)) > 2048)
    {
        finalizeCurrentTrackNote(lastProcessedHostSample);
        liveTrackAccumulatorLeft.clear();
        liveTrackAccumulatorRight.clear();
    }
    lastProcessedHostSample = hostPlayheadSample;

    const float* inL = buffer.getReadPointer(0);
    const float* inR = (buffer.getNumChannels() > 1) ? buffer.getReadPointer(1) : inL;

    liveTrackAccumulatorLeft.insert(liveTrackAccumulatorLeft.end(), inL, inL + numSamples);
    liveTrackAccumulatorRight.insert(liveTrackAccumulatorRight.end(), inR, inR + numSamples);

    int windowSize = 2048;
    int hopSize = 512;

    while ((int)liveTrackAccumulatorLeft.size() >= windowSize)
    {
        const float* winL = liveTrackAccumulatorLeft.data();
        const float* winR = liveTrackAccumulatorRight.data();

        float rms = 0.0f;
        for (int i = 0; i < windowSize; ++i)
        {
            float mono = (winL[i] + winR[i]) * 0.5f;
            rms += mono * mono;
        }
        rms = std::sqrt(rms / (float)windowSize);

        double pitchMidi = 0.0;
        if (rms > 0.008f)
        {
            pitchMidi = PitchDetector::detectPitch(winL, windowSize, sampleRate);
        }

        int64_t approxSampleTime = hostPlayheadSample + numSamples - (int64_t)liveTrackAccumulatorLeft.size();

        if (pitchMidi > 0.0)
        {
            if (currentNoteAudioLeft.empty())
            {
                currentNoteTimelineStart = approxSampleTime;
            }
            else if (currentNotePitchCount > 0)
            {
                double currentAvgPitch = currentNotePitchSum / currentNotePitchCount;
                if (std::abs(pitchMidi - currentAvgPitch) > 1.0)
                {
                    // Pitch changed to a new note
                    finalizeCurrentTrackNote(approxSampleTime);
                    currentNoteTimelineStart = approxSampleTime;
                }
            }

            currentNoteAudioLeft.insert(currentNoteAudioLeft.end(), winL, winL + hopSize);
            currentNoteAudioRight.insert(currentNoteAudioRight.end(), winR, winR + hopSize);
            currentNotePitchSum += pitchMidi;
            currentNotePitchCount++;
            silenceFrameCount = 0;
        }
        else
        {
            silenceFrameCount++;
            if (silenceFrameCount >= 4 && !currentNoteAudioLeft.empty())
            {
                finalizeCurrentTrackNote(approxSampleTime);
            }
        }

        liveTrackAccumulatorLeft.erase(liveTrackAccumulatorLeft.begin(), liveTrackAccumulatorLeft.begin() + hopSize);
        liveTrackAccumulatorRight.erase(liveTrackAccumulatorRight.begin(), liveTrackAccumulatorRight.begin() + hopSize);
    }
}

void NoteManager::finalizeCurrentTrackNote(int64_t currentEndSample)
{
    if (currentNoteAudioLeft.empty() || currentNotePitchCount == 0)
    {
        currentNoteAudioLeft.clear();
        currentNoteAudioRight.clear();
        currentNotePitchSum = 0.0;
        currentNotePitchCount = 0;
        silenceFrameCount = 0;
        return;
    }

    // Minimum note length: 80 ms
    if (currentNoteAudioLeft.size() >= (size_t)(sampleRate * 0.08))
    {
        int64_t noteLen = (int64_t)currentNoteAudioLeft.size();
        int64_t noteStart = currentNoteTimelineStart;
        if (noteStart < 0) noteStart = juce::jmax((int64_t)0, currentEndSample - noteLen);

        // Check if an identical or overlapping note is already registered
        bool alreadyExists = false;
        for (const auto& existing : notes)
        {
            if (std::abs(existing.startSample - noteStart) < (int64_t)(sampleRate * 0.1))
            {
                alreadyExists = true;
                break;
            }
        }

        if (!alreadyExists)
        {
            OpenMeloNote n;
            n.id = (int)notes.size() + 1;
            n.startSample = noteStart;
            n.lengthSamples = noteLen;
            n.originalPitchMidi = currentNotePitchSum / (double)currentNotePitchCount;
            n.semitoneOffset = 0;
            n.centsOffset = 0;

            n.audioData.setSize(2, (int)noteLen);
            std::copy(currentNoteAudioLeft.begin(), currentNoteAudioLeft.end(), n.audioData.getWritePointer(0));
            std::copy(currentNoteAudioRight.begin(), currentNoteAudioRight.end(), n.audioData.getWritePointer(1));

            updateNoteRendering(n);
            notes.push_back(n);
            sortNotes();

            if (onNotesChanged)
                onNotesChanged();
        }
    }

    currentNoteAudioLeft.clear();
    currentNoteAudioRight.clear();
    currentNotePitchSum = 0.0;
    currentNotePitchCount = 0;
    silenceFrameCount = 0;
}

bool NoteManager::scanAudioBuffer(const juce::AudioBuffer<float>& buffer, double fileSampleRate, int64_t startTimelineSample)
{
    int numSamples = buffer.getNumSamples();
    if (numSamples <= 0)
        return false;

    clearNotes();
    setSampleRate(fileSampleRate);

    const float* channelDataL = buffer.getReadPointer(0);
    const float* channelDataR = (buffer.getNumChannels() > 1) ? buffer.getReadPointer(1) : channelDataL;

    int windowSize = 2048;
    int hopSize = 512;
    int pos = 0;

    std::vector<float> segmentAudioL;
    std::vector<float> segmentAudioR;
    double segmentPitchSum = 0.0;
    int segmentPitchCount = 0;
    int silenceCount = 0;
    int64_t segmentStartSample = 0;

    while (pos + windowSize <= numSamples)
    {
        const float* winPtrL = channelDataL + pos;
        const float* winPtrR = channelDataR + pos;

        float rms = 0.0f;
        for (int i = 0; i < windowSize; ++i)
        {
            float mono = (winPtrL[i] + winPtrR[i]) * 0.5f;
            rms += mono * mono;
        }
        rms = std::sqrt(rms / (float)windowSize);

        double pitchMidi = 0.0;
        if (rms > 0.008f)
        {
            pitchMidi = PitchDetector::detectPitch(winPtrL, windowSize, sampleRate);
        }

        if (pitchMidi > 0.0)
        {
            if (segmentAudioL.empty())
            {
                segmentStartSample = startTimelineSample + pos;
            }
            else if (segmentPitchCount > 0)
            {
                double currentAvg = segmentPitchSum / segmentPitchCount;
                if (std::abs(pitchMidi - currentAvg) > 1.0)
                {
                    // Note change detected
                    if (segmentAudioL.size() >= (size_t)(sampleRate * 0.08))
                    {
                        OpenMeloNote n;
                        n.id = (int)notes.size() + 1;
                        n.startSample = segmentStartSample;
                        n.lengthSamples = (int64_t)segmentAudioL.size();
                        n.originalPitchMidi = segmentPitchSum / segmentPitchCount;
                        n.semitoneOffset = 0;
                        n.centsOffset = 0;

                        n.audioData.setSize(2, (int)n.lengthSamples);
                        std::copy(segmentAudioL.begin(), segmentAudioL.end(), n.audioData.getWritePointer(0));
                        std::copy(segmentAudioR.begin(), segmentAudioR.end(), n.audioData.getWritePointer(1));

                        updateNoteRendering(n);
                        notes.push_back(n);
                    }

                    segmentAudioL.clear();
                    segmentAudioR.clear();
                    segmentPitchSum = 0.0;
                    segmentPitchCount = 0;
                    segmentStartSample = startTimelineSample + pos;
                }
            }

            segmentAudioL.insert(segmentAudioL.end(), winPtrL, winPtrL + hopSize);
            segmentAudioR.insert(segmentAudioR.end(), winPtrR, winPtrR + hopSize);
            segmentPitchSum += pitchMidi;
            segmentPitchCount++;
            silenceCount = 0;
        }
        else
        {
            silenceCount++;
            if (silenceCount >= 4 && segmentAudioL.size() >= (size_t)(sampleRate * 0.08))
            {
                OpenMeloNote n;
                n.id = (int)notes.size() + 1;
                n.startSample = segmentStartSample;
                n.lengthSamples = (int64_t)segmentAudioL.size();
                n.originalPitchMidi = segmentPitchSum / segmentPitchCount;
                n.semitoneOffset = 0;
                n.centsOffset = 0;

                n.audioData.setSize(2, (int)n.lengthSamples);
                std::copy(segmentAudioL.begin(), segmentAudioL.end(), n.audioData.getWritePointer(0));
                std::copy(segmentAudioR.begin(), segmentAudioR.end(), n.audioData.getWritePointer(1));

                updateNoteRendering(n);
                notes.push_back(n);

                segmentAudioL.clear();
                segmentAudioR.clear();
                segmentPitchSum = 0.0;
                segmentPitchCount = 0;
            }
            else if (silenceCount >= 4)
            {
                segmentAudioL.clear();
                segmentAudioR.clear();
                segmentPitchSum = 0.0;
                segmentPitchCount = 0;
            }
        }

        pos += hopSize;
    }

    sortNotes();

    if (!notes.empty())
    {
        selectedNoteIndex = 0;
        triggerSpeech("Scanned " + juce::String(notes.size()) + " notes from track. Selected: " + notes[0].getSpeechDescription());
    }
    else
    {
        triggerSpeech("Scanning complete. No vocal notes detected.");
    }

    if (onNotesChanged)
        onNotesChanged();

    return !notes.empty();
}

bool NoteManager::scanAudioFile(const juce::File& file, int64_t startTimelineSample)
{
    if (!file.existsAsFile())
        return false;

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (reader == nullptr)
    {
        triggerSpeech("Could not read audio file");
        return false;
    }

    juce::AudioBuffer<float> tempBuffer((int)reader->numChannels, (int)reader->lengthInSamples);
    reader->read(&tempBuffer, 0, (int)reader->lengthInSamples, 0, true, true);

    return scanAudioBuffer(tempBuffer, reader->sampleRate, startTimelineSample);
}

void NoteManager::selectNoteIndex(int index)
{
    if (notes.empty())
    {
        triggerSpeech("No notes on track");
        return;
    }

    selectedNoteIndex = juce::jlimit(0, (int)notes.size() - 1, index);
    auto& note = notes[selectedNoteIndex];

    renderAndAuditionNote(note);
    triggerSpeech(note.getSpeechDescription());

    if (onNotesChanged)
        onNotesChanged();
}

void NoteManager::navigateLeft()
{
    if (notes.empty())
    {
        triggerSpeech("No notes on track");
        return;
    }

    if (selectedNoteIndex > 0)
        --selectedNoteIndex;

    auto& note = notes[selectedNoteIndex];
    renderAndAuditionNote(note);
    triggerSpeech(note.getSpeechDescription());

    if (onNotesChanged)
        onNotesChanged();
}

void NoteManager::navigateRight()
{
    if (notes.empty())
    {
        triggerSpeech("No notes on track");
        return;
    }

    if (selectedNoteIndex + 1 < (int)notes.size())
        ++selectedNoteIndex;

    auto& note = notes[selectedNoteIndex];
    renderAndAuditionNote(note);
    triggerSpeech(note.getSpeechDescription());

    if (onNotesChanged)
        onNotesChanged();
}

void NoteManager::playCurrentNote()
{
    if (notes.empty())
    {
        triggerSpeech("No notes on track");
        return;
    }

    auto& note = notes[selectedNoteIndex];
    renderAndAuditionNote(note);
    triggerSpeech(note.getSpeechDescription());
}

void NoteManager::transposeCurrentNote(int deltaSemitones)
{
    if (notes.empty())
    {
        triggerSpeech("No notes on track");
        return;
    }

    auto& note = notes[selectedNoteIndex];
    note.semitoneOffset += deltaSemitones;
    updateNoteRendering(note);

    renderAndAuditionNote(note);
    triggerSpeech(note.getSpeechDescription());

    if (onNotesChanged)
        onNotesChanged();
}

void NoteManager::tuneCentsCurrentNote(int deltaCents)
{
    if (notes.empty())
    {
        triggerSpeech("No notes on track");
        return;
    }

    auto& note = notes[selectedNoteIndex];
    note.centsOffset += deltaCents;
    updateNoteRendering(note);

    renderAndAuditionNote(note);
    triggerSpeech(note.getSpeechDescription());

    if (onNotesChanged)
        onNotesChanged();
}

void NoteManager::snapCurrentNoteToZero()
{
    if (notes.empty())
    {
        triggerSpeech("No notes on track");
        return;
    }

    auto& note = notes[selectedNoteIndex];

    double fractionalPart = note.originalPitchMidi - std::round(note.originalPitchMidi);
    int baseCents = juce::roundToInt(fractionalPart * 100.0);

    // Cancel out the cents deviation completely
    note.centsOffset = -baseCents;
    updateNoteRendering(note);

    renderAndAuditionNote(note);
    triggerSpeech(note.getNoteName() + ", 0 ct, tuned");

    if (onNotesChanged)
        onNotesChanged();
}

bool NoteManager::splitCurrentNote()
{
    if (notes.empty())
    {
        triggerSpeech("No notes on track");
        return false;
    }

    auto& orig = notes[selectedNoteIndex];
    int64_t totalLen = orig.lengthSamples;

    if (totalLen < (int64_t)(sampleRate * 0.16))
    {
        triggerSpeech("Note too short to split");
        return false;
    }

    int64_t halfLen = totalLen / 2;

    OpenMeloNote firstHalf = orig;
    firstHalf.lengthSamples = halfLen;
    firstHalf.audioData.setSize(orig.audioData.getNumChannels(), (int)halfLen);
    for (int ch = 0; ch < orig.audioData.getNumChannels(); ++ch)
    {
        firstHalf.audioData.copyFrom(ch, 0, orig.audioData, ch, 0, (int)halfLen);
    }
    updateNoteRendering(firstHalf);

    OpenMeloNote secondHalf = orig;
    secondHalf.id = (int)notes.size() + 1;
    secondHalf.startSample = orig.startSample + halfLen;
    secondHalf.lengthSamples = totalLen - halfLen;
    secondHalf.audioData.setSize(orig.audioData.getNumChannels(), (int)secondHalf.lengthSamples);
    for (int ch = 0; ch < orig.audioData.getNumChannels(); ++ch)
    {
        secondHalf.audioData.copyFrom(ch, 0, orig.audioData, ch, (int)halfLen, (int)secondHalf.lengthSamples);
    }
    updateNoteRendering(secondHalf);

    notes[selectedNoteIndex] = firstHalf;
    notes.insert(notes.begin() + selectedNoteIndex + 1, secondHalf);
    sortNotes();

    renderAndAuditionNote(notes[selectedNoteIndex]);
    triggerSpeech("Split note into 2. " + notes[selectedNoteIndex].getSpeechDescription());

    if (onNotesChanged)
        onNotesChanged();

    return true;
}

int NoteManager::sliceCurrentNoteIntoPieces(int targetSegmentMs)
{
    if (notes.empty())
    {
        triggerSpeech("No notes on track");
        return 0;
    }

    auto orig = notes[selectedNoteIndex];
    int segmentSamples = (int)(sampleRate * (targetSegmentMs / 1000.0));
    int count = (int)(orig.lengthSamples / segmentSamples);

    if (count <= 1)
    {
        triggerSpeech("Note too short to slice");
        return 0;
    }

    std::vector<OpenMeloNote> sliced;
    int numChannels = orig.audioData.getNumChannels();

    for (int i = 0; i < count; ++i)
    {
        int64_t sStart = orig.startSample + (int64_t)(i * segmentSamples);
        int sLen = (i == count - 1) ? (int)(orig.lengthSamples - (i * segmentSamples)) : segmentSamples;

        OpenMeloNote n = orig;
        n.id = (int)notes.size() + i + 1;
        n.startSample = sStart;
        n.lengthSamples = sLen;

        n.audioData.setSize(numChannels, sLen);
        for (int ch = 0; ch < numChannels; ++ch)
        {
            n.audioData.copyFrom(ch, 0, orig.audioData, ch, i * segmentSamples, sLen);
        }

        updateNoteRendering(n);
        sliced.push_back(n);
    }

    notes.erase(notes.begin() + selectedNoteIndex);
    notes.insert(notes.begin() + selectedNoteIndex, sliced.begin(), sliced.end());
    sortNotes();

    renderAndAuditionNote(notes[selectedNoteIndex]);
    triggerSpeech("Sliced note into " + juce::String(count) + " micro segments. " + notes[selectedNoteIndex].getSpeechDescription());

    if (onNotesChanged)
        onNotesChanged();

    return count;
}

void NoteManager::renderAndAuditionNote(OpenMeloNote& note)
{
    if (note.renderedBuffer.getNumSamples() <= 0)
    {
        updateNoteRendering(note);
    }

    if (onPlayAudition)
    {
        onPlayAudition(note.renderedBuffer);
    }
}

void NoteManager::triggerSpeech(const juce::String& text)
{
    NvdaBridge::getInstance().speak(text, true);
    NvdaBridge::getInstance().braille(text);
}

void NoteManager::serialize(juce::MemoryOutputStream& stream) const
{
    stream.writeInt(selectedNoteIndex);
    stream.writeInt((int)notes.size());

    for (const auto& n : notes)
    {
        stream.writeInt(n.id);
        stream.writeInt64(n.startSample);
        stream.writeInt64(n.lengthSamples);
        stream.writeDouble(n.originalPitchMidi);
        stream.writeInt(n.semitoneOffset);
        stream.writeInt(n.centsOffset);

        int numCh = n.audioData.getNumChannels();
        int numSmp = n.audioData.getNumSamples();
        stream.writeInt(numCh);
        stream.writeInt(numSmp);

        for (int ch = 0; ch < numCh; ++ch)
        {
            if (numSmp > 0)
            {
                stream.write(n.audioData.getReadPointer(ch), sizeof(float) * numSmp);
            }
        }
    }
}

void NoteManager::deserialize(juce::MemoryInputStream& stream)
{
    notes.clear();

    selectedNoteIndex = stream.readInt();
    int count = stream.readInt();

    for (int i = 0; i < count; ++i)
    {
        OpenMeloNote n;
        n.id = stream.readInt();
        n.startSample = stream.readInt64();
        n.lengthSamples = stream.readInt64();
        n.originalPitchMidi = stream.readDouble();
        n.semitoneOffset = stream.readInt();
        n.centsOffset = stream.readInt();

        int numCh = stream.readInt();
        int numSmp = stream.readInt();

        n.audioData.setSize(numCh, numSmp);
        for (int ch = 0; ch < numCh; ++ch)
        {
            if (numSmp > 0)
            {
                stream.read(n.audioData.getWritePointer(ch), sizeof(float) * numSmp);
            }
        }

        updateNoteRendering(n);
        notes.push_back(n);
    }

    if (selectedNoteIndex < 0 || selectedNoteIndex >= (int)notes.size())
        selectedNoteIndex = 0;

    sortNotes();

    if (onNotesChanged)
        onNotesChanged();
}

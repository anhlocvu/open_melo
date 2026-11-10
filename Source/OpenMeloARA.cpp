#include "OpenMeloARA.h"
#include "NvdaBridge.h"
#include <cmath>
#include <algorithm>

//==============================================================================
OpenMeloPlaybackRenderer::OpenMeloPlaybackRenderer(ARA::PlugIn::DocumentController* dc)
    : juce::ARAPlaybackRenderer(dc)
{
}

OpenMeloPlaybackRenderer::~OpenMeloPlaybackRenderer()
{
    if (isPrepared)
    {
        if (auto* doc = juce::ARADocumentControllerSpecialisation::getSpecialisedDocumentController<OpenMeloDocumentController>(getDocumentController()))
            doc->decrementActivePlaybackRenderers();
        isPrepared = false;
    }
}

void OpenMeloPlaybackRenderer::prepareToPlay(double sampleRateIn,
                                             int maximumSamplesPerBlockIn,
                                             int numChannelsIn,
                                             juce::AudioProcessor::ProcessingPrecision,
                                             AlwaysNonRealtime)
{
    sampleRate = (sampleRateIn > 0.0) ? sampleRateIn : 44100.0;
    maximumSamplesPerBlock = juce::jmax(256, maximumSamplesPerBlockIn);
    numChannels = juce::jmax(2, numChannelsIn);

    // Pre-allocate temporary buffer for sample-rate conversion up to 4x oversampling + margin
    resampleTempBuffer.setSize(numChannels, maximumSamplesPerBlock * 4 + 32, false, true, true);

    if (auto* doc = juce::ARADocumentControllerSpecialisation::getSpecialisedDocumentController<OpenMeloDocumentController>(getDocumentController()))
    {
        if (!isPrepared)
        {
            doc->incrementActivePlaybackRenderers();
            isPrepared = true;
        }
        doc->setDawSampleRate(sampleRate);
        doc->getAuditionVoice().setSampleRate(sampleRate);
    }

    // Pre-create readers for any already-assigned playback regions
    for (const auto& playbackRegion : getPlaybackRegions())
    {
        if (playbackRegion != nullptr && playbackRegion->getAudioModification() != nullptr)
        {
            if (auto* audioSource = playbackRegion->getAudioModification()->getAudioSource())
                getOrCreateReader(audioSource);
        }
    }
}

void OpenMeloPlaybackRenderer::releaseResources()
{
    if (isPrepared)
    {
        if (auto* doc = juce::ARADocumentControllerSpecialisation::getSpecialisedDocumentController<OpenMeloDocumentController>(getDocumentController()))
            doc->decrementActivePlaybackRenderers();
        isPrepared = false;
    }
    sourceReaders.clear();
}

void OpenMeloPlaybackRenderer::didAddPlaybackRegion(ARA::PlugIn::PlaybackRegion* playbackRegion) noexcept
{
    juce::ARAPlaybackRenderer::didAddPlaybackRegion(playbackRegion);
    auto* juceRegion = static_cast<juce::ARAPlaybackRegion*>(playbackRegion);
    if (juceRegion != nullptr && juceRegion->getAudioModification() != nullptr)
    {
        if (auto* audioSource = juceRegion->getAudioModification()->getAudioSource())
            getOrCreateReader(audioSource);
    }
}

void OpenMeloPlaybackRenderer::willRemovePlaybackRegion(ARA::PlugIn::PlaybackRegion* playbackRegion) noexcept
{
    juce::ARAPlaybackRenderer::willRemovePlaybackRegion(playbackRegion);
}

juce::ARAAudioSourceReader* OpenMeloPlaybackRenderer::getOrCreateReader(juce::ARAAudioSource* audioSource)
{
    if (audioSource == nullptr || !audioSource->isSampleAccessEnabled())
        return nullptr;

    auto it = sourceReaders.find(audioSource);
    if (it != sourceReaders.end() && it->second != nullptr && it->second->isValid())
        return it->second.get();

    auto newReader = std::make_unique<juce::ARAAudioSourceReader>(audioSource);
    if (!newReader->isValid())
        return nullptr;

    auto* rawPtr = newReader.get();
    sourceReaders[audioSource] = std::move(newReader);
    return rawPtr;
}

bool OpenMeloPlaybackRenderer::processBlock(juce::AudioBuffer<float>& buffer,
                                            juce::AudioProcessor::Realtime realtime,
                                            const juce::AudioPlayHead::PositionInfo& positionInfo) noexcept
{
    juce::ignoreUnused(realtime);
    const auto numSamples = buffer.getNumSamples();
    const auto timeInSamples = positionInfo.getTimeInSamples().orFallback(0);
    const bool isPlaying = positionInfo.getIsPlaying();

    // Always start with a clean output buffer for the track
    buffer.clear();

    auto* doc = juce::ARADocumentControllerSpecialisation::getSpecialisedDocumentController<OpenMeloDocumentController>(getDocumentController());
    if (doc == nullptr)
        return true;

    if (isPlaying && numSamples > 0)
    {
        const auto blockRange = juce::Range<juce::int64>::withStartAndLength(timeInSamples, numSamples);

        for (const auto& playbackRegion : getPlaybackRegions())
        {
            if (playbackRegion == nullptr)
                continue;

            const auto playbackSampleRange = playbackRegion->getSampleRange(sampleRate,
                                                                             juce::ARAPlaybackRegion::IncludeHeadAndTail::no);
            auto renderRange = blockRange.getIntersectionWith(playbackSampleRange);
            if (renderRange.isEmpty())
                continue;

            auto* audioModification = playbackRegion->getAudioModification();
            auto* audioSource = (audioModification != nullptr) ? audioModification->getAudioSource() : nullptr;
            if (audioSource == nullptr || !audioSource->isSampleAccessEnabled())
                continue;

            const double sourceRate = audioSource->getSampleRate();
            const int64_t totalSourceSamples = audioSource->getSampleCount();
            if (sourceRate <= 0.0 || totalSourceSamples <= 0)
                continue;

            auto* reader = getOrCreateReader(audioSource);
            if (reader == nullptr)
                continue;

            if (std::abs(sourceRate - sampleRate) < 0.5)
            {
                // Identical sample rate: 1:1 sample-accurate direct read
                juce::Range<juce::int64> modificationSampleRange {
                    playbackRegion->getStartInAudioModificationSamples(),
                    playbackRegion->getEndInAudioModificationSamples()
                };
                const auto modificationSampleOffset = modificationSampleRange.getStart() - playbackSampleRange.getStart();
                renderRange = renderRange.getIntersectionWith(modificationSampleRange.movedToStartAt(playbackSampleRange.getStart()));
                if (renderRange.isEmpty())
                    continue;

                const int numSamplesToRead = (int)renderRange.getLength();
                const int startInBuffer = (int)(renderRange.getStart() - blockRange.getStart());
                const int64_t startInSource = renderRange.getStart() + modificationSampleOffset;

                if (startInSource >= 0 && startInSource < totalSourceSamples)
                {
                    const int safeToRead = (int)juce::jmin<int64_t>(numSamplesToRead, totalSourceSamples - startInSource);
                    if (safeToRead > 0)
                        reader->read(&buffer, startInBuffer, safeToRead, startInSource, true, true);
                }
            }
            else
            {
                // Mismatched sample rate (e.g., 44.1 kHz audio source in a 48 kHz DAW project):
                // Stateless 4-point Catmull-Rom cubic interpolation aligned to exact timeline time
                const int numOutSamples = (int)renderRange.getLength();
                const int startInBuffer = (int)(renderRange.getStart() - blockRange.getStart());
                const double regionStartPlayback = playbackRegion->getStartInPlaybackTime();
                const double regionStartMod = playbackRegion->getStartInAudioModificationTime();
                const double ratio = sourceRate / sampleRate;

                const double firstOutTimeSec = (double)renderRange.getStart() / sampleRate;
                const double firstSourceTimeSec = firstOutTimeSec - regionStartPlayback + regionStartMod;
                const double exactFirstSourceSample = firstSourceTimeSec * sourceRate;

                const int64_t readStartSource = juce::jmax<int64_t>(0, (int64_t)std::floor(exactFirstSourceSample) - 1);
                const int64_t readEndSource = juce::jmin<int64_t>(totalSourceSamples,
                    (int64_t)std::ceil(exactFirstSourceSample + (double)numOutSamples * ratio) + 3);
                const int numSourceToRead = (int)(readEndSource - readStartSource);

                if (numSourceToRead > 0 && numSourceToRead <= resampleTempBuffer.getNumSamples())
                {
                    resampleTempBuffer.clear(0, numSourceToRead);
                    reader->read(&resampleTempBuffer, 0, numSourceToRead, readStartSource, true, true);

                    const int outChans = buffer.getNumChannels();
                    const int tempChans = resampleTempBuffer.getNumChannels();

                    for (int ch = 0; ch < outChans; ++ch)
                    {
                        float* dst = buffer.getWritePointer(ch, startInBuffer);
                        const float* src = resampleTempBuffer.getReadPointer(ch % tempChans);

                        for (int i = 0; i < numOutSamples; ++i)
                        {
                            const double globalSrcSample = exactFirstSourceSample + (double)i * ratio;
                            if (globalSrcSample < 0.0 || globalSrcSample >= (double)totalSourceSamples)
                                continue;

                            const double localSrcPos = globalSrcSample - (double)readStartSource;
                            const int idx1 = (int)std::floor(localSrcPos);
                            const float frac = (float)(localSrcPos - (double)idx1);

                            const int idx0 = juce::jlimit(0, numSourceToRead - 1, idx1 - 1);
                            const int idx1c = juce::jlimit(0, numSourceToRead - 1, idx1);
                            const int idx2 = juce::jlimit(0, numSourceToRead - 1, idx1 + 1);
                            const int idx3 = juce::jlimit(0, numSourceToRead - 1, idx1 + 2);

                            const float y0 = src[idx0];
                            const float y1 = src[idx1c];
                            const float y2 = src[idx2];
                            const float y3 = src[idx3];

                            const float c0 = y1;
                            const float c1 = 0.5f * (y2 - y0);
                            const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
                            const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);

                            dst[i] = ((c3 * frac + c2) * frac + c1) * frac + c0;
                        }
                    }
                }
            }
        }

        // Apply tuned note replacement from NoteManager at exact DAW timeline sample positions
        doc->getNoteManager().renderTrackPlayback(buffer, timeInSamples, numSamples);
    }

    return true;
}

//==============================================================================
OpenMeloEditorRenderer::OpenMeloEditorRenderer(ARA::PlugIn::DocumentController* dc)
    : juce::ARAEditorRenderer(dc)
{
}

void OpenMeloEditorRenderer::prepareToPlay(double sampleRateIn,
                                           int maximumSamplesPerBlockIn,
                                           int numChannelsIn,
                                           juce::AudioProcessor::ProcessingPrecision,
                                           AlwaysNonRealtime)
{
    sampleRate = (sampleRateIn > 0.0) ? sampleRateIn : 44100.0;
    maximumSamplesPerBlock = maximumSamplesPerBlockIn;
    numChannels = numChannelsIn;

    if (auto* doc = juce::ARADocumentControllerSpecialisation::getSpecialisedDocumentController<OpenMeloDocumentController>(getDocumentController()))
    {
        doc->setDawSampleRate(sampleRate);
        doc->getAuditionVoice().setSampleRate(sampleRate);
    }
}

void OpenMeloEditorRenderer::releaseResources()
{
}

bool OpenMeloEditorRenderer::processBlock(juce::AudioBuffer<float>& buffer,
                                          juce::AudioProcessor::Realtime realtime,
                                          const juce::AudioPlayHead::PositionInfo& positionInfo) noexcept
{
    // EditorRenderer shares the output buffer with PlaybackRenderer in AudioProcessorARAExtension::processBlockForARA.
    // Never clear the buffer here or it will mute the PlaybackRenderer output!
    juce::ignoreUnused(buffer, realtime, positionInfo);
    return true;
}

//==============================================================================
juce::ARAAudioModification* OpenMeloDocumentController::doCreateAudioModification(
    juce::ARAAudioSource* audioSource,
    ARA::ARAAudioModificationHostRef hostRef,
    const juce::ARAAudioModification* optionalModificationToClone) noexcept
{
    return new OpenMeloAudioModification(audioSource, hostRef, optionalModificationToClone);
}

juce::ARAPlaybackRenderer* OpenMeloDocumentController::doCreatePlaybackRenderer() noexcept
{
    return new OpenMeloPlaybackRenderer(getDocumentController());
}

juce::ARAEditorRenderer* OpenMeloDocumentController::doCreateEditorRenderer() noexcept
{
    return new OpenMeloEditorRenderer(getDocumentController());
}

juce::ARAEditorView* OpenMeloDocumentController::doCreateEditorView() noexcept
{
    return new OpenMeloEditorView(getDocumentController());
}

void OpenMeloDocumentController::setDawSampleRate(double newSampleRate)
{
    if (newSampleRate <= 0.0)
        return;

    const bool changed = (std::abs(dawSampleRate - newSampleRate) > 0.5);
    dawSampleRate = newSampleRate;
    auditionVoice.setSampleRate(dawSampleRate);

    if (changed)
    {
        noteManager.setSampleRate(dawSampleRate);
        syncRegionsWithDocument();
    }
}

void OpenMeloDocumentController::didEndEditing(juce::ARADocument* /*document*/)
{
    syncRegionsWithDocument();
}

void OpenMeloDocumentController::didAddAudioSourceToDocument(juce::ARADocument* /*document*/, juce::ARAAudioSource* /*audioSource*/)
{
}

void OpenMeloDocumentController::willRemoveAudioSourceFromDocument(juce::ARADocument* /*document*/, juce::ARAAudioSource* audioSource)
{
    cachedSourceBuffers.erase(audioSource);
}

void OpenMeloDocumentController::didEnableAudioSourceSamplesAccess(juce::ARAAudioSource* audioSource, bool enable)
{
    if (enable && audioSource != nullptr)
    {
        syncRegionsWithDocument();
    }
}

void OpenMeloDocumentController::doUpdateAudioSourceContent(juce::ARAAudioSource* audioSource, juce::ARAContentUpdateScopes /*scopeFlags*/)
{
    cachedSourceBuffers.erase(audioSource);
    for (auto it = scannedRegions.begin(); it != scannedRegions.end();)
    {
        if (it->second.audioSource == audioSource)
            it = scannedRegions.erase(it);
        else
            ++it;
    }
    syncRegionsWithDocument();
}

void OpenMeloDocumentController::didAddRegionSequenceToDocument(juce::ARADocument* /*document*/, juce::ARARegionSequence* /*regionSequence*/)
{
    syncRegionsWithDocument();
}

void OpenMeloDocumentController::didAddPlaybackRegionToRegionSequence(juce::ARARegionSequence* /*regionSequence*/, juce::ARAPlaybackRegion* /*playbackRegion*/)
{
    syncRegionsWithDocument();
}

void OpenMeloDocumentController::willRemovePlaybackRegionFromRegionSequence(juce::ARARegionSequence* /*regionSequence*/, juce::ARAPlaybackRegion* playbackRegion)
{
    scannedRegions.erase(playbackRegion);
}

void OpenMeloDocumentController::didUpdatePlaybackRegionProperties(juce::ARAPlaybackRegion* /*playbackRegion*/)
{
    syncRegionsWithDocument();
}

void OpenMeloDocumentController::willDestroyPlaybackRegion(juce::ARAPlaybackRegion* playbackRegion)
{
    scannedRegions.erase(playbackRegion);
}

const juce::AudioBuffer<float>* OpenMeloDocumentController::getOrReadAudioSourceBuffer(juce::ARAAudioSource* audioSource)
{
    if (audioSource == nullptr || !audioSource->isSampleAccessEnabled())
        return nullptr;

    auto it = cachedSourceBuffers.find(audioSource);
    if (it != cachedSourceBuffers.end() && it->second.getNumSamples() > 0)
        return &it->second;

    const int64_t sampleCount = audioSource->getSampleCount();
    const double sRate = audioSource->getSampleRate();
    const int numChannels = audioSource->getChannelCount();

    if (sampleCount <= 0 || sRate <= 0.0 || numChannels <= 0)
        return nullptr;

    juce::ARAAudioSourceReader reader(audioSource);
    if (!reader.isValid())
        return nullptr;

    juce::AudioBuffer<float> sourceBuffer(numChannels, (int)sampleCount);
    sourceBuffer.clear();

    if (!reader.read(&sourceBuffer, 0, (int)sampleCount, 0, true, true))
        return nullptr;

    auto& stored = cachedSourceBuffers[audioSource];
    stored = std::move(sourceBuffer);
    return &stored;
}

void OpenMeloDocumentController::syncRegionsWithDocument()
{
    auto* document = getDocument();
    if (document == nullptr)
        return;

    // Collect all currently readable playback regions and their current properties
    struct ActiveRegionEntry
    {
        juce::ARAPlaybackRegion* region = nullptr;
        ScannedRegionInfo info;
    };

    std::vector<ActiveRegionEntry> activeRegions;
    for (auto* regionSequence : document->getRegionSequences())
    {
        if (regionSequence == nullptr)
            continue;

        for (auto* playbackRegion : regionSequence->getPlaybackRegions())
        {
            if (playbackRegion == nullptr)
                continue;

            auto* audioModification = playbackRegion->getAudioModification();
            if (audioModification == nullptr)
                continue;

            auto* audioSource = audioModification->getAudioSource();
            if (audioSource == nullptr || !audioSource->isSampleAccessEnabled())
                continue;

            const double sourceRate = audioSource->getSampleRate();
            if (sourceRate <= 0.0 || audioSource->getSampleCount() <= 0)
                continue;

            const double targetRate = (dawSampleRate > 0.0) ? dawSampleRate : sourceRate;

            ScannedRegionInfo info;
            info.audioSource = audioSource;
            info.startInPlaybackTime = playbackRegion->getStartInPlaybackTime();
            info.durationInPlaybackTime = playbackRegion->getDurationInPlaybackTime();
            info.startInModificationTime = playbackRegion->getStartInAudioModificationTime();
            info.durationInModificationTime = playbackRegion->getDurationInAudioModificationTime();
            info.scannedSampleRate = targetRate;

            activeRegions.push_back({ playbackRegion, info });
        }
    }

    if (activeRegions.empty())
        return;

    // If notes were just restored from a saved DAW project archive and match the current regions,
    // record the regions as scanned without overwriting the user's saved note edits.
    if (stateRestoredFromArchive && scannedRegions.empty() && noteManager.getNumNotes() > 0)
    {
        for (const auto& entry : activeRegions)
            scannedRegions[entry.region] = entry.info;
        stateRestoredFromArchive = false;
        return;
    }

    // Check if all active regions are already scanned with identical timeline & sample-rate properties
    bool needsRescan = (activeRegions.size() != scannedRegions.size());
    if (!needsRescan)
    {
        for (const auto& entry : activeRegions)
        {
            auto it = scannedRegions.find(entry.region);
            if (it == scannedRegions.end() || !it->second.matches(entry.info))
            {
                needsRescan = true;
                break;
            }
        }
    }

    if (!needsRescan)
        return;

    // Preserve any existing user pitch edits (semitoneOffset / centsOffset) indexed by note start time in seconds
    struct SavedNoteEdit
    {
        double startTimeSec = 0.0;
        int semitoneOffset = 0;
        int centsOffset = 0;
    };
    std::vector<SavedNoteEdit> savedEdits;
    const double previousRate = (noteManager.getSampleRate() > 0.0) ? noteManager.getSampleRate() : 44100.0;
    for (const auto& existingNote : noteManager.getAllNotes())
    {
        if (existingNote.semitoneOffset != 0 || existingNote.centsOffset != 0)
        {
            savedEdits.push_back({
                (double)existingNote.startSample / previousRate,
                existingNote.semitoneOffset,
                existingNote.centsOffset
            });
        }
    }

    const bool wasEmptyBefore = scannedRegions.empty();
    std::map<juce::ARAPlaybackRegion*, ScannedRegionInfo> newScannedRegions;

    noteManager.clearNotes();

    for (const auto& entry : activeRegions)
    {
        auto* playbackRegion = entry.region;
        auto* audioSource = entry.info.audioSource;

        const auto* fullSourceBuffer = getOrReadAudioSourceBuffer(audioSource);
        if (fullSourceBuffer == nullptr)
            continue;

        const double sourceRate = audioSource->getSampleRate();
        const double targetRate = entry.info.scannedSampleRate;
        const int64_t totalSourceSamples = fullSourceBuffer->getNumSamples();
        const int numCh = fullSourceBuffer->getNumChannels();

        // Extract only the active modification time range of this region (fixing trimmed clip bar delay!)
        const double modStartTime = playbackRegion->getStartInAudioModificationTime();
        const double modEndTime = playbackRegion->getEndInAudioModificationTime();
        const double playbackStartTime = playbackRegion->getStartInPlaybackTime();

        const double clampedModStart = std::max(0.0, modStartTime);
        const double effectivePlaybackStart = playbackStartTime + (clampedModStart - modStartTime);

        const int64_t readStartSample = juce::jlimit<int64_t>(0, totalSourceSamples, std::llround(clampedModStart * sourceRate));
        const int64_t readEndSample = juce::jlimit<int64_t>(readStartSample, totalSourceSamples, std::llround(modEndTime * sourceRate));
        const int sliceLength = (int)(readEndSample - readStartSample);

        if (sliceLength <= 0)
            continue;

        juce::AudioBuffer<float> regionSliceBuffer(numCh, sliceLength);
        for (int ch = 0; ch < numCh; ++ch)
        {
            regionSliceBuffer.copyFrom(ch, 0, *fullSourceBuffer, ch, (int)readStartSample, sliceLength);
        }

        const int64_t regionTimelineStartSample = std::llround(effectivePlaybackStart * targetRate);

        if (std::abs(sourceRate - targetRate) < 0.5)
        {
            noteManager.scanAudioBuffer(regionSliceBuffer, targetRate, regionTimelineStartSample, true, false);
        }
        else
        {
            // Resample region slice from sourceRate to targetRate (DAW sample rate) so pitch and timeline match 100%
            const double speedRatio = sourceRate / targetRate;
            const int resampledLength = std::max(1, (int)std::llround((double)sliceLength * (targetRate / sourceRate)));
            juce::AudioBuffer<float> resampledBuffer(numCh, resampledLength);

            for (int ch = 0; ch < numCh; ++ch)
            {
                juce::LagrangeInterpolator interpolator;
                interpolator.reset();
                interpolator.process(speedRatio,
                                     regionSliceBuffer.getReadPointer(ch),
                                     resampledBuffer.getWritePointer(ch),
                                     resampledLength);
            }

            noteManager.scanAudioBuffer(resampledBuffer, targetRate, regionTimelineStartSample, true, false);
        }

        newScannedRegions[playbackRegion] = entry.info;
    }

    scannedRegions = std::move(newScannedRegions);

    // Re-apply any preserved pitch edits if regions were merely re-synced for sample rate
    if (!savedEdits.empty() && noteManager.getNumNotes() > 0)
    {
        const double currentRate = noteManager.getSampleRate();
        for (int idx = 0; idx < noteManager.getNumNotes(); ++idx)
        {
            const auto& n = noteManager.getAllNotes()[idx];
            const double noteStartSec = (double)n.startSample / currentRate;
            for (const auto& edit : savedEdits)
            {
                if (std::abs(noteStartSec - edit.startTimeSec) < 0.05)
                {
                    noteManager.restoreNoteTuning(idx, edit.semitoneOffset, edit.centsOffset);
                    break;
                }
            }
        }
    }

    if (wasEmptyBefore && !scannedRegions.empty())
    {
        int numNotes = noteManager.getNumNotes();
        NvdaBridge::getInstance().speak(
            "Open Melo ARA: Vocal track scanned automatically. " + juce::String(numNotes) + " notes ready.", true);
    }
}

bool OpenMeloDocumentController::doStoreObjectsToStream(juce::ARAOutputStream& output, const juce::ARAStoreObjectsFilter* filter) noexcept
{
    juce::ignoreUnused(filter);
    juce::MemoryBlock block;
    juce::MemoryOutputStream memStream(block, false);
    noteManager.serialize(memStream);

    auto blockSize = (juce::int64)block.getSize();
    if (!output.writeInt64(blockSize))
        return false;

    if (blockSize > 0)
    {
        if (!output.write(block.getData(), (size_t)blockSize))
            return false;
    }

    return true;
}

bool OpenMeloDocumentController::doRestoreObjectsFromStream(juce::ARAInputStream& input, const juce::ARARestoreObjectsFilter* filter) noexcept
{
    juce::ignoreUnused(filter);
    auto blockSize = input.readInt64();
    if (blockSize > 0 && !input.failed())
    {
        juce::MemoryBlock block((size_t)blockSize);
        if (input.read(block.getData(), (int)blockSize))
        {
            juce::MemoryInputStream memStream(block, false);
            noteManager.deserialize(memStream);
            if (noteManager.getNumNotes() > 0)
                stateRestoredFromArchive = true;
        }
    }
    return !input.failed();
}

//==============================================================================
#if JucePlugin_Enable_ARA
const ARA::ARAFactory* JUCE_CALLTYPE createARAFactory()
{
    return juce::ARADocumentControllerSpecialisation::createARAFactory<OpenMeloDocumentController>();
}
#endif

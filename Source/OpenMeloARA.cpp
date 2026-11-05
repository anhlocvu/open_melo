#include "OpenMeloARA.h"
#include "NvdaBridge.h"

//==============================================================================
OpenMeloPlaybackRenderer::OpenMeloPlaybackRenderer(ARA::PlugIn::DocumentController* dc)
    : juce::ARAPlaybackRenderer(dc)
{
}

void OpenMeloPlaybackRenderer::prepareToPlay(double sampleRateIn,
                                             int maximumSamplesPerBlockIn,
                                             int numChannelsIn,
                                             juce::AudioProcessor::ProcessingPrecision,
                                             AlwaysNonRealtime)
{
    sampleRate = sampleRateIn;
    maximumSamplesPerBlock = maximumSamplesPerBlockIn;
    numChannels = numChannelsIn;

    auto* doc = juce::ARADocumentControllerSpecialisation::getSpecialisedDocumentController<OpenMeloDocumentController>(getDocumentController());
    if (doc != nullptr)
        doc->setTargetSampleRate(sampleRateIn);
}

void OpenMeloPlaybackRenderer::releaseResources()
{
}

bool OpenMeloPlaybackRenderer::processBlock(juce::AudioBuffer<float>& buffer,
                                            juce::AudioProcessor::Realtime realtime,
                                            const juce::AudioPlayHead::PositionInfo& positionInfo) noexcept
{
    juce::ignoreUnused(realtime);
    const auto numSamples = buffer.getNumSamples();
    const auto timeInSamples = positionInfo.getTimeInSamples().orFallback(0);
    const bool isPlaying = positionInfo.getIsPlaying();

    auto* doc = juce::ARADocumentControllerSpecialisation::getSpecialisedDocumentController<OpenMeloDocumentController>(getDocumentController());
    if (doc == nullptr)
        return true;

    bool didRenderAnyRegion = false;

    if (isPlaying)
    {
        const auto blockRange = juce::Range<juce::int64>::withStartAndLength(timeInSamples, numSamples);

        for (const auto& playbackRegion : getPlaybackRegions())
        {
            const auto playbackSampleRange = playbackRegion->getSampleRange(sampleRate,
                                                                             juce::ARAPlaybackRegion::IncludeHeadAndTail::no);
            auto renderRange = blockRange.getIntersectionWith(playbackSampleRange);

            if (renderRange.isEmpty())
                continue;

            juce::Range<juce::int64> modificationSampleRange { playbackRegion->getStartInAudioModificationSamples(),
                                                               playbackRegion->getEndInAudioModificationSamples() };
            const auto modificationSampleOffset = modificationSampleRange.getStart() - playbackSampleRange.getStart();

            renderRange = renderRange.getIntersectionWith(modificationSampleRange.movedToStartAt(playbackSampleRange.getStart()));

            if (renderRange.isEmpty())
                continue;

            auto* audioSource = playbackRegion->getAudioModification()->getAudioSource();
            if (audioSource == nullptr)
                continue;

            juce::ARAAudioSourceReader reader(audioSource);
            const int numSamplesToRead = (int)renderRange.getLength();
            const int startInBuffer = (int)(renderRange.getStart() - blockRange.getStart());
            const auto startInSource = renderRange.getStart() + modificationSampleOffset;

            reader.read(&buffer, startInBuffer, numSamplesToRead, startInSource, true, true);
            didRenderAnyRegion = true;
        }

        // Apply tuned note replacement from NoteManager
        doc->getNoteManager().renderTrackPlayback(buffer, timeInSamples, numSamples);
    }

    if (!didRenderAnyRegion && isPlaying)
    {
        buffer.clear();
    }

    // Mix in user Audition voice for keyboard preview (P, Arrow keys)
    doc->getAuditionVoice().renderNextBlock(buffer, 0, numSamples);

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
    sampleRate = sampleRateIn;
    maximumSamplesPerBlock = maximumSamplesPerBlockIn;
    numChannels = numChannelsIn;

    auto* doc = juce::ARADocumentControllerSpecialisation::getSpecialisedDocumentController<OpenMeloDocumentController>(getDocumentController());
    if (doc != nullptr)
        doc->setTargetSampleRate(sampleRateIn);
}

void OpenMeloEditorRenderer::releaseResources()
{
}

void OpenMeloEditorRenderer::reset()
{
}

bool OpenMeloEditorRenderer::processBlock(juce::AudioBuffer<float>& buffer,
                                          juce::AudioProcessor::Realtime realtime,
                                          const juce::AudioPlayHead::PositionInfo& positionInfo) noexcept
{
    juce::ignoreUnused(realtime, positionInfo);
    auto* doc = juce::ARADocumentControllerSpecialisation::getSpecialisedDocumentController<OpenMeloDocumentController>(getDocumentController());
    if (doc == nullptr)
        return true;

    // Audition keyboard note preview when DAW host is stopped
    doc->getAuditionVoice().renderNextBlock(buffer, 0, buffer.getNumSamples());
    return true;
}

//==============================================================================
OpenMeloDocumentController::OpenMeloDocumentController(const ARA::PlugIn::PlugInEntry* entry,
                                                       const ARA::ARADocumentControllerHostInstance* hostInstance)
    : juce::ARADocumentControllerSpecialisation(entry, hostInstance)
{
    noteManager.onPlayAudition = [this](const juce::AudioBuffer<float>& buf)
    {
        auditionVoice.startPlayback(buf);
    };
}

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

void OpenMeloDocumentController::didAddAudioSourceToDocument(juce::ARADocument* /*document*/, juce::ARAAudioSource* /*audioSource*/)
{
}

void OpenMeloDocumentController::didAddRegionSequenceToDocument(juce::ARADocument* /*document*/, juce::ARARegionSequence* regionSequence)
{
    if (regionSequence != nullptr)
    {
        for (auto* region : regionSequence->getPlaybackRegions())
        {
            scanAudioSourceForRegion(region);
        }
    }
}

void OpenMeloDocumentController::didAddPlaybackRegionToRegionSequence(juce::ARARegionSequence* /*regionSequence*/, juce::ARAPlaybackRegion* playbackRegion)
{
    scanAudioSourceForRegion(playbackRegion);
}

void OpenMeloDocumentController::scanAudioSourceForRegion(juce::ARAPlaybackRegion* playbackRegion)
{
    if (playbackRegion == nullptr)
        return;

    auto* audioModification = playbackRegion->getAudioModification();
    if (audioModification == nullptr)
        return;

    auto* audioSource = audioModification->getAudioSource();
    if (audioSource == nullptr)
        return;

    if (scannedSources.find(audioSource) != scannedSources.end())
        return;

    int64_t sampleCount = audioSource->getSampleCount();
    double sRate = audioSource->getSampleRate();
    int numChannels = audioSource->getChannelCount();

    if (sampleCount <= 0 || sRate <= 0.0 || numChannels <= 0)
        return;

    scannedSources.insert(audioSource);

    // Read full audio samples via ARAAudioSourceReader
    juce::ARAAudioSourceReader reader(audioSource);
    juce::AudioBuffer<float> sourceBuffer(numChannels, (int)sampleCount);

    bool readSuccess = reader.read(&sourceBuffer, 0, (int)sampleCount, 0, true, true);
    if (!readSuccess)
        return;

    // Calculate timeline start position of sample 0 of the audio source
    double timeOffset = playbackRegion->getStartInPlaybackTime() - playbackRegion->getStartInAudioModificationTime();
    double effectiveRate = (targetSampleRate > 0.0) ? targetSampleRate : sRate;
    int64_t startTimelineSample = (int64_t)(timeOffset * effectiveRate);
    if (startTimelineSample < 0)
        startTimelineSample = 0;

    noteManager.scanAudioBuffer(sourceBuffer, sRate, startTimelineSample);

    int numNotes = noteManager.getNumNotes();
    NvdaBridge::getInstance().speak(
        "Open Melo ARA: Vocal track scanned automatically. " + juce::String(numNotes) + " notes ready.", true);
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

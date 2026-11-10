#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "NoteManager.h"
#include "AuditionVoice.h"
#include <map>
#include <set>
#include <memory>
#include <atomic>

class OpenMeloDocumentController;

//==============================================================================
class OpenMeloAudioModification : public juce::ARAAudioModification
{
public:
    using juce::ARAAudioModification::ARAAudioModification;
};

//==============================================================================
class OpenMeloPlaybackRenderer : public juce::ARAPlaybackRenderer
{
public:
    explicit OpenMeloPlaybackRenderer(ARA::PlugIn::DocumentController* dc);
    ~OpenMeloPlaybackRenderer() override;

    void prepareToPlay(double sampleRate,
                       int maximumSamplesPerBlock,
                       int numChannels,
                       juce::AudioProcessor::ProcessingPrecision,
                       AlwaysNonRealtime alwaysNonRealtime) override;
    void releaseResources() override;

    void didAddPlaybackRegion(ARA::PlugIn::PlaybackRegion* playbackRegion) noexcept override;
    void willRemovePlaybackRegion(ARA::PlugIn::PlaybackRegion* playbackRegion) noexcept override;

    bool processBlock(juce::AudioBuffer<float>& buffer,
                      juce::AudioProcessor::Realtime realtime,
                      const juce::AudioPlayHead::PositionInfo& positionInfo) noexcept override;

private:
    juce::ARAAudioSourceReader* getOrCreateReader(juce::ARAAudioSource* audioSource);

    double sampleRate = 44100.0;
    int maximumSamplesPerBlock = 4096;
    int numChannels = 2;
    bool isPrepared = false;

    juce::AudioBuffer<float> resampleTempBuffer;
    std::map<juce::ARAAudioSource*, std::unique_ptr<juce::ARAAudioSourceReader>> sourceReaders;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenMeloPlaybackRenderer)
};

//==============================================================================
class OpenMeloEditorRenderer : public juce::ARAEditorRenderer
{
public:
    explicit OpenMeloEditorRenderer(ARA::PlugIn::DocumentController* dc);

    void prepareToPlay(double sampleRate,
                       int maximumSamplesPerBlock,
                       int numChannels,
                       juce::AudioProcessor::ProcessingPrecision,
                       AlwaysNonRealtime alwaysNonRealtime) override;
    void releaseResources() override;

    bool processBlock(juce::AudioBuffer<float>& buffer,
                      juce::AudioProcessor::Realtime realtime,
                      const juce::AudioPlayHead::PositionInfo& positionInfo) noexcept override;

private:
    double sampleRate = 44100.0;
    int maximumSamplesPerBlock = 4096;
    int numChannels = 2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenMeloEditorRenderer)
};

//==============================================================================
class OpenMeloEditorView : public juce::ARAEditorView
{
public:
    using juce::ARAEditorView::ARAEditorView;
};

//==============================================================================
class OpenMeloDocumentController : public juce::ARADocumentControllerSpecialisation
{
public:
    template <typename... Args>
    explicit OpenMeloDocumentController(Args&&... args)
        : juce::ARADocumentControllerSpecialisation(std::forward<Args>(args)...)
    {
        noteManager.onPlayAudition = [this](const juce::AudioBuffer<float>& buf)
        {
            auditionVoice.startPlayback(buf);
        };
    }

    ~OpenMeloDocumentController() override = default;

    NoteManager& getNoteManager() { return noteManager; }
    AuditionVoice& getAuditionVoice() { return auditionVoice; }

    void setDawSampleRate(double newSampleRate);
    double getDawSampleRate() const noexcept { return dawSampleRate; }

    void incrementActivePlaybackRenderers() noexcept { ++activePlaybackRendererCount; }
    void decrementActivePlaybackRenderers() noexcept { --activePlaybackRendererCount; }
    int getActivePlaybackRendererCount() const noexcept { return activePlaybackRendererCount.load(); }

    void notifyEditorRendererProcessed() noexcept { lastEditorProcessTimeMs.store(juce::Time::getMillisecondCounter()); }
    juce::uint32 getLastEditorProcessTimeMs() const noexcept { return lastEditorProcessTimeMs.load(); }

    // ARADocument::Listener callbacks
    void didEndEditing(juce::ARADocument* document) override;
    void didAddAudioSourceToDocument(juce::ARADocument* document, juce::ARAAudioSource* audioSource) override;
    void willRemoveAudioSourceFromDocument(juce::ARADocument* document, juce::ARAAudioSource* audioSource) override;
    void didAddRegionSequenceToDocument(juce::ARADocument* document, juce::ARARegionSequence* regionSequence) override;

    // ARAAudioSource::Listener callbacks
    void didEnableAudioSourceSamplesAccess(juce::ARAAudioSource* audioSource, bool enable) override;
    void doUpdateAudioSourceContent(juce::ARAAudioSource* audioSource, juce::ARAContentUpdateScopes scopeFlags) override;

    // ARARegionSequence::Listener callbacks
    void didAddPlaybackRegionToRegionSequence(juce::ARARegionSequence* regionSequence, juce::ARAPlaybackRegion* playbackRegion) override;
    void willRemovePlaybackRegionFromRegionSequence(juce::ARARegionSequence* regionSequence, juce::ARAPlaybackRegion* playbackRegion) override;

    // ARAPlaybackRegion::Listener callbacks
    void didUpdatePlaybackRegionProperties(juce::ARAPlaybackRegion* playbackRegion) override;
    void willDestroyPlaybackRegion(juce::ARAPlaybackRegion* playbackRegion) override;

    // Synchronize and scan all active playback regions in the document
    void syncRegionsWithDocument();

protected:
    juce::ARAAudioModification* doCreateAudioModification(juce::ARAAudioSource* audioSource,
                                                          ARA::ARAAudioModificationHostRef hostRef,
                                                          const juce::ARAAudioModification* optionalModificationToClone) noexcept override;

    juce::ARAPlaybackRenderer* doCreatePlaybackRenderer() noexcept override;
    juce::ARAEditorRenderer* doCreateEditorRenderer() noexcept override;
    juce::ARAEditorView* doCreateEditorView() noexcept override;

    bool doRestoreObjectsFromStream(juce::ARAInputStream& input, const juce::ARARestoreObjectsFilter* filter) noexcept override;
    bool doStoreObjectsToStream(juce::ARAOutputStream& output, const juce::ARAStoreObjectsFilter* filter) noexcept override;

private:
    struct ScannedRegionInfo
    {
        juce::ARAAudioSource* audioSource = nullptr;
        double startInPlaybackTime = 0.0;
        double durationInPlaybackTime = 0.0;
        double startInModificationTime = 0.0;
        double durationInModificationTime = 0.0;
        double scannedSampleRate = 0.0;

        bool matches(const ScannedRegionInfo& other) const noexcept
        {
            return audioSource == other.audioSource
                && std::abs(startInPlaybackTime - other.startInPlaybackTime) < 1e-5
                && std::abs(durationInPlaybackTime - other.durationInPlaybackTime) < 1e-5
                && std::abs(startInModificationTime - other.startInModificationTime) < 1e-5
                && std::abs(durationInModificationTime - other.durationInModificationTime) < 1e-5
                && std::abs(scannedSampleRate - other.scannedSampleRate) < 0.5;
        }
    };

    const juce::AudioBuffer<float>* getOrReadAudioSourceBuffer(juce::ARAAudioSource* audioSource);

    NoteManager noteManager;
    AuditionVoice auditionVoice;

    double dawSampleRate = 0.0;
    bool stateRestoredFromArchive = false;
    std::atomic<int> activePlaybackRendererCount { 0 };
    std::atomic<juce::uint32> lastEditorProcessTimeMs { 0 };

    std::map<juce::ARAAudioSource*, juce::AudioBuffer<float>> cachedSourceBuffers;
    std::map<juce::ARAPlaybackRegion*, ScannedRegionInfo> scannedRegions;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenMeloDocumentController)
};

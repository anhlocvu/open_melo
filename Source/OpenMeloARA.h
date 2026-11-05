#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "NoteManager.h"
#include <set>
#include <memory>

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenMeloPlaybackRenderer)
};

//==============================================================================
class OpenMeloEditorRenderer : public juce::ARAEditorRenderer
{
public:
    using juce::ARAEditorRenderer::ARAEditorRenderer;
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
    using juce::ARADocumentControllerSpecialisation::ARADocumentControllerSpecialisation;
    ~OpenMeloDocumentController() override = default;

    NoteManager& getNoteManager() { return noteManager; }

    // ARADocument::Listener callbacks
    void didAddAudioSourceToDocument(juce::ARADocument* document, juce::ARAAudioSource* audioSource) override;
    void didAddRegionSequenceToDocument(juce::ARADocument* document, juce::ARARegionSequence* regionSequence) override;

    // ARARegionSequence::Listener callbacks
    void didAddPlaybackRegionToRegionSequence(juce::ARARegionSequence* regionSequence, juce::ARAPlaybackRegion* playbackRegion) override;

    // Automatic vocal track scanning
    void scanAudioSourceForRegion(juce::ARAPlaybackRegion* playbackRegion);

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
    NoteManager noteManager;
    std::set<juce::ARAAudioSource*> scannedSources;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenMeloDocumentController)
};

#include "PluginProcessor.h"
#include "PluginEditor.h"

OpenMeloAudioProcessor::OpenMeloAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    fallbackNoteManager.onPlayAudition = [this](const juce::AudioBuffer<float>& buf)
    {
        getAuditionVoice().startPlayback(buf);
    };
}

OpenMeloAudioProcessor::~OpenMeloAudioProcessor()
{
}

const juce::String OpenMeloAudioProcessor::getName() const
{
    return "Open Melo";
}

bool OpenMeloAudioProcessor::acceptsMidi() const
{
    return true;
}

bool OpenMeloAudioProcessor::producesMidi() const
{
    return false;
}

bool OpenMeloAudioProcessor::isMidiEffect() const
{
    return false;
}

double OpenMeloAudioProcessor::getTailLengthSeconds() const
{
    double tail = 0.0;
    if (getTailLengthSecondsForARA(tail))
        return tail;
    return 0.0;
}

int OpenMeloAudioProcessor::getNumPrograms()
{
    return 1;
}

int OpenMeloAudioProcessor::getCurrentProgram()
{
    return 0;
}

void OpenMeloAudioProcessor::setCurrentProgram(int /*index*/)
{
}

const juce::String OpenMeloAudioProcessor::getProgramName(int /*index*/)
{
    return {};
}

void OpenMeloAudioProcessor::changeProgramName(int /*index*/, const juce::String& /*newName*/)
{
}

void OpenMeloAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    getAuditionVoice().setSampleRate(sampleRate);
    fallbackNoteManager.setSampleRate(sampleRate);

    if (documentController != nullptr)
    {
        documentController->setTargetSampleRate(sampleRate);
    }

    prepareToPlayForARA(sampleRate, samplesPerBlock, getMainBusNumOutputChannels(), getProcessingPrecision());
}

void OpenMeloAudioProcessor::releaseResources()
{
    getAuditionVoice().stopPlayback();
    releaseResourcesForARA();
}

bool OpenMeloAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}

void OpenMeloAudioProcessor::didBindToARA() noexcept
{
    AudioProcessorARAExtension::didBindToARA();

    if (auto* pr = getPlaybackRenderer())
        documentController = juce::ARADocumentControllerSpecialisation::getSpecialisedDocumentController<OpenMeloDocumentController>(pr->getDocumentController());
    else if (auto* er = getEditorRenderer())
        documentController = juce::ARADocumentControllerSpecialisation::getSpecialisedDocumentController<OpenMeloDocumentController>(er->getDocumentController());
    else if (auto* ev = getEditorView())
        documentController = juce::ARADocumentControllerSpecialisation::getSpecialisedDocumentController<OpenMeloDocumentController>(ev->getDocumentController());

    if (documentController != nullptr)
    {
        documentController->getNoteManager().onPlayAudition = [this](const juce::AudioBuffer<float>& buf)
        {
            documentController->getAuditionVoice().startPlayback(buf);
        };
    }
}

AuditionVoice& OpenMeloAudioProcessor::getAuditionVoice()
{
    if (documentController != nullptr)
        return documentController->getAuditionVoice();

    if (auto* pr = getPlaybackRenderer())
    {
        documentController = juce::ARADocumentControllerSpecialisation::getSpecialisedDocumentController<OpenMeloDocumentController>(pr->getDocumentController());
        if (documentController != nullptr)
            return documentController->getAuditionVoice();
    }
    else if (auto* er = getEditorRenderer())
    {
        documentController = juce::ARADocumentControllerSpecialisation::getSpecialisedDocumentController<OpenMeloDocumentController>(er->getDocumentController());
        if (documentController != nullptr)
            return documentController->getAuditionVoice();
    }
    else if (auto* ev = getEditorView())
    {
        documentController = juce::ARADocumentControllerSpecialisation::getSpecialisedDocumentController<OpenMeloDocumentController>(ev->getDocumentController());
        if (documentController != nullptr)
            return documentController->getAuditionVoice();
    }

    return fallbackAuditionVoice;
}

NoteManager& OpenMeloAudioProcessor::getNoteManager()
{
    if (documentController != nullptr)
        return documentController->getNoteManager();

    if (auto* pr = getPlaybackRenderer())
    {
        documentController = juce::ARADocumentControllerSpecialisation::getSpecialisedDocumentController<OpenMeloDocumentController>(pr->getDocumentController());
        if (documentController != nullptr)
        {
            documentController->getNoteManager().onPlayAudition = [this](const juce::AudioBuffer<float>& buf)
            {
                documentController->getAuditionVoice().startPlayback(buf);
            };
            return documentController->getNoteManager();
        }
    }
    else if (auto* er = getEditorRenderer())
    {
        documentController = juce::ARADocumentControllerSpecialisation::getSpecialisedDocumentController<OpenMeloDocumentController>(er->getDocumentController());
        if (documentController != nullptr)
        {
            documentController->getNoteManager().onPlayAudition = [this](const juce::AudioBuffer<float>& buf)
            {
                documentController->getAuditionVoice().startPlayback(buf);
            };
            return documentController->getNoteManager();
        }
    }
    else if (auto* ev = getEditorView())
    {
        documentController = juce::ARADocumentControllerSpecialisation::getSpecialisedDocumentController<OpenMeloDocumentController>(ev->getDocumentController());
        if (documentController != nullptr)
        {
            documentController->getNoteManager().onPlayAudition = [this](const juce::AudioBuffer<float>& buf)
            {
                documentController->getAuditionVoice().startPlayback(buf);
            };
            return documentController->getNoteManager();
        }
    }

    return fallbackNoteManager;
}

void OpenMeloAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    int numSamples = buffer.getNumSamples();
    int numChannels = buffer.getNumChannels();

    if (numSamples <= 0 || numChannels <= 0)
        return;

    // 1. ARA Mode
    if (isBoundToARA())
    {
        if (!processBlockForARA(buffer, isRealtime(), getPlayHead()))
            processBlockBypassed(buffer, midiMessages);

        // Mix in user Audition voice for keyboard preview
        getAuditionVoice().renderNextBlock(buffer, 0, numSamples);
        return;
    }

    // 2. Real-time Fallback Mode for non-ARA hosts
    auto* hostPlayHead = getPlayHead();
    juce::Optional<juce::AudioPlayHead::PositionInfo> posInfo;
    if (hostPlayHead != nullptr)
        posInfo = hostPlayHead->getPosition();

    bool isHostPlaying = false;
    int64_t currentHostSample = 0;

    if (posInfo.hasValue())
    {
        isHostPlaying = posInfo->getIsPlaying();
        auto timeInSamples = posInfo->getTimeInSamples();
        if (timeInSamples.hasValue())
            currentHostSample = *timeInSamples;
    }

    if (isHostPlaying)
    {
        juce::AudioBuffer<float> incomingTrackCopy;
        incomingTrackCopy.makeCopyOf(buffer);

        fallbackNoteManager.renderTrackPlayback(buffer, currentHostSample, numSamples);
        fallbackNoteManager.processIncomingTrackAudio(incomingTrackCopy, currentHostSample, numSamples);
    }

    getAuditionVoice().renderNextBlock(buffer, 0, numSamples);
}

bool OpenMeloAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* OpenMeloAudioProcessor::createEditor()
{
    return new OpenMeloAudioProcessorEditor(*this);
}

void OpenMeloAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream stream(destData, true);
    stream.writeString("OPENMELO");
    stream.writeInt(3);
    getNoteManager().serialize(stream);
}

void OpenMeloAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0)
        return;

    juce::MemoryInputStream stream(data, (size_t)sizeInBytes, false);
    juce::String magic = stream.readString();
    if (magic == "OPENMELO")
    {
        int version = stream.readInt();
        if (version >= 1)
        {
            getNoteManager().deserialize(stream);
        }
    }
}

// Juce createPluginFilter
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OpenMeloAudioProcessor();
}

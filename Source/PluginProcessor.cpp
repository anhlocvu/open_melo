#include "PluginProcessor.h"
#include "PluginEditor.h"

OpenMeloAudioProcessor::OpenMeloAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    noteManager.onPlayAudition = [this](const juce::AudioBuffer<float>& buf)
    {
        auditionVoice.startPlayback(buf);
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

void OpenMeloAudioProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    auditionVoice.setSampleRate(sampleRate);
    noteManager.setSampleRate(sampleRate);
}

void OpenMeloAudioProcessor::releaseResources()
{
    auditionVoice.stopPlayback();
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

void OpenMeloAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;

    int numSamples = buffer.getNumSamples();
    int numChannels = buffer.getNumChannels();

    if (numSamples <= 0 || numChannels <= 0)
        return;

    // Check DAW host playhead position
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
        // 1. Keep a copy of incoming track audio for continuous real-time vocal ingestion
        juce::AudioBuffer<float> incomingTrackCopy;
        incomingTrackCopy.makeCopyOf(buffer);

        // 2. Sample-accurately replace the original vocal on track with tuned/formant-preserved audio (for edited notes)
        noteManager.renderTrackPlayback(buffer, currentHostSample, numSamples);

        // 3. Continuously ingest and detect vocal notes from incoming audio across the entire timeline
        noteManager.processIncomingTrackAudio(incomingTrackCopy, currentHostSample, numSamples);
    }

    // 4. Mix in user Audition voice (when navigating notes or editing with keyboard)
    auditionVoice.renderNextBlock(buffer, 0, numSamples);
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

    // Magic identifier and version for Open Melo state chunk
    stream.writeString("OPENMELO");
    stream.writeInt(2);

    noteManager.serialize(stream);
}

void OpenMeloAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes < 8)
        return;

    juce::MemoryInputStream stream(data, static_cast<size_t>(sizeInBytes), false);

    juce::String magic = stream.readString();
    if (magic != "OPENMELO")
        return;

    int version = stream.readInt();
    if (version == 1 || version == 2)
    {
        noteManager.deserialize(stream);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OpenMeloAudioProcessor();
}

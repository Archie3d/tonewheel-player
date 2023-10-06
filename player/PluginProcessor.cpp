// *****************************************************************************
//
//  Tonewheel Audio Engine
//
//  Sandbox plugin
//
//  Copyright (C) 2021 Arthur Benilov <arthur.benilov@gmail.com>
//
// *****************************************************************************

#include <chrono>

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
TonewheelAudioProcessor::TonewheelAudioProcessor()
    : AudioProcessor (getBusesProperties())
    , processEnabled (false)
    , inProcess (false)
    , engine()
    , console()
    , engineProxy(engine, console)
    , processLoad (0.0f)
    , dummyBuffer(tonewheel::MIX_BUFFER_NUM_CHANNELS, tonewheel::MIX_BUFFER_NUM_FRAMES)
{
}

TonewheelAudioProcessor::~TonewheelAudioProcessor()
{
}

//==============================================================================
const String TonewheelAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool TonewheelAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool TonewheelAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool TonewheelAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double TonewheelAudioProcessor::getTailLengthSeconds() const
{
    // @todo Get max tail from the buses
    return 0.0;
}

int TonewheelAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int TonewheelAudioProcessor::getCurrentProgram()
{
    return 0;
}

void TonewheelAudioProcessor::setCurrentProgram (int index)
{
    ignoreUnused (index);
}

const String TonewheelAudioProcessor::getProgramName (int index)
{
    ignoreUnused (index);

    return {};
}

void TonewheelAudioProcessor::changeProgramName (int index, const String& newName)
{
    ignoreUnused (index);
    ignoreUnused (newName);
}

//==============================================================================
void TonewheelAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepareToPlay((float)sampleRate, samplesPerBlock);
    processEnabled = true;
}

void TonewheelAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations

bool TonewheelAudioProcessor::canAddBus(bool isInput) const
{
    return true;
}

bool TonewheelAudioProcessor::canRemoveBus(bool isInput) const
{
    const auto nInputs{ getBusCount(true) };
    const auto nOutputs{ getBusCount(false) };

    if (isInput)
        return getBusCount(true) > 0;

    // We need at least one output bus
    return getBusCount(false) > 1;
}

bool TonewheelAudioProcessor::canApplyBusCountChange(bool isInput, bool isAdding, BusProperties& outProperties)
{
    if (getBusCount(isInput) == 0)
        return false;

    if (isAdding && !canAddBus(isInput))
        return false;

    if (!isAdding && !canRemoveBus(isInput))
        return false;

    if (isAdding) {
        outProperties.busName = String(isInput ? "In[" : "Out[") + String(getBusCount(false) + 1) + String("]");
        outProperties.defaultLayout = AudioChannelSet::stereo();
        outProperties.isActivatedByDefault = true;
    }

    return true;
}

bool TonewheelAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    for (int i = 0; i < layouts.inputBuses.size(); ++i) {
        auto channelSet{ layouts.inputBuses.getUnchecked(i) };

        if (channelSet != AudioChannelSet::stereo() && channelSet != AudioChannelSet::disabled())
            return false;
    }

    for (int i = 0; i < layouts.outputBuses.size(); ++i) {
        auto channelSet{ layouts.outputBuses.getUnchecked(i) };

        if (channelSet != AudioChannelSet::stereo() && channelSet != AudioChannelSet::disabled())
            return false;
    }

    return true;
}

#endif

void TonewheelAudioProcessor::processBlock(AudioBuffer<float>& buffer, MidiBuffer& midiMessages)
{
    using namespace std::chrono;

    auto timestampStart = high_resolution_clock::now();

    ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    if (totalNumInputChannels % 2 != 0)
        return;

    int totalActiveChannels = jmin(totalNumOutputChannels, engine.getAudioBusPool().getNumBuses() * 2);

    if (totalActiveChannels < 2 && totalActiveChannels % 2 != 0)
        return;

    int numInputBusesToProcess{ totalNumInputChannels / 2 };
    int numOutputBusesToProcess{ totalActiveChannels / 2 };

    // @todo This needs better synchronisation (when reloading patch).
    if (! processEnabled)
        return;

    inProcess = true;
    {
        if (auto* playHead{ getPlayHead() }) {
            juce::AudioPlayHead::CurrentPositionInfo posInfo{};
            playHead->getCurrentPosition(posInfo);

            tonewheel::Engine::TransportInfo transport{};
            transport.bpm = posInfo.bpm;
            transport.time = posInfo.timeInSeconds;
            transport.ppqPosition = posInfo.ppqPosition;
            engine.setTransportInfo(transport);
        }

        const auto nonRT{ isNonRealtime() };
        engine.setNonRealtime (nonRT);

        if (nonRT)
            processMidiNonRealtime(midiMessages);
        else
            processMidi(midiMessages);

        int numFrames{ buffer.getNumSamples() };
        int sampleIndex{ 0 };

        auto& buses{ engine.getAudioBusPool() };

        while (numFrames > 0) {
            engine.processAudioEvents();

            int processThisTime{ std::min(numFrames, tonewheel::MIX_BUFFER_NUM_FRAMES) };
            int channelIndex{ 0 };

            for (int busIndex = 0; busIndex < buses.getNumBuses(); ++busIndex) {
                if (busIndex < numInputBusesToProcess) {
                    // Feed input into the send buffer
                    const float* inL = buffer.getReadPointer(channelIndex, sampleIndex);
                    const float* inR = buffer.getReadPointer(channelIndex + 1, sampleIndex);
                    auto& sendBuffer{ buses[busIndex].getSendBuffer() };
                    float* sendL{ sendBuffer.getChannelData(0) };
                    float* sendR{ sendBuffer.getChannelData(1) };

                    for (int i = 0; i < processThisTime; ++i) {
                        sendL[i] += inL[i];
                        sendR[i] += inR[i];
                    }
                }

                if (busIndex < numOutputBusesToProcess) {
                    float* outL = buffer.getWritePointer(channelIndex, sampleIndex);
                    float* outR = buffer.getWritePointer(channelIndex + 1, sampleIndex);
                    ::memset(outL, 0, sizeof(float) * processThisTime);
                    ::memset(outR, 0, sizeof(float) * processThisTime);
                    buses[busIndex].processAndMix(outL, outR, processThisTime);
                } else {
                    // Process inaudible but to a dummy buffer
                    buses[busIndex].processAndMix(dummyBuffer.getWritePointer(0), dummyBuffer.getWritePointer(1), processThisTime);
                }

                channelIndex += 2;
            }

            sampleIndex += processThisTime;
            numFrames -= processThisTime;
        }
    } // inProcess

    inProcess = false;

    auto timestampStop = high_resolution_clock::now();
    auto duration_us = duration_cast<microseconds> (timestampStop - timestampStart).count();

    float realTime_us = 1e6f * (float) buffer.getNumSamples() / engine.getSampleRate();
    float load = duration_us / realTime_us;

    processLoad = jmin(1.0f, 0.99f * processLoad + 0.01f * load);
}

AudioProcessor::BusesProperties TonewheelAudioProcessor::getBusesProperties()
{
    BusesProperties buses;

    for (int i = 0; i < tonewheel::NUM_BUSES; ++i)
        buses = buses.withInput(String("In[") + String(i + 1) + String("]"), AudioChannelSet::stereo(), true);

    for (int i = 0; i < tonewheel::NUM_BUSES; ++i)
        buses = buses.withOutput(String("Out[") + String(i + 1) + String("]"), AudioChannelSet::stereo(), true);

    return buses;
}

void TonewheelAudioProcessor::processMidi(MidiBuffer& midiMessages)
{
    static int voiceId;

    if (midiMessages.getNumEvents() == 0)
        return;

    for (auto msgIter : midiMessages) {
        const auto msg{ msgIter.getMessage() };

        tonewheel::MidiMessage m(msg.getRawData(), (size_t) msg.getRawDataSize(), msg.getTimeStamp());
        engineProxy.postMidiMessage(m, false);
    }

    engineProxy.notify();
}

void TonewheelAudioProcessor::processMidiNonRealtime(MidiBuffer& midiMessages)
{
    if (midiMessages.getNumEvents() == 0)
        return;

    for (auto msgIter : midiMessages) {
        const auto msg{ msgIter.getMessage() };

        if (!engineProxy.sendMidiMessage(msg))
            break; // Abort processing it timing out
    }
}

//==============================================================================
bool TonewheelAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

AudioProcessorEditor* TonewheelAudioProcessor::createEditor()
{
    return new TonewheelAudioProcessorEditor(*this);
}

//==============================================================================
void TonewheelAudioProcessor::getStateInformation(MemoryBlock& destData)
{
    MemoryOutputStream os (destData, true);
    os.writeString (currentScript);
    os.writeString (contentFolder.getFullPathName());
}

void TonewheelAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    MemoryInputStream is (data, static_cast<size_t> (sizeInBytes), false);
    const auto script = is.readString();
    const auto path = is.readString();

    setPatchScript(script, File(path));

    notifyStateRestored();
}

void TonewheelAudioProcessor::notifyStateRestored()
{
    processorListeners.call ([&](Listener& listener) {
        listener.processorStateRestored();
    });
}

void TonewheelAudioProcessor::getPrebufferStatus(int& loaded, int& total) const
{
    auto& samplePool{ tonewheel::GlobalEngine::getInstance()->getSamplePool() };
    loaded = samplePool.getNumPreloadedSamples();
    total = samplePool.getNumSamples();
}

int TonewheelAudioProcessor::getActiveVoiceCount() const noexcept
{
    return tonewheel::GlobalEngine::getInstance()->getVoicePool().getNumActiveVoices();
}

void TonewheelAudioProcessor::setPatchScript(const String& script, const File& dir)
{
    processEnabled = false;

    if (inProcess)
    {
        // Wait for the processor to go out of the processing loop.
        MessageManager::callAsync ([this, s=script, d=dir] {
            setPatchScript(s, d);
        });

        return;
    }

    currentScript = script;

    engineProxy.stop();
    engine.reset();
    contentFolder = dir;
    engineProxy.setContentFolder(contentFolder);
    engineProxy.start(script);

    // Initiate samples preload
    auto* g{ tonewheel::GlobalEngine::getInstance() };
    auto& samplePool{ g->getSamplePool() };
    samplePool.preload(tonewheel::DEFAULT_STREAM_BUFFER_SIZE);

    engine.prepareToPlay();

    processEnabled = true;
}

void TonewheelAudioProcessor::addProcessorListener(TonewheelAudioProcessor::Listener* listener)
{
    jassert (listener);
    processorListeners.add (listener);
}

void TonewheelAudioProcessor::removeProcessorListener(TonewheelAudioProcessor::Listener* listener)
{
    processorListeners.remove (listener);
}

//==============================================================================
// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TonewheelAudioProcessor();
}

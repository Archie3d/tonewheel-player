// *****************************************************************************
//
//  Tonewheel Audio Engine
//
//  Sandbox plugin
//
//  Copyright (C) 2021 Arthur Benilov <arthur.benilov@gmail.com>
//
// *****************************************************************************

#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include "EngineProxy.h"
#include "PluginConsole.h"
#include <memory>
#include <thread>
#include "engine.h"

//==============================================================================

class TonewheelAudioProcessor final : public AudioProcessor
{
public:

    struct Listener
    {
        virtual ~Listener() = default;
        virtual void processorStateRestored() = 0;
    };

    //==============================================================================
    TonewheelAudioProcessor();
    ~TonewheelAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
    bool canAddBus(bool isInput) const override;
    bool canRemoveBus(bool isInput) const override;
    bool canApplyBusCountChange(bool isInput, bool isAdding, BusProperties& outProperties) override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
#endif

    void processBlock (AudioBuffer<float>&, MidiBuffer&) override;

    //==============================================================================
    AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const String getProgramName (int index) override;
    void changeProgramName (int index, const String& newName) override;

    //==============================================================================
    void getStateInformation (MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    tonewheel::Engine& getEngine() noexcept { return engine; }

    float getProcessLoad() const noexcept { return processLoad; }
    int getActiveVoiceCount() const noexcept;
    void getPrebufferStatus (int& loaded, int& total) const;

    void setPatchScript(const String& script, const File& dir = {});
    const File& getContentFolder() const noexcept { return contentFolder; }

    String getCurrentScript() const { return currentScript; }

    Console& getConsole() noexcept { return console; }

    void addProcessorListener (Listener* listener);
    void removeProcessorListener (Listener* listener);

private:

    static BusesProperties getBusesProperties();

    void processMidi (MidiBuffer& midiMessages);
    void processMidiNonRealtime (MidiBuffer& midiMessages);

    void notifyStateRestored();

    std::atomic<bool> processEnabled;
    std::atomic<bool> inProcess;

    tonewheel::Engine engine;
    Console console;
    EngineProxy engineProxy;

    std::atomic<float> processLoad;

    AudioBuffer<float> dummyBuffer;

    String currentScript;
    File contentFolder;

    ListenerList<TonewheelAudioProcessor::Listener> processorListeners;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TonewheelAudioProcessor)
};

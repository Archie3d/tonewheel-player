#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include "ScriptX/ScriptX.h"
#include "PluginConsole.h"
#include "engine/engine.h"
#include "engine/midi.h"
#include "engine/core/ring_buffer.h"
#include <functional>

class EngineProxy final : public juce::Thread
{
public:

    EngineProxy(tonewheel::Engine& eng, Console& con);
    ~EngineProxy();

    void start(const String& code);
    void stop();

    void setContentFolder(const File& dir);

    /**
     * Post a function to be executed on the script thread.
     */
    void postMessage(std::function<void()> func);

    /**
     * Post MIDI message to be processed asynchronously.
     *
     * @note This method returns immediately without waiting
     *       for the message to be processed.
     */
    void postMidiMessage(const MidiMessage& midiMessage);

    void postMidiMessage(const tonewheel::MidiMessage& midiMessage, bool notify = true);

    void notify();

    /**
     * Send MIDI message to be processed immediately.
     *
     * @note This method blocks until the message is processed.
     *
     * @return true If message got processed, false on timeout.
     */
    bool sendMidiMessage(const MidiMessage& midiMessage);

    bool sendMidiMessage(const tonewheel::MidiMessage& midiMessage);

    // juce::String
    void run() override;

private:

    void registerGlobals();

    void eval(const String& code);

    void callOnMidiMessage(const MidiMessage& midiMessage);
    void callOnMidiMessage(const tonewheel::MidiMessage& midiMessage);

    File contentFolder{};

    tonewheel::core::RingBuffer<tonewheel::MidiMessage, 1024> midiBuffer;

    tonewheel::Engine& engine;
    Console& console;
    std::shared_ptr<script::ScriptEngine> scriptEngine{ nullptr };
};

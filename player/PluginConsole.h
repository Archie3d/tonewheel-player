#pragma once

#include "JuceHeader.h"

#include "core/ring_buffer.h"
#include <mutex>
#include <queue>

class Console final : private juce::AsyncUpdater
{
public:

    class Listener
    {
    public:
        virtual void consoleMessageReceived(const String& message) = 0;
        virtual ~Listener() = default;
    };

    Console();
    ~Console();

    void addListener(Listener* listener);
    void removeListener(Listener* listener);

    void postMessage(const String& message);

private:

    void handleAsyncUpdate() override;

    juce::ListenerList<Listener> listeners;

    using MessagePtr = std::shared_ptr<String>;

    tonewheel::core::RingBuffer<MessagePtr, 1024> queue;
};

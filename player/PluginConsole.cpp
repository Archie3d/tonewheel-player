#include "PluginConsole.h"

Console::Console()
    : listeners{}
    , queue()
{
}

Console::~Console() = default;

void Console::addListener(Listener* listener)
{
    jassert(listener != nullptr);
    listeners.add(listener);
}

void Console::removeListener(Listener* listener)
{
    jassert(listener != nullptr);
    listeners.remove(listener);
}

void Console::postMessage(const String& message)
{
    queue.send(std::make_shared<String>(message));
    triggerAsyncUpdate();
}

void Console::handleAsyncUpdate()
{
    MessagePtr messagePtr{};

    while (queue.receive(messagePtr))
        listeners.call([msg = messagePtr.get()](Listener& l) { l.consoleMessageReceived(*msg); });
}

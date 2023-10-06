#pragma once

#include <JuceHeader.h>
#include <list>
#include "PluginProcessor.h"
#include "PluginConsole.h"

class TonewheelAudioProcessorEditor final : public AudioProcessorEditor,
                                            public Timer,
                                            public TonewheelAudioProcessor::Listener,
                                            public Console::Listener
{
public:
    TonewheelAudioProcessorEditor (TonewheelAudioProcessor&);
    ~TonewheelAudioProcessorEditor() override;

    //==============================================================================
    void paint (Graphics&) override;
    void resized() override;

    void timerCallback() override;

    void processorStateRestored() override;

    void consoleMessageReceived(const String& msg) override;

private:

    void loadUI();

    void reloadScriptFromEditor();

    void updateScriptFromProcessor();

    void clearConsole();

    TonewheelAudioProcessor& audioProcessor;

    std::list<String> consoleMessages;

    File scriptFile;

    vitro::ViewContainer viewContainer{};
    std::weak_ptr<vitro::CodeEditor> codeEditor{};

    juce::TextEditor* consoleEditor{};
    juce::Label* samplesLabel{};
    juce::Label* cpuLoadLabel{};
    juce::Label* voicesLabel{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TonewheelAudioProcessorEditor)
};

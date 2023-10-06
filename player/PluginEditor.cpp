#include "PluginEditor.h"

TonewheelAudioProcessorEditor::TonewheelAudioProcessorEditor (TonewheelAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
    , consoleMessages{}
{
    audioProcessor.addProcessorListener(this);
    audioProcessor.getConsole().addListener(this);

    addAndMakeVisible(viewContainer);

    loadUI();

    setResizable (true, true); // Must be called before setSize()!
    setResizeLimits(600, 400, 1000000, 1000000);
    setSize (800, 600);

    updateScriptFromProcessor();

    startTimerHz (10);
}

TonewheelAudioProcessorEditor::~TonewheelAudioProcessorEditor()
{
    audioProcessor.getConsole().removeListener(this);
    audioProcessor.removeProcessorListener (this);
}

//==============================================================================
void TonewheelAudioProcessorEditor::paint (Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll(getLookAndFeel().findColour(ResizableWindow::backgroundColourId));
}

void TonewheelAudioProcessorEditor::resized()
{
    viewContainer.setBounds(getLocalBounds());
}

void TonewheelAudioProcessorEditor::timerCallback()
{
    if (samplesLabel != nullptr) {
        int loaded{};
        int total{};
        audioProcessor.getPrebufferStatus(loaded, total);
        auto strPreload = String (loaded) + "/" + String (total);
        samplesLabel->setText(strPreload, juce::dontSendNotification);
    }

    if (cpuLoadLabel != nullptr) {
        auto strLoad{ String(int(audioProcessor.getProcessLoad() * 100.0f)) + "%" };
        cpuLoadLabel->setText(strLoad, dontSendNotification);
    }

    if (voicesLabel != nullptr) {
        auto strVoices{ String(audioProcessor.getActiveVoiceCount()) };
        voicesLabel->setText(strVoices, dontSendNotification);
    }
}

void TonewheelAudioProcessorEditor::processorStateRestored()
{
    updateScriptFromProcessor();
}

void TonewheelAudioProcessorEditor::consoleMessageReceived(const String& msg)
{
    while (consoleMessages.size() >= 100)
        consoleMessages.pop_front();

    consoleMessages.push_back(msg);

    if (consoleEditor != nullptr) {
        String text;

        for (const auto& line : consoleMessages)
            text += line + "\n";

        consoleEditor->setText(text);
        consoleEditor->moveCaretToEnd();
    } else {
        DBG(msg);
    }
}

void TonewheelAudioProcessorEditor::loadUI()
{
    viewContainer.loadFromResource("view.xml", "style.css");

    auto* view{ viewContainer.getView() };
    jassert(view != nullptr);

    const auto getComponentElement = [&](const String& id) -> vitro::ComponentElement::Ptr {
        if (auto el{ view->getElementById(id) })
            return std::dynamic_pointer_cast<vitro::ComponentElement>(el);

        jassertfalse; // Element not found
        return nullptr;
    };

    if (auto el{ getComponentElement("reload_button") }) {
        if (auto* button{ dynamic_cast<juce::TextButton*>(el->getComponent()) })
            button->onClick = [this]() { reloadScriptFromEditor(); };
    }

    if (auto el{ getComponentElement("open_button") }) {
        if (auto* button{ dynamic_cast<juce::TextButton*>(el->getComponent()) }) {
            button->onClick = [this]() {
                FileChooser chooser("Select a JS file to load...", scriptFile, "*.js");

                if (chooser.browseForFileToOpen()) {
                    scriptFile = chooser.getResult();
                    File dir{ scriptFile.getParentDirectory() };
                    const auto code{ scriptFile.loadFileAsString() };
                    clearConsole();
                    audioProcessor.setPatchScript(code, dir);
                    updateScriptFromProcessor();
                }
            };
        }
    }

    if (auto el{ getComponentElement("samples")})
        samplesLabel = dynamic_cast<juce::Label*>(el->getComponent());

    if (auto el{ getComponentElement("cpu_load")})
        cpuLoadLabel = dynamic_cast<juce::Label*>(el->getComponent());

    if (auto el{ getComponentElement("voices")})
        voicesLabel = dynamic_cast<juce::Label*>(el->getComponent());

    if (auto el{ getComponentElement("script")})
        codeEditor = std::dynamic_pointer_cast<vitro::CodeEditor>(el);

    if (auto el{ getComponentElement("console") })
        consoleEditor = dynamic_cast<juce::TextEditor*>(el->getComponent());
}

void TonewheelAudioProcessorEditor::reloadScriptFromEditor()
{
    if (auto codeEditorPtr{ codeEditor.lock() }) {
        auto code{ codeEditorPtr->getDocument().getAllContent() };
        clearConsole();
        audioProcessor.setPatchScript(code, audioProcessor.getContentFolder());
    }
}

void TonewheelAudioProcessorEditor::updateScriptFromProcessor()
{
    if (auto codeEditorPtr{ codeEditor.lock() })
        codeEditorPtr->getDocument().replaceAllContent(audioProcessor.getCurrentScript());
}

void TonewheelAudioProcessorEditor::clearConsole()
{
    consoleMessages.clear();

    if (consoleEditor != nullptr)
        consoleEditor->clear();
}

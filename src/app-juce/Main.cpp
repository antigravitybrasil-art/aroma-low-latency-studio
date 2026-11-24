// AromaStudio — interface premium em JUCE (Fase B, v0.5.0).
//
// Licença: JUCE sob GPLv3 (decisão do Allex, 23/11/2026) — este app é
// código aberto. O motor (DrumEngine) e o áudio (WASAPI) são os mesmos
// do standalone Win32; só a UI é JUCE.
#include <juce_gui_extra/juce_gui_extra.h>
#include "MainComponent.h"

namespace {

class AromaStudioApplication : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override { return "AromaStudio"; }
    const juce::String getApplicationVersion() override { return "0.5.0"; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise(const juce::String&) override {
        mainWindow = std::make_unique<MainWindow>(getApplicationName());
    }

    void shutdown() override { mainWindow = nullptr; }
    void systemRequestedQuit() override { quit(); }
    void anotherInstanceStarted(const juce::String&) override {}

private:
    class MainWindow : public juce::DocumentWindow {
    public:
        explicit MainWindow(juce::String name)
            : DocumentWindow(name, juce::Colour(0xff121216),
                             DocumentWindow::allButtons) {
            setUsingNativeTitleBar(true);
            setContentOwned(new aroma::MainComponent(), true);
            setResizable(true, true);
            setResizeLimits(900, 600, 2560, 1600);
            centreWithSize(1280, 820);
            setVisible(true);
        }
        void closeButtonPressed() override {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
    };

    std::unique_ptr<MainWindow> mainWindow;
};

} // namespace

START_JUCE_APPLICATION(AromaStudioApplication)

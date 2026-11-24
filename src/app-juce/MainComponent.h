#pragma once
// MainComponent — janela principal do AromaStudio (Fase B, v0.5.0).
//
// Liga tudo: AppModel (kit + motor), AudioStream (WASAPI contínuo),
// MidiMonitor (WinMM) e os componentes de UI (vista da bateria, lista
// de peças, painel de edição, navegador de kits, barra de status).
//
// Regra de tempo real: a thread de áudio/MIDI nunca toca em controles.
// O MIDI só escreve no AppModel (fila lock-free + atômicos); a UI
// atualiza por timer (100 ms). Mudanças estruturais (trocar sample,
// trocar de kit) param o áudio antes e recomeçam depois.
#include <juce_gui_basics/juce_gui_basics.h>
#include <chrono>
#include <memory>
#include <string>
#include <vector>
#include "../diagnostic/MidiMonitor.h"
#include "../standalone/AudioStream.h"
#include "AppModel.h"
#include "DrumKitView.h"
#include "InstrumentPanel.h"
#include "KitBrowser.h"
#include "StatusBar.h"

namespace aroma {

class MainComponent : public juce::Component, private juce::Timer {
public:
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    // Timer 100 ms: último hit, contadores, dispositivo desplugado.
    void timerCallback() override;

    // Inicialização
    std::string findKitsDir() const;
    void loadDemoKit();
    void openMidi();
    void openAudio();
    void stopAudio();
    void restartAudio();
    void refreshKitUi();
    void selectInstrument(int note);
    void onKitViewHit(int note);
    void onUploadSample();
    void onKitNew(const std::string& name);
    void onKitSave();
    void onKitLoad();
    void onKitDuplicate(const std::string& name);
    void showError(const juce::String& message);
    // Roda fn com o áudio parado (e recomeça se estava rodando).
    template <typename Fn> void withAudioStopped(Fn&& fn);

    uint64_t nowMs() const;

    AppModel model_;
    AudioStream stream_;
    MidiMonitor midi_;
    std::string kitsDir_;
    bool audioOn_ = false;
    bool invalidatedShown_ = false;
    // Último erro de abertura do áudio, para a barra de status não
    // apagá-lo: o timer reescreve o rótulo a cada 100 ms e, sem isto,
    // o motivo real sumia e só ficava "Áudio: fechado".
    std::string audioError_;
    uint64_t lastSeenHitTick_ = 0;
    // Contador do timer (100 ms) para tentar o MIDI de novo a cada ~2 s.
    int midiRetryTick_ = 0;
    std::chrono::steady_clock::time_point startTime_;

    juce::Label headerLabel_;
    DrumKitView kitView_;
    KitBrowser kitBrowser_;
    juce::Component instrumentList_;
    std::vector<std::unique_ptr<juce::TextButton>> instrumentButtons_;
    std::vector<int> instrumentNotes_; // nota de cada botão, na mesma ordem
    InstrumentPanel instrumentPanel_;
    // Rolagem do painel da peça (o conteúdo tem ~350 px e a janela pode
    // ser mais baixa que isso — sem isto os últimos sliders de EQ
    // ficavam cortados e inalcançáveis).
    juce::Viewport panelViewport_;
    StatusBar statusBar_;
};

} // namespace aroma

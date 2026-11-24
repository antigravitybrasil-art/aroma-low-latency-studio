#pragma once
// AppModel — lógica não-visual do AromaStudio (Fase B, v0.5.0).
//
// Sem JUCE e sem Windows: compila e é testado no Linux. A interface
// JUCE (MainComponent) é só uma casca em cima disto; o áudio
// (AudioStream) e o MIDI (MidiMonitor) são ligados por ela.
//
// O modelo é dono do DrumEngine e do kit atual. A UI lê o estado por
// aqui e nunca toca no motor por fora — exceto o AudioStream, que
// recebe engine() para chamar render() na thread de áudio.
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>
#include "../common/Kit.h"
#include "../engine/DrumEngine.h"

namespace aroma {

// Notas padrão de um kit novo (mesmas do mapa real da TDX-18S).
inline constexpr int kStandardKitNotes[] = {36, 37, 38, 42, 43, 44, 46, 47, 49, 50, 51, 59};

struct LastHit {
    int note = -1;
    int velocity = 0;
    uint64_t tickMs = 0; // ms (relógio monotônico, só p/ decaimento do flash)
};

class AppModel {
public:
    AppModel() = default;

    // ---- Kit ----
    // Carrega um kit do disco e aplica no motor. Se falhar, o kit atual
    // continua valendo. Devolve false com `error` preenchido.
    bool loadKit(const std::string& kitFilePath, std::string& error);
    // Salva o kit atual no caminho atual (kitPath_). Falha se não houver
    // caminho (use saveKitAs primeiro).
    bool saveKit(std::string& error);
    // Salva o kit atual num novo caminho (nova pasta) e passa a usá-lo.
    bool saveKitAs(const std::string& kitFilePath, std::string& error);
    // Kit novo: copia a estrutura do kit demo (12 peças, samples do
    // demo) para kitsDir/<name>/ e carrega. Se o demo não existir,
    // cria as 12 peças sem samples (tocam em silêncio até subir WAVs).
    bool newKit(const std::string& name, const std::string& kitsDir,
                std::string& error);
    // Duplica o kit atual para uma pasta irmã com outro nome.
    bool duplicateKit(const std::string& newName, std::string& error);
    // Pastas dentro de kitsDir que contêm kit.txt (ordenadas).
    static std::vector<std::string> listKits(const std::string& kitsDir);

    const std::string& kitPath() const { return kitPath_; }
    std::string kitName() const { return engine_.kit().name; }
    const Kit& kit() const { return engine_.kit(); }
    bool hasKit() const { return engine_.hasKit(); }
    bool isDirty() const { return dirty_; }
    void markClean() { dirty_ = false; }

    // ---- Peças ----
    bool selectInstrument(int note); // false se não houver peça nessa nota
    int selectedNote() const { return selectedNote_; }
    bool selectedParams(InstrumentParams& out) const;
    // Aplica os parâmetros da peça selecionada (ao vivo, sem parar o áudio).
    bool updateSelectedParams(const InstrumentParams& params);
    // Troca o sample de uma peça (stream PARADO — a UI para o áudio
    // antes de chamar). Devolve false com `error` se o WAV for inválido.
    bool uploadSample(int note, const std::string& wavPath, std::string& error);

    // ---- MIDI -> motor ----
    // Chamado pelo callback do WinMM (via MainComponent). Toca o sample
    // e registra o hit para a UI (flash da peça).
    void noteOn(int note, int velocity, uint64_t tickMs);
    void noteOff(int note) { engine_.noteOff(note); }

    DrumEngine& engine() { return engine_; }
    const DrumEngine& engine() const { return engine_; }

    // ---- Estado para a UI (timer lê sem tocar no áudio) ----
    LastHit lastHit() const;
    uint64_t noteOnCount() const { return noteOnCount_.load(std::memory_order_relaxed); }
    void setMasterGain(float g) { engine_.setMasterGain(g); }
    float masterGain() const { return engine_.masterGain(); }

private:
    DrumEngine engine_;
    std::string kitPath_;
    bool dirty_ = false;
    int selectedNote_ = 36;
    std::atomic<int> lastHitNote_{-1};
    std::atomic<int> lastHitVelocity_{0};
    std::atomic<uint64_t> lastHitTickMs_{0};
    std::atomic<uint64_t> noteOnCount_{0};
};

} // namespace aroma

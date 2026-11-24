#pragma once
// DrumEngine — o motor de áudio compartilhado (standalone + VST3).
//
// Regras de tempo real (ver docs/ARQUITETURA.md):
//  - pool FIXO de vozes pré-alocadas (32); nenhuma alocação no callback;
//  - nenhum acesso a disco no callback; samples já estão na RAM;
//  - nenhum mutex no callback; UI -> áudio por fila lock-free SPSC;
//  - parâmetros numéricos da peça por std::atomic<float> (ao vivo);
//  - o motor trabalha sempre a 48 kHz (kKitSampleRate); samples de
//    outras taxas são reamostrados no carregamento do kit.
//
// Cadeia por peça (nesta ordem, por voz): sample -> tune (taxa de
// reprodução, interpolação linear) -> EQ 3 bandas -> ganho (velocity
// ^ gamma * volume) -> pan (equal-power) -> master + soft-clip.
// Bateria é one-shot: Note Off é ignorado; o sample toca até o fim.
// O choke de grupo corta com fade de ~3 ms (não estala).
//
// Uso típico:
//   DrumEngine engine;
//   engine.loadKitFromFile("kits/demo-kit/kit.txt", err); // fora do áudio
//   engine.noteOn(note, velocity);       // qualquer thread (UI/MIDI)
//   engine.render(buffer, frames, ch);   // thread de áudio, sem alocação
//
// O motor é agnóstico a WASAPI/JUCE: quem chama render() é a "casca".
#include <array>
#include <atomic>
#include <cstdint>
#include <string>
#include "../common/Kit.h"
#include "../common/MidiTypes.h"
#include "Eq.h"

namespace aroma {

// Taxa do motor (a mesma do kit: kKitSampleRate).
inline constexpr int kEngineSampleRate = kKitSampleRate;

// Parâmetros numéricos de uma peça — é o que a interface (Fase B)
// mostra e edita. Limites aplicados no updateInstrument/loadKit:
// volume 0..2, pan -1..+1, tune -24..+24 semitons, gamma 0.1..4,
// EQ -18..+18 dB, freq do médio 50..16000 Hz.
struct InstrumentParams {
    float volume = 1.0f;
    float pan = 0.0f;
    float tune = 0.0f;       // semitons; taxa de reprodução = 2^(tune/12)
    float gamma = 1.0f;      // curva de velocity: v' = v^gamma
    float eqLowDb = 0.0f;    // grave (lowshelf 200 Hz)
    float eqMidDb = 0.0f;    // médio (peaking, freq configurável)
    float eqMidFreq = 1000.0f;
    float eqHighDb = 0.0f;   // agudo (highshelf 4 kHz)
};

class DrumEngine {
public:
    static constexpr int kMaxVoices = 32;
    static constexpr int kQueueSize = 256; // eventos MIDI pendentes (potência de 2)

    DrumEngine();
    ~DrumEngine();

    // Carrega um kit já lido (loadKit). Fora da thread de áudio.
    // Devolve false se o kit estiver vazio. A casca deve garantir que
    // render() não esteja rodando (parar o stream antes).
    bool setKit(Kit kit);
    bool hasKit() const { return hasKit_; }
    const Kit& kit() const { return kit_; }

    // Chamado pela thread MIDI/UI. Enfileira; o callback consome.
    // Se a fila estiver cheia, o evento mais antigo é descartado e
    // droppedEvents_ conta — nunca bloqueia.
    void noteOn(int note, int velocity);
    // Bateria é one-shot: Note Off é IGNORADO de propósito. A Aroma
    // TDX-18S manda Note Off 15–37 ms depois de cada Note On (medido
    // no relatório de 23/11/2026, mediana 19 ms) — obedecer a ele
    // cortaria todo hit aos 19 ms, no meio do sample, com um estalo
    // (foi exatamente o que o Allex ouviu na v0.3.0/v0.3.1). O sample
    // toca até o fim; quem corta som aqui é só o choke de grupo.
    void noteOff(int note);
    void allNotesOff();

    // Thread de áudio: mistura `frames` amostras em `out` (o buffer é
    // zerado e preenchido aqui; a casca soma se precisar mixar com
    // outra fonte). `outChannels` = 1 ou 2. Sem alocação, sem mutex,
    // sem disco.
    void render(float* out, int frames, int outChannels);

    // Parâmetros (atômicos; podem mudar de qualquer thread).
    void setMasterGain(float g) { masterGain_.store(g < 0.f ? 0.f : (g > 2.f ? 2.f : g)); }
    float masterGain() const { return masterGain_.load(); }
    void setMuted(bool m) { muted_.store(m); }

    // Estatísticas (leitura segura de qualquer thread).
    int activeVoices() const;
    uint64_t droppedEvents() const { return droppedEvents_.load(); }
    bool clipped() const { return clipped_.load(); }
    void clearClipFlag() { clipped_.store(false); }
    int sampleRate() const { return sampleRate_; }

    // ---- API para a interface (Fase B) ----
    // Lista de peças: kit() — cada KitInstrument tem nome, nota,
    // arquivo, volume, pan, tune, EQ, gamma, choke e camadas.
    //
    // updateInstrument(): muda os parâmetros numéricos de uma peça.
    // Seguro com o áudio rodando (atômicos) e vale para o próximo hit;
    // vozes já tocando guardam os valores do início do hit. Também
    // espelha no Kit, para o saveKit escrever os valores atuais.
    // Devolve false se não houver peça nessa nota.
    bool updateInstrument(int note, const InstrumentParams& params);
    // Lê os parâmetros atuais de uma peça (os mesmos da interface).
    bool instrumentParams(int note, InstrumentParams& out) const;
    // Troca o sample de uma peça por um WAV (16/24 bits, mono/estéreo,
    // qualquer taxa — reamostrado para 48 kHz). Stream PARADO (mesmo
    // contrato do setKit): troca o ponteiro do sample e o nome do
    // arquivo. Devolve false com `error` se o arquivo for inválido.
    bool replaceInstrumentSample(int note, const std::string& wavPath,
                                 std::string& error);
    // Adiciona uma peça ao kit (nota livre, 0..127) ou remove a peça
    // da nota. Stream PARADO (o vetor de peças pode realocar).
    bool addInstrument(const KitInstrument& inst);
    bool removeInstrument(int note);
    // Carrega um kit do disco e aplica (loadKit + setKit). Se falhar,
    // o kit atual continua valendo.
    bool loadKitFromFile(const std::string& kitFilePath, std::string& error);
    // Salva o kit atual no formato v2 (kit.txt + WAVs). Stream parado
    // não é obrigatório (lê o Kit, não o áudio), mas evite salvar
    // enquanto edita parâmetros na mesma thread.
    bool saveKitToFile(const std::string& kitFilePath, std::string& error);

private:
    struct QueuedEvent {
        uint8_t note = 0;
        uint8_t velocity = 0;
        bool on = false;
    };
    // Parâmetros "vivos" de uma peça, indexados pela nota MIDI.
    // A UI escreve, o callback lê; nunca há mutex nem alocação aqui.
    struct LiveParams {
        std::atomic<float> volume{1.0f};
        std::atomic<float> pan{0.0f};
        std::atomic<float> tune{0.0f};
        std::atomic<float> gamma{1.0f};
        std::atomic<float> eqLowDb{0.0f};
        std::atomic<float> eqMidDb{0.0f};
        std::atomic<float> eqMidFreq{1000.0f};
        std::atomic<float> eqHighDb{0.0f};
    };
    struct Voice {
        bool active = false;
        const float* data = nullptr; // aponta para dentro do WavFile (não possui)
        size_t lengthFrames = 0;
        int channels = 1;
        double pos = 0.0;            // posição fracionária no sample (tune)
        double rate = 1.0;           // taxa de reprodução: 2^(tune/12)
        float gain = 0.0f;           // já inclui velocity + volume do instrumento
        float panL = 1.0f, panR = 1.0f;
        std::string chokeGroup;      // cópia curta; só lida no callback
        int note = -1;
        uint64_t startedAt = 0;      // para roubo da voz mais antiga
        bool fading = false;         // choke em andamento: fade curto p/ não clicar
        int fadePos = 0;             // frames já desvanecidos
        Eq3Band eqL, eqR;            // EQ por canal (estados separados)
        bool eqBypass = true;
    };

    static constexpr int kFadeFrames = 144; // ~3 ms @48 kHz: fade de choke

    void startVoice(const KitInstrument& inst, int velocity);
    void chokeGroup(const std::string& group, int exceptNote);
    void applyLiveParams(const KitInstrument& inst);

    Kit kit_;
    bool hasKit_ = false;
    int sampleRate_ = kEngineSampleRate;

    std::array<Voice, kMaxVoices> voices_{};
    uint64_t voiceCounter_ = 0;
    std::array<LiveParams, 128> live_{}; // por nota MIDI

    // Fila SPSC lock-free (um produtor: noteOn/noteOff; um consumidor: render).
    std::array<QueuedEvent, kQueueSize> queue_{};
    std::atomic<size_t> writeIndex_{0};
    std::atomic<size_t> readIndex_{0};

    std::atomic<float> masterGain_{1.0f};
    std::atomic<bool> muted_{false};
    std::atomic<uint64_t> droppedEvents_{0};
    std::atomic<bool> clipped_{false};
};

} // namespace aroma

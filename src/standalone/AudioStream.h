#pragma once
// AudioStream — saída WASAPI contínua (event-driven) do standalone.
//
// Abre o dispositivo de renderização PADRÃO e tenta o Exclusive
// primeiro, com a mesma matriz da v0.2.6 do diagnóstico (formatos ×
// durações, e só vale o que o Initialize aceitar de verdade); se o
// Exclusive não abrir, cai para o Shared. O loop de áudio roda numa
// thread própria com MMCSS ("Pro Audio") e chama DrumEngine::render()
// a cada evento, convertendo float -> formato do dispositivo.
//
// Restrição honesta: o kit demo é 48 kHz e o motor reproduz os samples
// 1:1 nessa taxa (não há reamostragem nesta etapa). Por isso a matriz
// Exclusive só tenta formatos de 48 kHz. Se o dispositivo acabar em
// outra taxa (Shared com mix a 44,1 kHz, por exemplo), o stream toca
// mesmo assim, mas rateMismatch() fica true e a UI avisa que o som
// sairá fora do tom — silêncio ou desafinação escondida seriam piores.
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <atomic>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>
#include "../engine/DrumEngine.h"

namespace aroma {

class AudioStream {
public:
    AudioStream();
    ~AudioStream();
    AudioStream(const AudioStream&) = delete;
    AudioStream& operator=(const AudioStream&) = delete;

    // Abre o dispositivo padrão. preferredFrames: tamanho pedido em
    // Exclusive (256 = 5,3 ms @48k; o Shared ignora e usa o buffer do
    // motor do Windows). Devolve false com `error` preenchido se nem
    // Exclusive nem Shared abrirem.
    bool open(int preferredFrames, std::string& error);
    // Inicia o loop de áudio com o motor dado. O motor precisa viver
    // enquanto o stream roda.
    bool start(DrumEngine& engine);
    void stop();  // para o loop, mantém o cliente aberto (start de novo é rápido)
    void close(); // stop + libera o cliente WASAPI (open de novo refaz tudo)

    bool isOpen() const { return client_ != nullptr; }
    bool isRunning() const { return running_.load(std::memory_order_relaxed); }

    // Estado (leitura segura de qualquer thread).
    std::string mode() const { return mode_; } // "Exclusive" ou "Shared"
    std::string deviceName() const { return deviceName_; }
    std::string formatDescription() const { return formatDesc_; }
    int sampleRate() const { return sampleRate_; }
    int channels() const { return channels_; }
    int bufferFrames() const { return bufferFrames_; }
    double estimatedLatencyMs() const { return estLatencyMs_; }
    // Taxa do dispositivo diferente da do kit (48 kHz): o som sai fora do tom.
    bool rateMismatch() const { return rateMismatch_; }
    uint64_t underruns() const { return underruns_.load(std::memory_order_relaxed); }
    uint64_t callbacks() const { return callbacks_.load(std::memory_order_relaxed); }
    // O dispositivo foi desplugado/invalidado: a UI avisa e oferece reiniciar.
    bool deviceInvalidated() const { return deviceInvalidated_.load(std::memory_order_relaxed); }
    std::string lastError() const { return lastError_; }

private:
    void audioThreadMain();
    void writeSamples(BYTE* dst, UINT32 frames);

    IAudioClient* client_ = nullptr;
    IAudioRenderClient* render_ = nullptr;
    HANDLE event_ = nullptr;
    bool comInit_ = false;

    DrumEngine* engine_ = nullptr;
    std::thread audioThread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> deviceInvalidated_{false};
    std::atomic<uint64_t> underruns_{0};
    std::atomic<uint64_t> callbacks_{0};

    std::string mode_;
    std::string deviceName_;
    std::string formatDesc_;
    std::string lastError_;
    int sampleRate_ = 0;
    int channels_ = 2;
    int bits_ = 32;
    bool isFloat_ = false;
    int bufferFrames_ = 0;
    double estLatencyMs_ = 0.0;
    bool rateMismatch_ = false;

    std::vector<float> scratch_; // buffer de mix do motor (estéreo, pré-alocado)
};

} // namespace aroma

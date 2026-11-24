#pragma once
// Testes de áudio via WASAPI (MMDevice + IAudioClient), sem ASIO.
// - Enumera dispositivos de renderização e o formato de mixagem.
// - Testa taxas suportadas (IsFormatSupported).
// - Para cada modo (Exclusive / Shared) e buffer (64..1024), renderiza
//   2 s de silêncio em modo event-driven e mede:
//     intervalo médio entre callbacks, jitter (desvio padrão),
//     underruns (GetCurrentPadding == 0 com buffer pedido, ou
//     AUDCLNT_E_BUFFER_ERROR / xruns reportados), e a
//     LATÊNCIA ESTIMADA DE SAÍDA = buffer + período do dispositivo.
//   Isso é uma ESTIMATIVA — não uma medição física de ida e volta
//   (ver docs/ARQUITETURA.md).
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <string>
#include <vector>

namespace aroma {

struct AudioDeviceInfo {
    std::string id;   // id do MMDevice (para abrir depois)
    std::string name; // nome amigável
    bool isDefault = false;
};

struct RateSupport {
    int rate = 0;
    bool supported = false;
};

struct AudioTestResult {
    std::string mode;        // "Exclusive" ou "Shared"
    int bufferFrames = 0;
    int sampleRateUsed = 0;  // taxa realmente usada (pode cair para a do mix)
    std::string formatUsed;  // ex.: "48 kHz, 2 canais, 32 bits float"
    double bufferMs = 0.0;   // bufferFrames / sampleRate
    double devicePeriodMs = 0.0;
    double estimatedOutputLatencyMs = 0.0; // buffer + período (ESTIMATIVA)
    double avgCallbackIntervalMs = 0.0;
    double jitterMs = 0.0;   // desvio padrão do intervalo
    int callbacks = 0;
    int underruns = 0;
    bool ok = false;
    std::string error;
};

class AudioTester {
public:
    std::vector<AudioDeviceInfo> enumerateDevices();
    // Formato de mixagem do dispositivo (taxa/canais/bits).
    bool getMixFormat(const std::string& deviceId, WAVEFORMATEX& out, std::string& error);
    std::vector<RateSupport> testRates(const std::string& deviceId);
    // Um teste de 2 s. Roda na thread que chama (a UI usa thread de trabalho).
    AudioTestResult runTest(const std::string& deviceId, bool exclusive,
                            int bufferFrames, int sampleRate);
};

std::string waveFormatToString(const WAVEFORMATEX& fmt);

} // namespace aroma

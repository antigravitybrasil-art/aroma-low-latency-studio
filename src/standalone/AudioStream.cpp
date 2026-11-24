// AudioStream — implementação (ver AudioStream.h).
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include "AudioStream.h"
#include "PcmConvert.h"
#include <algorithm>
#include <avrt.h>
#include <cmath>
#include <cstdio>
#include <cstring>
// INITGUID antes do header de property keys: instancia
// PKEY_Device_FriendlyName neste TU (sem isso, o MinGW não resolve
// o símbolo; no MSVC, propsys.lib resolve).
#include <initguid.h>
#include <functiondiscoverykeys_devpkey.h>

namespace aroma {
namespace {

struct ComInit {
    ComInit() {
        hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (hr == RPC_E_CHANGED_MODE) {
            // COM já inicializado nesta thread noutro modo — é o caso
            // do AromaStudio: o JUCE inicializa COM como STA na thread
            // de mensagens, e pedir MTA devolve RPC_E_CHANGED_MODE.
            // Sem este tratamento, open() falhava sempre ("CoInitializeEx
            // falhou") e o app ficava em "Áudio: fechado" para sempre,
            // mesmo com o standalone (sem JUCE) funcionando no mesmo PC.
            // Usa o modo já ativo e NÃO desfaz no destructor/close().
            hr = S_OK;
            alreadyInit = true;
        }
    }
    // Por padrão o destructor desfaz o CoInitialize (caminhos de erro).
    // No caminho de sucesso, open() chama detach(): o cliente WASAPI
    // vive além desta função e o CoUninitialize fica para o close().
    ~ComInit() { if (SUCCEEDED(hr) && !detached && !alreadyInit) CoUninitialize(); }
    void detach() { detached = true; }
    HRESULT hr = E_FAIL;
    bool detached = false;
    bool alreadyInit = false; // COM já era desta thread; não é nosso p/ desfazer
};

template <typename T>
void safeRelease(T*& p) {
    if (p) { p->Release(); p = nullptr; }
}

std::string wideToUtf8(const std::wstring& w) {
    if (w.empty()) return "";
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (n <= 0) return "";
    std::string s(static_cast<size_t>(n - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, s.data(), n, nullptr, nullptr);
    return s;
}

std::string hresultName(HRESULT hr) {
    switch (static_cast<unsigned long>(hr)) {
    case 0x88890001UL: return "AUDCLNT_E_NOT_INITIALIZED";
    case 0x88890003UL: return "AUDCLNT_E_SERVICE_NOT_RUNNING";
    case 0x88890004UL: return "AUDCLNT_E_EVENTHANDLE_NOT_SET";
    case 0x88890005UL: return "AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED";
    case 0x88890008UL: return "AUDCLNT_E_UNSUPPORTED_FORMAT";
    case 0x8889000AUL: return "AUDCLNT_E_DEVICE_IN_USE";
    case 0x88890010UL: return "AUDCLNT_E_BUFFER_TOO_LARGE";
    case 0x88890012UL: return "AUDCLNT_E_BUFFER_SIZE_ERROR";
    case 0x88890016UL: return "AUDCLNT_E_INVALID_SIZE";
    case 0x88890017UL: return "AUDCLNT_E_DEVICE_INVALIDATED";
    case 0x80070057UL: return "E_INVALIDARG";
    case 0x80070005UL: return "E_ACCESSDENIED";
    default: return "";
    }
}

std::string formatHResult(const char* what, HRESULT hr) {
    char buf[220];
    const std::string name = hresultName(hr);
    if (!name.empty())
        std::snprintf(buf, sizeof(buf), "%s falhou (HRESULT 0x%08lX = %s)", what,
                      static_cast<unsigned long>(hr), name.c_str());
    else
        std::snprintf(buf, sizeof(buf), "%s falhou (HRESULT 0x%08lX)", what,
                      static_cast<unsigned long>(hr));
    return buf;
}

std::string waveFormatToString(const WAVEFORMATEX& fmt) {
    char buf[128];
    std::snprintf(buf, sizeof(buf), "%lu Hz, %d canal(is), %d bits",
                  static_cast<unsigned long>(fmt.nSamplesPerSec),
                  static_cast<int>(fmt.nChannels),
                  static_cast<int>(fmt.wBitsPerSample));
    return buf;
}

std::string describeFormat(const WAVEFORMATEX& fmt) {
    std::string desc = waveFormatToString(fmt);
    if (fmt.wFormatTag == WAVE_FORMAT_EXTENSIBLE && fmt.cbSize >= 22) {
        const auto* ext = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(&fmt);
        const uint8_t* guid = reinterpret_cast<const uint8_t*>(&ext->SubFormat);
        desc += isFloatFormat(fmt.wFormatTag, guid, fmt.cbSize) ? " float" : " PCM";
    } else if (fmt.wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
        desc += " float";
    }
    return desc;
}

// Monta um formato PCM para sondar no modo Exclusive: o hardware
// geralmente não aceita o formato de mixagem float 32 em exclusivo.
// 16 bits = WAVE_FORMAT_PCM simples; 24 bits = extensível com GUID PCM.
std::vector<uint8_t> makePcmFormat(int rate, int bits, int channels) {
    static const GUID kPcm = {0x00000001, 0x0000, 0x0010,
        {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
    if (bits == 16) {
        std::vector<uint8_t> buf(sizeof(WAVEFORMATEX), 0);
        WAVEFORMATEX* f = reinterpret_cast<WAVEFORMATEX*>(buf.data());
        f->wFormatTag = WAVE_FORMAT_PCM;
        f->nChannels = static_cast<WORD>(channels);
        f->nSamplesPerSec = static_cast<DWORD>(rate);
        f->wBitsPerSample = 16;
        f->nBlockAlign = static_cast<WORD>(channels * 2);
        f->nAvgBytesPerSec = f->nSamplesPerSec * f->nBlockAlign;
        f->cbSize = 0;
        return buf;
    }
    std::vector<uint8_t> buf(sizeof(WAVEFORMATEXTENSIBLE), 0);
    WAVEFORMATEXTENSIBLE* f = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(buf.data());
    f->Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    f->Format.nChannels = static_cast<WORD>(channels);
    f->Format.nSamplesPerSec = static_cast<DWORD>(rate);
    f->Format.wBitsPerSample = 24;
    f->Format.nBlockAlign = static_cast<WORD>(channels * 3);
    f->Format.nAvgBytesPerSec = f->Format.nSamplesPerSec * f->Format.nBlockAlign;
    f->Format.cbSize = 22;
    f->Samples.wValidBitsPerSample = 24;
    f->dwChannelMask = channels == 2 ? 3 : 1;
    f->SubFormat = kPcm;
    return buf;
}

bool isFloatWfx(const WAVEFORMATEX& fmt) {
    if (fmt.wFormatTag == WAVE_FORMAT_IEEE_FLOAT) return true;
    if (fmt.wFormatTag == WAVE_FORMAT_EXTENSIBLE && fmt.cbSize >= 22) {
        const auto* ext = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(&fmt);
        const uint8_t* guid = reinterpret_cast<const uint8_t*>(&ext->SubFormat);
        return isFloatFormat(fmt.wFormatTag, guid, fmt.cbSize);
    }
    return false;
}

} // namespace

AudioStream::AudioStream() = default;

AudioStream::~AudioStream() {
    close();
}

bool AudioStream::open(int preferredFrames, std::string& error) {
    close();
    ComInit com;
    if (FAILED(com.hr)) { error = "CoInitializeEx falhou"; return false; }

    IMMDeviceEnumerator* enumerator = nullptr;
    if (CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                         __uuidof(IMMDeviceEnumerator),
                         reinterpret_cast<void**>(&enumerator)) != S_OK) {
        error = "enumerador de audio indisponivel";
        return false;
    }
    IMMDevice* device = nullptr;
    HRESULT hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    safeRelease(enumerator);
    if (hr != S_OK || !device) {
        error = "nenhum dispositivo de saida padrao encontrado";
        return false;
    }
    // Nome amigável do dispositivo.
    IPropertyStore* props = nullptr;
    if (device->OpenPropertyStore(STGM_READ, &props) == S_OK) {
        PROPVARIANT v{};
        PropVariantInit(&v);
        if (props->GetValue(PKEY_Device_FriendlyName, &v) == S_OK && v.pwszVal)
            deviceName_ = wideToUtf8(v.pwszVal);
        PropVariantClear(&v);
        safeRelease(props);
    }
    if (deviceName_.empty()) deviceName_ = "(dispositivo padrao)";

    hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                          reinterpret_cast<void**>(&client_));
    safeRelease(device);
    if (hr != S_OK || !client_) {
        error = "Activate(IAudioClient) falhou";
        return false;
    }

    // Formato: CÓPIA COMPLETA do formato de mixagem (o bug da v0.2.2 do
    // diagnóstico era copiar só 18 bytes de um WAVE_FORMAT_EXTENSIBLE
    // de 40 — o GUID do subformato virava lixo e o Initialize recusava
    // tudo).
    WAVEFORMATEX* mix = nullptr;
    if (client_->GetMixFormat(&mix) != S_OK || !mix) {
        error = "GetMixFormat falhou";
        safeRelease(client_);
        return false;
    }
    const size_t fmtSize = sizeof(WAVEFORMATEX) + mix->cbSize;
    std::vector<uint8_t> fmtBuf(fmtSize, 0);
    std::memcpy(fmtBuf.data(), mix, fmtSize);
    WAVEFORMATEX* fmt = reinterpret_cast<WAVEFORMATEX*>(fmtBuf.data());
    const int mixRate = static_cast<int>(mix->nSamplesPerSec);
    const int mixChannels = static_cast<int>(mix->nChannels);
    CoTaskMemFree(mix);

    REFERENCE_TIME defaultPeriod = 0, minimumPeriod = 0;
    client_->GetDevicePeriod(&defaultPeriod, &minimumPeriod);
    const double devicePeriodMs = static_cast<double>(defaultPeriod) / 10000.0;

    REFERENCE_TIME requestedDuration =
        static_cast<REFERENCE_TIME>(static_cast<double>(preferredFrames) * 10000000.0 / 48000.0);

    event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!event_) {
        error = "CreateEvent falhou";
        safeRelease(client_);
        return false;
    }

    const DWORD streamFlags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
    HRESULT lastHr = S_OK;
    bool opened = false;

    // --- Tentativa 1: Exclusive (matriz formato x duração, 48 kHz) -----
    // Só 48 kHz: o kit é 48 kHz e o motor não reamostra nesta etapa.
    // A sonda IsFormatSupported já mentiu neste PC (aceitou PCM 16 @48k
    // e o Initialize recusou tudo), então tenta TODOS os formatos,
    // aceitos pela sonda primeiro.
    {
        std::vector<std::vector<uint8_t>> formats;
        if (mixRate == 48000) formats.push_back(fmtBuf); // mix float @48k
        formats.push_back(makePcmFormat(48000, 16, mixChannels));
        formats.push_back(makePcmFormat(48000, 24, mixChannels));
        std::vector<bool> probeOk;
        for (auto& cand : formats) {
            WAVEFORMATEX* cf = reinterpret_cast<WAVEFORMATEX*>(cand.data());
            WAVEFORMATEX* closest = nullptr;
            const HRESULT hrProbe = client_->IsFormatSupported(
                AUDCLNT_SHAREMODE_EXCLUSIVE, cf, &closest);
            if (closest) CoTaskMemFree(closest);
            probeOk.push_back(hrProbe == S_OK);
        }
        std::vector<size_t> order(formats.size());
        for (size_t i = 0; i < order.size(); ++i) order[i] = i;
        std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
            return probeOk[a] && !probeOk[b];
        });
        std::vector<std::vector<uint8_t>> sorted;
        for (size_t i : order) sorted.push_back(formats[i]);
        formats = sorted;

        auto buildDurations = [&](int rate) {
            auto roundUp = [](REFERENCE_TIME v, REFERENCE_TIME q) -> REFERENCE_TIME {
                if (q <= 0) return v;
                return ((v + q - 1) / q) * q;
            };
            std::vector<REFERENCE_TIME> d;
            d.push_back(roundUp(requestedDuration, minimumPeriod));
            d.push_back(roundUp(requestedDuration, defaultPeriod));
            if (defaultPeriod > 0) d.push_back(defaultPeriod);
            if (minimumPeriod > 0) d.push_back(minimumPeriod);
            const int msValues[] = {1, 2, 3, 4, 5, 6, 8, 10, 12, 15, 20, 25, 30, 40, 50, 100};
            for (int ms : msValues) d.push_back(ms * 10000);
            const int frameValues[] = {128, 256, 512, 1024, 2048, 4096};
            for (int fr : frameValues)
                d.push_back(static_cast<REFERENCE_TIME>(
                    static_cast<double>(fr) * 10000000.0 / rate));
            std::sort(d.begin(), d.end());
            d.erase(std::unique(d.begin(), d.end()), d.end());
            return d;
        };

        for (auto& cand : formats) {
            WAVEFORMATEX* cf = reinterpret_cast<WAVEFORMATEX*>(cand.data());
            for (REFERENCE_TIME dur : buildDurations(48000)) {
                hr = client_->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE, streamFlags,
                                         dur, 0, cf, nullptr);
                if (hr == S_OK) {
                    fmtBuf = cand;
                    fmt = reinterpret_cast<WAVEFORMATEX*>(fmtBuf.data());
                    mode_ = "Exclusive";
                    opened = true;
                    break;
                }
                lastHr = hr;
            }
            if (opened) break;
        }
    }

    // --- Tentativa 2: Shared (formato de mixagem do dispositivo) -------
    if (!opened) {
        REFERENCE_TIME sharedDuration = requestedDuration;
        if (sharedDuration < defaultPeriod) sharedDuration = defaultPeriod;
        hr = client_->Initialize(AUDCLNT_SHAREMODE_SHARED, streamFlags,
                                 sharedDuration, 0, fmt, nullptr);
        if (hr == S_OK) {
            mode_ = "Shared";
            opened = true;
        } else {
            lastHr = hr;
        }
    }

    if (!opened) {
        error = formatHResult("Initialize (Exclusive e Shared)", lastHr);
        CloseHandle(event_);
        event_ = nullptr;
        safeRelease(client_);
        return false;
    }

    UINT32 actualBuffer = 0;
    client_->GetBufferSize(&actualBuffer);
    if (actualBuffer > 0) bufferFrames_ = static_cast<int>(actualBuffer);
    else bufferFrames_ = preferredFrames;

    sampleRate_ = static_cast<int>(fmt->nSamplesPerSec);
    channels_ = static_cast<int>(fmt->nChannels);
    bits_ = static_cast<int>(fmt->wBitsPerSample);
    isFloat_ = isFloatWfx(*fmt);
    formatDesc_ = describeFormat(*fmt);
    const double bufferMs = static_cast<double>(bufferFrames_) * 1000.0 / sampleRate_;
    estLatencyMs_ = bufferMs + devicePeriodMs;
    // O kit é 48 kHz; se o dispositivo ficou em outra taxa, o som sai
    // fora do tom — a UI avisa, não esconde.
    rateMismatch_ = (sampleRate_ != 48000);

    hr = client_->SetEventHandle(event_);
    if (hr != S_OK) {
        error = formatHResult("SetEventHandle", hr);
        CloseHandle(event_);
        event_ = nullptr;
        safeRelease(client_);
        return false;
    }
    hr = client_->GetService(__uuidof(IAudioRenderClient),
                             reinterpret_cast<void**>(&render_));
    if (hr != S_OK || !render_) {
        error = formatHResult("GetService(IAudioRenderClient)", hr);
        CloseHandle(event_);
        event_ = nullptr;
        safeRelease(client_);
        return false;
    }

    // O COM fica inicializado enquanto o cliente viver: o loop de áudio
    // (outra thread) chama métodos do IAudioClient. O CoUninitialize
    // correspondente é feito no close() — mas SÓ se fomos nós que
    // inicializamos (se o JUCE já tinha COM como STA, não desfazemos).
    com.detach();
    comInit_ = !com.alreadyInit;
    lastError_.clear();
    return true;
}

bool AudioStream::start(DrumEngine& engine) {
    if (!client_ || !render_ || !event_) {
        lastError_ = "stream nao aberto (chame open antes de start)";
        return false;
    }
    if (running_.load(std::memory_order_relaxed)) return true;
    engine_ = &engine;
    deviceInvalidated_.store(false, std::memory_order_relaxed);
    scratch_.assign(static_cast<size_t>(bufferFrames_) * 2, 0.0f);

    // Pré-enche o buffer com o motor antes do Start: o primeiro som
    // sai sem atraso e o stream não começa vazio.
    BYTE* data = nullptr;
    if (render_->GetBuffer(static_cast<UINT32>(bufferFrames_), &data) == S_OK && data) {
        engine_->render(scratch_.data(), bufferFrames_, 2);
        writeSamples(data, static_cast<UINT32>(bufferFrames_));
        render_->ReleaseBuffer(static_cast<UINT32>(bufferFrames_), 0);
    }
    HRESULT hr = client_->Start();
    if (hr != S_OK) {
        lastError_ = formatHResult("Start", hr);
        return false;
    }
    running_.store(true, std::memory_order_relaxed);
    audioThread_ = std::thread(&AudioStream::audioThreadMain, this);
    return true;
}

void AudioStream::stop() {
    if (!running_.load(std::memory_order_relaxed) && !audioThread_.joinable()) return;
    running_.store(false, std::memory_order_relaxed);
    if (event_) SetEvent(event_);
    if (audioThread_.joinable()) audioThread_.join();
    if (client_) client_->Stop();
}

void AudioStream::close() {
    stop();
    safeRelease(render_);
    safeRelease(client_);
    if (event_) { CloseHandle(event_); event_ = nullptr; }
    if (comInit_) { CoUninitialize(); comInit_ = false; }
    engine_ = nullptr;
    mode_.clear();
    formatDesc_.clear();
    deviceName_.clear();
    bufferFrames_ = 0;
    sampleRate_ = 0;
    estLatencyMs_ = 0.0;
    rateMismatch_ = false;
    underruns_.store(0, std::memory_order_relaxed);
    callbacks_.store(0, std::memory_order_relaxed);
    deviceInvalidated_.store(false, std::memory_order_relaxed);
}

void AudioStream::writeSamples(BYTE* dst, UINT32 frames) {
    convertFloatToDevice(scratch_.data(), dst, static_cast<int>(frames), 2, bits_, isFloat_);
}

void AudioStream::audioThreadMain() {
    // Prioridade de thread de áudio (MMCSS): tenta, não é fatal se falhar.
    DWORD taskIndex = 0;
    HANDLE avrt = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);

    while (running_.load(std::memory_order_relaxed)) {
        const DWORD wait = WaitForSingleObject(event_, 100);
        if (!running_.load(std::memory_order_relaxed)) break;
        if (wait != WAIT_OBJECT_0) continue;

        UINT32 padding = 0;
        HRESULT hr = client_->GetCurrentPadding(&padding);
        if (hr == AUDCLNT_E_DEVICE_INVALIDATED) {
            deviceInvalidated_.store(true, std::memory_order_relaxed);
            running_.store(false, std::memory_order_relaxed);
            break;
        }
        if (hr != S_OK) continue;
        // O evento disparou e o buffer já está vazio: o dispositivo
        // ficou sem som para tocar — conta como underrun (estalo).
        if (padding == 0) underruns_.fetch_add(1, std::memory_order_relaxed);
        const UINT32 available = bufferFrames_ > static_cast<int>(padding)
            ? static_cast<UINT32>(bufferFrames_) - padding : 0;
        if (available == 0) continue;

        BYTE* data = nullptr;
        hr = render_->GetBuffer(available, &data);
        if (hr == AUDCLNT_E_DEVICE_INVALIDATED) {
            deviceInvalidated_.store(true, std::memory_order_relaxed);
            running_.store(false, std::memory_order_relaxed);
            break;
        }
        if (hr == AUDCLNT_E_BUFFER_ERROR) {
            // O buffer ainda está com o dispositivo; tenta no próximo evento.
            underruns_.fetch_add(1, std::memory_order_relaxed);
            continue;
        }
        if (hr != S_OK || !data) continue;

        engine_->render(scratch_.data(), static_cast<int>(available), 2);
        writeSamples(data, available);
        hr = render_->ReleaseBuffer(available, 0);
        if (hr == AUDCLNT_E_DEVICE_INVALIDATED) {
            deviceInvalidated_.store(true, std::memory_order_relaxed);
            running_.store(false, std::memory_order_relaxed);
            break;
        }
        callbacks_.fetch_add(1, std::memory_order_relaxed);
    }

    if (client_) client_->Stop();
    if (avrt) AvRevertMmThreadCharacteristics(avrt);
}

} // namespace aroma

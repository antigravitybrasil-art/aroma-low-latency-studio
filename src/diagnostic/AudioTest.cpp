#include "AudioTest.h"
#include <algorithm>
#include <avrt.h>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
// INITGUID antes do header de property keys: instancia PKEY_Device_FriendlyName
// neste TU (sem isso, o MinGW não resolve o símbolo; no MSVC, propsys.lib resolve).
#include <initguid.h>
#include <functiondiscoverykeys_devpkey.h>

namespace aroma {
namespace {

struct ComInit {
    ComInit() { CoInitializeEx(nullptr, COINIT_MULTITHREADED); }
    ~ComInit() { CoUninitialize(); }
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

} // namespace

std::string waveFormatToString(const WAVEFORMATEX& fmt) {
    char buf[128];
    std::snprintf(buf, sizeof(buf), "%lu Hz, %d canal(is), %d bits",
                  static_cast<unsigned long>(fmt.nSamplesPerSec),
                  static_cast<int>(fmt.nChannels),
                  static_cast<int>(fmt.wBitsPerSample));
    return buf;
}

std::vector<AudioDeviceInfo> AudioTester::enumerateDevices() {
    std::vector<AudioDeviceInfo> out;
    ComInit com;
    IMMDeviceEnumerator* enumerator = nullptr;
    if (CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                         __uuidof(IMMDeviceEnumerator),
                         reinterpret_cast<void**>(&enumerator)) != S_OK)
        return out;
    IMMDeviceCollection* collection = nullptr;
    if (enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection) == S_OK) {
        UINT count = 0;
        collection->GetCount(&count);
        for (UINT i = 0; i < count; ++i) {
            IMMDevice* device = nullptr;
            if (collection->Item(i, &device) != S_OK) continue;
            LPWSTR id = nullptr;
            std::string idStr;
            if (device->GetId(&id) == S_OK) { idStr = wideToUtf8(id); CoTaskMemFree(id); }
            std::string name = idStr;
            IPropertyStore* props = nullptr;
            if (device->OpenPropertyStore(STGM_READ, &props) == S_OK) {
                PROPVARIANT v{};
                PropVariantInit(&v);
                if (props->GetValue(PKEY_Device_FriendlyName, &v) == S_OK && v.pwszVal)
                    name = wideToUtf8(v.pwszVal);
                PropVariantClear(&v);
                safeRelease(props);
            }
            out.push_back(AudioDeviceInfo{idStr, name, false});
            safeRelease(device);
        }
        safeRelease(collection);
    }
    // Marca o padrão.
    IMMDevice* def = nullptr;
    if (enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &def) == S_OK) {
        LPWSTR id = nullptr;
        if (def->GetId(&id) == S_OK) {
            const std::string defId = wideToUtf8(id);
            CoTaskMemFree(id);
            for (auto& d : out) if (d.id == defId) d.isDefault = true;
        }
        safeRelease(def);
    }
    safeRelease(enumerator);
    return out;
}

bool AudioTester::getMixFormat(const std::string& deviceId, WAVEFORMATEX& out,
                               std::string& error) {
    ComInit com;
    IMMDeviceEnumerator* enumerator = nullptr;
    if (CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                         __uuidof(IMMDeviceEnumerator),
                         reinterpret_cast<void**>(&enumerator)) != S_OK) {
        error = "nao foi possivel criar o enumerador de audio";
        return false;
    }
    std::wstring wid(deviceId.begin(), deviceId.end());
    IMMDevice* device = nullptr;
    HRESULT hr = enumerator->GetDevice(wid.c_str(), &device);
    safeRelease(enumerator);
    if (hr != S_OK || !device) { error = "dispositivo de audio nao encontrado"; return false; }
    IAudioClient* client = nullptr;
    hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                          reinterpret_cast<void**>(&client));
    safeRelease(device);
    if (hr != S_OK || !client) { error = "nao foi possivel ativar o cliente de audio"; return false; }
    WAVEFORMATEX* mix = nullptr;
    hr = client->GetMixFormat(&mix);
    if (hr != S_OK || !mix) {
        error = "GetMixFormat falhou";
        safeRelease(client);
        return false;
    }
    out = *mix;
    CoTaskMemFree(mix);
    safeRelease(client);
    return true;
}

std::vector<RateSupport> AudioTester::testRates(const std::string& deviceId) {
    std::vector<RateSupport> out;
    static const int kRates[] = {44100, 48000, 88200, 96000};
    ComInit com;
    IMMDeviceEnumerator* enumerator = nullptr;
    if (CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                         __uuidof(IMMDeviceEnumerator),
                         reinterpret_cast<void**>(&enumerator)) != S_OK)
        return out;
    std::wstring wid(deviceId.begin(), deviceId.end());
    IMMDevice* device = nullptr;
    if (enumerator->GetDevice(wid.c_str(), &device) != S_OK || !device) {
        safeRelease(enumerator);
        return out;
    }
    IAudioClient* client = nullptr;
    if (device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                         reinterpret_cast<void**>(&client)) != S_OK || !client) {
        safeRelease(device);
        safeRelease(enumerator);
        return out;
    }
    WAVEFORMATEX mix{};
    std::string err;
    if (getMixFormat(deviceId, mix, err)) {
        for (int rate : kRates) {
            WAVEFORMATEX test = mix;
            test.nSamplesPerSec = static_cast<DWORD>(rate);
            test.nAvgBytesPerSec = test.nSamplesPerSec * test.nBlockAlign;
            WAVEFORMATEX* closest = nullptr;
            const HRESULT hr = client->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, &test, &closest);
            if (closest) CoTaskMemFree(closest);
            out.push_back(RateSupport{rate, hr == S_OK});
        }
    }
    safeRelease(client);
    safeRelease(device);
    safeRelease(enumerator);
    return out;
}


// Nome legível dos HRESULTs mais comuns do WASAPI (para o relatório não
// trazer só o código hexadecimal).
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

// Escreve um tom senoidal (440 Hz) no buffer de render, no formato que o
// dispositivo usa (float 32, PCM 16/24/32). Fase contínua entre chamadas.
class ToneWriter {
public:
    explicit ToneWriter(const WAVEFORMATEX& fmt)
        : channels_(fmt.nChannels), rate_(fmt.nSamplesPerSec),
          bits_(fmt.wBitsPerSample), isFloat_(false) {
        if (fmt.wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
            isFloat_ = true;
        } else if (fmt.wFormatTag == WAVE_FORMAT_EXTENSIBLE && fmt.cbSize >= 22) {
            const auto* ext = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(&fmt);
            static const GUID kFloat = {0x00000003, 0x0000, 0x0010,
                {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
            isFloat_ = std::memcmp(&ext->SubFormat, &kFloat, sizeof(GUID)) == 0;
        }
    }

    void fill(BYTE* data, UINT32 frames) {
        const double step = 2.0 * 3.141592653589793 * 440.0 / rate_;
        for (UINT32 i = 0; i < frames; ++i) {
            const float s = static_cast<float>(std::sin(phase_) * 0.2);
            phase_ += step;
            if (phase_ > 2.0 * 3.141592653589793) phase_ -= 2.0 * 3.141592653589793;
            for (int ch = 0; ch < channels_; ++ch) {
                BYTE* p = data + (static_cast<size_t>(i) * channels_ + ch) * (bits_ / 8);
                writeSample(p, s);
            }
        }
    }

private:
    void writeSample(BYTE* p, float s) {
        if (isFloat_ && bits_ == 32) {
            *reinterpret_cast<float*>(p) = s;
        } else if (bits_ == 16) {
            const int v = static_cast<int>(s * 32767.0f);
            *reinterpret_cast<int16_t*>(p) = static_cast<int16_t>(v);
        } else if (bits_ == 24) {
            int v = static_cast<int>(s * 8388607.0f);
            p[0] = static_cast<BYTE>(v & 0xFF);
            p[1] = static_cast<BYTE>((v >> 8) & 0xFF);
            p[2] = static_cast<BYTE>((v >> 16) & 0xFF);
        } else if (bits_ == 32) {
            const int v = static_cast<int>(s * 2147483647.0);
            *reinterpret_cast<int32_t*>(p) = v;
        } else {
            std::memset(p, 0, bits_ / 8); // formato desconhecido: silêncio seguro
        }
    }

    int channels_ = 2;
    int rate_ = 48000;
    int bits_ = 16;
    bool isFloat_ = false;
    double phase_ = 0.0;
};


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


// Descrição legível do formato: "48000 Hz, 2 canal(is), 16 bits PCM".
std::string formatDescription(const WAVEFORMATEX& f) {
    std::string desc = waveFormatToString(f);
    if (f.wFormatTag == WAVE_FORMAT_EXTENSIBLE && f.cbSize >= 22) {
        const auto* ext = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(&f);
        static const GUID kFloat = {0x00000003, 0x0000, 0x0010,
            {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
        if (std::memcmp(&ext->SubFormat, &kFloat, sizeof(GUID)) == 0)
            desc += " float";
        else
            desc += " PCM";
    } else if (f.wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
        desc += " float";
    }
    return desc;
}

AudioTestResult AudioTester::runTest(const std::string& deviceId, bool exclusive,
                                     int bufferFrames, int sampleRate) {
    AudioTestResult res;
    res.mode = exclusive ? "Exclusive" : "Shared";
    res.bufferFrames = bufferFrames;
    res.bufferMs = sampleRate > 0 ? (static_cast<double>(bufferFrames) * 1000.0 / sampleRate) : 0.0;
    ComInit com;
    IMMDeviceEnumerator* enumerator = nullptr;
    if (CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                         __uuidof(IMMDeviceEnumerator),
                         reinterpret_cast<void**>(&enumerator)) != S_OK) {
        res.error = "enumerador de audio indisponivel";
        return res;
    }
    std::wstring wid(deviceId.begin(), deviceId.end());
    IMMDevice* device = nullptr;
    if (enumerator->GetDevice(wid.c_str(), &device) != S_OK || !device) {
        res.error = "dispositivo nao encontrado";
        safeRelease(enumerator);
        return res;
    }
    IAudioClient* client = nullptr;
    HRESULT hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                  reinterpret_cast<void**>(&client));
    safeRelease(device);
    safeRelease(enumerator);
    if (hr != S_OK || !client) { res.error = "Activate(IAudioClient) falhou"; return res; }

    // Formato: COPIA COMPLETA do formato de mixagem do dispositivo.
    // BUG CORRIGIDO (v0.2.2): o código copiava só sizeof(WAVEFORMATEX)
    // (18 bytes). O formato de mixagem do Realtek/USB CODEC é
    // WAVE_FORMAT_EXTENSIBLE (40 bytes: + GUID do subformato, máscara de
    // canais, bits válidos). O corte deixava o GUID como lixo e o
    // Initialize recusava tudo com AUDCLNT_E_UNSUPPORTED_FORMAT
    // (0x88890008) — foi exatamente o que aconteceu no PC do Allex em
    // 23/11/2026 (10 de 10 configs falharam).
    WAVEFORMATEX* mix = nullptr;
    if (client->GetMixFormat(&mix) != S_OK || !mix) {
        res.error = "GetMixFormat falhou";
        safeRelease(client);
        return res;
    }
    const size_t fmtSize = sizeof(WAVEFORMATEX) + mix->cbSize;
    std::vector<uint8_t> fmtBuf(fmtSize, 0);
    std::memcpy(fmtBuf.data(), mix, fmtSize);
    WAVEFORMATEX* fmt = reinterpret_cast<WAVEFORMATEX*>(fmtBuf.data());
    const int mixRate = static_cast<int>(mix->nSamplesPerSec);
    const int mixChannels = static_cast<int>(mix->nChannels);
    CoTaskMemFree(mix);

    // Seleção de formato (v0.2.6): em Exclusive, a sonda IsFormatSupported
    // pode dizer que um formato é aceito e o Initialize recusar mesmo
    // assim — foi o que aconteceu no PC do Allex: PCM 16 @48k passou na
    // sonda, mas o Initialize devolveu AUDCLNT_E_INVALID_SIZE para TODAS
    // as durações, nos DOIS dispositivos (runs 19:05 de 23/11). Por isso
    // o Exclusive agora testa a MATRIZ formato x duração (ver o bloco do
    // Initialize): cada formato candidato é tentado com várias durações,
    // e só vale o que o Initialize aceitar de verdade.
    int useRate = sampleRate;
    std::vector<std::vector<uint8_t>> formats; // só usado em Exclusive
    if (exclusive) {
        formats.push_back(fmtBuf); // mix float @48k
        formats.push_back(makePcmFormat(48000, 16, mixChannels));
        formats.push_back(makePcmFormat(44100, 16, mixChannels));
        formats.push_back(makePcmFormat(48000, 24, mixChannels));
        formats.push_back(makePcmFormat(44100, 24, mixChannels));
        {
            std::vector<uint8_t> float441 = fmtBuf;
            WAVEFORMATEX* f441 = reinterpret_cast<WAVEFORMATEX*>(float441.data());
            f441->nSamplesPerSec = 44100;
            f441->nAvgBytesPerSec = f441->nSamplesPerSec * f441->nBlockAlign;
            formats.push_back(float441);
        }
        // Ordena: aceitos pela sonda primeiro (mas tenta TODOS — a sonda
        // já mentiu uma vez).
        std::vector<bool> probeOk;
        for (auto& cand : formats) {
            WAVEFORMATEX* cf = reinterpret_cast<WAVEFORMATEX*>(cand.data());
            WAVEFORMATEX* closest = nullptr;
            const HRESULT hrProbe = client->IsFormatSupported(
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
        // fmt aponta para o primeiro candidato por enquanto; o vencedor
        // da matriz assume o lugar antes do teste de fato.
        fmtBuf = formats[0];
        fmt = reinterpret_cast<WAVEFORMATEX*>(fmtBuf.data());
        useRate = static_cast<int>(fmt->nSamplesPerSec);
    } else {
        // Shared: sonda a taxa pedida; se não aceitar, cai para a do mix.
        std::vector<uint8_t> probeBuf = fmtBuf;
        WAVEFORMATEX* probe = reinterpret_cast<WAVEFORMATEX*>(probeBuf.data());
        probe->nSamplesPerSec = static_cast<DWORD>(sampleRate);
        probe->nAvgBytesPerSec = probe->nSamplesPerSec * probe->nBlockAlign;
        WAVEFORMATEX* closest = nullptr;
        const HRESULT hrProbe = client->IsFormatSupported(
            AUDCLNT_SHAREMODE_SHARED, probe, &closest);
        if (closest) CoTaskMemFree(closest);
        if (hrProbe != S_OK) useRate = mixRate;
        fmt->nSamplesPerSec = static_cast<DWORD>(useRate);
        fmt->nAvgBytesPerSec = fmt->nSamplesPerSec * fmt->nBlockAlign;
    }
    // Descrição do primeiro candidato (a matriz recalcula com o vencedor).
    res.sampleRateUsed = useRate;
    res.formatUsed = formatDescription(*fmt);

    REFERENCE_TIME defaultPeriod = 0, minimumPeriod = 0;
    client->GetDevicePeriod(&defaultPeriod, &minimumPeriod);
    res.devicePeriodMs = static_cast<double>(defaultPeriod) / 10000.0;

    // Duração do buffer: respeita o período do dispositivo.
    // Shared: não pode ser menor que o período padrão (~10 ms).
    // Exclusive: não pode ser menor que o período mínimo do dispositivo.
    // Pedir 64 frames (1,3 ms) onde o mínimo é 3–10 ms falha por tamanho —
    // isso é dado, não bug: o teste mostra onde cada dispositivo aceita.
    REFERENCE_TIME floorPeriod = exclusive ? minimumPeriod : defaultPeriod;
    REFERENCE_TIME bufferDuration =
        static_cast<REFERENCE_TIME>(static_cast<double>(bufferFrames) * 10000000.0 / useRate);
    if (bufferDuration < floorPeriod) bufferDuration = floorPeriod;
    res.bufferMs = static_cast<double>(bufferDuration) / 10000.0;
    res.estimatedOutputLatencyMs = res.bufferMs + res.devicePeriodMs;

    HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!event) { res.error = "CreateEvent falhou"; safeRelease(client); return res; }

    const DWORD streamFlags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
    // Initialize — em Shared, uma única chamada. Em Exclusive, a matriz
    // formato x duração: para cada formato, tenta as durações (escada de
    // períodos, ms inteiros, frames redondos). Chamar Initialize de novo
    // após uma falha é o comportamento documentado: o cliente continua
    // não inicializado.
    HRESULT lastHr = S_OK;
    size_t attempts = 0;
    auto buildDurations = [&](int rate) {
        auto roundUp = [](REFERENCE_TIME v, REFERENCE_TIME q) -> REFERENCE_TIME {
            if (q <= 0) return v;
            return ((v + q - 1) / q) * q;
        };
        std::vector<REFERENCE_TIME> d;
        d.push_back(roundUp(bufferDuration, minimumPeriod));
        d.push_back(roundUp(bufferDuration, defaultPeriod));
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
    if (exclusive) {
        bool ok = false;
        for (auto& cand : formats) {
            WAVEFORMATEX* cf = reinterpret_cast<WAVEFORMATEX*>(cand.data());
            const int candRate = static_cast<int>(cf->nSamplesPerSec);
            for (REFERENCE_TIME dur : buildDurations(candRate)) {
                ++attempts;
                hr = client->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE, streamFlags,
                                        dur, 0, cf, nullptr);
                if (hr == S_OK) {
                    fmtBuf = cand;
                    fmt = reinterpret_cast<WAVEFORMATEX*>(fmtBuf.data());
                    useRate = candRate;
                    bufferDuration = dur;
                    ok = true;
                    break;
                }
                lastHr = hr;
            }
            if (ok) break;
        }
        if (!ok) hr = lastHr;
    } else {
        hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED, streamFlags,
                                bufferDuration, 0, fmt, nullptr);
    }
    // Descrição final do formato que vai rodar (ou do primeiro candidato,
    // se nada aceitou).
    res.sampleRateUsed = useRate;
    res.formatUsed = formatDescription(*fmt);
    res.bufferMs = static_cast<double>(bufferDuration) / 10000.0;
    res.estimatedOutputLatencyMs = res.bufferMs + res.devicePeriodMs;
    if (hr != S_OK) {
        res.error = formatHResult("Initialize", hr);
        if (exclusive) {
            char extra[260];
            std::snprintf(extra, sizeof(extra),
                          " [%zu formatos x %zu tentativas; periodo padrao %.1f ms; periodo minimo %.1f ms]",
                          formats.size(), attempts, res.devicePeriodMs,
                          static_cast<double>(minimumPeriod) / 10000.0);
            res.error += extra;
        }
        CloseHandle(event);
        safeRelease(client);
        return res;
    }
    UINT32 actualBuffer = 0;
    client->GetBufferSize(&actualBuffer);
    // O buffer real pode diferir do pedido (Shared fixa em 1056 frames;
    // Exclusive arredonda para o múltiplo do período) — registra o real.
    if (actualBuffer > 0) {
        res.bufferFrames = static_cast<int>(actualBuffer);
        res.bufferMs = static_cast<double>(actualBuffer) * 1000.0 / useRate;
        res.estimatedOutputLatencyMs = res.bufferMs + res.devicePeriodMs;
    }
    client->SetEventHandle(event);
    IAudioRenderClient* render = nullptr;
    hr = client->GetService(__uuidof(IAudioRenderClient),
                            reinterpret_cast<void**>(&render));
    if (hr != S_OK || !render) {
        res.error = formatHResult("GetService(IAudioRenderClient)", hr);
        CloseHandle(event);
        safeRelease(client);
        return res;
    }

    // Tom de teste AUDÍVEL (440 Hz, volume 0.2) em vez de silêncio.
    // A v0.2.1 renderizava silêncio por 2 s — ouvir "nada" era o
    // comportamento esperado, e o Allex não tinha como saber se o stream
    // estava vivo. Agora dá para ouvir o teste no fone ou nos falantes.
    ToneWriter tone(*fmt);
    {
        BYTE* data = nullptr;
        if (render->GetBuffer(actualBuffer, &data) == S_OK && data) {
            tone.fill(data, actualBuffer);
            render->ReleaseBuffer(actualBuffer, 0);
        }
    }
    // Prioridade de thread de áudio (MMC SS): tenta, não é fatal se falhar.
    DWORD taskIndex = 0;
    HANDLE avrt = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);

    hr = client->Start();
    if (hr != S_OK) {
        res.error = formatHResult("Start", hr);
        if (avrt) AvRevertMmThreadCharacteristics(avrt);
        safeRelease(render);
        CloseHandle(event);
        safeRelease(client);
        return res;
    }
    // Mede por 2 s: cada evento = um callback de render.
    const DWORD testMs = 2000;
    const DWORD t0 = timeGetTime();
    DWORD lastTick = t0;
    std::vector<double> intervals;
    int underruns = 0;
    while (true) {
        const DWORD elapsed = timeGetTime() - t0;
        if (elapsed >= testMs) break;
        const DWORD wait = WaitForSingleObject(event, 100);
        if (wait != WAIT_OBJECT_0) continue;
        const DWORD now = timeGetTime();
        intervals.push_back(static_cast<double>(now - lastTick));
        lastTick = now;
        UINT32 padding = 0;
        if (client->GetCurrentPadding(&padding) == S_OK && padding == 0) ++underruns;
        BYTE* data = nullptr;
        UINT32 framesAvailable = 0;
        // Quantos frames cabem: buffer - padding.
        if (client->GetCurrentPadding(&padding) == S_OK) {
            framesAvailable = actualBuffer > padding ? actualBuffer - padding : 0;
        }
        if (framesAvailable > 0 && render->GetBuffer(framesAvailable, &data) == S_OK && data) {
            tone.fill(data, framesAvailable);
            render->ReleaseBuffer(framesAvailable, 0);
        }
    }
    client->Stop();
    if (avrt) AvRevertMmThreadCharacteristics(avrt);
    res.callbacks = static_cast<int>(intervals.size());
    res.underruns = underruns;
    if (!intervals.empty()) {
        double sum = 0.0;
        for (double v : intervals) sum += v;
        res.avgCallbackIntervalMs = sum / intervals.size();
        double var = 0.0;
        for (double v : intervals) var += (v - res.avgCallbackIntervalMs) * (v - res.avgCallbackIntervalMs);
        res.jitterMs = std::sqrt(var / intervals.size());
    }
    res.ok = res.callbacks > 0;
    if (!res.ok && res.error.empty()) res.error = "nenhum callback em 2 s";
    safeRelease(render);
    CloseHandle(event);
    safeRelease(client);
    return res;
}

} // namespace aroma

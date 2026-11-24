// AromaDiagnostico — Etapa 2.
// App Win32 (sem JUCE, sem dependências externas):
//  - lista dispositivos MIDI, detecta a Aroma TDX-18S, monitora Note On/Off + velocity;
//  - lista dispositivos de áudio, testa taxas e WASAPI Exclusive/Shared;
//  - exporta relatório TXT + CSV.
// A thread de áudio/MIDI nunca toca em controles: a UI atualiza por timer
// e os testes de áudio rodam em thread de trabalho.
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <commctrl.h>
#include <cstdio>
#include <string>
#include <vector>
#include "../common/SystemInfo.h"
#include "../common/MidiTypes.h"
#include "MidiMonitor.h"
#include "AudioTest.h"

#pragma comment(lib, "comctl32.lib")

namespace {

constexpr int ID_SYSINFO = 101;
constexpr int ID_MIDI_LIST = 102;
constexpr int ID_BTN_MIDI_REFRESH = 103;
constexpr int ID_BTN_MIDI_START = 104;
constexpr int ID_MIDI_LOG = 105;
constexpr int ID_NOTE_COUNT = 106;
constexpr int ID_AUDIO_LIST = 107;
constexpr int ID_BTN_AUDIO_REFRESH = 108;
constexpr int ID_BTN_AUDIO_TEST = 109;
constexpr int ID_AUDIO_RESULTS = 110;
constexpr int ID_BTN_EXPORT = 111;
constexpr int ID_STATUS = 112;
constexpr int ID_RATE_INFO = 113;
constexpr UINT WM_APP_AUDIO_DONE = WM_APP + 2;
constexpr UINT TIMER_UI = 1;

HWND g_hWnd = nullptr;
HWND g_hSys, g_hMidiList, g_hMidiLog, g_hNoteCount, g_hAudioList,
     g_hAudioResults, g_hStatus, g_hRateInfo, g_hBtnMidiStart, g_hBtnAudioTest;
aroma::MidiMonitor g_midi;
aroma::AudioTester g_audio;
std::vector<aroma::MidiDeviceInfo> g_midiDevices;
std::vector<aroma::AudioDeviceInfo> g_audioDevices;
std::vector<aroma::AudioTestResult> g_audioResults;
std::string g_testedAudioName; // dispositivo do último teste de áudio
CRITICAL_SECTION g_resultsLock;
bool g_monitoring = false;
bool g_audioTesting = false;
size_t g_logShown = 0;
int g_selectedAudio = -1;

std::wstring utf8ToWide(const std::string& s) {
    if (s.empty()) return L"";
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(static_cast<size_t>(n > 0 ? n - 1 : 0), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
    return w;
}

void setStatus(const std::wstring& s) {
    SetWindowTextW(g_hStatus, s.c_str());
}

void appendLogLine(const std::wstring& line) {
    int len = GetWindowTextLengthW(g_hMidiLog);
    SendMessageW(g_hMidiLog, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessageW(g_hMidiLog, EM_REPLACESEL, FALSE, (LPARAM)line.c_str());
    SendMessageW(g_hMidiLog, EM_SCROLLCARET, 0, 0);
}

std::wstring midiEventLine(const aroma::MidiEvent& ev) {
    wchar_t buf[256];
    const char* type = ev.isNoteOn() ? "Note On" : ev.isNoteOff() ? "Note Off"
                       : ev.isControlChange() ? "CC" : "Outro";
    // NOTA: em swprintf/fwprintf, %s espera wchar_t*. O %S (string estreita)
    // tem sentido diferente no MSVC e no MinGW — foi ele que gerou os
    // caracteres orientais na v0.2.0. Tudo aqui passa por utf8ToWide().
    if (ev.isNoteOn() || ev.isNoteOff()) {
        swprintf(buf, L"%7llu ms  %-8s  nota %3d (%s)  vel %3d  canal %d\r\n",
                   (unsigned long long)(ev.timestampUs / 1000), utf8ToWide(type).c_str(),
                   (int)ev.data1, utf8ToWide(aroma::noteName(ev.data1)).c_str(),
                   (int)ev.data2, (int)ev.channel() + 1);
    } else {
        swprintf(buf, L"%7llu ms  %-8s  data1 %3d  data2 %3d  canal %d\r\n",
                   (unsigned long long)(ev.timestampUs / 1000), utf8ToWide(type).c_str(),
                   (int)ev.data1, (int)ev.data2, (int)ev.channel() + 1);
    }
    return buf;
}

void refreshNoteCounts() {
    SendMessageW(g_hNoteCount, LB_RESETCONTENT, 0, 0);
    const auto& counts = g_midi.perNoteCount();
    for (int n = 0; n < 128; ++n) {
        if (counts[n] == 0) continue;
        wchar_t buf[160];
        // Nome do instrumento provável, se houver no mapa padrão.
        std::string inst;
        for (const auto& e : aroma::kDefaultDrumMap)
            if (e.note == n) inst = e.instrument;
        swprintf(buf, L"nota %3d (%s)  x%llu  %s",
                   n, utf8ToWide(aroma::noteName(static_cast<uint8_t>(n))).c_str(),
                   (unsigned long long)counts[n],
                   inst.empty() ? L"(sem nome no mapa GM)" : utf8ToWide(inst).c_str());
        SendMessageW(g_hNoteCount, LB_ADDSTRING, 0, (LPARAM)buf);
    }
    wchar_t total[128];
    swprintf(total, L"Total: %llu eventos, %llu Note On",
               (unsigned long long)g_midi.totalEvents(),
               (unsigned long long)g_midi.noteOnCount());
    setStatus(total);
}

void refreshMidiDevices() {
    g_midiDevices = g_midi.enumerateDevices();
    SendMessageW(g_hMidiList, LB_RESETCONTENT, 0, 0);
    if (g_midiDevices.empty()) {
        SendMessageW(g_hMidiList, LB_ADDSTRING, 0,
                     (LPARAM)L"(nenhum dispositivo MIDI de entrada encontrado)");
        setStatus(L"Nenhum dispositivo MIDI encontrado. Ligue a Aroma e clique em Atualizar MIDI.");
        return;
    }
    for (const auto& d : g_midiDevices) {
        std::wstring line = utf8ToWide(d.name);
        if (d.looksLikeAroma) line += L"   [AROMA? — provavel TDX-18S]";
        SendMessageW(g_hMidiList, LB_ADDSTRING, 0, (LPARAM)line.c_str());
    }
    setStatus(L"Dispositivos MIDI listados. Selecione a Aroma e inicie o monitoramento.");
}

void refreshAudioDevices() {
    g_audioDevices = g_audio.enumerateDevices();
    SendMessageW(g_hAudioList, LB_RESETCONTENT, 0, 0);
    if (g_audioDevices.empty()) {
        SendMessageW(g_hAudioList, LB_ADDSTRING, 0,
                     (LPARAM)L"(nenhum dispositivo de audio encontrado)");
        return;
    }
    for (const auto& d : g_audioDevices) {
        std::wstring line = utf8ToWide(d.name);
        if (d.isDefault) line += L"   [padrao]";
        SendMessageW(g_hAudioList, LB_ADDSTRING, 0, (LPARAM)line.c_str());
    }
}

void showRateInfo(int index) {
    if (index < 0 || index >= (int)g_audioDevices.size()) return;
    WAVEFORMATEX mix{};
    std::string err;
    std::wstring text;
    if (g_audio.getMixFormat(g_audioDevices[index].id, mix, err)) {
        wchar_t buf[256];
        swprintf(buf, L"Formato de mixagem: %s",
                   utf8ToWide(aroma::waveFormatToString(mix)).c_str());
        text = buf;
    } else {
        text = L"Formato de mixagem: nao foi possivel ler.";
    }
    auto rates = g_audio.testRates(g_audioDevices[index].id);
    text += L"   Taxas: ";
    for (const auto& r : rates) {
        wchar_t b[32];
        swprintf(b, L"%d Hz %s  ", r.rate, r.supported ? L"OK" : L"nao");
        text += b;
    }
    SetWindowTextW(g_hRateInfo, text.c_str());
}

DWORD WINAPI audioTestThread(LPVOID param) {
    const int index = static_cast<int>(reinterpret_cast<intptr_t>(param));
    if (index < 0 || index >= (int)g_audioDevices.size()) {
        g_audioTesting = false;
        PostMessageW(g_hWnd, WM_APP_AUDIO_DONE, 0, 0);
        return 0;
    }
    const std::string deviceId = g_audioDevices[index].id;
    g_testedAudioName = g_audioDevices[index].name;
    // Taxa preferida: 48000 se suportada, senão a do mix.
    int rate = 48000;
    WAVEFORMATEX mix{};
    std::string err;
    if (g_audio.getMixFormat(deviceId, mix, err)) {
        bool ok48 = false;
        for (const auto& r : g_audio.testRates(deviceId))
            if (r.rate == 48000 && r.supported) ok48 = true;
        if (!ok48) rate = mix.nSamplesPerSec;
    }
    static const int kBuffers[] = {64, 128, 256, 512, 1024};
    for (bool exclusive : {true, false}) {
        for (int frames : kBuffers) {
            auto res = g_audio.runTest(deviceId, exclusive, frames, rate);
            EnterCriticalSection(&g_resultsLock);
            g_audioResults.push_back(res);
            LeaveCriticalSection(&g_resultsLock);
            PostMessageW(g_hWnd, WM_APP_AUDIO_DONE, 0, 0);
        }
    }
    g_audioTesting = false;
    PostMessageW(g_hWnd, WM_APP_AUDIO_DONE, 0, 0);
    return 0;
}

void showAudioResults() {
    SendMessageW(g_hAudioResults, LB_RESETCONTENT, 0, 0);
    EnterCriticalSection(&g_resultsLock);
    const auto results = g_audioResults;
    LeaveCriticalSection(&g_resultsLock);
    for (const auto& r : results) {
        wchar_t buf[512];
        if (r.ok) {
            swprintf(buf, L"[%s] buffer %4d @%d Hz (%s)  lat. est. %5.1f ms  callback %.1f ms  jitter %.2f ms  underruns %d  OK",
                       utf8ToWide(r.mode).c_str(), r.bufferFrames, r.sampleRateUsed,
                       utf8ToWide(r.formatUsed).c_str(), r.estimatedOutputLatencyMs,
                       r.avgCallbackIntervalMs, r.jitterMs, r.underruns);
        } else {
            swprintf(buf, L"[%s] buffer %4d  FALHOU: %s",
                       utf8ToWide(r.mode).c_str(), r.bufferFrames, utf8ToWide(r.error).c_str());
        }
        SendMessageW(g_hAudioResults, LB_ADDSTRING, 0, (LPARAM)buf);
    }
    SendMessageW(g_hAudioResults, LB_ADDSTRING, 0,
                 (LPARAM)L"Latencia acima = ESTIMATIVA (buffer + periodo). Nao e medicao fisica de ida e volta.");
}

bool exportReport(HWND hwnd) {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring dir(exePath);
    const size_t slash = dir.find_last_of(L"\\/");
    if (slash != std::wstring::npos) dir = dir.substr(0, slash + 1);
    // Nome único por processo: duas janelas do app exportando no mesmo
    // segundo escreviam no MESMO arquivo e misturavam o conteúdo (foi o
    // que aconteceu nos relatórios de 23/11/2026).
    const std::string stamp = aroma::currentTimestampForFilename() +
        "_p" + std::to_string(GetCurrentProcessId());
    const std::wstring txtPath = dir + L"diagnostico_aroma_" + utf8ToWide(stamp) + L".txt";
    const std::wstring csvPath = dir + L"diagnostico_aroma_" + utf8ToWide(stamp) + L".csv";

    const auto sys = aroma::collectSystemInfo();
    FILE* f = _wfopen(txtPath.c_str(), L"w, ccs=UTF-8");
    if (!f) {
        MessageBoxW(hwnd, L"Nao foi possivel criar o arquivo TXT.", L"Exportar", MB_ICONERROR);
        return false;
    }
    fwprintf(f, L"AROMA LOW LATENCY STUDIO — RELATORIO DE DIAGNOSTICO\n");
    fwprintf(f, L"Gerado em: %s\n\n", utf8ToWide(aroma::currentTimestampHuman()).c_str());
    fwprintf(f, L"[SISTEMA]\nCPU: %s\nNucleos: %d logicos / %d fisicos\nRAM: %llu MB total / %llu MB livre\nWindows: %s\nArquitetura: %s\nComputador: %s\n\n",
             utf8ToWide(sys.cpuBrand).c_str(), sys.logicalCores, sys.physicalCores,
             sys.ramTotalMB, sys.ramFreeMB, utf8ToWide(sys.windowsVersion).c_str(),
             utf8ToWide(sys.architecture).c_str(), utf8ToWide(sys.computerName).c_str());
    fwprintf(f, L"[DISPOSITIVOS MIDI]\n");
    for (const auto& d : g_midiDevices)
        fwprintf(f, L"  %d: %s%s\n", d.index, utf8ToWide(d.name).c_str(), d.looksLikeAroma ? L" [AROMA?]" : L"");
    if (g_midiDevices.empty()) fwprintf(f, L"  (nenhum)\n");
    fwprintf(f, L"\n[MONITOR MIDI] dispositivo: %s | eventos: %llu | Note On: %llu\n",
             utf8ToWide(g_midi.deviceName()).c_str(),
             (unsigned long long)g_midi.totalEvents(),
             (unsigned long long)g_midi.noteOnCount());
    fwprintf(f, L"Contagem por nota:\n");
    const auto& counts = g_midi.perNoteCount();
    for (int n = 0; n < 128; ++n)
        if (counts[n]) fwprintf(f, L"  nota %d (%s): %llu\n", n,
                                 utf8ToWide(aroma::noteName(static_cast<uint8_t>(n))).c_str(),
                                 (unsigned long long)counts[n]);
    fwprintf(f, L"\n[DISPOSITIVOS DE AUDIO]\n");
    for (const auto& d : g_audioDevices)
        fwprintf(f, L"  %s%s\n", utf8ToWide(d.name).c_str(), d.isDefault ? L" [padrao]" : L"");
    fwprintf(f, L"\n[TESTES WASAPI] (latencia = ESTIMATIVA buffer + periodo)\n");
    fwprintf(f, L"Dispositivo testado: %s\n", utf8ToWide(g_testedAudioName).c_str());
    EnterCriticalSection(&g_resultsLock);
    const auto results = g_audioResults;
    LeaveCriticalSection(&g_resultsLock);
    for (const auto& r : results) {
        if (r.ok)
            fwprintf(f, L"  %s buffer %d @%d Hz (%s): lat. est. %.1f ms, callback %.2f ms, jitter %.2f ms, underruns %d, callbacks %d\n",
                     utf8ToWide(r.mode).c_str(), r.bufferFrames, r.sampleRateUsed,
                     utf8ToWide(r.formatUsed).c_str(), r.estimatedOutputLatencyMs,
                     r.avgCallbackIntervalMs, r.jitterMs, r.underruns, r.callbacks);
        else
            fwprintf(f, L"  %s buffer %d: FALHOU — %s\n", utf8ToWide(r.mode).c_str(), r.bufferFrames, utf8ToWide(r.error).c_str());
    }
    fwprintf(f, L"\n[LOG MIDI]\n");
    for (const auto& ev : g_midi.eventLog()) {
        const std::wstring line = midiEventLine(ev);
        fputws(line.c_str(), f);
    }
    fclose(f);

    FILE* c = _wfopen(csvPath.c_str(), L"w, ccs=UTF-8");
    if (c) {
        fwprintf(c, L"timestamp_ms,tipo,canal,nota,nota_nome,velocity\n");
        for (const auto& ev : g_midi.eventLog()) {
            const char* type = ev.isNoteOn() ? "Note On" : ev.isNoteOff() ? "Note Off"
                               : ev.isControlChange() ? "CC" : "Outro";
            fwprintf(c, L"%llu,%s,%d,%d,%s,%d\n",
                     (unsigned long long)(ev.timestampUs / 1000), utf8ToWide(type).c_str(),
                     (int)ev.channel() + 1, (int)ev.data1,
                     utf8ToWide(aroma::noteName(ev.data1)).c_str(), (int)ev.data2);
        }
        fclose(c);
    }
    std::wstring msg = L"Relatorio exportado:\n" + txtPath + L"\n" + csvPath;
    MessageBoxW(hwnd, msg.c_str(), L"Exportar", MB_ICONINFORMATION);
    setStatus(L"Relatorio exportado. Envie os arquivos TXT e CSV ao Muse.");
    return true;
}

HWND makeLabel(HWND parent, const wchar_t* text, int x, int y, int w, int h) {
    return CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE,
                           x, y, w, h, parent, nullptr, nullptr, nullptr);
}
HWND makeButton(HWND parent, const wchar_t* text, int id, int x, int y, int w, int h) {
    return CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                           x, y, w, h, parent, (HMENU)(intptr_t)id, nullptr, nullptr);
}
HWND makeList(HWND parent, int id, int x, int y, int w, int h) {
    return CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
                           WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY,
                           x, y, w, h, parent, (HMENU)(intptr_t)id, nullptr, nullptr);
}

LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        g_hWnd = hwnd;
        InitializeCriticalSection(&g_resultsLock);
        makeLabel(hwnd, L"AROMA LOW LATENCY STUDIO — Diagnóstico (Etapa 2)", 12, 8, 600, 20);
        // Sistema
        makeLabel(hwnd, L"Sistema", 12, 32, 200, 16);
        g_hSys = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                 WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY,
                                 12, 50, 460, 86, hwnd, (HMENU)(intptr_t)ID_SYSINFO, nullptr, nullptr);
        const auto sys = aroma::collectSystemInfo();
        wchar_t sysBuf[512];
        swprintf(sysBuf, L"CPU: %s\r\nNúcleos: %d lógicos / %d físicos\r\nRAM: %llu MB total / %llu MB livre\r\nWindows: %s (%s)\r\nComputador: %s",
                   utf8ToWide(sys.cpuBrand).c_str(), sys.logicalCores, sys.physicalCores,
                   sys.ramTotalMB, sys.ramFreeMB, utf8ToWide(sys.windowsVersion).c_str(),
                   utf8ToWide(sys.architecture).c_str(), utf8ToWide(sys.computerName).c_str());
        SetWindowTextW(g_hSys, sysBuf);
        // MIDI
        makeLabel(hwnd, L"Dispositivos MIDI (entrada)", 488, 32, 220, 16);
        g_hMidiList = makeList(hwnd, ID_MIDI_LIST, 488, 50, 480, 86);
        makeButton(hwnd, L"Atualizar MIDI", ID_BTN_MIDI_REFRESH, 488, 142, 130, 26);
        g_hBtnMidiStart = makeButton(hwnd, L"Iniciar monitoramento", ID_BTN_MIDI_START, 626, 142, 180, 26);
        // Log MIDI
        makeLabel(hwnd, L"Eventos MIDI em tempo real (toque nos pads)", 12, 176, 460, 16);
        g_hMidiLog = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                     WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY |
                                     ES_AUTOVSCROLL | WS_VSCROLL,
                                     12, 194, 460, 180, hwnd, (HMENU)(intptr_t)ID_MIDI_LOG, nullptr, nullptr);
        // Contagem por nota
        makeLabel(hwnd, L"Contagem por nota (cada pad/prato)", 488, 176, 300, 16);
        g_hNoteCount = makeList(hwnd, ID_NOTE_COUNT, 488, 194, 480, 180);
        // Áudio
        makeLabel(hwnd, L"Dispositivos de áudio (saída)", 12, 384, 220, 16);
        g_hAudioList = makeList(hwnd, ID_AUDIO_LIST, 12, 402, 460, 76);
        makeButton(hwnd, L"Atualizar áudio", ID_BTN_AUDIO_REFRESH, 12, 484, 130, 26);
        g_hBtnAudioTest = makeButton(hwnd, L"Testar áudio (WASAPI)", ID_BTN_AUDIO_TEST, 150, 484, 180, 26);
        g_hRateInfo = CreateWindowExW(0, L"STATIC", L"Selecione um dispositivo de áudio para ver taxas suportadas.",
                                      WS_CHILD | WS_VISIBLE, 12, 516, 460, 32, hwnd,
                                      (HMENU)(intptr_t)ID_RATE_INFO, nullptr, nullptr);
        // Resultados
        makeLabel(hwnd, L"Resultados WASAPI — latência ESTIMADA (buffer + período)", 488, 384, 460, 16);
        g_hAudioResults = makeList(hwnd, ID_AUDIO_RESULTS, 488, 402, 480, 108);
        // Rodapé
        makeButton(hwnd, L"Exportar relatório TXT/CSV", ID_BTN_EXPORT, 12, 556, 220, 30);
        g_hStatus = CreateWindowExW(0, L"STATIC", L"Pronto.",
                                    WS_CHILD | WS_VISIBLE, 248, 556, 720, 30, hwnd,
                                    (HMENU)(intptr_t)ID_STATUS, nullptr, nullptr);
        refreshMidiDevices();
        refreshAudioDevices();
        SetTimer(hwnd, TIMER_UI, 100, nullptr);
        return 0;
    }
    case WM_TIMER:
        if (g_monitoring) {
            const auto& log = g_midi.eventLog();
            while (g_logShown < log.size()) {
                appendLogLine(midiEventLine(log[g_logShown]));
                ++g_logShown;
            }
            refreshNoteCounts();
        }
        return 0;
    case WM_APP_AUDIO_DONE:
        showAudioResults();
        if (!g_audioTesting) {
            EnableWindow(g_hBtnAudioTest, TRUE);
            setStatus(L"Teste de audio concluido. Exporte o relatorio.");
        }
        return 0;
    case WM_COMMAND: {
        const int id = LOWORD(wp);
        const int code = HIWORD(wp);
        if (id == ID_BTN_MIDI_REFRESH) { refreshMidiDevices(); return 0; }
        if (id == ID_BTN_AUDIO_REFRESH) { refreshAudioDevices(); return 0; }
        if (id == ID_BTN_EXPORT) { exportReport(hwnd); return 0; }
        if (id == ID_MIDI_LIST && code == LBN_SELCHANGE) return 0;
        if (id == ID_AUDIO_LIST && code == LBN_SELCHANGE) {
            g_selectedAudio = (int)SendMessageW(g_hAudioList, LB_GETCURSEL, 0, 0);
            showRateInfo(g_selectedAudio);
            return 0;
        }
        if (id == ID_BTN_MIDI_START) {
            if (!g_monitoring) {
                const int sel = (int)SendMessageW(g_hMidiList, LB_GETCURSEL, 0, 0);
                if (sel < 0 || sel >= (int)g_midiDevices.size()) {
                    MessageBoxW(hwnd, L"Selecione a Aroma na lista de dispositivos MIDI.",
                                L"Monitoramento", MB_ICONWARNING);
                    return 0;
                }
                std::string err;
                if (!g_midi.open(g_midiDevices[sel].index, nullptr, err)) {
                    MessageBoxW(hwnd, utf8ToWide(err).c_str(), L"Monitoramento", MB_ICONERROR);
                    return 0;
                }
                g_monitoring = true;
                g_logShown = 0;
                SetWindowTextW(g_hMidiLog, L"");
                SetWindowTextW(g_hBtnMidiStart, L"Parar monitoramento");
                setStatus(L"Monitorando. Toque em cada pad/prato, um de cada vez.");
            } else {
                g_midi.close();
                g_monitoring = false;
                SetWindowTextW(g_hBtnMidiStart, L"Iniciar monitoramento");
                setStatus(L"Monitoramento parado.");
            }
            return 0;
        }
        if (id == ID_BTN_AUDIO_TEST) {
            if (g_audioTesting) return 0;
            if (g_selectedAudio < 0 || g_selectedAudio >= (int)g_audioDevices.size()) {
                MessageBoxW(hwnd, L"Selecione um dispositivo de audio na lista.",
                            L"Teste de audio", MB_ICONWARNING);
                return 0;
            }
            EnterCriticalSection(&g_resultsLock);
            g_audioResults.clear();
            LeaveCriticalSection(&g_resultsLock);
            SendMessageW(g_hAudioResults, LB_RESETCONTENT, 0, 0);
            g_audioTesting = true;
            EnableWindow(g_hBtnAudioTest, FALSE);
            setStatus(L"Testando WASAPI (10 configuracoes x 2 s). Voce vai ouvir um tom de 440 Hz em cada uma — e o teste que nao tocar nada, falhou.");
            CreateThread(nullptr, 0, audioTestThread,
                         reinterpret_cast<LPVOID>(static_cast<intptr_t>(g_selectedAudio)), 0, nullptr);
            return 0;
        }
        return 0;
    }
    case WM_DESTROY:
        KillTimer(hwnd, TIMER_UI);
        g_midi.close();
        DeleteCriticalSection(&g_resultsLock);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

} // namespace

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int nCmdShow) {
    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icc);
    const wchar_t* kClass = L"AromaDiagnosticoClass";
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = kClass;
    RegisterClassExW(&wc);
    HWND hwnd = CreateWindowExW(0, kClass,
                                L"Aroma Low Latency Studio — Diagnóstico",
                                WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX,
                                CW_USEDEFAULT, CW_USEDEFAULT, 1000, 640,
                                nullptr, nullptr, hInst, nullptr);
    if (!hwnd) return 1;
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}

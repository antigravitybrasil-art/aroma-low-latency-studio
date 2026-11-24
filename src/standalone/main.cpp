// AromaStandalone — Etapa 3.
// Janela Win32 mínima (sem JUCE — a decisão de licença do JUCE está
// pendente e a GUI é a Etapa 5):
//  - abre a Aroma TDX-18S via WinMM (midiInOpen) e entrega Note On/Off
//    ao DrumEngine pela fila lock-free;
//  - abre o WASAPI (Exclusive primeiro, Shared como fallback) e chama
//    DrumEngine::render() no loop de áudio event-driven;
//  - mostra dispositivo, modo, latência estimada, última nota tocada
//    (instrumento + velocity) e contador de underruns.
// A thread de áudio/MIDI nunca toca em controles: a UI atualiza por
// timer (100 ms) e o MIDI só escreve numa struct protegida por mutex.
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>
#include "../common/Kit.h"
#include "../common/MidiTypes.h"
#include "../diagnostic/MidiMonitor.h"
#include "../engine/DrumEngine.h"
#include "AudioStream.h"

namespace {

constexpr int ID_KIT = 101;
constexpr int ID_MIDI = 102;
constexpr int ID_AUDIO = 103;
constexpr int ID_LAST_NOTE = 104;
constexpr int ID_COUNTERS = 105;
constexpr int ID_STATUS = 106;
constexpr int ID_BTN_TOGGLE = 107;
constexpr int ID_BTN_RESTART = 108;
constexpr int ID_HINT = 109;
constexpr UINT TIMER_UI = 1;

constexpr int kPreferredBufferFrames = 256; // 5,3 ms @48k em Exclusive

HWND g_hWnd = nullptr;
HWND g_hKit, g_hMidi, g_hAudio, g_hLastNote, g_hCounters, g_hStatus, g_hBtnToggle;
HFONT g_hFontBig = nullptr;

aroma::DrumEngine g_engine;
aroma::MidiMonitor g_midi;
aroma::AudioStream g_stream;
aroma::Kit g_kit;
bool g_kitLoaded = false;
bool g_audioOn = false;
bool g_invalidatedShown = false;

struct LastNote {
    int note = -1;
    int velocity = 0;
    std::string instrument;
    std::string pad;
};
std::mutex g_lastNoteMutex;
LastNote g_lastNote;
uint64_t g_noteOnCount = 0;

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

std::string exeDir() {
    char buf[MAX_PATH] = {};
    const DWORD n = GetModuleFileNameA(nullptr, buf, sizeof(buf));
    if (n == 0) return ".";
    std::string path(buf, n);
    const size_t p = path.find_last_of("/\\");
    return p == std::string::npos ? "." : path.substr(0, p);
}

bool loadDemoKit() {
    const std::string dir = exeDir();
    const std::vector<std::string> candidates = {
        dir + "\\kits\\demo-kit\\kit.txt",
        dir + "\\..\\kits\\demo-kit\\kit.txt",
        "kits\\demo-kit\\kit.txt",
        "..\\kits\\demo-kit\\kit.txt",
    };
    for (const auto& path : candidates) {
        std::string err;
        if (aroma::loadKit(path, g_kit, err)) {
            g_kitLoaded = g_engine.setKit(g_kit);
            return g_kitLoaded;
        }
    }
    return false;
}

std::string instrumentNameForNote(int note, std::string& padOut) {
    if (g_kitLoaded) {
        if (const auto* inst = g_kit.findByNote(note)) {
            padOut = inst->name;
            return inst->name;
        }
    }
    for (const auto& e : aroma::kDefaultDrumMap) {
        if (e.note == note) {
            padOut = e.pad;
            return e.instrument;
        }
    }
    padOut = "(sem instrumento no mapa)";
    return "(sem instrumento)";
}

// Chamado na thread do WinMM (callback do midiIn). Só toca no motor
// (fila lock-free) e na struct da última nota (mutex) — nunca em controles.
void onMidiEvent(const aroma::MidiEvent& ev) {
    if (ev.isNoteOn()) {
        g_engine.noteOn(ev.data1, ev.data2);
        std::string pad;
        const std::string inst = instrumentNameForNote(ev.data1, pad);
        {
            std::lock_guard<std::mutex> lock(g_lastNoteMutex);
            g_lastNote = LastNote{ev.data1, ev.data2, inst, pad};
            ++g_noteOnCount;
        }
    } else if (ev.isNoteOff()) {
        g_engine.noteOff(ev.data1);
    }
}

bool openMidi() {
    const auto devices = g_midi.enumerateDevices();
    if (devices.empty()) {
        setStatus(L"Nenhum dispositivo MIDI de entrada encontrado. Ligue a Aroma e reinicie o app.");
        return false;
    }
    int chosen = 0;
    for (const auto& d : devices) {
        if (d.looksLikeAroma) { chosen = d.index; break; }
    }
    std::string err;
    if (!g_midi.open(chosen, onMidiEvent, err)) {
        setStatus(utf8ToWide("MIDI: " + err));
        return false;
    }
    wchar_t buf[320];
    swprintf(buf, L"MIDI: %s  —  pronto (canal 10)",
               utf8ToWide(g_midi.deviceName()).c_str());
    SetWindowTextW(g_hMidi, buf);
    return true;
}

void updateAudioLine() {
    wchar_t buf[512];
    swprintf(buf, L"Áudio: %s  |  %s  |  %s  |  buffer %d frames  |  lat. est. %.1f ms",
               utf8ToWide(g_stream.deviceName()).c_str(),
               utf8ToWide(g_stream.mode()).c_str(),
               utf8ToWide(g_stream.formatDescription()).c_str(),
               g_stream.bufferFrames(),
               g_stream.estimatedLatencyMs());
    SetWindowTextW(g_hAudio, buf);
}

bool openAudio() {
    std::string err;
    if (!g_stream.open(kPreferredBufferFrames, err)) {
        setStatus(utf8ToWide("Áudio: " + err));
        return false;
    }
    if (!g_stream.start(g_engine)) {
        setStatus(utf8ToWide("Áudio: " + g_stream.lastError()));
        g_stream.close();
        return false;
    }
    g_audioOn = true;
    g_invalidatedShown = false;
    updateAudioLine();
    SetWindowTextW(g_hBtnToggle, L"Parar áudio");
    std::wstring status = L"Pronto — toque na bateria.";
    if (g_stream.rateMismatch()) {
        wchar_t warn[160];
        swprintf(warn, L" ATENÇÃO: dispositivo a %d Hz, kit a 48 kHz — o som sairá fora do tom.",
                   g_stream.sampleRate());
        status += warn;
    }
    if (g_stream.mode() == "Shared")
        status += L" (Shared: o Exclusive não abriu neste PC — esperado, ver diagnóstico.)";
    setStatus(status);
    return true;
}

void stopAudio() {
    g_stream.stop();
    g_audioOn = false;
    SetWindowTextW(g_hBtnToggle, L"Iniciar áudio");
    setStatus(L"Áudio parado.");
}

void restartAudio() {
    g_stream.close();
    if (!openAudio())
        SetWindowTextW(g_hBtnToggle, L"Iniciar áudio");
}

void refreshUi() {
    // Última nota.
    {
        std::lock_guard<std::mutex> lock(g_lastNoteMutex);
        wchar_t buf[256];
        if (g_lastNote.note < 0) {
            swprintf(buf, L"(nenhuma nota ainda — toque na bateria)");
        } else {
            swprintf(buf, L"%s  —  nota %d (%s)  —  vel %d",
                       utf8ToWide(g_lastNote.instrument).c_str(),
                       g_lastNote.note,
                       utf8ToWide(aroma::noteName(static_cast<uint8_t>(g_lastNote.note))).c_str(),
                       g_lastNote.velocity);
        }
        SetWindowTextW(g_hLastNote, buf);
    }
    // Contadores.
    {
        wchar_t buf[320];
        swprintf(buf, L"Underruns: %llu    Callbacks: %llu    Vozes ativas: %d    Note Ons: %llu%s",
                   (unsigned long long)g_stream.underruns(),
                   (unsigned long long)g_stream.callbacks(),
                   g_engine.activeVoices(),
                   (unsigned long long)g_noteOnCount,
                   g_engine.clipped() ? L"    CLIP!" : L"");
        if (g_engine.clipped()) g_engine.clearClipFlag();
        SetWindowTextW(g_hCounters, buf);
    }
    // Dispositivo desplugado/invalidado: avisa uma vez e oferece reiniciar.
    if (g_stream.deviceInvalidated() && !g_invalidatedShown) {
        g_invalidatedShown = true;
        g_audioOn = false;
        SetWindowTextW(g_hBtnToggle, L"Iniciar áudio");
        setStatus(L"Dispositivo de áudio desplugado ou inválido — reconecte e clique em Reiniciar áudio.");
    }
}

HWND makeStatic(HWND parent, int id, int x, int y, int w, int h) {
    return CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT,
                           x, y, w, h, parent, (HMENU)(INT_PTR)id,
                           GetModuleHandleW(nullptr), nullptr);
}

HWND makeButton(HWND parent, const wchar_t* text, int id, int x, int y, int w, int h) {
    return CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                           x, y, w, h, parent, (HMENU)(INT_PTR)id,
                           GetModuleHandleW(nullptr), nullptr);
}

LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        g_hKit = makeStatic(hwnd, ID_KIT, 12, 12, 536, 20);
        g_hMidi = makeStatic(hwnd, ID_MIDI, 12, 36, 536, 20);
        g_hAudio = makeStatic(hwnd, ID_AUDIO, 12, 60, 536, 40);
        g_hLastNote = makeStatic(hwnd, ID_LAST_NOTE, 12, 108, 536, 32);
        g_hCounters = makeStatic(hwnd, ID_COUNTERS, 12, 148, 536, 20);
        g_hStatus = makeStatic(hwnd, ID_STATUS, 12, 172, 536, 40);
        g_hBtnToggle = makeButton(hwnd, L"Parar áudio", ID_BTN_TOGGLE, 12, 220, 140, 30);
        makeButton(hwnd, L"Reiniciar áudio", ID_BTN_RESTART, 164, 220, 140, 30);
        makeStatic(hwnd, ID_HINT, 12, 262, 536, 20);
        SetWindowTextW(GetDlgItem(hwnd, ID_HINT),
                       L"Toque na bateria — cada pad toca o sample do kit. Feche a janela para sair.");

        g_hFontBig = CreateFontW(-22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        if (g_hFontBig) SendMessageW(g_hLastNote, WM_SETFONT, (WPARAM)g_hFontBig, TRUE);

        if (loadDemoKit()) {
            wchar_t buf[256];
            swprintf(buf, L"Kit: %s  —  %zu instrumentos  —  48 kHz",
                       utf8ToWide(g_kit.name).c_str(), g_kit.instruments.size());
            SetWindowTextW(g_hKit, buf);
        } else {
            SetWindowTextW(g_hKit, L"Kit: NÃO CARREGADO — kits/demo-kit/kit.txt não encontrado.");
        }
        SetWindowTextW(g_hMidi, L"MIDI: procurando a Aroma...");
        SetWindowTextW(g_hAudio, L"Áudio: abrindo...");
        SetWindowTextW(g_hLastNote, L"(nenhuma nota ainda — toque na bateria)");
        SetWindowTextW(g_hCounters, L"Underruns: 0    Callbacks: 0    Vozes ativas: 0    Note Ons: 0");

        openMidi();
        openAudio();
        SetTimer(hwnd, TIMER_UI, 100, nullptr);
        return 0;
    }
    case WM_COMMAND: {
        const int id = LOWORD(wParam);
        if (id == ID_BTN_TOGGLE) {
            if (g_audioOn) {
                stopAudio();
            } else {
                if (!g_stream.isOpen()) {
                    if (!openAudio()) break;
                } else if (!g_stream.start(g_engine)) {
                    setStatus(utf8ToWide("Áudio: " + g_stream.lastError()));
                    break;
                } else {
                    g_audioOn = true;
                    SetWindowTextW(g_hBtnToggle, L"Parar áudio");
                    updateAudioLine();
                    setStatus(L"Pronto — toque na bateria.");
                }
            }
        } else if (id == ID_BTN_RESTART) {
            restartAudio();
        }
        return 0;
    }
    case WM_TIMER:
        if (wParam == TIMER_UI) refreshUi();
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, TIMER_UI);
        g_stream.close();
        g_midi.close();
        if (g_hFontBig) DeleteObject(g_hFontBig);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE /*hPrev*/, PWSTR /*cmd*/, int nCmdShow) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"AromaStandalone";
    if (!RegisterClassExW(&wc)) return 1;

    g_hWnd = CreateWindowExW(0, L"AromaStandalone",
                             L"AROMA LOW LATENCY STUDIO — Standalone v0.3.0",
                             WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME,
                             CW_USEDEFAULT, CW_USEDEFAULT, 576, 320,
                             nullptr, nullptr, hInstance, nullptr);
    if (!g_hWnd) return 1;
    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

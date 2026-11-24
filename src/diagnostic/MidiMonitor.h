#pragma once
// Monitor MIDI via WinMM (midiIn*). Sem dependências externas.
// - Enumera dispositivos de entrada e detecta a Aroma por nome.
// - Abre o dispositivo escolhido e entrega eventos por callback de thread
//   do WinMM; a UI só recebe os eventos via PostMessage (nunca toca em
//   controles dentro do callback).
// - Mantém contagem por nota (para identificar cada pad/prato) e o log
//   completo para exportar em CSV.
#include <windows.h>
#include <mmsystem.h>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include <array>
#include "../common/MidiTypes.h"

namespace aroma {

struct MidiDeviceInfo {
    int index = -1;
    std::string name;
    bool looksLikeAroma = false; // nome contém AROMA / TDX
};

class MidiMonitor {
public:
    using EventCallback = std::function<void(const MidiEvent&)>;

    std::vector<MidiDeviceInfo> enumerateDevices();
    bool open(int deviceIndex, EventCallback cb, std::string& error);
    void close();
    bool isOpen() const { return hMidi_ != nullptr; }
    std::string deviceName() const { return deviceName_; }

    // Estatísticas (lidas pela UI).
    uint64_t totalEvents() const { return totalEvents_; }
    uint64_t noteOnCount() const { return noteOnCount_; }
    const std::array<uint64_t, 128>& perNoteCount() const { return perNoteCount_; }
    const std::vector<MidiEvent>& eventLog() const { return eventLog_; }
    void clearLog();

private:
    static void CALLBACK midiCallback(HMIDIIN hMidi, UINT msg, DWORD_PTR instance,
                                      DWORD_PTR param1, DWORD_PTR param2);
    void onMidiMessage(DWORD_PTR param1, DWORD_PTR param2);

    HMIDIIN hMidi_ = nullptr;
    std::string deviceName_;
    EventCallback callback_;
    uint64_t startTickMs_ = 0;
    uint64_t totalEvents_ = 0;
    uint64_t noteOnCount_ = 0;
    std::array<uint64_t, 128> perNoteCount_{};
    std::vector<MidiEvent> eventLog_;
};

bool nameLooksLikeAroma(const std::string& name);

} // namespace aroma

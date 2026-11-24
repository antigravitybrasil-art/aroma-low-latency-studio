#include "MidiMonitor.h"
#include <algorithm>
#include <cctype>
#include <cstring>

namespace aroma {

bool nameLooksLikeAroma(const std::string& name) {
    std::string upper = name;
    std::transform(upper.begin(), upper.end(), upper.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return upper.find("AROMA") != std::string::npos ||
           upper.find("TDX") != std::string::npos;
}

std::vector<MidiDeviceInfo> MidiMonitor::enumerateDevices() {
    std::vector<MidiDeviceInfo> out;
    const UINT count = midiInGetNumDevs();
    for (UINT i = 0; i < count; ++i) {
        MIDIINCAPSA caps{};
        if (midiInGetDevCapsA(i, &caps, sizeof(caps)) != MMSYSERR_NOERROR) continue;
        MidiDeviceInfo info;
        info.index = static_cast<int>(i);
        // szPname tem 32 chars e o driver nem sempre termina com NUL:
        // copia limitada para não ler lixo da memória vizinha (o nome
        // "DRUM.VBS;.VBE;..." do relatório de 23/11 era esse vazamento).
        info.name.assign(caps.szPname,
                         strnlen(caps.szPname, sizeof(caps.szPname)));
        info.looksLikeAroma = nameLooksLikeAroma(info.name);
        out.push_back(info);
    }
    return out;
}

bool MidiMonitor::open(int deviceIndex, EventCallback cb, std::string& error) {
    close();
    callback_ = std::move(cb);
    const MMRESULT r = midiInOpen(&hMidi_, static_cast<UINT>(deviceIndex),
                                  reinterpret_cast<DWORD_PTR>(&MidiMonitor::midiCallback),
                                  reinterpret_cast<DWORD_PTR>(this),
                                  CALLBACK_FUNCTION);
    if (r != MMSYSERR_NOERROR) {
        char buf[128];
        midiInGetErrorTextA(r, buf, sizeof(buf));
        error = std::string("midiInOpen falhou: ") + buf;
        hMidi_ = nullptr;
        return false;
    }
    MIDIINCAPSA caps{};
    if (midiInGetDevCapsA(static_cast<UINT>(deviceIndex), &caps, sizeof(caps)) == MMSYSERR_NOERROR)
        deviceName_.assign(caps.szPname,
                           strnlen(caps.szPname, sizeof(caps.szPname)));
    startTickMs_ = timeGetTime();
    totalEvents_ = 0;
    noteOnCount_ = 0;
    perNoteCount_.fill(0);
    eventLog_.clear();
    if (midiInStart(hMidi_) != MMSYSERR_NOERROR) {
        error = "midiInStart falhou";
        midiInClose(hMidi_);
        hMidi_ = nullptr;
        return false;
    }
    return true;
}

void MidiMonitor::close() {
    if (hMidi_) {
        midiInStop(hMidi_);
        midiInClose(hMidi_);
        hMidi_ = nullptr;
    }
}

void MidiMonitor::clearLog() {
    eventLog_.clear();
    totalEvents_ = 0;
    noteOnCount_ = 0;
    perNoteCount_.fill(0);
}

void CALLBACK MidiMonitor::midiCallback(HMIDIIN /*hMidi*/, UINT msg,
                                        DWORD_PTR instance, DWORD_PTR param1,
                                        DWORD_PTR param2) {
    if (msg != MIM_DATA) return; // ignora MIM_OPEN/CLOSE/ERROR aqui
    auto* self = reinterpret_cast<MidiMonitor*>(instance);
    if (self) self->onMidiMessage(param1, param2);
}

void MidiMonitor::onMidiMessage(DWORD_PTR param1, DWORD_PTR /*param2*/) {
    // param1: byte0 = status, byte1 = data1, byte2 = data2 (mensagem curta).
    const uint8_t status = static_cast<uint8_t>(param1 & 0xFF);
    const uint8_t data1 = static_cast<uint8_t>((param1 >> 8) & 0xFF);
    const uint8_t data2 = static_cast<uint8_t>((param1 >> 16) & 0xFF);
    MidiEvent ev;
    ev.timestampUs = static_cast<uint64_t>(timeGetTime() - startTickMs_) * 1000ULL;
    ev.status = status;
    ev.data1 = data1;
    ev.data2 = data2;
    ++totalEvents_;
    if (ev.isNoteOn()) {
        ++noteOnCount_;
        if (data1 < 128) ++perNoteCount_[data1];
    }
    eventLog_.push_back(ev);
    if (callback_) callback_(ev);
}

} // namespace aroma

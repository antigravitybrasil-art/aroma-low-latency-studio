#pragma once
// Tipos MIDI compartilhados pelo diagnóstico, motor, standalone e VST3.
// Sem dependência de Windows aqui: o motor é portátil.
#include <cstdint>
#include <string>

namespace aroma {

// Um evento MIDI de 3 bytes (o caso de Note On/Off/CC; SysEx não é usado
// pelo caminho de bateria e é descartado pelo monitor).
struct MidiEvent {
    uint64_t timestampUs = 0; // microssegundos desde o início do monitor
    uint8_t status = 0;       // byte de status (0x90 = Note On canal 0, ...)
    uint8_t data1 = 0;        // nota (0-127) ou nº do CC
    uint8_t data2 = 0;        // velocity (0-127) ou valor do CC

    uint8_t type() const { return status & 0xF0; }
    uint8_t channel() const { return status & 0x0F; }
    bool isNoteOn() const { return type() == 0x90 && data2 > 0; }
    // Note On com velocity 0 é Note Off, por convenção MIDI.
    bool isNoteOff() const { return type() == 0x80 || (type() == 0x90 && data2 == 0); }
    bool isControlChange() const { return type() == 0xB0; }
};

// Nome de nota estilo "C1", "F#1" (convenção C4 = 60).
inline std::string noteName(uint8_t note) {
    static const char* names[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    int octave = static_cast<int>(note) / 12 - 1;
    return std::string(names[note % 12]) + std::to_string(octave);
}

// Mapa REAL da Aroma TDX-18S, confirmado no diagnóstico de 23/11/2026
// (relatórios 18:41 e 18:55, canal 10). Os 3 toms são 50/47/43 —
// a hipótese GM (48/45/41) estava errada. Aro = 37 (velocity fixa 78)
// e bell do ride = 59 (não 53; só dispara em batidas fortes, 92–106).
struct DrumMapEntry {
    const char* instrument; // nome interno do kit
    const char* pad;        // pad/prato físico
    uint8_t note;           // nota real da TDX-18S
};

inline constexpr DrumMapEntry kDefaultDrumMap[] = {
    {"kick",         "Bumbo",              36},
    {"snare",        "Caixa (centro)",     38},
    {"rim",          "Aro da caixa",       37},
    {"tom1",         "Tom 1 (agudo)",      50},
    {"tom2",         "Tom 2 (médio)",      47},
    {"floor_tom",    "Tom de chão",        43},
    {"hihat_closed", "Chimbal fechado",    42},
    {"hihat_open",   "Chimbal aberto",     46},
    {"hihat_pedal",  "Pedal do chimbal",   44},
    {"crash",        "Crash",              49},
    {"ride",         "Ride (corpo)",       51},
    {"ride_bell",    "Ride (bell)",        59},
};

} // namespace aroma

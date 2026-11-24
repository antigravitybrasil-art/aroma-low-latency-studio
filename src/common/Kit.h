#pragma once
// Kit de bateria: descrição em texto simples (kit.txt) + samples WAV.
// Formato v1 (uma linha por instrumento; linhas com '#' são comentários):
//   nome  nota  arquivo  volume  pan  choke_group  gamma
//   kick  36    kick.wav  1.0     0.0   none         1.0
// Formato v2: os 7 campos acima, seguidos de tokens opcionais
// chave=valor (ausentes = padrão; kit v1 carrega igual):
//   tune=0  eq_low=0  eq_mid=0  eq_mid_freq=1000  eq_high=0
//   soft=kick_soft.wav  hard=kick_hard.wav
//   kick 36 kick.wav 1.0 0.0 none 1.0 tune=2 eq_low=3 eq_mid=-2 eq_mid_freq=800 eq_high=1
// Variações round-robin: kick_2.wav, kick_3.wav na mesma pasta.
// Camadas por velocity: kick_soft.wav (vel < 64), kick_hard.wav (vel >= 64)
// — detectadas na pasta, ou indicadas por soft=/hard=.
// O motor trabalha sempre a 48 kHz: todo WAV carregado é reamostrado
// para kKitSampleRate no load (interpolação linear).
// O carregamento de arquivos acontece fora da thread de áudio.
#include <string>
#include <vector>
#include "WavFile.h"

namespace aroma {

// Taxa do motor. Todo sample de kit vira 48 kHz no carregamento.
inline constexpr int kKitSampleRate = 48000;

struct KitInstrument {
    std::string name;
    int note = -1;                 // nota MIDI que dispara
    std::string file;              // arquivo base (ex.: snare.wav)
    float volume = 1.0f;           // 0..2
    float pan = 0.0f;              // -1 (esq) .. +1 (dir)
    std::string chokeGroup = "none"; // "hihat", "crash", ... ou "none"
    float gamma = 1.0f;            // curva de velocity: v' = v^gamma
    float tune = 0.0f;             // afinação em semitons (-24..+24)
    float eqLowDb = 0.0f;          // EQ: grave, dB (-18..+18)
    float eqMidDb = 0.0f;          // EQ: médio, dB (-18..+18)
    float eqMidFreq = 1000.0f;     // EQ: freq do médio, Hz (50..16000)
    float eqHighDb = 0.0f;         // EQ: agudo, dB (-18..+18)
    std::string softFile;          // arquivo da camada soft ("" = auto-detectar)
    std::string hardFile;          // arquivo da camada hard ("" = auto-detectar)

    std::vector<WavFile> variations; // round-robin (índice 0 = arquivo base)
    WavFile softLayer;               // opcional, velocity < 64
    WavFile hardLayer;               // opcional, velocity >= 64
    bool hasLayers = false;
    size_t nextVariation = 0;
};

struct Kit {
    std::string name = "Sem nome";
    std::string path; // pasta do kit
    std::vector<KitInstrument> instruments;

    const KitInstrument* findByNote(int note) const;
    KitInstrument* findByNote(int note);
};

// Carrega um kit (v1 ou v2) por caminho. Erro claro em `error`, sem travar.
bool loadKit(const std::string& kitFilePath, Kit& out, std::string& error);

// Salva o kit no formato v2: escreve kit.txt + copia (ou reescreve,
// quando o sample foi reamostrado) os WAVs para <pasta>/samples/.
// Cria as pastas se preciso. Erro claro em `error`, sem travar.
bool saveKit(const Kit& kit, const std::string& kitFilePath, std::string& error);

} // namespace aroma

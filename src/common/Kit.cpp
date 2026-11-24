#include "Kit.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

namespace aroma {
namespace {

std::string trim(const std::string& s) {
    const size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    const size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

std::string parentDir(const std::string& path) {
    const size_t p = path.find_last_of("/\\");
    return p == std::string::npos ? "." : path.substr(0, p);
}

std::string joinPath(const std::string& dir, const std::string& file) {
    if (dir.empty() || dir == ".") return file;
    char last = dir.back();
    if (last == '/' || last == '\\') return dir + file;
    return dir + "/" + file;
}

std::string stemOf(const std::string& file) {
    const size_t dot = file.find_last_of('.');
    return dot == std::string::npos ? file : file.substr(0, dot);
}

bool fileExists(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    return f.good();
}

// Cria a pasta e os pais que faltarem (ignora "já existe"; a escrita
// posterior falha com erro claro se a pasta de verdade não existir).
void makeDirs(const std::string& path) {
    std::string cur;
    auto mkdirOne = [](const std::string& d) {
        if (d.empty()) return;
#ifdef _WIN32
        _mkdir(d.c_str());
#else
        mkdir(d.c_str(), 0755);
#endif
    };
    for (char c : path) {
        if (c == '/' || c == '\\') mkdirOne(cur);
        cur += c;
    }
    mkdirOne(cur);
}

bool copyFile(const std::string& from, const std::string& to, std::string& error) {
    if (from == to) return true; // salvar no mesmo lugar: nada a copiar
    std::ifstream in(from, std::ios::binary);
    if (!in) { error = "nao foi possivel ler: " + from; return false; }
    std::ofstream out(to, std::ios::binary | std::ios::trunc);
    if (!out) { error = "nao foi possivel escrever: " + to; return false; }
    out << in.rdbuf();
    if (!out.good()) { error = "falha ao copiar para: " + to; return false; }
    return true;
}

// Exporta um WAV do kit para o destino: copia o arquivo original
// quando dá (preserva 24 bits); senão reescreve em 16 bits/48 kHz
// a partir dos samples em memória (caso reamostrado ou da memória).
bool exportWav(const WavFile& wav, const std::string& dst, std::string& error) {
    if (!wav.loaded()) { error = "sample vazio ao salvar: " + dst; return false; }
    if (!wav.wasResampled() && !wav.sourcePath().empty() && fileExists(wav.sourcePath()))
        return copyFile(wav.sourcePath(), dst, error);
    return wav.writeWav16(dst, error);
}

float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

std::string fmt2(float v) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2) << v;
    return ss.str();
}

// Resolve um arquivo de sample: samples/<f> primeiro, depois <f> na
// raiz do kit. Devolve "" se não existir em nenhum lugar.
std::string resolveSample(const std::string& kitDir, const std::string& file) {
    const std::string inSamples = joinPath(kitDir, "samples/" + file);
    if (fileExists(inSamples)) return inSamples;
    const std::string inRoot = joinPath(kitDir, file);
    if (fileExists(inRoot)) return inRoot;
    return "";
}

} // namespace

const KitInstrument* Kit::findByNote(int note) const {
    for (const auto& inst : instruments)
        if (inst.note == note) return &inst;
    return nullptr;
}

KitInstrument* Kit::findByNote(int note) {
    for (auto& inst : instruments)
        if (inst.note == note) return &inst;
    return nullptr;
}

bool loadKit(const std::string& kitFilePath, Kit& out, std::string& error) {
    std::ifstream f(kitFilePath);
    if (!f) { error = "nao foi possivel abrir o kit: " + kitFilePath; return false; }
    out = Kit();
    out.path = parentDir(kitFilePath);
    std::string line;
    int lineNo = 0;
    while (std::getline(f, line)) {
        ++lineNo;
        line = trim(line);
        if (line.empty() || line[0] == '#') {
            if (line.rfind("# name:", 0) == 0) out.name = trim(line.substr(7));
            continue;
        }
        std::istringstream ss(line);
        KitInstrument inst;
        std::string noteStr;
        if (!(ss >> inst.name >> noteStr >> inst.file)) {
            error = "kit linha " + std::to_string(lineNo) + ": formato invalido";
            return false;
        }
        try {
            inst.note = std::stoi(noteStr);
        } catch (...) {
            error = "kit linha " + std::to_string(lineNo) + ": nota invalida";
            return false;
        }
        // Campos posicionais v1, opcionais, com padrão sensato.
        float volume = 1.0f, pan = 0.0f, gamma = 1.0f;
        std::string choke = "none";
        ss >> volume >> pan >> choke >> gamma;
        inst.volume = clampf(volume, 0.0f, 2.0f);
        inst.pan = clampf(pan, -1.0f, 1.0f);
        inst.chokeGroup = choke;
        inst.gamma = gamma > 0.0f ? gamma : 1.0f;
        // Tokens v2 chave=valor (tune, eq_*, soft, hard). Chave
        // desconhecida é ignorada (compatibilidade futura).
        std::string token;
        while (ss >> token) {
            const size_t eq = token.find('=');
            if (eq == std::string::npos) continue;
            const std::string key = token.substr(0, eq);
            const std::string val = token.substr(eq + 1);
            try {
                if (key == "tune") inst.tune = std::stof(val);
                else if (key == "eq_low") inst.eqLowDb = std::stof(val);
                else if (key == "eq_mid") inst.eqMidDb = std::stof(val);
                else if (key == "eq_mid_freq") inst.eqMidFreq = std::stof(val);
                else if (key == "eq_high") inst.eqHighDb = std::stof(val);
                else if (key == "soft") inst.softFile = val;
                else if (key == "hard") inst.hardFile = val;
            } catch (...) {
                // Valor inválido: mantém o padrão.
            }
        }
        inst.tune = clampf(inst.tune, -24.0f, 24.0f);
        inst.eqLowDb = clampf(inst.eqLowDb, -18.0f, 18.0f);
        inst.eqMidDb = clampf(inst.eqMidDb, -18.0f, 18.0f);
        inst.eqMidFreq = clampf(inst.eqMidFreq, 50.0f, 16000.0f);
        inst.eqHighDb = clampf(inst.eqHighDb, -18.0f, 18.0f);
        if (inst.note < 0 || inst.note > 127) {
            error = "kit linha " + std::to_string(lineNo) + ": nota fora de 0-127";
            return false;
        }
        // Carrega um WAV e reamostra para a taxa do motor, se preciso.
        auto loadResampled = [&](const std::string& path, WavFile& wav, std::string& err) {
            if (!wav.load(path, err)) return false;
            if (wav.sampleRate() != kKitSampleRate && !wav.resampleTo(kKitSampleRate)) {
                err = "falha ao reamostrar para 48 kHz: " + path;
                return false;
            }
            return true;
        };
        // Arquivo base (samples/arquivo.wav ou arquivo.wav na raiz).
        std::string err;
        WavFile base;
        const std::string basePath = resolveSample(out.path, inst.file);
        if (basePath.empty()) {
            error = "kit linha " + std::to_string(lineNo) + " (" + inst.name +
                    "): sample nao encontrado: " + inst.file;
            return false;
        }
        if (!loadResampled(basePath, base, err)) {
            error = "kit linha " + std::to_string(lineNo) + " (" + inst.name + "): " + err;
            return false;
        }
        inst.variations.push_back(std::move(base));
        // Variações round-robin: nome_2.wav, nome_3.wav, ...
        const std::string stem = stemOf(inst.file);
        for (int v = 2; v <= 4; ++v) {
            const std::string varName = stem + "_" + std::to_string(v) + ".wav";
            const std::string varPath = resolveSample(out.path, varName);
            if (varPath.empty()) break;
            WavFile var;
            if (!loadResampled(varPath, var, err)) break;
            inst.variations.push_back(std::move(var));
        }
        // Camadas por velocity: explícitas (soft=/hard=) ou auto-detectadas
        // (nome_soft.wav / nome_hard.wav). Precisa das DUAS; se só uma
        // explícita existir, é erro (o usuário pediu e o arquivo falta).
        std::string softName = inst.softFile.empty() ? stem + "_soft.wav" : inst.softFile;
        std::string hardName = inst.hardFile.empty() ? stem + "_hard.wav" : inst.hardFile;
        const std::string softPath = resolveSample(out.path, softName);
        const std::string hardPath = resolveSample(out.path, hardName);
        if (!inst.softFile.empty() || !inst.hardFile.empty()) {
            if (softPath.empty() || hardPath.empty()) {
                error = "kit linha " + std::to_string(lineNo) + " (" + inst.name +
                        "): camada soft/hard nao encontrada (" + softName + " / " + hardName + ")";
                return false;
            }
        }
        if (!softPath.empty() && !hardPath.empty()) {
            WavFile soft, hard;
            if (loadResampled(softPath, soft, err) && loadResampled(hardPath, hard, err)) {
                inst.softFile = softName;
                inst.hardFile = hardName;
                inst.softLayer = std::move(soft);
                inst.hardLayer = std::move(hard);
                inst.hasLayers = true;
            }
        }
        out.instruments.push_back(std::move(inst));
    }
    if (out.instruments.empty()) { error = "kit vazio: " + kitFilePath; return false; }
    if (out.name == "Sem nome") out.name = out.path;
    return true;
}

bool saveKit(const Kit& kit, const std::string& kitFilePath, std::string& error) {
    if (kit.instruments.empty()) { error = "kit vazio, nada para salvar"; return false; }
    const std::string dir = parentDir(kitFilePath);
    const std::string samplesDir = joinPath(dir, "samples");
    makeDirs(samplesDir);
    std::string body;
    body += "# kit-format: 2\n";
    body += "# name: " + kit.name + "\n";
    body += "# nome  nota  arquivo  volume  pan  choke  gamma  tune  eq_low  eq_mid  eq_mid_freq  eq_high  [soft=... hard=...]\n";
    for (const auto& inst : kit.instruments) {
        if (inst.note < 0 || inst.note > 127) {
            error = "nota invalida ao salvar (" + inst.name + ")";
            return false;
        }
        const std::string baseName = inst.file.empty() ? inst.name + ".wav" : inst.file;
        // Exporta base + variações.
        for (size_t i = 0; i < inst.variations.size(); ++i) {
            std::string varName = inst.variations[i].fileName();
            if (varName.empty())
                varName = (i == 0) ? baseName : inst.name + "_" + std::to_string(i + 1) + ".wav";
            if (!exportWav(inst.variations[i], joinPath(samplesDir, varName), error))
                return false;
        }
        std::string softName, hardName;
        if (inst.hasLayers) {
            softName = !inst.softFile.empty() ? inst.softFile
                : (!inst.softLayer.fileName().empty() ? inst.softLayer.fileName()
                                                      : inst.name + "_soft.wav");
            hardName = !inst.hardFile.empty() ? inst.hardFile
                : (!inst.hardLayer.fileName().empty() ? inst.hardLayer.fileName()
                                                      : inst.name + "_hard.wav");
            if (!exportWav(inst.softLayer, joinPath(samplesDir, softName), error))
                return false;
            if (!exportWav(inst.hardLayer, joinPath(samplesDir, hardName), error))
                return false;
        }
        std::ostringstream ln;
        ln << inst.name << ' ' << inst.note << ' ' << baseName
           << ' ' << fmt2(inst.volume) << ' ' << fmt2(inst.pan) << ' ' << inst.chokeGroup
           << ' ' << fmt2(inst.gamma)
           << " tune=" << fmt2(inst.tune)
           << " eq_low=" << fmt2(inst.eqLowDb)
           << " eq_mid=" << fmt2(inst.eqMidDb)
           << " eq_mid_freq=" << fmt2(inst.eqMidFreq)
           << " eq_high=" << fmt2(inst.eqHighDb);
        if (inst.hasLayers) ln << " soft=" << softName << " hard=" << hardName;
        ln << '\n';
        body += ln.str();
    }
    std::ofstream out(kitFilePath, std::ios::binary | std::ios::trunc);
    if (!out) { error = "nao foi possivel escrever o kit: " + kitFilePath; return false; }
    out << body;
    if (!out.good()) { error = "falha ao escrever o kit: " + kitFilePath; return false; }
    return true;
}

} // namespace aroma

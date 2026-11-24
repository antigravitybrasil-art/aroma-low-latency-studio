#include "AppModel.h"
#include <algorithm>
#include <filesystem>
#include <fstream>

namespace aroma {
namespace {

std::string sanitizeKitName(const std::string& name) {
    std::string out;
    for (char c : name) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == ' ' || c == '-' || c == '_')
            out += c;
    }
    // Tira espaços das pontas.
    const size_t first = out.find_first_not_of(' ');
    if (first == std::string::npos) return "";
    const size_t last = out.find_last_not_of(' ');
    return out.substr(first, last - first + 1);
}

std::string parentDirOf(const std::string& path) {
    const size_t p = path.find_last_of("/\\");
    return p == std::string::npos ? std::string() : path.substr(0, p);
}

std::string joinPath(const std::string& dir, const std::string& file) {
    if (dir.empty()) return file;
    const char last = dir.back();
    if (last == '/' || last == '\\') return dir + file;
    return dir + "/" + file;
}

} // namespace

bool AppModel::loadKit(const std::string& kitFilePath, std::string& error) {
    if (!engine_.loadKitFromFile(kitFilePath, error)) return false;
    kitPath_ = kitFilePath;
    dirty_ = false;
    const Kit& kit = engine_.kit();
    selectedNote_ = !kit.instruments.empty() ? kit.instruments[0].note : 36;
    return true;
}

bool AppModel::saveKit(std::string& error) {
    if (kitPath_.empty()) { error = "kit sem caminho — use Salvar como"; return false; }
    if (!engine_.saveKitToFile(kitPath_, error)) return false;
    dirty_ = false;
    return true;
}

bool AppModel::saveKitAs(const std::string& kitFilePath, std::string& error) {
    if (!engine_.saveKitToFile(kitFilePath, error)) return false;
    kitPath_ = kitFilePath;
    dirty_ = false;
    return true;
}

bool AppModel::newKit(const std::string& name, const std::string& kitsDir,
                      std::string& error) {
    const std::string clean = sanitizeKitName(name);
    if (clean.empty()) { error = "nome de kit invalido"; return false; }
    const std::string targetDir = joinPath(kitsDir, clean);
    const std::string targetKit = joinPath(targetDir, "kit.txt");
    std::error_code ec;
    if (std::filesystem::exists(targetDir, ec)) {
        error = "ja existe um kit com esse nome: " + clean;
        return false;
    }
    // Kit novo = estrutura do demo (12 peças com os samples do demo).
    // Um kit sem samples não recarrega (loadKit exige o WAV), então
    // copiar o demo é o único "novo" honesto — o usuário troca os
    // samples depois, peça por peça.
    const std::string demoKit = joinPath(joinPath(kitsDir, "demo-kit"), "kit.txt");
    Kit kit;
    if (!::aroma::loadKit(demoKit, kit, error)) {
        error = "kit demo nao encontrado para copiar (" + demoKit + "): " + error;
        return false;
    }
    kit.name = clean;
    if (!engine_.setKit(kit)) { error = "kit demo vazio"; return false; }
    if (!saveKitAs(targetKit, error)) return false;
    selectedNote_ = 36;
    return true;
}

bool AppModel::duplicateKit(const std::string& newName, std::string& error) {
    if (kitPath_.empty()) { error = "nenhum kit carregado para duplicar"; return false; }
    const std::string clean = sanitizeKitName(newName);
    if (clean.empty()) { error = "nome de kit invalido"; return false; }
    const std::string dir = parentDirOf(parentDirOf(kitPath_));
    const std::string targetKit = joinPath(joinPath(dir, clean), "kit.txt");
    std::error_code ec;
    if (std::filesystem::exists(joinPath(dir, clean), ec)) {
        error = "ja existe um kit com esse nome: " + clean;
        return false;
    }
    // A cópia existe no disco, mas o usuário continua editando o
    // original: saveKitAs troca o caminho, então restaura.
    const std::string current = kitPath_;
    if (!saveKitAs(targetKit, error)) return false;
    kitPath_ = current;
    return true;
}

std::vector<std::string> AppModel::listKits(const std::string& kitsDir) {
    std::vector<std::string> names;
    std::error_code ec;
    const std::filesystem::path dir(kitsDir);
    if (!std::filesystem::is_directory(dir, ec)) return names;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (!entry.is_directory(ec)) continue;
        if (std::filesystem::exists(entry.path() / "kit.txt", ec))
            names.push_back(entry.path().filename().string());
    }
    std::sort(names.begin(), names.end());
    return names;
}

bool AppModel::selectInstrument(int note) {
    if (!engine_.kit().findByNote(note)) return false;
    selectedNote_ = note;
    return true;
}

bool AppModel::selectedParams(InstrumentParams& out) const {
    return engine_.instrumentParams(selectedNote_, out);
}

bool AppModel::updateSelectedParams(const InstrumentParams& params) {
    if (!engine_.updateInstrument(selectedNote_, params)) return false;
    dirty_ = true;
    return true;
}

bool AppModel::uploadSample(int note, const std::string& wavPath, std::string& error) {
    if (!engine_.replaceInstrumentSample(note, wavPath, error)) return false;
    dirty_ = true;
    return true;
}

void AppModel::noteOn(int note, int velocity, uint64_t tickMs) {
    engine_.noteOn(note, velocity);
    lastHitNote_.store(note, std::memory_order_relaxed);
    lastHitVelocity_.store(velocity, std::memory_order_relaxed);
    lastHitTickMs_.store(tickMs, std::memory_order_relaxed);
    noteOnCount_.fetch_add(1, std::memory_order_relaxed);
}

LastHit AppModel::lastHit() const {
    LastHit h;
    h.note = lastHitNote_.load(std::memory_order_relaxed);
    h.velocity = lastHitVelocity_.load(std::memory_order_relaxed);
    h.tickMs = lastHitTickMs_.load(std::memory_order_relaxed);
    return h;
}

} // namespace aroma

// Testes de kit v2: reamostrador, parsing, salvar/carregar (round-trip),
// camadas por velocity via arquivo e API de arquivo do motor.
// Usa arquivos reais em pasta temporária (fora do repo, em /tmp).
#include <cstdio>
#include <cmath>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>
#include "../src/common/Kit.h"
#include "../src/engine/DrumEngine.h"

#ifdef _WIN32
#include <direct.h>
#define TEST_TMP ".\\test_tmp_kit\\"
#else
#include <sys/stat.h>
#include <sys/types.h>
#define TEST_TMP "/tmp/aroma_kit_test/"
#endif

namespace {

int failures = 0;
void check(bool ok, const char* name) {
    std::printf("  [%s] %s\n", ok ? "OK" : "FALHOU", name);
    if (!ok) ++failures;
}

void makeDir(const std::string& d) {
#ifdef _WIN32
    _mkdir(d.c_str());
#else
    mkdir(d.c_str(), 0755);
#endif
}

bool writeFile(const std::string& path, const std::string& content) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f << content;
    return f.good();
}

// WAV PCM 16 bits mono a partir de valores float.
std::vector<uint8_t> makeWavBytes(int rate, const std::vector<float>& vals) {
    std::vector<uint8_t> b;
    auto u16 = [&](uint16_t v) { b.push_back(v & 0xFF); b.push_back((v >> 8) & 0xFF); };
    auto u32 = [&](uint32_t v) { for (int i = 0; i < 4; ++i) b.push_back((v >> (8 * i)) & 0xFF); };
    auto put = [&](const char* s, size_t n) { b.insert(b.end(), s, s + n); };
    const int frames = static_cast<int>(vals.size());
    put("RIFF", 4); u32(36 + frames * 2); put("WAVE", 4);
    put("fmt ", 4); u32(16); u16(1); u16(1); u32(rate); u32(rate * 2); u16(2); u16(16);
    put("data", 4); u32(frames * 2);
    for (float v : vals) {
        const int16_t s = static_cast<int16_t>(v * 32767);
        u16(static_cast<uint16_t>(s));
    }
    return b;
}

bool writeWavFile(const std::string& path, int rate, const std::vector<float>& vals) {
    const auto bytes = makeWavBytes(rate, vals);
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    return f.good();
}

float maxAbs(const std::vector<float>& buf) {
    float m = 0;
    for (float s : buf) { float a = s < 0 ? -s : s; if (a > m) m = a; }
    return m;
}

} // namespace

int testKit() {
    std::printf("test_kit:\n");
    const std::string tmp = TEST_TMP;
    makeDir(tmp);

    // ---- Reamostrador ----
    {
        aroma::WavFile w;
        std::string err;
        const auto bytes = makeWavBytes(44100, std::vector<float>(441, 0.5f));
        check(w.loadFromMemory(bytes.data(), bytes.size(), err), "carrega WAV 44.1k");
        check(w.sampleRate() == 44100, "taxa original 44100");
        check(w.resampleTo(48000), "reamostra 44.1k -> 48k");
        check(w.sampleRate() == 48000, "taxa virou 48000");
        check(w.frames() == 480, "441 frames viram 480");
        check(w.data()[0] > 0.49f && w.data()[0] < 0.51f, "primeira amostra preservada");
        check(w.data()[479] > 0.49f && w.data()[479] < 0.51f, "ultima amostra preservada");
        check(w.wasResampled(), "marca reamostrado");
    }
    {
        aroma::WavFile w;
        std::string err;
        const auto bytes = makeWavBytes(96000, std::vector<float>(960, 0.25f));
        check(w.loadFromMemory(bytes.data(), bytes.size(), err), "carrega WAV 96k");
        check(w.resampleTo(48000), "reamostra 96k -> 48k");
        check(w.frames() == 480, "960 frames viram 480 (downsample)");
        check(w.resampleTo(48000), "reamostrar para a mesma taxa = no-op");
        check(w.frames() == 480, "no-op nao muda os frames");
        aroma::WavFile empty;
        check(!empty.resampleTo(48000), "reamostrar vazio falha");
    }
    // Rampa: interpolação linear preserva os valores intermediários.
    {
        std::vector<float> ramp(441);
        for (int i = 0; i < 441; ++i) ramp[i] = i / 440.0f;
        aroma::WavFile w;
        std::string err;
        const auto bytes = makeWavBytes(44100, ramp);
        w.loadFromMemory(bytes.data(), bytes.size(), err);
        w.resampleTo(48000);
        // Amostra 240 de 480 ~= posição 220.5 de 441 -> ~0.5.
        check(w.data()[240] > 0.48f && w.data()[240] < 0.52f, "rampa interpolada no meio");
    }

    // ---- Kit v2: parsing de tune/EQ/camadas explícitas ----
    {
        const std::string dir = tmp + "v2/";
        makeDir(dir);
        makeDir(dir + "samples/");
        check(writeWavFile(dir + "samples/kick.wav", 48000, std::vector<float>(480, 0.5f)),
              "escreve kick.wav de teste");
        check(writeWavFile(dir + "samples/kick_2.wav", 48000, std::vector<float>(480, 0.4f)),
              "escreve kick_2.wav de teste");
        check(writeWavFile(dir + "samples/kick_soft.wav", 48000, std::vector<float>(480, 0.25f)),
              "escreve kick_soft.wav de teste");
        check(writeWavFile(dir + "samples/kick_hard.wav", 48000, std::vector<float>(480, 0.9f)),
              "escreve kick_hard.wav de teste");
        check(writeFile(dir + "kit.txt",
                        "# kit-format: 2\n"
                        "# name: Teste v2\n"
                        "kick 36 kick.wav 1.0 0.0 none 1.0 tune=3 eq_low=2 eq_mid=-1 "
                        "eq_mid_freq=800 eq_high=1 soft=kick_soft.wav hard=kick_hard.wav\n"),
              "escreve kit.txt v2");
        aroma::Kit kit;
        std::string err;
        check(aroma::loadKit(dir + "kit.txt", kit, err), "carrega kit v2");
        if (!err.empty()) std::printf("      erro: %s\n", err.c_str());
        check(kit.instruments.size() == 1, "kit v2 tem 1 peca");
        if (kit.instruments.size() == 1) {
            const auto& k = kit.instruments[0];
            check(k.tune == 3.0f, "tune=3 lido");
            check(k.eqLowDb == 2.0f, "eq_low=2 lido");
            check(k.eqMidDb == -1.0f, "eq_mid=-1 lido");
            check(k.eqMidFreq == 800.0f, "eq_mid_freq=800 lido");
            check(k.eqHighDb == 1.0f, "eq_high=1 lido");
            check(k.hasLayers, "camadas explicitas detectadas");
            check(k.variations.size() == 2, "round-robin kick + kick_2");
            check(k.softLayer.loaded() && k.hardLayer.loaded(), "soft/hard carregados");
            check(k.softFile == "kick_soft.wav" && k.hardFile == "kick_hard.wav",
                  "nomes das camadas guardados");
        }
        // O motor usa as camadas do arquivo: vel 30 = soft (0.25), 100 = hard (0.9).
        aroma::DrumEngine e;
        check(e.loadKitFromFile(dir + "kit.txt", err), "motor carrega kit v2 do disco");
        std::vector<float> soft(480 * 2, 0.0f), hard(480 * 2, 0.0f);
        e.noteOn(36, 30); e.render(soft.data(), 480, 2);
        e.noteOn(36, 100); e.render(hard.data(), 480, 2);
        check(maxAbs(soft) < 0.06f, "vel 30 usa soft do arquivo");
        check(maxAbs(hard) > 0.4f, "vel 100 usa hard do arquivo");
    }

    // ---- Kit demo v1: compatibilidade (campos ausentes = padrão) ----
    {
        aroma::Kit kit;
        std::string err;
        check(aroma::loadKit("kits/demo-kit/kit.txt", kit, err), "kit demo v1 carrega");
        check(kit.instruments.size() == 12, "kit demo tem 12 pecas");
        bool defaultsOk = true;
        for (const auto& k : kit.instruments) {
            if (k.tune != 0.0f || k.eqLowDb != 0.0f || k.eqMidDb != 0.0f ||
                k.eqHighDb != 0.0f || k.eqMidFreq != 1000.0f || k.hasLayers)
                defaultsOk = false;
            if (k.variations.empty() || k.variations[0].sampleRate() != 48000)
                defaultsOk = false;
        }
        check(defaultsOk, "kit demo: tune/EQ zerados, sem camadas, 48 kHz");
    }

    // ---- Round-trip: salvar v2 e recarregar ----
    {
        aroma::Kit kit;
        std::string err;
        check(aroma::loadKit("kits/demo-kit/kit.txt", kit, err), "carrega demo p/ round-trip");
        // Sem checar o retorno, um kit que não carregou (ex.: teste
        // rodado fora da raiz do projeto) derrubava o processo com
        // segfault em vez de reportar a falha (visto em 24/11/2026).
        auto* k36e = kit.findByNote(36);
        auto* k38e = kit.findByNote(38);
        check(k36e != nullptr && k38e != nullptr, "demo tem bumbo (36) e caixa (38)");
        if (k36e) k36e->tune = 6.0f;
        if (k38e) { k38e->eqMidDb = 3.0f; k38e->eqMidFreq = 1200.0f; }
        const std::string dir = tmp + "rt/";
        check(aroma::saveKit(kit, dir + "kit.txt", err), "salva kit v2");
        if (!err.empty()) std::printf("      erro save: %s\n", err.c_str());
        aroma::Kit kit2;
        check(aroma::loadKit(dir + "kit.txt", kit2, err), "recarrega kit salvo");
        check(kit2.instruments.size() == 12, "round-trip mantem 12 pecas");
        const auto* k36 = kit2.findByNote(36);
        const auto* k38 = kit2.findByNote(38);
        check(k36 && k36->tune == 6.0f, "tune=+6 sobrevive ao round-trip");
        check(k38 && k38->eqMidDb == 3.0f && k38->eqMidFreq == 1200.0f,
              "EQ sobrevive ao round-trip");
        bool notesOk = true;
        for (const auto& k : kit.instruments)
            if (!kit2.findByNote(k.note)) notesOk = false;
        check(notesOk, "round-trip mantem todas as notas");
        // O áudio renderizado do kit salvo é igual ao do kit modificado.
        aroma::DrumEngine e1, e2;
        e1.setKit(kit);
        e2.setKit(kit2);
        std::vector<float> b1(48000 * 2, 0.0f), b2(48000 * 2, 0.0f);
        e1.noteOn(36, 100); e1.render(b1.data(), 48000, 2);
        e2.noteOn(36, 100); e2.render(b2.data(), 48000, 2);
        check(std::fabs(maxAbs(b1) - maxAbs(b2)) < 0.01f, "round-trip: pico igual");
        double diff = 0;
        for (int i = 0; i < 200 * 2; ++i) diff += std::fabs(b1[i] - b2[i]);
        check(diff < 0.05, "round-trip: primeiros samples iguais");
        // Os WAVs foram copiados para samples/ (12 base + 3 variações).
        std::ifstream f1(dir + "samples/kick.wav", std::ios::binary);
        std::ifstream f2(dir + "samples/snare_2.wav", std::ios::binary);
        std::ifstream f3(dir + "samples/hihat_closed_2.wav", std::ios::binary);
        check(f1.good() && f2.good() && f3.good(), "WAVs copiados p/ samples/");
    }

    // ---- Arquivo inválido: erro claro, sem travar ----
    {
        const std::string dir = tmp + "bad/";
        makeDir(dir);
        makeDir(dir + "samples/");
        check(writeFile(dir + "kit.txt", "# name: Ruim\nkick 36 falta.wav 1.0 0.0 none 1.0\n"),
              "escreve kit com sample faltando");
        aroma::Kit kit;
        std::string err;
        check(!aroma::loadKit(dir + "kit.txt", kit, err), "sample faltando rejeita o kit");
        check(!err.empty(), "erro do sample faltando tem mensagem");
        aroma::Kit empty;
        check(!aroma::saveKit(empty, dir + "out.txt", err), "salvar kit vazio falha");
        check(!err.empty(), "erro do kit vazio tem mensagem");
        aroma::DrumEngine e;
        check(!e.loadKitFromFile(dir + "kit.txt", err), "motor rejeita kit invalido");
        check(!e.hasKit(), "motor sem kit apos falha");
    }

    // ---- API de arquivo do motor: trocar sample, add/remove ----
    {
        const std::string dir = tmp + "api/";
        makeDir(dir);
        makeDir(dir + "samples/");
        check(writeWavFile(dir + "samples/new.wav", 48000, std::vector<float>(480, 0.3f)),
              "escreve new.wav p/ troca de sample");
        aroma::DrumEngine e;
        std::string err;
        check(e.loadKitFromFile("kits/demo-kit/kit.txt", err), "motor carrega demo p/ API");
        check(e.replaceInstrumentSample(36, dir + "samples/new.wav", err),
              "troca o sample do kick");
        std::vector<float> b(480 * 2, 0.0f);
        e.noteOn(36, 100);
        e.render(b.data(), 480, 2);
        // 0.3 * (100/127) * 0.707 (pan) ~= 0.167
        check(maxAbs(b) > 0.14f && maxAbs(b) < 0.19f, "novo sample toca no lugar");
        check(!e.replaceInstrumentSample(36, dir + "samples/falta.wav", err),
              "trocar por arquivo inexistente falha");
        check(!err.empty(), "erro da troca tem mensagem");
        aroma::KitInstrument extra;
        extra.name = "extra"; extra.note = 60; extra.file = "new.wav";
        aroma::WavFile w;
        const auto bytes = makeWavBytes(48000, std::vector<float>(100, 0.5f));
        w.loadFromMemory(bytes.data(), bytes.size(), err);
        extra.variations.push_back(std::move(w));
        check(e.addInstrument(extra), "adiciona peca nova");
        check(!e.addInstrument(extra), "adicionar nota duplicada falha");
        std::vector<float> b2(480 * 2, 0.0f);
        e.noteOn(60, 100);
        e.render(b2.data(), 480, 2);
        check(maxAbs(b2) > 0.2f, "peca nova toca");
        check(e.removeInstrument(60), "remove peca");
        check(!e.removeInstrument(60), "remover de novo falha");
        std::vector<float> b3(480 * 2, 0.0f);
        e.noteOn(60, 100);
        e.render(b3.data(), 480, 2);
        check(maxAbs(b3) == 0.0f, "peca removida nao toca");
        check(e.saveKitToFile(dir + "saved.txt", err), "motor salva kit no disco");
        aroma::Kit reloaded;
        check(aroma::loadKit(dir + "saved.txt", reloaded, err), "kit salvo pelo motor recarrega");
        check(reloaded.instruments.size() == 12, "kit salvo tem 12 pecas (extra removida)");
    }

    return failures;
}

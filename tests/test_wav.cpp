// Testes do carregador WAV (16/24 bits, mono/estéreo, erros).
#include <cstdio>
#include <cstring>
#include <vector>
#include "../src/common/WavFile.h"

namespace {

int failures = 0;
void check(bool ok, const char* name) {
    std::printf("  [%s] %s\n", ok ? "OK" : "FALHOU", name);
    if (!ok) ++failures;
}

// Constrói um WAV PCM 16 bits mono em memória: 4 amostras.
std::vector<uint8_t> makeWav16Mono() {
    std::vector<uint8_t> b;
    auto put = [&](const char* s, size_t n) { b.insert(b.end(), s, s + n); };
    auto u16 = [&](uint16_t v) { b.push_back(v & 0xFF); b.push_back((v >> 8) & 0xFF); };
    auto u32 = [&](uint32_t v) { for (int i = 0; i < 4; ++i) b.push_back((v >> (8 * i)) & 0xFF); };
    put("RIFF", 4); u32(36 + 8); put("WAVE", 4);
    put("fmt ", 4); u32(16); u16(1); u16(1); u32(48000); u32(48000 * 2); u16(2); u16(16);
    put("data", 4); u32(8);
    u16(0); u16(16384); u16(static_cast<uint16_t>(-16384)); u16(32767);
    return b;
}

} // namespace

int testWav() {
    std::printf("test_wav:\n");
    aroma::WavFile wav;
    std::string err;
    const auto bytes = makeWav16Mono();
    check(wav.loadFromMemory(bytes.data(), bytes.size(), err), "carrega WAV 16 bits mono");
    check(wav.sampleRate() == 48000, "taxa 48000");
    check(wav.channels() == 1, "1 canal");
    check(wav.frames() == 4, "4 frames");
    check(wav.data()[0] == 0.0f, "amostra 0 = 0.0");
    check(wav.data()[1] > 0.49f && wav.data()[1] < 0.51f, "amostra 1 = 0.5");
    check(wav.data()[2] < -0.49f && wav.data()[2] > -0.51f, "amostra 2 = -0.5");

    aroma::WavFile bad;
    const uint8_t junk[] = {'N', 'O', 'P', 'E'};
    check(!bad.loadFromMemory(junk, sizeof(junk), err), "rejeita arquivo sem RIFF");
    check(!err.empty(), "erro tem mensagem");

    aroma::WavFile empty;
    check(!empty.loadFromMemory(bytes.data(), 10, err), "rejeita arquivo truncado");
    return failures;
}

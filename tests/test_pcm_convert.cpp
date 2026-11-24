// Testes do conversor float -> formato do dispositivo (PcmConvert.h).
// Sem Windows, sem áudio real.
#include "../src/standalone/PcmConvert.h"
#include <cstdint>
#include <cstdio>

int testPcmConvert() {
    int failures = 0;
    auto check = [&](bool ok, const char* what) {
        if (!ok) { std::printf("FALHOU: %s\n", what); ++failures; }
    };

    // float32: cópia exata.
    {
        const float in[4] = {0.5f, -0.25f, 1.0f, -1.0f};
        float out[4] = {};
        aroma::convertFloatToDevice(in, reinterpret_cast<uint8_t*>(out), 2, 2, 32, true);
        check(out[0] == 0.5f && out[1] == -0.25f && out[2] == 1.0f && out[3] == -1.0f,
              "float32 copia exata");
    }
    // PCM16: 0.5 -> 16383, -1.0 -> -32767, acima de 1.0 clipa em 32767.
    {
        const float in[3] = {0.5f, -1.0f, 2.0f};
        int16_t out[3] = {};
        aroma::convertFloatToDevice(in, reinterpret_cast<uint8_t*>(out), 3, 1, 16, false);
        check(out[0] == 16383, "pcm16 0.5");
        check(out[1] == -32767, "pcm16 -1.0");
        check(out[2] == 32767, "pcm16 clip 2.0 -> 32767");
    }
    // PCM24: 1.0 -> 0x7FFFFF; -1.0 -> -8388607 = 0xFF800001 (bytes 01 00 80).
    {
        const float in[2] = {1.0f, -1.0f};
        uint8_t out[6] = {};
        aroma::convertFloatToDevice(in, out, 2, 1, 24, false);
        check(out[0] == 0xFF && out[1] == 0xFF && out[2] == 0x7F, "pcm24 1.0");
        check(out[3] == 0x01 && out[4] == 0x00 && out[5] == 0x80, "pcm24 -1.0");
    }
    // PCM32: 0.5 -> 1073741823.
    {
        const float in[1] = {0.5f};
        int32_t out[1] = {};
        aroma::convertFloatToDevice(in, reinterpret_cast<uint8_t*>(out), 1, 1, 32, false);
        check(out[0] == 1073741823, "pcm32 0.5");
    }
    // Silêncio: zeros.
    {
        const float in[2] = {0.0f, 0.0f};
        uint8_t out[4] = {0xFF, 0xFF, 0xFF, 0xFF};
        aroma::convertFloatToDevice(in, out, 1, 2, 16, false);
        check(out[0] == 0 && out[1] == 0 && out[2] == 0 && out[3] == 0, "pcm16 silencio");
    }
    // Detecção de float.
    {
        check(aroma::isFloatFormat(3, nullptr, 0), "isFloat ieee");
        static const uint8_t floatGuid[16] = {0x03, 0, 0, 0, 0, 0, 0x10, 0,
                                              0x80, 0, 0, 0xaa, 0, 0x38, 0x9b, 0x71};
        check(aroma::isFloatFormat(0xFFFE, floatGuid, 22), "isFloat extensible");
        static const uint8_t pcmGuid[16] = {0x01, 0, 0, 0, 0, 0, 0x10, 0,
                                            0x80, 0, 0, 0xaa, 0, 0x38, 0x9b, 0x71};
        check(!aroma::isFloatFormat(0xFFFE, pcmGuid, 22), "isFloat pcm nao");
        check(!aroma::isFloatFormat(1, nullptr, 0), "isFloat pcm tag nao");
    }

    if (failures == 0) std::printf("testPcmConvert: OK\n");
    return failures;
}

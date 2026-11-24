#pragma once
// Conversão de amostras float [-1, 1] (saída do DrumEngine) para o
// formato do dispositivo WASAPI. Portátil (sem Windows): a mesma lógica
// do ToneWriter do diagnóstico, testada em tests/test_pcm_convert.cpp.
// `frames`: nº de frames; `channels`: 1 ou 2 (intercalado).
// `bits`: 16, 24 ou 32; `isFloat` só vale com 32 bits.
// Escreve exatamente frames*channels*(bits/8) bytes. Não aloca nada.
#include <cstdint>
#include <cstring>

namespace aroma {

inline void convertFloatToDevice(const float* in, uint8_t* out, int frames,
                                 int channels, int bits, bool isFloat) {
    if (!in || !out || frames <= 0 || channels <= 0) return;
    const int total = frames * channels;
    if (isFloat && bits == 32) {
        std::memcpy(out, in, static_cast<size_t>(total) * sizeof(float));
        return;
    }
    if (bits == 16) {
        int16_t* dst = reinterpret_cast<int16_t*>(out);
        for (int i = 0; i < total; ++i) {
            float s = in[i];
            if (s > 1.0f) s = 1.0f; else if (s < -1.0f) s = -1.0f;
            dst[i] = static_cast<int16_t>(s * 32767.0f);
        }
    } else if (bits == 24) {
        for (int i = 0; i < total; ++i) {
            float s = in[i];
            if (s > 1.0f) s = 1.0f; else if (s < -1.0f) s = -1.0f;
            const int v = static_cast<int>(s * 8388607.0f);
            out[i * 3 + 0] = static_cast<uint8_t>(v & 0xFF);
            out[i * 3 + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
            out[i * 3 + 2] = static_cast<uint8_t>((v >> 16) & 0xFF);
        }
    } else if (bits == 32) {
        int32_t* dst = reinterpret_cast<int32_t*>(out);
        for (int i = 0; i < total; ++i) {
            float s = in[i];
            if (s > 1.0f) s = 1.0f; else if (s < -1.0f) s = -1.0f;
            dst[i] = static_cast<int32_t>(s * 2147483647.0);
        }
    } else {
        std::memset(out, 0, static_cast<size_t>(total) * static_cast<size_t>(bits / 8));
    }
}

// Detecta float a partir do wFormatTag + GUID do SubFormat (EXTENSIBLE).
// WAVE_FORMAT_IEEE_FLOAT = 3; WAVE_FORMAT_EXTENSIBLE = 0xFFFE.
// O GUID do IEEE float, em bytes little-endian: 03 00 00 00 ...
inline bool isFloatFormat(unsigned short wFormatTag, const uint8_t* subFormatGuid,
                          unsigned short cbSize) {
    static const uint8_t kFloatGuid[16] = {0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
                                           0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71};
    if (wFormatTag == 3) return true; // WAVE_FORMAT_IEEE_FLOAT
    if (wFormatTag == 0xFFFE && cbSize >= 22 && subFormatGuid)
        return std::memcmp(subFormatGuid, kFloatGuid, 16) == 0;
    return false;
}

} // namespace aroma

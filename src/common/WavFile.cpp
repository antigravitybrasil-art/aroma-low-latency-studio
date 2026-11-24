#include "WavFile.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>

namespace aroma {
namespace {

uint16_t readU16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0] | (p[1] << 8));
}
uint32_t readU32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

std::string baseNameOf(const std::string& path) {
    const size_t p = path.find_last_of("/\\");
    return p == std::string::npos ? path : path.substr(p + 1);
}

void putU16(std::ofstream& f, uint16_t v) {
    const uint8_t b[2] = {static_cast<uint8_t>(v & 0xFF),
                          static_cast<uint8_t>((v >> 8) & 0xFF)};
    f.write(reinterpret_cast<const char*>(b), 2);
}
void putU32(std::ofstream& f, uint32_t v) {
    const uint8_t b[4] = {static_cast<uint8_t>(v & 0xFF),
                          static_cast<uint8_t>((v >> 8) & 0xFF),
                          static_cast<uint8_t>((v >> 16) & 0xFF),
                          static_cast<uint8_t>((v >> 24) & 0xFF)};
    f.write(reinterpret_cast<const char*>(b), 4);
}
void putTag(std::ofstream& f, const char* tag) { f.write(tag, 4); }

} // namespace

bool WavFile::load(const std::string& path, std::string& error) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) { error = "nao foi possivel abrir: " + path; return false; }
    std::streamsize size = f.tellg();
    if (size < 44) { error = "arquivo muito pequeno para WAV: " + path; return false; }
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    if (!f.read(reinterpret_cast<char*>(bytes.data()), size)) {
        error = "falha ao ler: " + path; return false;
    }
    if (!loadFromMemory(bytes.data(), bytes.size(), error)) return false;
    sourcePath_ = path;
    fileName_ = baseNameOf(path);
    resampled_ = false;
    return true;
}

bool WavFile::resampleTo(int targetRate) {
    if (!loaded() || sampleRate_ <= 0 || targetRate <= 0) return false;
    if (sampleRate_ == targetRate) return true;
    const double ratio = static_cast<double>(sampleRate_) / targetRate;
    const size_t outFrames =
        static_cast<size_t>(std::llround(static_cast<double>(frames()) * targetRate / sampleRate_));
    if (outFrames == 0) return false;
    std::vector<float> out(outFrames * static_cast<size_t>(channels_));
    for (size_t i = 0; i < outFrames; ++i) {
        const double srcPos = static_cast<double>(i) * ratio;
        size_t i0 = static_cast<size_t>(srcPos);
        if (i0 >= frames()) i0 = frames() - 1;
        const size_t i1 = (i0 + 1 < frames()) ? i0 + 1 : i0;
        const double frac = srcPos - static_cast<double>(i0);
        for (int ch = 0; ch < channels_; ++ch) {
            const float a = samples_[i0 * static_cast<size_t>(channels_) + static_cast<size_t>(ch)];
            const float b = samples_[i1 * static_cast<size_t>(channels_) + static_cast<size_t>(ch)];
            out[i * static_cast<size_t>(channels_) + static_cast<size_t>(ch)] =
                static_cast<float>(a + (b - a) * frac);
        }
    }
    samples_.swap(out);
    sampleRate_ = targetRate;
    resampled_ = true;
    return true;
}

bool WavFile::writeWav16(const std::string& path, std::string& error) const {
    if (!loaded() || sampleRate_ <= 0 || channels_ < 1 || channels_ > 2) {
        error = "nada para escrever em: " + path;
        return false;
    }
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) { error = "nao foi possivel escrever: " + path; return false; }
    const uint32_t dataBytes =
        static_cast<uint32_t>(frames()) * static_cast<uint32_t>(channels_) * 2u;
    putTag(f, "RIFF"); putU32(f, 36 + dataBytes); putTag(f, "WAVE");
    putTag(f, "fmt "); putU32(f, 16);
    putU16(f, 1); // PCM
    putU16(f, static_cast<uint16_t>(channels_));
    putU32(f, static_cast<uint32_t>(sampleRate_));
    putU32(f, static_cast<uint32_t>(sampleRate_ * channels_ * 2));
    putU16(f, static_cast<uint16_t>(channels_ * 2));
    putU16(f, 16);
    putTag(f, "data"); putU32(f, dataBytes);
    for (float s : samples_) {
        if (s > 1.0f) s = 1.0f;
        else if (s < -1.0f) s = -1.0f;
        putU16(f, static_cast<uint16_t>(static_cast<int16_t>(s * 32767.0f)));
    }
    if (!f.good()) { error = "falha ao escrever: " + path; return false; }
    return true;
}

bool WavFile::loadFromMemory(const uint8_t* data, size_t size, std::string& error) {
    samples_.clear();
    sampleRate_ = 0;
    channels_ = 0;
    sourcePath_.clear();
    fileName_.clear();
    resampled_ = false;
    if (size < 44 || std::memcmp(data, "RIFF", 4) != 0 ||
        std::memcmp(data + 8, "WAVE", 4) != 0) {
        error = "cabecalho RIFF/WAVE invalido";
        return false;
    }
    // Percorre os chunks (fmt, data, e outros ignorados como LIST/fact).
    size_t pos = 12;
    uint16_t audioFormat = 0, bitsPerSample = 0;
    uint32_t dataOffset = 0, dataSize = 0;
    bool foundFmt = false, foundData = false;
    while (pos + 8 <= size) {
        uint32_t chunkSize = readU32(data + pos + 4);
        if (pos + 8 + chunkSize > size && std::memcmp(data + pos, "data", 4) == 0) {
            chunkSize = static_cast<uint32_t>(size - (pos + 8)); // data truncado: usa o que há
        }
        if (std::memcmp(data + pos, "fmt ", 4) == 0) {
            if (chunkSize < 16) { error = "chunk fmt muito pequeno"; return false; }
            audioFormat = readU16(data + pos + 8);
            channels_ = readU16(data + pos + 10);
            sampleRate_ = static_cast<int>(readU32(data + pos + 12));
            bitsPerSample = readU16(data + pos + 22);
            foundFmt = true;
        } else if (std::memcmp(data + pos, "data", 4) == 0) {
            dataOffset = static_cast<uint32_t>(pos + 8);
            dataSize = chunkSize;
            foundData = true;
            break; // data costuma ser o último chunk relevante
        }
        pos += 8 + chunkSize + (chunkSize & 1); // chunks são alinhados em 2 bytes
    }
    if (!foundFmt) { error = "chunk fmt nao encontrado"; return false; }
    if (!foundData) { error = "chunk data nao encontrado"; return false; }
    if (audioFormat != 1) { // 1 = PCM; 3 = float (não suportado aqui, por enquanto)
        error = "formato WAV nao-PCM (format=" + std::to_string(audioFormat) + ")";
        return false;
    }
    if (channels_ < 1 || channels_ > 2) {
        error = "canais nao suportados: " + std::to_string(channels_);
        return false;
    }
    if (sampleRate_ < 8000 || sampleRate_ > 384000) {
        error = "taxa de amostragem invalida: " + std::to_string(sampleRate_);
        return false;
    }
    if (bitsPerSample != 16 && bitsPerSample != 24) {
        error = "bits por amostra nao suportados: " + std::to_string(bitsPerSample);
        return false;
    }
    const size_t bytesPerSample = bitsPerSample / 8;
    const size_t frameBytes = bytesPerSample * static_cast<size_t>(channels_);
    if (frameBytes == 0 || dataSize < frameBytes) {
        error = "chunk data vazio"; return false;
    }
    const size_t nFrames = dataSize / frameBytes;
    samples_.resize(nFrames * static_cast<size_t>(channels_));
    const uint8_t* src = data + dataOffset;
    for (size_t i = 0; i < nFrames * static_cast<size_t>(channels_); ++i) {
        if (bitsPerSample == 16) {
            int32_t v = static_cast<int16_t>(readU16(src + i * 2));
            samples_[i] = static_cast<float>(v) / 32768.0f;
        } else { // 24 bits, little-endian, com sinal
            int32_t v = static_cast<int32_t>(src[i * 3]) |
                        (static_cast<int32_t>(src[i * 3 + 1]) << 8) |
                        (static_cast<int32_t>(src[i * 3 + 2]) << 16);
            if (v & 0x800000) v -= 0x1000000;
            samples_[i] = static_cast<float>(v) / 8388608.0f;
        }
    }
    return true;
}

} // namespace aroma

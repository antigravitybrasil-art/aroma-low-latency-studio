#pragma once
// Carregador WAV mínimo e seguro: PCM 16/24 bits, mono/estéreo.
// Carrega o arquivo INTEIRO para a RAM (uma vez, fora da thread de áudio)
// e o converte para float intercalado normalizado [-1, 1].
// Não aloca nada depois de carregado.
#include <cstdint>
#include <string>
#include <vector>

namespace aroma {

class WavFile {
public:
    bool load(const std::string& path, std::string& error);
    bool loadFromMemory(const uint8_t* data, size_t size, std::string& error);

    // Reamostra para a taxa do motor (interpolação linear), fora da
    // thread de áudio. Sem efeito se já estiver na taxa. Devolve false
    // se não houver nada carregado ou a taxa for inválida.
    bool resampleTo(int targetRate);
    // Escreve os samples como WAV PCM 16 bits (taxa atual). Usado pelo
    // saveKit quando não dá para copiar o arquivo original (ex.: o
    // sample foi reamostrado ou veio da memória).
    bool writeWav16(const std::string& path, std::string& error) const;

    bool loaded() const { return !samples_.empty(); }
    int sampleRate() const { return sampleRate_; }
    int channels() const { return channels_; }
    size_t frames() const { return channels_ > 0 ? samples_.size() / channels_ : 0; }
    double durationSeconds() const {
        return sampleRate_ > 0 ? static_cast<double>(frames()) / sampleRate_ : 0.0;
    }
    // Amostras float intercaladas, só leitura. Válido enquanto o WavFile viver.
    const float* data() const { return samples_.data(); }
    // De onde veio ("" quando carregado da memória). O saveKit copia o
    // arquivo original quando pode, para não perder qualidade (24 bits).
    const std::string& sourcePath() const { return sourcePath_; }
    const std::string& fileName() const { return fileName_; }
    bool wasResampled() const { return resampled_; }

private:
    std::vector<float> samples_;
    int sampleRate_ = 0;
    int channels_ = 0;
    std::string sourcePath_;
    std::string fileName_;
    bool resampled_ = false;
};

} // namespace aroma

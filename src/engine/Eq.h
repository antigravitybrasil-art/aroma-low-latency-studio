#pragma once
// EQ de 3 bandas por peça — biquads RBJ, sem alocação, portátil.
//   low:  lowshelf 200 Hz | mid: peaking (freq configurável, Q=1)
//   high: highshelf 4 kHz
// Ganhos em dB (-18..+18). Com os 3 ganhos em 0, bypass=true e o
// process() devolve a amostra intacta (sem colorir o som nem gastar
// CPU). Cada voz tem um Eq3Band por canal (L/R com estados separados);
// os coeficientes são calculados uma vez no startVoice, não por sample.
// Coeficientes normalizados por a0 (a0 = 1). Fórmulas do RBJ Audio EQ
// Cookbook; os ganhos em DC/Nyquist não dependem do alpha, só o joelho.
#include <cmath>

namespace aroma {

// Biquad genérico (Direct Form I transposto, estado em floats).
struct Biquad {
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
    float a1 = 0.0f, a2 = 0.0f;
    float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;

    void reset() { x1 = x2 = y1 = y2 = 0.0f; }

    void setCoeffs(double nb0, double nb1, double nb2, double na1, double na2) {
        b0 = static_cast<float>(nb0);
        b1 = static_cast<float>(nb1);
        b2 = static_cast<float>(nb2);
        a1 = static_cast<float>(na1);
        a2 = static_cast<float>(na2);
    }

    void setLowShelf(double freq, double gainDb, double sr) {
        const double A = std::pow(10.0, gainDb / 40.0);
        const double w0 = 2.0 * 3.14159265358979323846 * freq / sr;
        const double cs = std::cos(w0), sn = std::sin(w0);
        const double alpha = sn / 2.0 * std::sqrt(2.0); // S = 1
        const double sqA = std::sqrt(A);
        const double b0v = A * ((A + 1) - (A - 1) * cs + 2 * sqA * alpha);
        const double b1v = 2 * A * ((A - 1) - (A + 1) * cs);
        const double b2v = A * ((A + 1) - (A - 1) * cs - 2 * sqA * alpha);
        const double a0v = (A + 1) + (A - 1) * cs + 2 * sqA * alpha;
        const double a1v = -2 * ((A - 1) + (A + 1) * cs);
        const double a2v = (A + 1) + (A - 1) * cs - 2 * sqA * alpha;
        setCoeffs(b0v / a0v, b1v / a0v, b2v / a0v, a1v / a0v, a2v / a0v);
    }

    void setHighShelf(double freq, double gainDb, double sr) {
        const double A = std::pow(10.0, gainDb / 40.0);
        const double w0 = 2.0 * 3.14159265358979323846 * freq / sr;
        const double cs = std::cos(w0), sn = std::sin(w0);
        const double alpha = sn / 2.0 * std::sqrt(2.0); // S = 1
        const double sqA = std::sqrt(A);
        const double b0v = A * ((A + 1) + (A - 1) * cs + 2 * sqA * alpha);
        const double b1v = -2 * A * ((A - 1) + (A + 1) * cs);
        const double b2v = A * ((A + 1) + (A - 1) * cs - 2 * sqA * alpha);
        const double a0v = (A + 1) - (A - 1) * cs + 2 * sqA * alpha;
        const double a1v = 2 * ((A - 1) - (A + 1) * cs);
        const double a2v = (A + 1) - (A - 1) * cs - 2 * sqA * alpha;
        setCoeffs(b0v / a0v, b1v / a0v, b2v / a0v, a1v / a0v, a2v / a0v);
    }

    void setPeaking(double freq, double gainDb, double q, double sr) {
        const double A = std::pow(10.0, gainDb / 40.0);
        const double w0 = 2.0 * 3.14159265358979323846 * freq / sr;
        const double cs = std::cos(w0), sn = std::sin(w0);
        const double alpha = sn / (2.0 * q);
        const double b0v = 1 + alpha * A, b1v = -2 * cs, b2v = 1 - alpha * A;
        const double a0v = 1 + alpha / A, a1v = -2 * cs, a2v = 1 - alpha / A;
        setCoeffs(b0v / a0v, b1v / a0v, b2v / a0v, a1v / a0v, a2v / a0v);
    }

    float process(float x) {
        const float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1; x1 = x;
        y2 = y1; y1 = y;
        return y;
    }
};

// As 3 bandas em série: low -> mid -> high.
struct Eq3Band {
    Biquad low, mid, high;
    bool bypass = true;

    // Ganho 0 nas 3 bandas = bypass (sem processar nada).
    // midFreq fora de 50..16000 Hz é travada em 1000 Hz.
    void setup(double lowDb, double midDb, double midFreq, double highDb, double sr) {
        if (midFreq < 50.0 || midFreq > 16000.0) midFreq = 1000.0;
        if (sr < 8000.0) sr = 48000.0;
        bypass = (std::fabs(lowDb) < 0.01 && std::fabs(midDb) < 0.01 &&
                  std::fabs(highDb) < 0.01);
        low.setLowShelf(200.0, lowDb, sr);
        mid.setPeaking(midFreq, midDb, 1.0, sr);
        high.setHighShelf(4000.0, highDb, sr);
        reset();
    }

    void reset() { low.reset(); mid.reset(); high.reset(); }

    float process(float x) {
        if (bypass) return x;
        return high.process(mid.process(low.process(x)));
    }
};

} // namespace aroma

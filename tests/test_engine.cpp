// Testes do DrumEngine: mixagem, velocity, choke de hi-hat, polifonia.
#include <cstdio>
#include <cmath>
#include <vector>
#include "../src/engine/DrumEngine.h"

namespace {

int failures = 0;
void check(bool ok, const char* name) {
    std::printf("  [%s] %s\n", ok ? "OK" : "FALHOU", name);
    if (!ok) ++failures;
}

// WAV 16 bits mono em memória a partir de valores float.
aroma::WavFile makeWavFromFloats(const std::vector<float>& vals, int rate = 48000) {
    aroma::WavFile w;
    std::vector<uint8_t> b;
    auto u16 = [&](uint16_t v) { b.push_back(v & 0xFF); b.push_back((v >> 8) & 0xFF); };
    auto u32 = [&](uint32_t v) { for (int i = 0; i < 4; ++i) b.push_back((v >> (8 * i)) & 0xFF); };
    auto put = [&](const char* s, size_t n) { b.insert(b.end(), s, s + n); };
    const int frames = static_cast<int>(vals.size());
    put("RIFF", 4); u32(36 + frames * 2); put("WAVE", 4);
    put("fmt ", 4); u32(16); u16(1); u16(1); u32(rate); u32(rate * 2); u16(2); u16(16);
    put("data", 4); u32(frames * 2);
    for (float v : vals) {
        const int16_t sample = static_cast<int16_t>(v * 32767);
        u16(static_cast<uint16_t>(sample));
    }
    std::string err;
    if (!w.loadFromMemory(b.data(), b.size(), err)) std::printf("erro wav teste: %s\n", err.c_str());
    return w;
}

aroma::WavFile makeWav(float value) {
    return makeWavFromFloats(std::vector<float>(100, value));
}

// Kit sintético em memória: 2 instrumentos com 100 amostras cada.
aroma::Kit makeTestKit() {
    aroma::Kit kit;
    kit.name = "teste";
    aroma::KitInstrument kick;
    kick.name = "kick"; kick.note = 36; kick.file = "kick.wav";
    kick.variations.push_back(makeWav(0.5f));
    aroma::KitInstrument hhClosed;
    hhClosed.name = "hihat_closed"; hhClosed.note = 42; hhClosed.file = "hh.wav";
    hhClosed.chokeGroup = "hihat"; hhClosed.variations.push_back(makeWav(0.5f));
    aroma::KitInstrument hhOpen;
    hhOpen.name = "hihat_open"; hhOpen.note = 46; hhOpen.file = "hho.wav";
    hhOpen.chokeGroup = "hihat"; hhOpen.variations.push_back(makeWav(0.5f));
    kit.instruments = {kick, hhClosed, hhOpen};
    return kit;
}

// Kit com 1 instrumento (kick, nota 36) e sample arbitrário.
aroma::Kit makeSingleKit(const std::vector<float>& vals, int rate = 48000) {
    aroma::Kit kit;
    kit.name = "single";
    aroma::KitInstrument k;
    k.name = "kick"; k.note = 36; k.file = "kick.wav";
    k.variations.push_back(makeWavFromFloats(vals, rate));
    kit.instruments = {k};
    return kit;
}

float maxAbs(const std::vector<float>& buf) {
    float m = 0;
    for (float s : buf) { float a = s < 0 ? -s : s; if (a > m) m = a; }
    return m;
}

} // namespace

int testEngine() {
    std::printf("test_engine:\n");
    aroma::DrumEngine engine;
    check(engine.setKit(makeTestKit()), "setKit aceita kit de teste");
    check(engine.hasKit(), "hasKit true");

    // Sample de teste = 0.5 constante, pan central (equal-power: x0.707).
    std::vector<float> buf(50 * 2, 0.0f); // 50 frames < 100 do sample: voz segue ativa
    engine.noteOn(36, 127);
    engine.render(buf.data(), 50, 2);
    check(maxAbs(buf) > 0.3f && maxAbs(buf) < 0.4f, "kick soa apos noteOn (velocity 127, ~0.35)");
    check(engine.activeVoices() == 1, "1 voz ativa durante o sample");
    std::vector<float> tail(480 * 2, 0.0f);
    engine.render(tail.data(), 480, 2);
    check(engine.activeVoices() == 0, "voz libera ao fim do sample");

    // Velocity baixa => som mais baixo.
    aroma::DrumEngine engine2;
    engine2.setKit(makeTestKit());
    std::vector<float> loud(480 * 2, 0.0f), soft(480 * 2, 0.0f);
    engine2.noteOn(36, 127);
    engine2.render(loud.data(), 480, 2);
    aroma::DrumEngine engine3;
    engine3.setKit(makeTestKit());
    engine3.noteOn(36, 20);
    engine3.render(soft.data(), 480, 2);
    check(maxAbs(loud) > maxAbs(soft) * 2.0f, "velocity 127 bem mais alta que 20");

    // Hi-hat: aberto cortado pelo fechado (mesmo choke group).
    aroma::DrumEngine engine4;
    engine4.setKit(makeTestKit());
    engine4.noteOn(46, 100); // aberto
    std::vector<float> b1(50 * 2, 0.0f);
    engine4.render(b1.data(), 50, 2);
    check(engine4.activeVoices() == 1, "hi-hat aberto ativo");
    engine4.noteOn(42, 100); // fechado corta o aberto
    std::vector<float> b2(50 * 2, 0.0f);
    engine4.render(b2.data(), 50, 2);
    check(engine4.activeVoices() == 1, "só 1 voz após choke (fechado cortou aberto)");

    // Polifonia: 40 kicks seguidos não passam de 32 vozes e não travam.
    aroma::DrumEngine engine5;
    engine5.setKit(makeTestKit());
    for (int i = 0; i < 40; ++i) engine5.noteOn(36, 100);
    std::vector<float> b3(10 * 2, 0.0f);
    engine5.render(b3.data(), 10, 2);
    check(engine5.activeVoices() <= 32, "polifonia limitada a 32 vozes");
    check(maxAbs(b3) <= 1.0f, "saida nunca passa de 1.0 (soft-clip)");

    // Bateria é one-shot: Note Off NÃO corta o som. A Aroma manda Note
    // Off ~19 ms depois do Note On; obedecer a ele cortava todo hit no
    // meio do sample (o defeito ouvido na v0.3.0/v0.3.1).
    aroma::DrumEngine engine7;
    engine7.setKit(makeTestKit());
    engine7.noteOn(36, 100);
    std::vector<float> c1(10 * 2, 0.0f);
    engine7.render(c1.data(), 10, 2);
    engine7.noteOff(36); // ~19 ms depois, como a Aroma faz
    std::vector<float> c2(480 * 2, 0.0f);
    engine7.render(c2.data(), 480, 2);
    check(maxAbs(c2) > 0.2f, "noteOff nao corta o sample (one-shot)");
    check(engine7.activeVoices() == 0, "voz termina sozinha ao fim do sample");

    // Choke com fade: o aberto some em rampa, não num corte seco.
    // (Com corte seco, o início do trecho seria só o fechado, igual ao
    // meio; com fade, o início tem aberto + fechado e cai em rampa.)
    aroma::DrumEngine engine8;
    engine8.setKit(makeTestKit());
    engine8.noteOn(46, 100); // aberto
    std::vector<float> d1(10 * 2, 0.0f);
    engine8.render(d1.data(), 10, 2);
    engine8.noteOn(42, 100); // fechado corta o aberto (fade de ~3 ms)
    std::vector<float> d2(200 * 2, 0.0f);
    engine8.render(d2.data(), 200, 2);
    float early = 0, mid = 0;
    for (int i = 0; i < 10 * 2; ++i) { float a = d2[i] < 0 ? -d2[i] : d2[i]; if (a > early) early = a; }
    for (int i = 40 * 2; i < 50 * 2; ++i) { float a = d2[i] < 0 ? -d2[i] : d2[i]; if (a > mid) mid = a; }
    check(early > mid * 1.1f, "choke some em rampa (inicio mais alto que o meio)");
    check(engine8.activeVoices() == 0, "choke com fade libera as vozes ao fim");

    // Nota sem instrumento não quebra nada.
    aroma::DrumEngine engine6;
    engine6.setKit(makeTestKit());
    engine6.noteOn(99, 100);
    std::vector<float> b4(10 * 2, 0.0f);
    engine6.render(b4.data(), 10, 2);
    check(maxAbs(b4) == 0.0f, "nota sem instrumento = silencio");
    check(engine6.droppedEvents() == 0, "nenhum evento descartado em uso normal");

    // ---- EQ 3 bandas ----
    // Low shelf +12 dB em DC: constante 0.1 por 4800 frames; o estado
    // estacionário deve ser ~0.1 * 0.707 (pan) * 10^(12/20)=3.98.
    {
        aroma::DrumEngine flat, boosted;
        flat.setKit(makeSingleKit(std::vector<float>(4800, 0.1f)));
        boosted.setKit(makeSingleKit(std::vector<float>(4800, 0.1f)));
        aroma::InstrumentParams p;
        p.eqLowDb = 12.0f;
        boosted.updateInstrument(36, p);
        std::vector<float> bf(4800 * 2, 0.0f), bb(4800 * 2, 0.0f);
        flat.noteOn(36, 127); flat.render(bf.data(), 4800, 2);
        boosted.noteOn(36, 127); boosted.render(bb.data(), 4800, 2);
        auto steadyMean = [](const std::vector<float>& b) {
            double m = 0; int n = 0;
            for (int i = 3800 * 2; i < 4800 * 2; i += 2) { m += b[i]; ++n; }
            return m / n;
        };
        const double ratio = steadyMean(bb) / steadyMean(bf);
        check(ratio > 3.7 && ratio < 4.3, "EQ low +12dB multiplica o grave por ~4");
        check(steadyMean(bf) > 0.06 && steadyMean(bf) < 0.08, "EQ flat nao muda o grave");
    }
    // Mid peaking +6 dB em 1000 Hz: seno de 1000 Hz, RMS x2.
    {
        std::vector<float> sine(4800);
        for (int i = 0; i < 4800; ++i)
            sine[i] = 0.1f * static_cast<float>(std::sin(2 * 3.141592653589793 * 1000.0 * i / 48000.0));
        aroma::DrumEngine flat, boosted;
        flat.setKit(makeSingleKit(sine));
        boosted.setKit(makeSingleKit(sine));
        aroma::InstrumentParams p;
        p.eqMidDb = 6.0f; p.eqMidFreq = 1000.0f;
        boosted.updateInstrument(36, p);
        std::vector<float> bf(4800 * 2, 0.0f), bb(4800 * 2, 0.0f);
        flat.noteOn(36, 127); flat.render(bf.data(), 4800, 2);
        boosted.noteOn(36, 127); boosted.render(bb.data(), 4800, 2);
        auto steadyRms = [](const std::vector<float>& b) {
            double s = 0; int n = 0;
            for (int i = 3000 * 2; i < 4800 * 2; i += 2) { s += b[i] * b[i]; ++n; }
            return std::sqrt(s / n);
        };
        const double ratio = steadyRms(bb) / steadyRms(bf);
        check(ratio > 1.8 && ratio < 2.2, "EQ mid +6dB em 1kHz dobra o medio");
    }
    // High shelf +12 dB: DC não muda, Nyquist (alterna +-) x4.
    {
        aroma::DrumEngine dcEng, nyEng;
        dcEng.setKit(makeSingleKit(std::vector<float>(4800, 0.1f)));
        std::vector<float> alt(4800);
        for (int i = 0; i < 4800; ++i) alt[i] = (i % 2 == 0) ? 0.1f : -0.1f;
        nyEng.setKit(makeSingleKit(alt));
        aroma::InstrumentParams p;
        p.eqHighDb = 12.0f;
        dcEng.updateInstrument(36, p);
        nyEng.updateInstrument(36, p);
        std::vector<float> bd(4800 * 2, 0.0f), bn(4800 * 2, 0.0f);
        dcEng.noteOn(36, 127); dcEng.render(bd.data(), 4800, 2);
        nyEng.noteOn(36, 127); nyEng.render(bn.data(), 4800, 2);
        auto steadyMeanAbs = [](const std::vector<float>& b) {
            double m = 0; int n = 0;
            for (int i = 4000 * 2; i < 4800 * 2; i += 2) { m += std::fabs(b[i]); ++n; }
            return m / n;
        };
        check(steadyMeanAbs(bd) > 0.06 && steadyMeanAbs(bd) < 0.08,
              "EQ high +12dB nao muda o DC");
        check(steadyMeanAbs(bn) > 0.25 && steadyMeanAbs(bn) < 0.31,
              "EQ high +12dB eleva o agudo (~x4)");
    }
    // EQ zerado = bypass exato (bit a bit igual ao flat).
    {
        aroma::DrumEngine a, b;
        a.setKit(makeSingleKit(std::vector<float>(480, 0.3f)));
        b.setKit(makeSingleKit(std::vector<float>(480, 0.3f)));
        aroma::InstrumentParams p; // tudo zero
        b.updateInstrument(36, p);
        std::vector<float> ba(480 * 2, 0.0f), bb(480 * 2, 0.0f);
        a.noteOn(36, 127); a.render(ba.data(), 480, 2);
        b.noteOn(36, 127); b.render(bb.data(), 480, 2);
        check(ba == bb, "EQ zerado = bypass exato");
    }

    // ---- Tune (taxa de reprodução) ----
    {
        std::vector<float> ramp(100);
        for (int i = 0; i < 100; ++i) ramp[i] = i * 0.01f;
        // +12 semitons = taxa 2: sample de 100 frames termina em 50.
        aroma::DrumEngine up;
        up.setKit(makeSingleKit(ramp));
        aroma::InstrumentParams p; p.tune = 12.0f;
        up.updateInstrument(36, p);
        up.noteOn(36, 127);
        std::vector<float> u1(60 * 2, 0.0f);
        up.render(u1.data(), 60, 2);
        check(up.activeVoices() == 0, "tune +12 termina em metade do tempo");
        // -12 semitons = taxa 0.5: precisa de 200 frames.
        aroma::DrumEngine down;
        down.setKit(makeSingleKit(ramp));
        aroma::InstrumentParams pd; pd.tune = -12.0f;
        down.updateInstrument(36, pd);
        down.noteOn(36, 127);
        std::vector<float> d1(100 * 2, 0.0f);
        down.render(d1.data(), 100, 2);
        check(down.activeVoices() == 1, "tune -12 ainda toca apos 100 frames");
        std::vector<float> d2(150 * 2, 0.0f);
        down.render(d2.data(), 150, 2);
        check(down.activeVoices() == 0, "tune -12 termina em 200 frames");
        // Interpolação linear: taxa 0.5, frame 1 lê a posição 0.5 da
        // rampa (entre 0.00 e 0.01) = 0.005 * 0.707 (pan) = 0.00354.
        // Sem interpolação seria 0.0 ou 0.00707.
        aroma::DrumEngine interp;
        interp.setKit(makeSingleKit(ramp));
        interp.updateInstrument(36, pd);
        interp.noteOn(36, 127);
        std::vector<float> ib(4 * 2, 0.0f);
        interp.render(ib.data(), 4, 2);
        check(ib[2] > 0.003f && ib[2] < 0.0041f, "tune usa interpolacao linear");
    }

    // ---- Camadas por velocity (em memória) ----
    {
        aroma::Kit kit;
        kit.name = "layers";
        aroma::KitInstrument k;
        k.name = "kick"; k.note = 36; k.file = "kick.wav";
        k.variations.push_back(makeWav(0.5f));
        k.softLayer = makeWav(0.25f);
        k.hardLayer = makeWav(0.9f);
        k.hasLayers = true;
        kit.instruments = {k};
        aroma::DrumEngine e1, e2;
        e1.setKit(kit);
        e2.setKit(kit);
        std::vector<float> soft(480 * 2, 0.0f), hard(480 * 2, 0.0f);
        e1.noteOn(36, 30); e1.render(soft.data(), 480, 2);
        e2.noteOn(36, 100); e2.render(hard.data(), 480, 2);
        check(maxAbs(soft) < 0.06f, "velocity 30 usa camada soft");
        check(maxAbs(hard) > 0.4f, "velocity 100 usa camada hard");
    }

    // ---- API ao vivo: update/instrumentParams ----
    {
        aroma::DrumEngine e;
        e.setKit(makeTestKit());
        aroma::InstrumentParams p;
        check(!e.updateInstrument(99, p), "update em nota inexistente falha");
        check(e.updateInstrument(36, p), "update em nota existente ok");
        aroma::InstrumentParams out;
        check(e.instrumentParams(36, out), "instrumentParams le a peca");
        check(out.volume == 1.0f && out.eqMidFreq == 1000.0f, "params padrao lidos");
        p.volume = 0.25f; p.tune = -5.0f; p.eqLowDb = 6.0f;
        e.updateInstrument(36, p);
        check(e.instrumentParams(36, out) && out.volume == 0.25f && out.tune == -5.0f &&
              out.eqLowDb == 6.0f, "update muda os params ao vivo");
        // Vale para o próximo hit: volume 0.25 (EQ zerado aqui, para
        // isolar o volume) => 0.5 * 0.25 * 0.707 ~= 0.088.
        aroma::InstrumentParams pv;
        pv.volume = 0.25f;
        e.updateInstrument(36, pv);
        std::vector<float> b(480 * 2, 0.0f);
        e.noteOn(36, 127);
        e.render(b.data(), 480, 2);
        check(maxAbs(b) > 0.08f && maxAbs(b) < 0.10f, "volume 0.25 vale no proximo hit");
    }
    return failures;
}

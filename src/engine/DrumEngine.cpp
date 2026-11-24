#include "DrumEngine.h"
#include <algorithm>
#include <cmath>

namespace aroma {

DrumEngine::DrumEngine() = default;
DrumEngine::~DrumEngine() = default;

bool DrumEngine::setKit(Kit kit) {
    if (kit.instruments.empty()) return false;
    // Fora da thread de áudio: é seguro trocar o kit aqui; a casca deve
    // garantir que render() não esteja rodando (parar o stream antes).
    allNotesOff();
    kit_ = std::move(kit);
    hasKit_ = true;
    sampleRate_ = kEngineSampleRate; // samples já chegam a 48 kHz (loadKit)
    for (const auto& inst : kit_.instruments) applyLiveParams(inst);
    return true;
}

void DrumEngine::applyLiveParams(const KitInstrument& inst) {
    if (inst.note < 0 || inst.note > 127) return;
    LiveParams& lp = live_[static_cast<size_t>(inst.note)];
    lp.volume.store(inst.volume, std::memory_order_relaxed);
    lp.pan.store(inst.pan, std::memory_order_relaxed);
    lp.tune.store(inst.tune, std::memory_order_relaxed);
    lp.gamma.store(inst.gamma, std::memory_order_relaxed);
    lp.eqLowDb.store(inst.eqLowDb, std::memory_order_relaxed);
    lp.eqMidDb.store(inst.eqMidDb, std::memory_order_relaxed);
    lp.eqMidFreq.store(inst.eqMidFreq, std::memory_order_relaxed);
    lp.eqHighDb.store(inst.eqHighDb, std::memory_order_relaxed);
}

namespace {
float clampParam(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}
} // namespace

bool DrumEngine::updateInstrument(int note, const InstrumentParams& params) {
    if (note < 0 || note > 127) return false;
    KitInstrument* inst = kit_.findByNote(note);
    if (!inst) return false;
    LiveParams& lp = live_[static_cast<size_t>(note)];
    lp.volume.store(clampParam(params.volume, 0.0f, 2.0f), std::memory_order_relaxed);
    lp.pan.store(clampParam(params.pan, -1.0f, 1.0f), std::memory_order_relaxed);
    lp.tune.store(clampParam(params.tune, -24.0f, 24.0f), std::memory_order_relaxed);
    lp.gamma.store(clampParam(params.gamma, 0.1f, 4.0f), std::memory_order_relaxed);
    lp.eqLowDb.store(clampParam(params.eqLowDb, -18.0f, 18.0f), std::memory_order_relaxed);
    lp.eqMidDb.store(clampParam(params.eqMidDb, -18.0f, 18.0f), std::memory_order_relaxed);
    lp.eqMidFreq.store(clampParam(params.eqMidFreq, 50.0f, 16000.0f), std::memory_order_relaxed);
    lp.eqHighDb.store(clampParam(params.eqHighDb, -18.0f, 18.0f), std::memory_order_relaxed);
    // Espelha no Kit (o que o saveKit escreve). O callback não lê mais
    // esses campos do Kit — só os atômicos — então não há corrida aqui.
    inst->volume = lp.volume.load(std::memory_order_relaxed);
    inst->pan = lp.pan.load(std::memory_order_relaxed);
    inst->tune = lp.tune.load(std::memory_order_relaxed);
    inst->gamma = lp.gamma.load(std::memory_order_relaxed);
    inst->eqLowDb = lp.eqLowDb.load(std::memory_order_relaxed);
    inst->eqMidDb = lp.eqMidDb.load(std::memory_order_relaxed);
    inst->eqMidFreq = lp.eqMidFreq.load(std::memory_order_relaxed);
    inst->eqHighDb = lp.eqHighDb.load(std::memory_order_relaxed);
    return true;
}

bool DrumEngine::instrumentParams(int note, InstrumentParams& out) const {
    if (note < 0 || note > 127) return false;
    if (!kit_.findByNote(note)) return false;
    const LiveParams& lp = live_[static_cast<size_t>(note)];
    out.volume = lp.volume.load(std::memory_order_relaxed);
    out.pan = lp.pan.load(std::memory_order_relaxed);
    out.tune = lp.tune.load(std::memory_order_relaxed);
    out.gamma = lp.gamma.load(std::memory_order_relaxed);
    out.eqLowDb = lp.eqLowDb.load(std::memory_order_relaxed);
    out.eqMidDb = lp.eqMidDb.load(std::memory_order_relaxed);
    out.eqMidFreq = lp.eqMidFreq.load(std::memory_order_relaxed);
    out.eqHighDb = lp.eqHighDb.load(std::memory_order_relaxed);
    return true;
}

bool DrumEngine::replaceInstrumentSample(int note, const std::string& wavPath,
                                         std::string& error) {
    if (note < 0 || note > 127) { error = "nota fora de 0-127"; return false; }
    KitInstrument* inst = kit_.findByNote(note);
    if (!inst) { error = "sem peca na nota " + std::to_string(note); return false; }
    WavFile wav;
    if (!wav.load(wavPath, error)) return false;
    if (wav.sampleRate() != kEngineSampleRate && !wav.resampleTo(kEngineSampleRate)) {
        error = "falha ao reamostrar para 48 kHz: " + wavPath;
        return false;
    }
    const size_t slash = wavPath.find_last_of("/\\");
    inst->file = slash == std::string::npos ? wavPath : wavPath.substr(slash + 1);
    inst->variations.clear();
    inst->variations.push_back(std::move(wav));
    inst->nextVariation = 0;
    return true;
}

bool DrumEngine::addInstrument(const KitInstrument& inst) {
    if (inst.note < 0 || inst.note > 127) return false;
    if (kit_.findByNote(inst.note)) return false; // nota já ocupada
    if (inst.variations.empty() || !inst.variations[0].loaded()) return false;
    kit_.instruments.push_back(inst);
    applyLiveParams(kit_.instruments.back());
    hasKit_ = true;
    return true;
}

bool DrumEngine::removeInstrument(int note) {
    if (note < 0 || note > 127) return false;
    auto& v = kit_.instruments;
    for (auto it = v.begin(); it != v.end(); ++it) {
        if (it->note == note) {
            v.erase(it);
            LiveParams& lp = live_[static_cast<size_t>(note)];
            lp.volume.store(1.0f, std::memory_order_relaxed);
            lp.pan.store(0.0f, std::memory_order_relaxed);
            lp.tune.store(0.0f, std::memory_order_relaxed);
            lp.gamma.store(1.0f, std::memory_order_relaxed);
            lp.eqLowDb.store(0.0f, std::memory_order_relaxed);
            lp.eqMidDb.store(0.0f, std::memory_order_relaxed);
            lp.eqMidFreq.store(1000.0f, std::memory_order_relaxed);
            lp.eqHighDb.store(0.0f, std::memory_order_relaxed);
            return true;
        }
    }
    return false;
}

bool DrumEngine::loadKitFromFile(const std::string& kitFilePath, std::string& error) {
    Kit kit;
    if (!loadKit(kitFilePath, kit, error)) return false; // kit atual continua
    if (!setKit(std::move(kit))) { error = "kit vazio: " + kitFilePath; return false; }
    return true;
}

bool DrumEngine::saveKitToFile(const std::string& kitFilePath, std::string& error) {
    if (!hasKit_) { error = "nenhum kit carregado"; return false; }
    return saveKit(kit_, kitFilePath, error);
}

void DrumEngine::noteOn(int note, int velocity) {
    if (note < 0 || note > 127) return;
    if (velocity < 1) velocity = 1;
    if (velocity > 127) velocity = 127;
    const size_t w = writeIndex_.load(std::memory_order_relaxed);
    const size_t r = readIndex_.load(std::memory_order_acquire);
    if (w - r >= kQueueSize) { droppedEvents_.fetch_add(1, std::memory_order_relaxed); return; }
    queue_[w & (kQueueSize - 1)] = QueuedEvent{static_cast<uint8_t>(note),
                                                static_cast<uint8_t>(velocity), true};
    writeIndex_.store(w + 1, std::memory_order_release);
}

void DrumEngine::noteOff(int note) {
    // One-shot de propósito: ignora. Ver DrumEngine.h — a Aroma manda
    // Note Off ~19 ms depois do Note On; enfileirar o Off cortaria o
    // sample no meio e estalaria. (void)note evita warning de não usado.
    (void)note;
}

void DrumEngine::allNotesOff() {
    // Chamado fora do callback (troca de kit / parada). Zera a fila e as vozes.
    writeIndex_.store(0, std::memory_order_relaxed);
    readIndex_.store(0, std::memory_order_relaxed);
    for (auto& v : voices_) v.active = false;
}

int DrumEngine::activeVoices() const {
    int n = 0;
    for (const auto& v : voices_) if (v.active) ++n;
    return n;
}

void DrumEngine::chokeGroup(const std::string& group, int exceptNote) {
    if (group == "none" || group.empty()) return;
    // Não corta seco: um fade de ~3 ms. Cortar no meio do sample (a
    // voz em amplitude alta) produz um estalo — o mesmo defeito do
    // Note Off, só que aqui o corte é intencional (hi-hat aberto
    // fechado pelo pedal, prato cortado).
    for (auto& v : voices_) {
        if (v.active && !v.fading && v.chokeGroup == group && v.note != exceptNote) {
            v.fading = true;
            v.fadePos = 0;
        }
    }
}

void DrumEngine::startVoice(const KitInstrument& inst, int velocity) {
    // Parâmetros "vivos" (a UI pode mexer com o áudio rodando): a voz
    // tira uma cópia aqui; depois disso, só o próximo hit vê a mudança.
    const LiveParams& lp = live_[static_cast<size_t>(inst.note)];
    const float gamma = lp.gamma.load(std::memory_order_relaxed);
    // Curva de velocity: v' = (v/127)^gamma.
    const float vNorm = static_cast<float>(velocity) / 127.0f;
    const float curved = std::pow(vNorm, gamma);

    // Camada por velocity, quando o kit tem.
    const WavFile* wav = nullptr;
    if (inst.hasLayers) {
        wav = velocity < 64 ? &inst.softLayer : &inst.hardLayer;
    } else {
        // Round-robin entre variações.
        KitInstrument& mutableInst = *kit_.findByNote(inst.note);
        const size_t idx = mutableInst.nextVariation % mutableInst.variations.size();
        mutableInst.nextVariation++;
        wav = &mutableInst.variations[idx];
    }
    if (!wav || !wav->loaded()) return;

    // Choke: o novo hit corta o grupo (hi-hat aberto x fechado, pratos).
    chokeGroup(inst.chokeGroup, -1);

    // Voz livre; se não houver, rouba a mais antiga.
    Voice* slot = nullptr;
    uint64_t oldest = UINT64_MAX;
    Voice* oldestVoice = nullptr;
    for (auto& v : voices_) {
        if (!v.active) { slot = &v; break; }
        if (v.startedAt < oldest) { oldest = v.startedAt; oldestVoice = &v; }
    }
    if (!slot) slot = oldestVoice;
    if (!slot) return;

    const float pan = lp.pan.load(std::memory_order_relaxed);
    slot->active = true;
    slot->data = wav->data();
    slot->lengthFrames = wav->frames();
    slot->channels = wav->channels();
    slot->pos = 0.0;
    // Afinação: taxa de reprodução 2^(tune/12) (varispeed: afinar para
    // cima encurta o som, para baixo alonga — como um tambor de verdade).
    slot->rate = std::pow(2.0, static_cast<double>(lp.tune.load(std::memory_order_relaxed)) / 12.0);
    slot->gain = curved * lp.volume.load(std::memory_order_relaxed);
    // Pan por lei de potência constante (equal-power).
    const float angle = (pan + 1.0f) * 0.7853981634f; // 0..pi/2
    slot->panL = std::cos(angle);
    slot->panR = std::sin(angle);
    // EQ: coeficientes calculados uma vez por hit (não por sample).
    Eq3Band eq;
    eq.setup(lp.eqLowDb.load(std::memory_order_relaxed),
             lp.eqMidDb.load(std::memory_order_relaxed),
             lp.eqMidFreq.load(std::memory_order_relaxed),
             lp.eqHighDb.load(std::memory_order_relaxed),
             static_cast<double>(kEngineSampleRate));
    slot->eqL = eq;
    slot->eqR = eq;
    slot->eqBypass = eq.bypass;
    slot->chokeGroup = inst.chokeGroup;
    slot->note = inst.note;
    slot->startedAt = ++voiceCounter_;
    slot->fading = false;
    slot->fadePos = 0;
}

void DrumEngine::render(float* out, int frames, int outChannels) {
    if (!out || frames <= 0) return;
    const int total = frames * outChannels;
    for (int i = 0; i < total; ++i) out[i] = 0.0f;
    if (!hasKit_ || muted_.load(std::memory_order_relaxed)) return;

    // Consome os eventos MIDI enfileirados (SPSC: só render lê aqui).
    size_t r = readIndex_.load(std::memory_order_relaxed);
    const size_t w = writeIndex_.load(std::memory_order_acquire);
    while (r != w) {
        const QueuedEvent ev = queue_[r & (kQueueSize - 1)];
        ++r;
        // Só chegam eventos On aqui: noteOff() é no-op (one-shot) e
        // nunca enfileira Off. Se um Off aparecer, é ignorado.
        if (ev.on) {
            const KitInstrument* inst = kit_.findByNote(ev.note);
            if (inst) startVoice(*inst, ev.velocity);
        }
    }
    readIndex_.store(r, std::memory_order_release);

    const float master = masterGain_.load(std::memory_order_relaxed);
    for (auto& v : voices_) {
        if (!v.active) continue;
        const double len = static_cast<double>(v.lengthFrames);
        if (len <= 0.0) { v.active = false; continue; }
        // Frames de saída que faltam: com tune, a duração muda
        // (taxa 2 = metade do tempo, taxa 0.5 = o dobro).
        const double remainingOut = (len - v.pos) / v.rate;
        int n = frames;
        if (remainingOut < static_cast<double>(n))
            n = static_cast<int>(std::ceil(remainingOut));
        if (n <= 0) {
            if (v.pos >= len) v.active = false;
            continue;
        }
        for (int i = 0; i < n; ++i) {
            // Interpolação linear na posição fracionária (tune).
            const double p = v.pos;
            size_t i0 = static_cast<size_t>(p);
            if (i0 >= v.lengthFrames) i0 = v.lengthFrames - 1;
            const size_t i1 = (i0 + 1 < v.lengthFrames) ? i0 + 1 : i0;
            const float frac = static_cast<float>(p - static_cast<double>(i0));
            float s, sR;
            if (v.channels == 2) {
                const float* d = v.data;
                s = d[i0 * 2] + (d[i1 * 2] - d[i0 * 2]) * frac;
                sR = d[i0 * 2 + 1] + (d[i1 * 2 + 1] - d[i0 * 2 + 1]) * frac;
            } else {
                s = v.data[i0] + (v.data[i1] - v.data[i0]) * frac;
                sR = s;
            }
            // EQ por canal (sample mono: filtra uma vez só).
            if (!v.eqBypass) {
                s = v.eqL.process(s);
                sR = (v.channels == 2) ? v.eqR.process(sR) : s;
            }
            // Fade de choke: rampa linear 1 -> 0 em kFadeFrames.
            float fade = 1.0f;
            if (v.fading) {
                fade = 1.0f - static_cast<float>(v.fadePos + i) /
                                  static_cast<float>(kFadeFrames);
                if (fade < 0.0f) fade = 0.0f;
            }
            if (outChannels == 2) {
                out[i * 2] += s * v.gain * v.panL * master * fade;
                out[i * 2 + 1] += sR * v.gain * v.panR * master * fade;
            } else {
                out[i] += (s * v.panL + sR * v.panR) * 0.5f * v.gain * master * fade;
            }
            v.pos += v.rate;
        }
        if (v.fading) {
            v.fadePos += n;
            if (v.fadePos >= kFadeFrames) v.active = false;
        }
        if (v.pos >= len) v.active = false;
    }
    // Soft-clip + flag de clipping (nunca deixa passar de [-1, 1]).
    for (int i = 0; i < total; ++i) {
        float s = out[i];
        if (s > 0.99f || s < -0.99f) clipped_.store(true, std::memory_order_relaxed);
        // tanh aproximado: x / (1 + |x|) é barato e suave; aplica só acima de 0.8
        // para não colorir o som em nível normal.
        if (s > 0.8f) s = 0.8f + (s - 0.8f) / (1.0f + (s - 0.8f));
        else if (s < -0.8f) s = -0.8f + (s + 0.8f) / (1.0f + (-s - 0.8f));
        out[i] = s < -1.0f ? -1.0f : (s > 1.0f ? 1.0f : s);
    }
}

} // namespace aroma

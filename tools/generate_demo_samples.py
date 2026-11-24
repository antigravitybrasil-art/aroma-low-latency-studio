#!/usr/bin/env python3
"""Gera os samples WAV do kit demo — sintetizados, originais, livres de direitos.

Nenhum sample comercial é usado. Cada som é síntese feita offline, uma
única vez, neste script.
Uso: python3 tools/generate_demo_samples.py
Saída: kits/demo-kit/samples/*.wav — 48 kHz, 16 bits, mono.

v2 (23/11/2026): a v1 era seno puro + ruído branco — o Allex testou o
standalone e ouviu "um sinal", não bateria. Um tambor de verdade tem
(1) um TRANSIENTE de ataque (o golpe da baqueta: ruído curto e agudo
nos primeiros 5–10 ms), (2) corpo com parciais INARMÔNICOS (não um
seno puro) e (3) harmônicos que sobrevivem em falante pequeno. Esta
versão adiciona os três: click de ataque em tudo que é tambor, ruído
com passa-banda (não branco cru) na caixa, e parciais inarmônicos nos
pratos/bell — o bell da v1 era literalmente um acorde de senos.
"""
import math
import os
import random
import struct
import wave

SR = 48000
OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                   "kits", "demo-kit", "samples")


def save(name, samples):
    os.makedirs(OUT, exist_ok=True)
    path = os.path.join(OUT, name)
    peak = max((abs(s) for s in samples), default=0.0) or 1.0
    scale = 0.9 / peak if peak > 0.9 else 1.0
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        frames = b"".join(
            struct.pack("<h", max(-32767, min(32767, int(s * scale * 32767))))
            for s in samples)
        w.writeframes(frames)
    print(f"  {name}: {len(samples) / SR:.2f}s")


def sine_pitch_drop(dur, f0, f1, amp=1.0, decay=6.0, detune=1.0, harmonic2=0.0):
    """Seno com pitch descendente + 2º harmônico opcional (ajuda o grave
    a aparecer em falante pequeno, onde a fundamental some)."""
    n = int(SR * dur)
    out = []
    phase = 0.0
    phase2 = 0.0
    for i in range(n):
        t = i / SR
        f = f0 * detune + (f1 * detune - f0 * detune) * (t / dur)
        phase += 2 * math.pi * f / SR
        phase2 += 2 * math.pi * (2 * f) / SR
        env = math.exp(-decay * t)
        out.append(amp * (math.sin(phase) + harmonic2 * math.sin(phase2)) * env)
    return out


def noise(dur, amp=1.0, decay=10.0, hp_alpha=0.0, seed=1):
    rnd = random.Random(seed)
    n = int(SR * dur)
    out = []
    prev_in = 0.0
    prev_out = 0.0
    for i in range(n):
        t = i / SR
        x = rnd.uniform(-1, 1)
        # high-pass de 1 polo (remove o grave do ruído p/ hats/crash)
        y = hp_alpha * (prev_out + x - prev_in) if hp_alpha else x
        prev_in, prev_out = x, y
        out.append(amp * y * math.exp(-decay * t))
    return out


def bandpass(x, freq, q=0.8):
    """Passa-banda biquad (RBJ) — o ruído da caixa precisa de banda,
    não de branco cru: branco cru soa como chiado de TV, não como caixa."""
    w0 = 2 * math.pi * freq / SR
    alpha = math.sin(w0) / (2 * q)
    cosw = math.cos(w0)
    b0 = alpha
    b1 = 0.0
    b2 = -alpha
    a0 = 1 + alpha
    a1 = -2 * cosw
    a2 = 1 - alpha
    out = []
    x1 = x2 = y1 = y2 = 0.0
    for v in x:
        y = (b0 * v + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2) / a0
        x2, x1, y2, y1 = x1, v, y1, y
        out.append(y)
    return out


def click(dur=0.008, amp=1.0, freq=2500.0, seed=7):
    """Transiente de ataque: o golpe da baqueta. Ruído curto + blip agudo
    com decaimento rapidíssimo. Sem isso, tambor vira seno puro (tom)."""
    rnd = random.Random(seed)
    n = int(SR * dur)
    out = []
    phase = 0.0
    for i in range(n):
        t = i / SR
        phase += 2 * math.pi * freq / SR
        env = math.exp(-t * 700.0)
        out.append(amp * (0.6 * rnd.uniform(-1, 1) + 0.4 * math.sin(phase)) * env)
    return out


def mix(a, b):
    n = max(len(a), len(b))
    return [(a[i] if i < len(a) else 0.0) + (b[i] if i < len(b) else 0.0)
            for i in range(n)]


def metallic(dur, freqs, amp=0.5, decay=4.0):
    """Parciais INARMÔNICOS (números que não são múltiplos entre si):
    é o que faz um prato soar como metal e não como acorde de teclado."""
    n = int(SR * dur)
    out = []
    for i in range(n):
        t = i / SR
        s = sum(math.sin(2 * math.pi * f * t) / len(freqs) for f in freqs)
        out.append(amp * s * math.exp(-decay * t))
    return out


def main():
    print("Gerando samples demo v2 em", OUT)
    # Bumbo: seno 160→48 + click de batedor + 2º harmônico (falante pequeno)
    save("kick.wav", mix(sine_pitch_drop(0.35, 160, 48, amp=1.0, decay=9.0, harmonic2=0.25),
                         click(0.008, amp=0.9, freq=1800.0, seed=5)))
    save("kick_2.wav", mix(sine_pitch_drop(0.35, 158, 47, amp=1.0, decay=9.0, detune=1.01, harmonic2=0.25),
                           click(0.008, amp=0.9, freq=1750.0, seed=6)))
    # Caixa: corpo 195→150 + ruído COM passa-banda em 1800 Hz + snap inicial
    snare_noise = bandpass(noise(0.18, amp=1.0, decay=22.0, seed=11), 1800.0, q=0.8)
    save("snare.wav", mix(mix(sine_pitch_drop(0.18, 195, 150, amp=0.6, decay=18.0),
                               [s * 0.8 for s in snare_noise]),
                          click(0.005, amp=0.7, freq=3200.0, seed=15)))
    snare_noise2 = bandpass(noise(0.18, amp=1.0, decay=22.0, seed=12), 1800.0, q=0.8)
    save("snare_2.wav", mix(mix(sine_pitch_drop(0.18, 193, 148, amp=0.6, decay=18.0),
                                 [s * 0.8 for s in snare_noise2]),
                            click(0.005, amp=0.7, freq=3100.0, seed=16)))
    # Aro: ataque seco e agudo
    save("rim.wav", mix(sine_pitch_drop(0.06, 1700, 900, amp=0.7, decay=60.0),
                         click(0.004, amp=0.8, freq=4500.0, seed=17)))
    # Tons: seno com pitch + click + 2º harmônico
    save("tom1.wav", mix(sine_pitch_drop(0.30, 230, 150, amp=0.9, decay=8.0, harmonic2=0.2),
                         click(0.006, amp=0.6, freq=2200.0, seed=18)))
    save("tom2.wav", mix(sine_pitch_drop(0.30, 185, 120, amp=0.9, decay=8.0, harmonic2=0.2),
                         click(0.006, amp=0.6, freq=2000.0, seed=19)))
    save("floor_tom.wav", mix(sine_pitch_drop(0.35, 140, 85, amp=0.95, decay=7.0, harmonic2=0.25),
                              click(0.006, amp=0.6, freq=1800.0, seed=20)))
    # Chimbau: ruído HP + borda metálica (parciais agudos)
    save("hihat_closed.wav", mix(noise(0.06, amp=0.8, decay=55.0, hp_alpha=0.92, seed=21),
                                 metallic(0.06, [7800, 10300, 12900], amp=0.15, decay=55.0)))
    save("hihat_closed_2.wav", mix(noise(0.06, amp=0.8, decay=55.0, hp_alpha=0.92, seed=22),
                                   metallic(0.06, [7900, 10400, 13100], amp=0.15, decay=55.0)))
    save("hihat_open.wav", mix(noise(0.40, amp=0.8, decay=7.0, hp_alpha=0.92, seed=23),
                               metallic(0.40, [7800, 10300, 12900], amp=0.12, decay=7.0)))
    save("hihat_pedal.wav", noise(0.04, amp=0.9, decay=80.0, hp_alpha=0.92, seed=24))
    # Crash: ruído HP + parciais inarmônicos densos
    save("crash.wav", mix(noise(1.4, amp=0.7, decay=3.2, hp_alpha=0.85, seed=31),
                          metallic(1.0, [3800, 5130, 6270, 7740, 9210], amp=0.3, decay=4.0)))
    # Ride: ruído + parciais inarmônicos + ping inicial
    save("ride.wav", mix(mix(noise(0.9, amp=0.35, decay=5.0, hp_alpha=0.9, seed=32),
                             metallic(0.9, [2930, 4170, 5390, 6830, 8470], amp=0.45, decay=4.5)),
                        click(0.004, amp=0.5, freq=5200.0, seed=33)))
    # Bell: parciais INARMÔNICOS (1 : 2,76 : 3,89 : 5,4) + ping forte.
    # A v1 usava 820/1240/1870 — quase harmônicos, soava como acorde.
    save("ride_bell.wav", mix(metallic(0.6, [810, 2230, 3150, 4370], amp=0.8, decay=6.0),
                             click(0.005, amp=0.9, freq=3600.0, seed=34)))
    print("OK — 15 arquivos WAV (kit demo v2, 48 kHz).")


if __name__ == "__main__":
    main()

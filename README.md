# AROMA LOW LATENCY STUDIO

Instrumento virtual de bateria eletrônica de baixa latência para Windows 11,
controlado por uma bateria eletrônica **Aroma TDX-18S** via USB MIDI.

Dois modos, **um só motor de áudio** (`src/engine/`):

| Modo | O quê | Estado |
|---|---|---|
| **A — Standalone Win32** | App independente, sem DAW | ✅ v0.4.0 funcionando (confirmado pelo Allex em 23/11/2026) |
| **A — AromaStudio (JUCE)** | Interface premium, mesmo motor | ⚠️ v0.5.0: código pronto em `src/app-juce/`; o EXE só sai do build no Visual Studio — ver `docs/BUILD_JUCE_ZIG.md` |
| **B — Plugin VST3** | Instrumento VST3 para o Reaper | Etapa 6 — esqueleto JUCE pronto, build após validar o motor |

> **Regra de ouro deste projeto:** nada é anunciado como "pronto" antes de ser
> compilado e testado. A Etapa 1 não é o instrumento — é o **diagnóstico**:
> provar que a Aroma aparece como dispositivo MIDI, que os pads enviam as
> notas esperadas e qual latência o caminho de áudio do PC realmente entrega.

## Estado atual — v0.5.0 (23/11/2026)

- ✅ Diagnóstico encerrado: Aroma detectada ("DDRUM", canal 10), mapa MIDI
  real completo (12 notas), WASAPI Shared é o caminho (Exclusive esgotado)
- ✅ Standalone v0.4.0 confirmado pelo Allex ("funcionou perfeitamente")
- ✅ Motor Fase A: cadeia por peça (volume/pan/tune/EQ 3 bandas), upload de
  samples, kits v2, camadas por velocity — 144 testes unitários passando
- ✅ Interface premium JUCE (Fase B, v0.5.0): código completo em
  `src/app-juce/` (bateria que acende ao vivo, editor por peça, kits,
  master/latência/underruns) — verificado por build Linux + screenshot
- ⚠️ `AromaStudio.exe` **não** está no ZIP: o JUCE 8 não compila com o Zig
  (MinGW recusado pelo JUCE; target MSVC sem headers vcruntime) — o EXE
  sai do build no PC dele com Visual Studio 2022, ver
  `docs/BUILD_JUCE_ZIG.md`
- ⏳ VST3: Etapa 6 — esqueleto JUCE pronto, build após validar o motor

## O que este repositório NÃO faz (honestidade técnica)

- **Não detecta o PC do Allex a partir daqui.** Esta VM é Linux; não há
  Aroma, interface USB Audio CODEC nem Windows 11 aqui. O diagnóstico do
  computador dele só acontece quando ele rodar o `AromaDiagnostico.exe`
  no PC dele — o app foi feito exatamente para isso.
- **Não promete latência antes de medir.** O app distingue 3 coisas:
  1. *latência de processamento* (tempo do callback de áudio),
  2. *latência estimada de saída* (buffer + período do dispositivo),
  3. *latência física de ida e volta* (só com medição loopback — **não**
     apresentada como medida; o app explica como medi-la).
- **Não elimina drivers do Windows.** O caminho é WASAPI Exclusive
  (menor sobrecarga disponível sem driver ASIO nativo) com fallback para
  WASAPI Shared. ASIO4ALL não é obrigatório nem assumido.
- **Não usa Python no motor de áudio.** Python só gera os samples demo
  (offline, uma vez) em `tools/generate_demo_samples.py`.

## Estrutura

```
AROMA_LOW_LATENCY_STUDIO/
├── CMakeLists.txt            # build Windows (MSVC); alvo `diagnostico` não precisa de JUCE
├── build-windows.bat / .ps1  # scripts de compilação para Windows 11
├── src/
│   ├── common/               # WavFile, Kit, tipos MIDI, SystemInfo (portátil onde dá)
│   ├── engine/               # DrumEngine — o motor compartilhado (standalone + VST3)
│   ├── diagnostic/           # App de diagnóstico (Etapa 2) — WinMM + WASAPI
│   ├── standalone/           # App standalone (Etapa 3/5) — usa DrumEngine
│   └── plugin/               # VST3 via JUCE (Etapa 6) — usa DrumEngine
├── kits/demo-kit/            # kit demo: kit.txt + samples WAV sintetizados
├── tests/                    # testes unitários do motor (rodam também no Linux)
├── tools/                    # gerador dos samples demo
├── docs/                     # ARQUITETURA.md, DIAGNOSTICO.md
├── builds/                   # saída dos builds (vazia no repo)
├── INSTALL.md  CHANGELOG.md  RELATORIO_DE_TESTES.md
```

## Como começar (no PC Windows 11)

1. Leia `INSTALL.md`.
2. Rode `build-windows.bat` (precisa do Visual Studio 2022 + CMake).
3. Ligue a Aroma TDX-18S via USB, rode `builds\Release\AromaDiagnostico.exe`.
4. Siga `docs/DIAGNOSTICO.md` e exporte o relatório TXT/CSV.
5. Me envie o relatório — ele define as configurações reais (buffer,
   taxa de amostragem, mapa MIDI) das Etapas 3 em diante.

## Mapa MIDI inicial da Aroma (HIPÓTESE — confirmar no diagnóstico)

Base: General MIDI para bateria (o padrão que a maioria dos módulos segue).
O manual da TDX-18S não publica a tabela de notas — por isso o diagnóstico
existe. Se as notas reais forem outras, o mapa muda em `kits/demo-kit/kit.txt`
e em `src/common/MidiMap.h`, sem tocar no motor.

| Pad / prato | Nota GM provável | Instrumento |
|---|---|---|
| Bumbo | 36 (C1) | Kick |
| Caixa (centro) | 38 (D1) | Snare |
| Aro da caixa | 37 (C#1) | Rim / side stick |
| Tom 1 | 48 (C2) | Tom alto |
| Tom 2 | 45 (A1) | Tom médio |
| Tom de chão | 41 (F1) | Floor tom |
| Chimbal fechado | 42 (F#1) | Hi-hat closed |
| Chimbal aberto | 46 (A#1) | Hi-hat open |
| Pedal do chimbal | 44 (G#1) | Hi-hat pedal |
| Crash | 49 (C#2) | Crash |
| Ride (corpo) | 51 (D#2) | Ride |
| Ride (bell) | 53 (F2) | Ride bell |

## Licenças

- Código próprio: uso do Allex, sem restrição interna.
- JUCE (Etapas 5–6): avaliar antes do build final — JUCE Personal é
  gratuito sob GPLv3 (código aberto) ou licença paga para fechado/comercial.
  O alvo `diagnostico` **não usa JUCE** e não tem essa dependência.
- Samples demo: sintetizados por `tools/generate_demo_samples.py`, livres.
- Nenhum sample comercial é usado neste projeto.

## Histórico

Ver `CHANGELOG.md`.

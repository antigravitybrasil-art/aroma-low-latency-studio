# ARQUITETURA — AROMA LOW LATENCY STUDIO

## Visão geral

```
Aroma TDX-18S --USB MIDI--> [WinMM midiIn] --> DrumEngine --> [WASAPI] --> USB Audio CODEC
                                        |                         ^
                                        | (mesmo motor)           |
Reaper --MIDI--> [VST3 / JUCE] ---------+                         |
                                        +---- samples WAV (kits) --+
```

Um só motor (`src/engine/DrumEngine`), duas "cascas":

- **Standalone:** app JUCE (Etapa 5) com o motor dentro; abre o dispositivo
  WASAPI diretamente.
- **VST3:** plugin JUCE (Etapa 6) com o mesmo motor dentro; o Reaper abre o
  dispositivo de áudio — o plugin só recebe MIDI e devolve áudio no callback
  do host. Por isso os dois modos **não** disputam o dispositivo em modo
  exclusivo ao mesmo tempo: cada um usa o áudio do seu jeito, e o briefing
  proíbe presumir uso simultâneo — eles funcionam independentemente.

## Stack decidida (Etapa 1)

| Camada | Escolha | Por quê |
|---|---|---|
| Linguagem | C++17 | Padrão da indústria de áudio; controle de alocação/latência |
| Build | CMake 3.20+ + MSVC v143 (VS 2022) | Reprodutível no Windows; o mesmo CMake gera o VST3 depois |
| MIDI (diagnóstico) | WinMM (`midiIn*`) | Já existe no Windows, zero driver, suficiente para detectar e monitorar |
| Áudio (diagnóstico + standalone) | WASAPI (`IAudioClient`, event-driven) | Menor latência sem ASIO nativo; Exclusive quando o dispositivo aceita, Shared como fallback |
| GUI + VST3 (Etapas 5–6) | JUCE 8 | Um framework só resolve GUI, VST3, MIDI e áudio nos dois modos; alternativa seria escrever GUI Win32 + VST3 SDK na mão (meses a mais) |
| Samples | WAV PCM 16/24 bits | Formato universal; carregado inteiro para a RAM antes de tocar |
| Motor | Próprio, em `src/engine/` | Não depende do JUCE: o mesmo `.cpp` compila no standalone, no VST3 e nos testes |

### Licenciamento JUCE — avaliado, não ignorado

- JUCE Personal: gratuito se o projeto for distribuído sob **GPLv3**
  (código-fonte aberto) e a receita ficar abaixo do teto da licença.
- JUCE Pro / Indie: licença paga, permite código fechado/comercial.
- **Decisão (Allex, 23/11/2026): GPLv3 — grátis, código aberto.** A
  interface premium (Fase B) e o VST3 (Etapa 6) são feitos em JUCE sob
  GPLv3. O diagnóstico e o motor continuam sem JUCE.
- **Build do app JUCE:** só com Visual Studio no Windows. O Zig não
  compila JUCE 8 (MinGW é recusado pelo JUCE; o target MSVC do Zig não
  tem os headers vcruntime) — ver `docs/BUILD_JUCE_ZIG.md`. O
  standalone Win32 e o diagnóstico continuam com Zig (MinGW).

### Alternativas descartadas (com motivo)

- **RtMidi / PortAudio:** boas bibliotecas, mas somam dependências externas
  ao diagnóstico — que precisa rodar no PC dele sem instalar nada.
  WinMM + WASAPI nativos fazem o mesmo para este caso. RtMidi continua
  como plano B se o WinMM mostrar limitação real.
- **ASIO4ALL:** emula ASIO por cima do WDM; não é driver nativo e pode
  piorar a estabilidade em interface genérica. Não obrigatório; se a
  interface tiver ASIO nativo um dia, entra como terceira opção.
- **Python no motor:** descartado pelo briefing e pela latência — Python
  só gera os samples demo offline.
- **Eliminar drivers do Windows:** impossível e desnecessário — o que se
  faz é usar a API de menor sobrecarga (WASAPI Exclusive, event-driven)
  e evitar camadas (mixer do Windows, resampling) no caminho.

## Motor de áudio (`DrumEngine`) — regras de tempo real

1. **Zero alocação no callback de áudio.** Pool fixo de 32 vozes,
   pré-alocadas no construtor. `noteOn()` só escreve numa fila lock-free
   (anel SPSC) que o callback consome.
2. **Zero acesso a disco no callback.** Samples carregados na RAM no
   `loadKit()` (thread de UI/carregamento), nunca no áudio.
3. **Zero mutex no callback.** Comunicação UI ↔ áudio por fila lock-free
   e atômicos; parâmetros (volume, pan, ganho) via `std::atomic<float>`.
4. **Vozes:** cada `noteOn` ocupa uma voz livre (roubo da voz mais antiga
   se o pool lotar); choke libera com fade de ~3 ms (não corta seco).
   **Bateria é one-shot: `noteOff` é ignorado** — a Aroma manda Note Off
   ~19 ms depois de cada Note On; obedecer a ele cortava todo hit no
   meio do sample (v0.3.2).
5. **Hi-hat:** aberto e fechado estão no mesmo grupo de choke — um
   fechado corta o aberto (com fade); o pedal (nota 44) também corta.
6. **Crash/Ride:** choke por grupo; um segundo hit no mesmo prato reinicia
   (retrigger) em vez de somar em cima.
7. **Velocity:** curva `v' = v^gamma` (gamma configurável por instrumento)
   + camadas: se o kit tiver `kick_soft.wav` / `kick_hard.wav`, a camada
   é escolhida pela velocity (< 64 soft, ≥ 64 hard); senão, só o ganho muda.
8. **Round-robin:** cada instrumento tem N variações do mesmo sample;
   cada hit usa a próxima, para não soar mecânico.
9. **Clipping:** ganho master + soft-clip (`tanh` aproximado) no mix final;
   indicador de clip quando |saída| ≥ 0.99.
10. **Cadeia por peça (v0.4.0):** sample → tune (taxa `2^(tune/12)`,
    interpolação linear) → EQ 3 bandas (lowshelf 200 Hz, peaking com
    freq configurável, highshelf 4 kHz; tudo zerado = bypass) → ganho
    (velocity ^ gamma × volume) → pan (equal-power) → master. Os
    parâmetros numéricos vivem em atômicos por nota (`LiveParams`): a
    UI mexe com o áudio rodando e vale para o próximo hit; a voz tira
    uma cópia no `startVoice`. Mudanças estruturais (trocar sample,
    adicionar/remover peça, trocar o kit) exigem o stream parado.
11. **Taxa do motor (v0.4.0):** sempre 48 kHz. Todo WAV carregado é
    reamostrado para 48 kHz no `loadKit` (interpolação linear);
    arquivo inválido é rejeitado com mensagem, sem travar.

## Formato de kit (`kits/<nome>/kit.txt`)

Formato texto simples (sem dependência de JSON), um instrumento por linha:

```
# nome  nota  arquivo  volume  pan  choke_group  gamma
kick    36    kick.wav  1.0     0.0   none         1.0
snare   38    snare.wav 0.9     0.0   none         1.2
...
hihat_closed 42 hihat_closed.wav 0.8 0.0 hihat    1.0
hihat_open   46 hihat_open.wav   0.8 0.0 hihat    1.0
```

Formato v2 (v0.4.0): os 7 campos acima + tokens opcionais
`chave=valor` (ausentes = padrão; kit v1 carrega igual):
`tune` (semitons), `eq_low` / `eq_mid` / `eq_high` (dB),
`eq_mid_freq` (Hz), `soft=` / `hard=` (arquivos das camadas).
`saveKit()` escreve o kit.txt v2 + copia os WAVs para `samples/`.

Variações round-robin: `snare.wav`, `snare_2.wav`, `snare_3.wav` na mesma
pasta são detectadas automaticamente. Camadas por velocity:
`snare_soft.wav` (velocity < 64) e `snare_hard.wav` (≥ 64) —
detectadas na pasta ou indicadas por `soft=`/`hard=`.

O mapa MIDI vive no kit — trocar o mapa é editar o kit, não o motor.

## Latência — o que é medido e o que é estimado

| Termo | Definição | Como o diagnóstico obtém |
|---|---|---|
| Latência de processamento | Tempo dentro do callback de áudio (mix das vozes) | Medido: timestamp de entrada/saída do callback, em µs |
| Latência estimada de saída | Buffer + período do dispositivo | Calculada: `bufferFrames / sampleRate` (+ período reportado pelo driver) — **estimativa, não medida física** |
| Latência física de ida e volta | Do toque no pad até o som sair da caixa | **Não medida pelo app.** Requer loopback: gravar a saída com microfone/interface enquanto toca, ou medir com osciloscópio. O app explica o procedimento em `docs/DIAGNOSTICO.md` |
| Jitter / underrun | Variação do intervalo entre callbacks; vezes em que o buffer esvaziou | Medido durante o teste de 2 s por configuração |

Metas progressivas (a validar — **não são promessas**):

| Configuração | Estimativa de saída | Uso |
|---|---|---|
| 48 kHz, buffer 64 | ~1,3 ms + período | Só se o PC/interface aguentarem sem underrun |
| 48 kHz, buffer 128 | ~2,7 ms + período | Alvo realista para tocar |
| 48 kHz, buffer 256 | ~5,3 ms + período | Seguro, ainda confortável |
| 48 kHz, buffer 512 | ~10,7 ms + período | Fallback se houver estalos |

Abaixo de ~10 ms de saída estimada, a maioria dos bateristas não percebe
atraso; acima de ~20 ms começa a incomodar. O número que vale é o do
relatório do PC dele, não o desta tabela.

## Interrupções e reinicialização do dispositivo

- O motor tolera `AUDCLNT_E_DEVICE_INVALIDATED` (dispositivo desplugado):
  o callback para de forma segura, a UI é avisada e o usuário pode
  reconectar e apertar "Reiniciar áudio" sem fechar o app.
- Troca de buffer/taxa: o stream é parado, reconfigurado e reiniciado —
  nunca reconfigurado "por dentro" do callback.

## Testes

- `tests/test_engine.cpp`: mixagem, velocity, choke de hi-hat, roubo de voz,
  polifonia — sem Windows, sem áudio real (o motor é portátil).
- `tests/test_wav.cpp`: cabeçalhos WAV 16/24 bits, mono/estéreo.
- Diagnóstico no PC: `docs/DIAGNOSTICO.md` + `RELATORIO_DE_TESTES.md`.

## Fase B — interface premium em JUCE (v0.5.0)

- **Novo app `AromaStudio`** (`src/app-juce/`), sem tocar no standalone
  Win32 (que continua como fallback). Só a UI é JUCE: o motor
  (`DrumEngine`), o áudio (`AudioStream`, WASAPI contínuo) e o MIDI
  (`MidiMonitor`, WinMM) são os mesmos, reaproveitados.
- **Componentes:** `DrumKitView` (foto top-down + flash por peça, clique
  para audição; fallback vetorial sem a imagem), `InstrumentPanel`
  (volume/pan/tune/gamma/EQ 3 bandas + freq do médio, troca de sample),
  `KitBrowser` (novo/salvar/carregar/duplicar), `StatusBar` (master,
  modo/buffer/latência estimada, underruns, MIDI, parar/reiniciar).
- **AppModel** (`src/app-juce/AppModel.*`): estado do editor portátil
  (sem JUCE, sem Windows) — kits, seleção, params ao vivo, upload,
  último hit. A UI lê o modelo por timer (100 ms); a thread de áudio
  nunca é tocada pela UI. Mudanças estruturais (trocar sample/kit)
  param o áudio e recomeçam depois (`withAudioStopped`).
- **Arte:** `assets/drumkit_topdown.png` (1920×1280); zonas de clique
  medidas por visual grounding nos centros reais da foto
  (`kKitZones` em `DrumKitView.h`).
- **Status do EXE:** o `AromaStudio.exe` só sai do build no Visual
  Studio (ver `docs/BUILD_JUCE_ZIG.md`). O código-fonte está completo
  e os componentes de UI foram verificados por build Linux + screenshot
  no Xvfb; a fiação Windows (`MainComponent`) só é validada pelo build
  no VS.

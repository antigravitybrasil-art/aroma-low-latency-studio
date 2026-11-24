# CHANGELOG — AROMA LOW LATENCY STUDIO

Formato: `## [versão] — data — etapa`.

## [0.5.0] — 2026-11-23 — Fase B: interface premium em JUCE (código pronto; EXE pendente)

### Adicionado
- **AromaStudio — novo app JUCE** (`src/app-juce/`, JUCE 8.0.10 sob
  GPLv3, decisão do Allex de 23/11/2026): `Main.cpp`
  (`START_JUCE_APPLICATION`), `MainComponent` (liga AppModel +
  AudioStream WASAPI + MidiMonitor WinMM), `DrumKitView` (foto
  top-down do kit com flash por peça ao vivo, clique para audição,
  fallback vetorial sem a imagem), `InstrumentPanel` (volume, pan,
  afinação, curva de velocity, EQ 3 bandas + freq do médio, botão de
  troca de sample), `KitBrowser` (novo/salvar/carregar/duplicar),
  `StatusBar` (master, modo/buffer/latência estimada, underruns,
  dispositivo MIDI, parar/reiniciar áudio). Reaproveita o núcleo
  pronto — nada de áudio ou MIDI foi reescrito.
- **AppModel** (`src/app-juce/AppModel.*`): estado do editor sem JUCE
  e sem Windows (portátil, testável): carregar/salvar/novo/duplicar/
  listar kits, seleção de peça, params ao vivo, upload de sample,
  último hit + contadores atômicos para a UI.
- **Arte do kit** (`assets/drumkit_topdown.png`, 1920×1280): foto
  top-down gerada com as ferramentas de mídia; zonas de clique medidas
  por visual grounding nos centros reais da foto.
- **Build**: `AROMA_BUILD_JUCE_APP` no CMake raiz (só Windows);
  `src/app-juce/CMakeLists.txt` (JUCE por variável `JUCE_DIR` ou
  FetchContent 8.0.10); `toolchain-zig.cmake` (Zig p/ standalone e
  diagnóstico); `docs/BUILD_JUCE_ZIG.md` com a receita completa.

### ⚠️ Status honesto do EXE (ler antes de procurar o AromaStudio.exe)
- **O `AromaStudio.exe` NÃO está neste ZIP — não foi inventado.**
  O JUCE 8 não compila com o Zig 0.17.1 nesta VM, por dois motivos
  verificados com smoke test (app JUCE mínimo): (1) target MinGW
  (`x86_64-windows-gnu`): o JUCE recusa explicitamente —
  `juce_TargetPlatform.h:113: #error "MinGW is not supported"`;
  (2) target MSVC (`x86_64-windows-msvc`): o libc++ do Zig exige
  `vcruntime_exception.h` / `new.h`, headers do CRT da Microsoft que
  o Zig não traz. O caminho é compilar no PC do Allex com o Visual
  Studio 2022 (`build-windows.bat` ou os comandos em
  `docs/BUILD_JUCE_ZIG.md`); o código-fonte do app está completo no
  ZIP (`src/app-juce/`).
- **O que foi verificado de verdade:** os 4 componentes de UI +
  AppModel + motor compilam limpos contra o JUCE 8 (build Linux de
  checagem, Xvfb) e a janela foi vista em screenshot: foto do kit
  inteira, zonas e rótulos alinhados, acentos corretos, painel da
  caixa com os valores do kit demo. O `MainComponent` (fiação
  Windows: AudioStream/MidiMonitor) foi revisado contra os headers e
  não pôde ser compilado aqui — só o build no VS valida essa fiação.
- **O que continua funcionando:** `AromaStandalone.exe` (v0.4.0,
  fallback, reconstruído deste tree — mesmo binário, mesmo tamanho)
  e `AromaDiagnostico.exe`, ambos no ZIP.

### Corrigido (achado pela auditoria antes da entrega)
- `AlertWindow::runModalLoop()` não existe no JUCE 8 — diálogo de nome
  de kit refeito com `enterModalState` assíncrono.
- `String(const char*)` no JUCE 8 é ASCII, não UTF-8 — literais com
  acento viravam mojibake (`AfinaÃ§Ã£o`); todos os literais não-ASCII
  agora usam `CharPointer_UTF8`. `Font(float, int)` depreciado —
  trocado por `Font(FontOptions(...))`.
- `RectanglePlacement` com `onlyIncreaseInSize` não encolhia a foto
  (ficava cortada); trocado por `onlyReduceInSize` (contain).
- Zonas de clique dos toms estavam altas demais; medidas de novo por
  visual grounding nos centros reais da foto.
- Build Zig do standalone/diagnóstico quebrou ao reconfigurar
  (`undefined symbol: WinMain` / `InitCommonControlsEx`): faltavam
  `-municode` (os dois mains são `wWinMain`) e `comctl32` no
  diagnóstico — corrigido no CMake (só p/ MinGW; o MSVC não usa).
- Teste segfaultava fora da raiz do projeto: `test_kit.cpp` usava
  `findByNote(36)->` sem checar nulo — um `loadKit` falho (cwd
  errado) derrubava o processo em vez de reportar a falha. Agora o
  teste checa o nulo e o `add_test` fixa `WORKING_DIRECTORY` na raiz.

### Verificado em 23/11/2026 (sem Windows)
- Testes unitários: **144 verificações, todas passando** (113 + 31 do
  AppModel): demo carrega, seleção, params ao vivo, upload
  aceita/rejeita, último hit, novo kit, duplicar (sem trocar o kit
  atual), salvar/recarregar com o volume editado persistido.
- Build Zig (MinGW): standalone + diagnóstico + testes compilam
  (exit 0); JUCE pulado com aviso, sem quebrar o build.

## [0.4.0] — 2026-11-23 — Fase A: motor + kits (cadeia por peça, kit v2)

### Adicionado
- **Cadeia por peça no DrumEngine** (`src/engine/DrumEngine.*`,
  `src/engine/Eq.h`): cada instrumento tem volume, pan, afinação
  (tune em semitons, taxa de reprodução `2^(tune/12)` com
  interpolação linear no render) e **EQ de 3 bandas** (lowshelf 200 Hz,
  peaking com freq configurável, highshelf 4 kHz, biquads RBJ; ganho 0
  nas 3 bandas = bypass exato). Gamma de velocity mantido. Choke com
  fade de ~3 ms continua. Sem alocação e sem mutex no callback.
- **Upload de samples**: WAV 16/24 bits, mono/estéreo, qualquer taxa
  — reamostrado para 48 kHz no carregamento (interpolação linear).
  Arquivo inválido é rejeitado com mensagem clara, sem travar.
- **Formato de kit v2** (`src/common/Kit.*`): os 7 campos v1 mais
  tokens opcionais `tune=`, `eq_low=`, `eq_mid=`, `eq_mid_freq=`,
  `eq_high=`, `soft=`, `hard=` (ausentes = padrão; kit demo v1
  carrega igual). `saveKit()` escreve o kit.txt v2 + copia os WAVs
  para `<pasta>/samples/` (copia o original quando dá, para preservar
  24 bits; senão reescreve em 16 bits/48 kHz).
- **Camadas por velocity**: soft (< 64) / hard (>= 64) funcionam por
  auto-detecção (`*_soft.wav` / `*_hard.wav`) ou por `soft=`/`hard=`
  explícitos no kit. Kit demo (sem camadas) continua igual.
- **API para a futura GUI** (documentada em `DrumEngine.h`):
  `kit()` (lista de peças), `updateInstrument()` /
  `instrumentParams()` (params numéricos ao vivo, atômicos — valem
  para o próximo hit, sem parar o áudio),
  `replaceInstrumentSample()` (troca o sample de uma peça),
  `addInstrument()` / `removeInstrument()`, `loadKitFromFile()` /
  `saveKitToFile()`. Mudanças estruturais exigem o stream parado
  (mesmo contrato do `setKit`).

### Verificado em 23/11/2026 (sem Windows, harness Linux + testes)
- Testes unitários: **113 verificações, todas passando** (eram 25):
  EQ (low +12 dB ≈ x4 no grave, mid +6 dB ≈ x2 em 1 kHz, high +12 dB
  ≈ x4 no agudo e x1 no DC, bypass exato), tune (taxa 2 termina em
  metade do tempo, taxa 0,5 no dobro, interpolação linear medida),
  reamostrador (44,1k→48k, 96k→48k, rampa interpolada), kit v2
  (parse, round-trip salvar/carregar com áudio idêntico, arquivo
  inválido rejeitado com mensagem), camadas por velocity via arquivo
  e em memória, API de arquivo do motor.
- **Sem regressão no som**: o kit demo renderiza com os picos
  idênticos aos da v0.3.2 (bumbo 0,50, caixa 0,45, chimbal 0,38,
  crash 0,50, bell 0,49 etc.). Standalone e diagnóstico recompilados
  com Zig, sem erros nem warnings. GUI **não** feita nesta fase
  (Fase B, depende da decisão de licença do Allex).

## [0.3.2] — 2026-11-23 — Bateria é one-shot: Note Off não corta mais o som

### Corrigido
- **O Allex tinha razão: eu estava fazendo errado.** Os sons saíam
  distorcidos, cortados num ponto e rachando. Causa real, provada
  nos próprios relatórios dele: a Aroma manda **Note Off 15–37 ms
  depois de cada Note On (mediana 19 ms)** — e o motor obedecia o
  Off, matando a voz no meio do sample. Um bumbo de 350 ms morria
  aos 19 ms, um crash de 1,4 s morria aos 19 ms, e o corte seco em
  amplitude alta é um estalo. Foi isso que ele ouviu nas duas
  versões — na v0.3.0 parecia "um sinal" (19 ms de seno), na v0.3.1
  "ruído que racha" (19 ms de ruído + transiente, cortado).
- **Bateria é one-shot** (`src/engine/DrumEngine.*`): `noteOff()`
  agora é no-op documentado — o sample toca até o fim. O choke de
  grupo (hi-hat aberto/fechado, pratos) continua existindo, mas com
  **fade de ~3 ms** em vez de corte seco, para não estalar.
- Verificado: com o kit real, o crash continua soando 400–600 ms
  depois do Note Off (pico 0,12) e o bumbo 250–330 ms (pico 0,04,
  caindo naturalmente); antes do fix, tudo era 0,00 depois dos
  19 ms. Testes novos: "noteOff nao corta o sample (one-shot)" e
  "choke some em rampa". Testes unitários: 25/25 passando.

## [0.3.1] — 2026-11-23 — Kit demo v2: samples que soam como bateria

### Corrigido
- **O Allex testou o standalone e ouviu "um sinal", não bateria.**
  Causa encontrada por eliminação: o motor toca os samples com
  fidelidade (o harness renderiza cada nota e o espectro sai igual
  ao do WAV — kick 140→120 Hz, bell 820/1240 Hz etc.), o mapeamento
  está certo (cada nota toca seu sample) e a conversão p/ o
  dispositivo é só `memcpy` em float. O problema era o CONTEÚDO dos
  samples v1: seno puro + ruído branco. Um tambor de verdade tem
  transiente de ataque, corpo inarmônico e harmônicos — sem isso, no
  falante do notebook (que não reproduz o grave do seno), tudo vira
  tom. O bell v1 era literalmente um acorde de senos (820/1240/1870).
- **Kit demo v2** (`tools/generate_demo_samples.py`, 15 WAVs
  regenerados): click de ataque (baqueta) em bumbo, caixa, tons e
  bell; 2º harmônico no bumbo/tons (sobrevive em falante pequeno);
  ruído da caixa com passa-banda em 1800 Hz (não mais branco cru);
  parciais INARMÔNICOS nos pratos e no bell (1 : 2,76 : 3,89 : 5,4).
  Verificado no harness: ataque com 16% (bumbo) a 69% (chimbal) da
  energia nos primeiros 10 ms, espectro largo onde deve ser
  (chimbal 1107 bins, crash 1076, bell 219 — a v1 tinha ~3).
  Código do app inalterado; só os samples e a versão mudaram.

## [0.3.0] — 2026-11-23 — Etapa 3: standalone (MIDI → DrumEngine → WASAPI)

### Adicionado
- **AromaStandalone.exe** (novo, `src/standalone/`): janela Win32
  mínima, sem JUCE (a decisão de licença do JUCE continua pendente;
  a GUI é a Etapa 5).
  - `main.cpp`: abre o kit demo, a Aroma via WinMM (prefere o
    dispositivo com nome AROMA/TDX) e o áudio na abertura; mostra kit,
    dispositivo MIDI, dispositivo/modo/formato/buffer/latência estimada
    do áudio, última nota tocada (instrumento + nota + velocity, em
    destaque) e contadores (underruns, callbacks, vozes ativas, Note
    Ons, aviso de clip); botões Parar/Iniciar áudio e Reiniciar áudio.
  - `AudioStream.h/.cpp`: saída WASAPI contínua event-driven, thread
    própria com MMCSS ("Pro Audio"). Tenta o **Exclusive primeiro**
    com a matriz da v0.2.6 (formatos × durações, só vale o que o
    `Initialize` aceitar) e cai para o **Shared** se ele não abrir.
    Converte float → formato do dispositivo (float 32 / PCM 16/24/32).
    Conta underruns (buffer vazio ao acordar + `BUFFER_ERROR`) e
    detecta `AUDCLNT_E_DEVICE_INVALIDATED` (desplugar a interface
    avisa na UI e oferece reiniciar sem fechar o app).
  - `PcmConvert.h`: conversor portátil float → formato do dispositivo,
    com teste unitário próprio (`tests/test_pcm_convert.cpp`).
- `src/standalone/CMakeLists.txt` (alvo `AromaStandalone`) e
  `AROMA_BUILD_STANDALONE` ligado por padrão no CMakeLists raiz.

### Decisões (registradas, não escondidas)
- A matriz Exclusive do standalone só tenta **48 kHz**: o kit é
  48 kHz e o motor não reamostra nesta etapa — tocar a 44,1 kHz
  deixaria tudo ~8% mais grave/lento. Se o dispositivo acabar em
  outra taxa, o app toca mesmo assim e **avisa na tela** que o som
  sairá fora do tom (nunca em silêncio e nunca escondido).
- Buffer pedido em Exclusive: 256 frames (5,3 ms @48k). No PC do
  Allex o Exclusive não abre (matriz esgotada na v0.2.6), então o
  esperado é cair no Shared com o buffer do motor do Windows
  (1056 frames, lat. est. 32 ms — o mesmo do diagnóstico).

### Verificado em 23/11/2026 (sem Windows — só o PC do Allex testa de verdade)
- Build Zig (MinGW cross): `AromaStandalone.exe` 1.048.064 bytes,
  PE32+ GUI x64, sem erros. Dois erros de compilação corrigidos no
  caminho: colisão de nome (`formatDescription` getter × função
  livre → renomeada para `describeFormat`) e falta da `gdi32`
  (`CreateFontW`/`DeleteObject`).
- Testes unitários: **todos passam** (23 verificações, incluindo o
  novo `testPcmConvert`).
- Harness Linux (descartável, fora do repo): o kit demo real carrega
  (12 instrumentos, 48 kHz) e **as 12 notas reais da TDX-18S produzem
  som** (pico 0,35–0,57 por nota); choke do hi-hat confirmado
  (aberto cortado pelo fechado). O caminho MIDI → motor → render é
  o mesmo que o standalone usa; o que falta validar no Windows é
  só a abertura do WASAPI e o som na interface dele.

## [0.2.6] — 2026-11-23 — Exclusive: matriz formato x duração

### Corrigido
- **Exclusive falhou nos DOIS dispositivos com `0x88890016` mesmo
  após 22–23 tentativas de duração** (runs 19:05 de 23/11, USB CODEC e
  Realtek, períodos idênticos: padrão 10,0 ms, mínimo 3,0 ms). A
  conclusão é que a duração NÃO é o problema: a sonda
  `IsFormatSupported` disse que o PCM 16 @48k era aceito e o
  `Initialize` recusou mesmo assim. A v0.2.6 testa a **matriz formato
  x duração**: 6 formatos (mix float @48k, PCM 16 @48k, PCM 16 @44,1k,
  PCM 24 @48k, PCM 24 @44,1k, mix float @44,1k — aceitos pela sonda
  primeiro, mas todos são tentados) × ~22 durações cada, e só vale o
  que o `Initialize` aceitar de verdade. O erro agora registra
  quantos formatos e tentativas foram feitos.
- A descrição do formato usado (`48000 Hz, 2 canal(is), 16 bits PCM`)
  virou a função `formatDescription()`, usada tanto no relatório
  quanto na grade, para não divergir.

### Verificado em 23/11/2026 (relatórios 19:05, v0.2.5)
- USB Audio CODEC e Realtek: Exclusive 0 de 5 nos dois, mesmo erro,
  mesmos períodos (10,0/3,0 ms), formato sondado PCM 16 @48k. Shared:
  5 de 5 OK nos dois (USB CODEC jitter 0,93–1,47 ms; Realtek jitter
  0,75–0,81 ms; lat. est. 32 ms). Testes unitários: 21/21 passando.

### Verificado em 23/11/2026 (relatórios 19:10–19:11, v0.2.6)
- Matriz completa: 6 formatos × 132–138 tentativas por configuração,
  nos DOIS dispositivos — Exclusive 0 de 10 no total. O erro final
  mudou para `0x88890008` (`AUDCLNT_E_UNSUPPORTED_FORMAT`) porque o
  último formato tentado na matriz é um float que o `Initialize`
  rejeita por formato; os PCM continuam caindo com `0x88890016`.
  **Veredito: o Exclusive não abre neste PC por este caminho.**
  Resta uma única checagem sem código: a caixa "Permitir que
  aplicativos assumam o controle exclusivo deste dispositivo" nas
  propriedades de som do Windows — se estiver desmarcada, ela é a
  causa. Shared: 5 de 5 OK nos dois de novo (0 underruns, jitter
  0,79–1,41 ms; lat. est. 32 ms).
- **Diagnóstico (Etapa 2) encerrado**: mapa MIDI completo, Shared
  confirmado, Exclusive esgotado. A Etapa 3 usa o Shared (1056 frames
  @48k float, lat. est. 32 ms) com tentativa de Exclusive primeiro e
  fallback para Shared — se a caixa do Windows resolver o Exclusive,
  o standalone já aproveita.

## [0.2.5] — 2026-11-23 — Exclusive: busca por força bruta + períodos no erro

### Corrigido
- **Exclusive continuou com `0x88890016` na v0.2.4** (run 19:00 de
  23/11): os múltiplos dos dois períodos não bastaram — o quantum real
  deste dispositivo não é nenhum deles. A v0.2.5 tenta, por
  configuração, ~25 durações: escada de períodos, milissegundos
  inteiros (1–100 ms) e frames redondos (128–4096), até um `Initialize`
  aceitar. Se nada funcionar, o erro agora registra o formato usado e
  os períodos reais do dispositivo (padrão e mínimo), para o próximo
  diagnóstico não ficar no escuro.
- O relatório não dizia **qual** dispositivo o teste usou (só listava
  os dois e marcava o padrão). Agora o export registra "Dispositivo
  testado: ..." — importante porque o Exclusive pode funcionar no
  Realtek e não no USB CODEC, ou vice-versa.

### Verificado em 23/11/2026 (relatório 19:00, v0.2.4)
- Shared: 5 de 5 OK de novo (0 underruns, jitter 0,77–0,95 ms — o
  melhor até agora — lat. est. 32 ms). Exclusive: 5 de 5 com
  `0x88890016`. MIDI: 0 eventos (ele só rodou o teste de áudio).
  Testes unitários do motor: 21/21 passando.

## [0.2.4] — 2026-11-23 — Exclusive alinha o buffer ao período + bell 59

### Corrigido
- **Exclusive passou de `0x88890008` para `0x88890016`
  (`AUDCLNT_E_INVALID_SIZE`) na v0.2.3**: o formato PCM já era aceito,
  mas o tamanho do buffer não — em Exclusive o buffer precisa ser
  múltiplo do período do dispositivo. O app agora tenta em escada até
  um `Initialize` aceitar: múltiplo do período mínimo, múltiplo do
  período padrão, período padrão exato e período mínimo exato (chamar
  `Initialize` de novo após uma falha é o comportamento documentado).
- O buffer real (de `GetBufferSize`) agora é registrado nos DOIS modos,
  não só em Shared — em Exclusive ele também é arredondado pelo
  dispositivo, e o relatório mostra o que realmente rodou.

### Adicionado
- **Mapa MIDI completo (12 notas)**: o relatório das 18:55 confirmou
  o aro da caixa = **37** (velocity fixa 78) e achou o bell do ride =
  **59** (não 53; só dispara em batidas fortes, vel 92–106, no meio
  das batidas do corpo do ride). `kit.txt` e `MidiTypes.h` atualizados.

### Verificado em 23/11/2026 (relatório 18:55, v0.2.3, 179 eventos)
- Shared: 5 de 5 OK de novo (0 underruns, jitter 0,90–1,39 ms, lat.
  est. 32 ms, buffer fixo 1056 frames). Exclusive: 5 de 5 com
  `0x88890016` — corrigido na v0.2.4, a confirmar no próximo run.
  Testes unitários do motor: 21/21 passando.

## [0.2.3] — 2026-11-23 — Exclusive agora sonda PCM + mapa MIDI real

### Corrigido
- **Exclusive continuava falhando com `0x88890008`** na v0.2.2, mesmo
  com a cópia completa do formato: o mix float 32 funciona em Shared,
  mas o hardware não o aceita em Exclusive. Agora, em Exclusive, o app
  sonda candidatos com `IsFormatSupported()` e usa o primeiro aceito:
  mix float @48 kHz, PCM 16 @48 kHz, PCM 16 @44,1 kHz, PCM 24 @48 kHz,
  PCM 24 @44,1 kHz, mix float @44,1 kHz. Se nenhum for aceito, o
  `Initialize` devolve o erro real. O tom de teste já escrevia PCM
  16/24/32 e float, então nenhum outro código precisou mudar.
- **Mapa MIDI real aplicado** (`kits/demo-kit/kit.txt` e
  `src/common/MidiTypes.h`): os 3 toms da TDX-18S são **50/47/43**,
  não 48/45/41 da hipótese GM. Aro (37) e bell (53) seguem no kit como
  não confirmados — dependem de bater no aro da caixa e no bell do
  ride no próximo teste.

### Verificado em 23/11/2026 (relatório 18:41, v0.2.2, 138 eventos)
- **Shared: 5 de 5 OK, 0 underruns**, ~200 callbacks em 2 s, jitter
  ~1 ms, latência estimada 32 ms (buffer 22 ms + período 10 ms). Mas as
  5 configs colapsaram para o mesmo buffer de 1056 frames: em Shared o
  motor de áudio do Windows fixa o buffer e o app não consegue pedir
  menor — por isso o Exclusive é o caminho da baixa latência, não o
  Shared. O formato usado foi 48 kHz, 2 canais, 32 bits float.
- **Mapa completo dos 10 pads**: bumbo 36 (vel 63–68), caixa 38 (60–90),
  chimbal fechado 42 (28–50), tom de chão 43 (52–59), pedal do chimbal
  44 (99–107 — o pedal manda velocity alta e quase fixa), chimbal
  aberto 46 (28–47), tom 47 (59–71), crash 49 (28–47), tom 50 (53–63),
  ride 51 (28–57). Testes unitários do motor: 21/21 passando.

## [0.2.2] — 2026-11-23 — Correção: WASAPI falhando em tudo + tom audível

### Corrigido
- **WASAPI falhava nas 10 configurações** (Exclusive e Shared, buffers
  64–1024) nos 4 relatórios do Allex em 23/11/2026: Exclusive sempre com
  `0x88890008` (`AUDCLNT_E_UNSUPPORTED_FORMAT`) e Shared com `0x88890008`
  ou `0x80070057` (`E_INVALIDARG`). Causa (bug meu): `runTest()` copiava
  só `sizeof(WAVEFORMATEX)` (18 bytes) do formato de mixagem — mas o mix
  do Realtek/USB CODEC é `WAVE_FORMAT_EXTENSIBLE` (40 bytes: + GUID do
  subformato, máscara de canais, bits válidos). O corte deixava o GUID
  como lixo e o `Initialize()` recusava tudo. Agora a cópia é completa
  (`sizeof(WAVEFORMATEX) + cbSize`), a taxa pedida (48 kHz) é sondada com
  `IsFormatSupported()` antes (cai para a taxa do mix se não aceita) e a
  duração do buffer respeita o período do dispositivo (Shared ≥ período
  padrão; Exclusive ≥ período mínimo) — pedir 64 frames (1,3 ms) onde o
  mínimo é 3–10 ms falha por tamanho, e isso agora é dado, não bug.
- Nome do dispositivo MIDI vinha com lixo depois de "DDRUM" no relatório
  da v0.2.0 (`DDRUM.VBS;.VBE;...`): `szPname` (32 chars) nem sempre vem
  terminado em NUL e o `std::string` lia além do buffer. Cópia agora é
  limitada com `strnlen()`. O nome real que o WinMM reporta é "DDRUM".
- Exportar de duas janelas do app no mesmo segundo escrevia no MESMO
  arquivo e misturava TXT com CSV (relatórios de 18:26 de 23/11). O nome
  do arquivo agora inclui o PID do processo.

### Adicionado
- **Tom de teste audível (440 Hz, volume 0.2)** em vez de silêncio: a
  v0.2.1 renderizava 2 s de silêncio por configuração — o Allex colocou o
  fone, testou nos falantes do notebook e não ouviu nada, e isso era o
  comportamento esperado. Agora dá para ouvir cada configuração; a que
  não tocar, falhou. O tom respeita o formato do dispositivo (float 32,
  PCM 16/24/32).
- Relatório e grade mostram o formato usado e a taxa real (`@48000 Hz`,
  `48 kHz, 2 canais, 32 bits float`), e os erros trazem o nome do
  HRESULT (ex.: `AUDCLNT_E_UNSUPPORTED_FORMAT`), não só o código.
- `docs/DIAGNOSTICO.md` (Teste 5) e o status do botão de teste avisam
  sobre o tom audível.

### Verificado em 23/11/2026 (relatórios reais do PC do Allex)
- PC: AMD Ryzen 5 3500U, 8 lógicos / 4 físicos, 10.116 MB RAM, Windows 11
  (10.0.26200), Lenovo IdeaPad. MIDI: Aroma detectada ("DDRUM"), 222
  eventos no primeiro run, canal 10, velocities 13–88. Notas observadas:
  38, 43, 46, 47, 49, 50, 51 — todas batem com o mapa GM hipotético.
  **Bumbo (36), chimbal fechado (42) e pedal (44) nunca apareceram** —
  falta um teste batendo cada pad em ordem, um por vez.
- Testes unitários do motor: 21/21 passando (sem mudança no motor).

## [0.2.1] — 2026-11-22 — Correção: texto em idioma oriental (mojibake)

### Corrigido
- CPU e versão do Windows apareciam em caracteres orientais (print do Allex).
  Causa: `%S` em `swprintf`/`fwprintf` tem sentido diferente no MSVC e no
  MinGW — no MinGW ele leu os bytes ASCII como UTF-16 e cada par de letras
  virou um ideograma. Corrigidos os 14 pontos em `src/diagnostic/main.cpp`:
  agora tudo passa por `utf8ToWide()` e o formato usa `%s` (wchar_t*),
  que é o comportamento padrão C nos dois compiladores — inclusive o
  relatório TXT/CSV exportado, que tinha o mesmo defeito.
- EXE rebuildado com Zig 0.17.1 (cross-compile, `zig c++ -target
  x86_64-windows-gnu`): PE32+ x64, 509 KB, subsistema GUI, só DLLs do
  sistema. Build sem avisos.
- Infra: o MinGW instalado via apt sumiu quando a VM foi trocada no meio
  do trabalho — toolchain Windows permanente agora é o Zig em
  `~/workspace/tools/zig-x86_64-linux-0.17.1/` (sobrevive a trocas de VM).

## [0.2.0] — 2026-11-22 — Etapa 2 (diagnóstico) + motor base

### Adicionado
- App de diagnóstico `AromaDiagnostico.exe` (`src/diagnostic/`):
  Win32 + WinMM + WASAPI, sem dependências externas.
  - Lista todos os dispositivos MIDI (entrada) e detecta a Aroma por nome
    (AROMA / TDX-18S / TDX).
  - Monitor MIDI em tempo real: Note On/Off, velocity, canal, timestamp,
    contagem por nota (identifica cada pad/prato batido).
  - Lista dispositivos de áudio (MMDevice), formato de mixagem e taxas
    suportadas (44100 / 48000 / 88200 / 96000 via `IsFormatSupported`).
  - Teste WASAPI Exclusive e Shared com buffers 64/128/256/512/1024:
    mede intervalo de callback, jitter e subestimações (underruns);
    calcula **latência estimada de saída** = buffer + período do dispositivo.
  - Relatório exportável TXT e CSV (com carimbo de data/hora e hardware).
  - Informações do sistema: CPU (marca via CPUID), RAM, nº de núcleos,
    versão do Windows (RtlGetVersion), arquitetura.
- Motor de áudio compartilhado (`src/engine/DrumEngine.*`):
  pool fixo de 32 vozes pré-alocadas, zero alocação na thread de áudio,
  velocity com curva, round-robin por instrumento, choke de pratos
  (crash/ride/hi-hat), hi-hat aberto × fechado com corte mútuo,
  ganho master com soft-clip, pan por instrumento.
- Carregador WAV (`src/common/WavFile.*`): PCM 16/24 bits, mono/estéreo,
  validação de cabeçalho, sem alocação depois do carregamento.
- Kit demo (`kits/demo-kit/`): 12 instrumentos, samples WAV **sintetizados**
  por `tools/generate_demo_samples.py` (originais, livres de direitos).
- Testes unitários (`tests/`): WAV + motor (mixagem, choke, polifonia);
  rodam no Linux com g++ e no Windows com MSVC.
- Docs: `docs/ARQUITETURA.md`, `docs/DIAGNOSTICO.md`,
  `RELATORIO_DE_TESTES.md` (modelo a preencher com os resultados reais).
- Build: `CMakeLists.txt` + `build-windows.bat` / `build-windows.ps1`;
  CRT estático no MSVC (o EXE não precisa do VC++ Redistributable).
- **Verificado nesta VM (22/11/2026):**
  - Testes unitários: **21/21 passaram** (WAV + motor), via g++ e via CMake.
  - Kit demo carrega: 12 instrumentos, 15 WAVs 48 kHz.
  - Render de groove de 2 s pelo motor com o kit real: pico 0.987,
    0 eventos descartados, soft-clip atuando como projetado.
  - `AromaDiagnostico.exe` **compilado** (MinGW-w64 13.2, cross-compile):
    PE32+ x64, 775 KB, só DLLs do sistema (WINMM/ole32/AVRT/COMCTL32);
    incluído em `builds/` como teste rápido. O EXE **não foi executado**
    no Windows aqui (VM Linux) — o primeiro teste real é no PC do Allex.
  - 2 erros de compilação e 2 expectativas erradas de teste foram
    encontrados e corrigidos antes da entrega (ver testes acima).

### Decisões registradas
- Diagnóstico antes do instrumento (ordem exigida no briefing).
- WASAPI Exclusive como primeira opção; Shared como fallback; ASIO
  não assumido; ASIO4ALL não obrigatório.
- Mapa MIDI inicial = GM (hipótese, a confirmar pelo diagnóstico).
- JUCE só nas Etapas 5–6 (GUI + VST3); o diagnóstico não depende dele.

## [0.1.0] — 2026-11-22 — Etapa 1 (arquitetura)

### Adicionado
- Estrutura de pastas, README, INSTALL, CHANGELOG.
- Decisão de stack documentada em `docs/ARQUITETURA.md`:
  C++17 + CMake + MSVC; WinMM/WASAPI no diagnóstico; JUCE 8 no
  standalone/VST3; motor próprio sem alocação no callback de áudio.
- Levantamento: manual da TDX-18S **não publica** a tabela de notas MIDI —
  por isso o diagnóstico é pré-requisito (fonte: busca no manual oficial).

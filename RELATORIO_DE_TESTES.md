# RELATORIO DE TESTES — AROMA LOW LATENCY STUDIO

> Preencher com os resultados REAIS do PC do Allex, a partir do relatório
> exportado pelo `AromaDiagnostico.exe`. Nada aqui pode ser preenchido
> antes de rodar o app no Windows 11 dele.

## 1. Ambiente do teste

| Item | Valor |
|---|---|
| Data do teste | 2026-11-23 (4 runs: 18:23, 18:26, 18:26, 18:32) |
| Versão do app | 0.2.0 nos 3 primeiros runs; 0.2.1 no run das 18:32 (texto limpo) |
| CPU | AMD Ryzen 5 3500U with Radeon Vega Mobile Gfx |
| RAM total / livre | 10.116 MB total / 3.892–4.430 MB livre |
| Windows (versão/build) | Windows 11 (10.0.26200), x64, computador IDEAPAD-ALEXAND |
| Interface de áudio | Alto-falantes (Realtek(R) Audio) + Alto-falantes (USB Audio CODEC) — o CODEC era o padrão no run das 18:32 |
| Driver da interface | USB Audio Class do Windows (confirmar) |

## 2. Reconhecimento da Aroma

| Item | Resultado |
|---|---|
| Aroma aparece na lista MIDI? | ✅ sim |
| Nome exato no Windows | `DDRUM` (é o nome que o WinMM reporta; a v0.2.0 mostrava lixo depois dele por falta de NUL — corrigido na 0.2.2) |
| Nº de dispositivos MIDI listados | 1 |

## 3. Mapa MIDI real (preencher com o Teste 3)

Runs de 23/11: 222 eventos / 110 Note On (18:23), 210 / 105 (18:26),
129 / 64 (18:32). Tudo no canal 10, velocities 13–88.

| Pad / prato | Nota | Velocity fraca | Velocity forte |
|---|---|---|---|
| Bumbo | 36 (C2) | 57 | 84 |
| Caixa (centro) | 38 (D2) | 13 | 90 |
| Aro da caixa | 37 (C#2) | 78 | 78 |
| Tom 1 (agudo) | 50 (D3) | 20 | 88 |
| Tom 2 (médio) | 47 (B2) | 13 | 71 |
| Tom de chão | 43 (G2) | 20 | 59 |
| Chimbal fechado | 42 (F#2) | 28 | 50 |
| Chimbal aberto | 46 (A#2) | 20 | 47 |
| Pedal do chimbal | 44 (G#2) | 99 | 107 |
| Crash | 49 (C#3) | 28 | 106 |
| Ride | 51 (D#3) | 28 | 83 |
| Bell do ride | 59 (B3) | 92 | 106 |

Divergências em relação ao mapa GM hipotético: **os 3 toms** (50/47/43,
não 48/45/41) e **o bell do ride** (59, não 53). Todo o resto bateu com
o GM. O pedal do chimbal manda velocity alta e quase fixa (99–107) e o
aro manda velocity fixa 78. O bell só dispara em batidas fortes
(92–106). Mapa completo aplicado em `kits/demo-kit/kit.txt` e
`src/common/MidiTypes.h` (v0.2.4) — 12 notas, todos os pads.

## 4. Áudio — taxas suportadas

| Taxa | Aceita? |
|---|---|
| 44100 Hz | ☐ sim ☐ não |
| 48000 Hz | ☐ sim ☐ não |
| 88200 Hz | ☐ sim ☐ não |
| 96000 Hz | ☐ sim ☐ não |

## 5. Áudio — WASAPI (2 s por configuração)

| Modo | Buffer | Latência est. saída | Underruns | Jitter médio | Resultado |
|---|---|---|---|---|---|
| Exclusive | 64 | | | | ❌ FALHOU |
| Exclusive | 128 | | | | ❌ FALHOU |
| Exclusive | 256 | | | | ❌ FALHOU |
| Exclusive | 512 | | | | ❌ FALHOU |
| Exclusive | 1024 | | | | ❌ FALHOU |
| Shared | 64 | | | | ❌ FALHOU |
| Shared | 128 | | | | ❌ FALHOU |
| Shared | 256 | | | | ❌ FALHOU |
| Shared | 512 | | | | ❌ FALHOU |
| Shared | 1024 | | | | ❌ FALHOU |

Resultado de 23/11, run das 18:41 (v0.2.2): **Shared 5 de 5 OK, 0
underruns** — callback ~10 ms, jitter 0,95–1,31 ms, ~200 callbacks em
2 s, latência estimada 32 ms (buffer 22 ms + período 10 ms), formato
48 kHz, 2 canais, 32 bits float. **Exclusive: 5 de 5 FALHARAM** com
`0x88890008` (`AUDCLNT_E_UNSUPPORTED_FORMAT`) — o mix float não é
aceito em Exclusive; **corrigido na v0.2.3** (sonda PCM 16/24 antes do
`Initialize`). Observação: em Shared as 5 configs colapsaram para o
mesmo buffer de 1056 frames — o motor do Windows fixa o buffer em
Shared e o app não consegue pedir menor; o Exclusive é o caminho da
baixa latência.

Runs das 19:10–19:11 (v0.2.6, nos DOIS dispositivos): matriz completa
— 6 formatos × 132–138 tentativas por configuração — Exclusive 0 de
10 no total. O erro final mudou para `0x88890008` porque o último
formato tentado na matriz é um float rejeitado por formato; os PCM
continuam com `0x88890016`. **Veredito: Exclusive não abre neste PC
por este caminho.** Checagem final (print do `mmsys.cpl`, 23/11):
as duas caixas de Modo Exclusivo já estavam marcadas — não era
configuração. Shared: 5 de 5 OK nos dois de novo (0 underruns,
jitter 0,79–1,41 ms; lat. est. 32 ms).

Runs das 19:05 (v0.2.5, nos DOIS dispositivos): Exclusive 0 de 5 nos
dois, mesmo erro `0x88890016`, mesmos períodos (padrão 10,0 ms, mínimo
3,0 ms), formato sondado PCM 16 @48k, 22–23 tentativas de duração por
configuração. Conclusão: a duração não é o problema — a sonda aceitou
o formato e o `Initialize` recusou mesmo assim.

Run das 19:00 (v0.2.4): Shared 5 de 5 OK de novo (0 underruns, jitter
0,77–0,95 ms, lat. est. 32 ms). Exclusive: 5 de 5 com `0x88890016`
— os múltiplos dos dois períodos não bastaram.

Run das 18:55 (v0.2.3, 179 eventos): Shared 5 de 5 OK de novo (0
underruns, jitter 0,90–1,39 ms, lat. est. 32 ms). Exclusive: 5 de 5
com `0x88890016` — formato PCM aceito, mas o buffer precisa ser
múltiplo do período (tentativa em escada, v0.2.4).

Runs anteriores (v0.2.0/v0.2.1): 10 de 10 falharam no `Initialize`
(Exclusive `0x88890008`; Shared `0x88890008` ou `0x80070057`) — o app
copiava só 18 bytes do formato de mixagem (`WAVEFORMATEXTENSIBLE` tem
40), corrigido na v0.2.2.

Configuração escolhida para a Etapa 3: **Shared, 1056 frames @48 kHz
float 32, lat. est. 32 ms** (0 underruns em todos os runs desde a
v0.2.2). O standalone tenta o Exclusive primeiro (matriz da v0.2.6)
e cai para o Shared se ele não abrir — se a caixa de controle
exclusivo do Windows resolver o Exclusive, o app já aproveita.

## 6. Etapa 3 — standalone (v0.3.0 → v0.3.1) — a testar no PC do Allex

> O standalone não foi rodado no Windows ainda (esta VM é Linux).
> O que está provado aqui embaixo é o caminho do motor; o som na
> interface dele só se confirma quando ele abrir o
> `AromaStandalone.exe`.

**23/11, teste do Allex na v0.3.0:** ouviu sons, mas "não sons de
bateria, parece mais um sinal". Investigação: o motor renderiza cada
nota com fidelidade (espectro igual ao do WAV), o mapeamento está
certo e a conversão é `memcpy` em float — o problema era o CONTEÚDO
dos samples v1 (seno puro + ruído branco). **v0.3.1 traz o kit demo
v2**: click de ataque, 2º harmônico no bumbo/tons, ruído da caixa com
passa-banda, parciais inarmônicos nos pratos/bell. Verificado no
harness: ataque de 16% (bumbo) a 69% (chimbal) da energia nos
primeiros 10 ms.

**23/11, teste do Allex na v0.3.1:** "sons distorcidos, ruidos que se
cortam em determinado ponto, ruidos que racham". Causa real: a Aroma
manda Note Off 15–37 ms depois de cada Note On (mediana 19 ms,
medido no relatório de 18:55) e o motor obedecia o Off, matando a
voz no meio do sample com um estalo. **v0.3.2: bateria é one-shot**
— Note Off ignorado, sample toca até o fim; choke de grupo com fade
de ~3 ms. Verificado com o kit real: crash soa 400–600 ms depois do
Off (pico 0,12); antes do fix, 0,00.

| Item | Esperado no PC dele | Status |
|---|---|---|
| Kit demo carrega | 12 instrumentos, 48 kHz | ✅ provado no harness Linux |
| Cada pad toca o sample certo | 12 notas reais, pico 0,35–0,57 | ✅ provado no harness Linux |
| Choke do hi-hat | fechado corta o aberto | ✅ provado no harness Linux |
| Áudio abre | Shared 1056 frames @48k float 32, lat. est. 32 ms (Exclusive não abre — matriz esgotada) | ⏳ a testar |
| Última nota na tela | instrumento + nota + velocity | ⏳ a testar |
| Underruns tocando | 0 (como no diagnóstico) | ⏳ a testar |

**O que ele precisa fazer:** abrir o `AromaStandalone.exe` (pasta
`builds/` do ZIP), tocar cada pad e dizer: (1) ouviu cada
instrumento certo? (2) o que a janela mostra em "Áudio"
(modo/buffer/latência)? (3) underruns subiu de 0? (4) algum estalo
ou atraso perceptível?

## 7. Latência física (ida e volta) — só após medição loopback

| Medição | Valor |
|---|---|
| Método | microfone gravando pad + caixa (ver docs/DIAGNOSTICO.md) |
| Média de 5 batidas | _a preencher_ ms |
| Configuração usada | _a preencher_ |

> Sem essa medição, NÃO afirmar latência física. A latência estimada
> da tabela 5 é só o pedaço do buffer.

## 8. Estabilidade (Etapa 7)

| Teste | Resultado |
|---|---|
| Sessão de 30 min tocando | _a preencher_ |
| Sessão de 2 h (repetir) | _a preencher_ |
| Desplugar/replugar interface | _a preencher_ |
| VST3 no Reaper (Etapa 6) | _a preencher_ |

## 9. Pendências / observações

- 23/11: o teste de áudio da v0.2.1 renderizava **silêncio** — o Allex
  colocou o fone e testou nos falantes do notebook sem ouvir nada, e isso
  era o comportamento esperado daquela versão. A v0.2.2 toca um tom de
  440 Hz audível em cada configuração.
- 23/11: os 3 primeiros relatórios vieram do EXE **v0.2.0** (texto ainda
  embaralhado); o run das 18:32 já é da v0.2.1 (texto limpo). Os
  relatórios de 18:26 tinham trechos de CSV misturados no TXT — duas
  janelas do app exportando no mesmo segundo; corrigido na v0.2.2 (PID
  no nome do arquivo).
- Diagnóstico (Etapa 2) **encerrado 23/11/2026**: mapa MIDI completo
  (12 notas), Shared confirmado (5/5, 0 underruns, lat. est. 32 ms),
  Exclusive esgotado (matriz 6 formatos × 132+ tentativas, 0 de 10).
  Checagem final (print do `mmsys.cpl`, 23/11): as duas caixas de Modo
  Exclusivo já estavam marcadas — não era configuração.
- Etapa 3 **entregue 23/11/2026 (v0.3.0)**: standalone pronto para
  testar no PC dele — ver seção 6. Fase A (motor + kits, v0.4.0)
  entregue 23/11/2026 — ver seção 10. Próximo passo: Fase B, a
  interface premium (GUI JUCE — aguardando a decisão de licença
  dele, GPLv3 ou paga).

## 10. Fase A — motor + kits (v0.4.0) — verificado sem Windows

> Motor + dados, sem GUI (a interface premium é a Fase B). Verificado
> no harness Linux + testes unitários; o som no PC do Allex só se
> confirma quando ele abrir o `AromaStandalone.exe` da v0.4.0.

| Item | Verificação | Status |
|---|---|---|
| Cadeia por peça (volume/pan/tune/EQ/gamma) | EQ: low +12 dB ≈ x4 no grave, mid +6 dB ≈ x2 em 1 kHz, high +12 dB ≈ x4 no agudo e x1 no DC; bypass exato com tudo zerado | ✅ testes |
| Tune (semitons, varispeed) | taxa 2 termina em metade do tempo, taxa 0,5 no dobro; interpolação linear medida no valor | ✅ testes |
| Upload de samples (qualquer taxa) | 44,1k→48k (441→480 frames), 96k→48k (960→480), rampa interpolada; inválido rejeitado com mensagem | ✅ testes |
| Kit v2 (tune/EQ/soft/hard) | parse dos tokens, kit demo v1 carrega com padrões, round-trip salvar→carregar com áudio idêntico (pico e samples) | ✅ testes |
| Camadas por velocity | soft < 64 / hard >= 64, por arquivo explícito e por auto-detecção; demo sem camadas intacto | ✅ testes |
| API p/ GUI | update/instrumentParams ao vivo, trocar sample, add/remove peça, load/save kit pelo motor | ✅ testes |
| Sem regressão no som | kit demo: picos idênticos aos da v0.3.2 nas 12 notas (bumbo 0,50, caixa 0,45, crash 0,50, bell 0,49) | ✅ harness |
| Standalone v0.3.2 intacto | mesma janela, mesmo kit, compila sem erro/warning (Zig); diagnóstico não mexido | ✅ build |

**O que ele precisa fazer:** abrir o `AromaStandalone.exe` da v0.4.0
e confirmar que toca igual à v0.3.2 (mesmo som, mesma latência).
A edição por peça (volume/tune/EQ/upload) só aparece na Fase B, com
a interface.

## 11. Fase B — interface premium em JUCE (v0.5.0) — código pronto, EXE pendente

> Interface completa em `src/app-juce/` (JUCE 8.0.10, GPLv3). O
> `AromaStudio.exe` **não** foi gerado aqui: o JUCE 8 não compila com
> o Zig 0.17.1 nesta VM (ver abaixo). Nada foi inventado — o ZIP traz
> o código-fonte do app + os EXEs que compilam (standalone v0.4.0 e
> diagnóstico).

| Item | Verificação | Status |
|---|---|---|
| AppModel (lógica não-visual) | 31 checks: demo carrega, seleção, params ao vivo, upload aceita/rejeita, último hit, novo kit, duplicar sem trocar o kit atual, salvar/recarregar com volume persistido | ✅ testes (144 no total) |
| UI compila contra JUCE 8 | DrumKitView + InstrumentPanel + KitBrowser + StatusBar + AppModel + motor: build Linux de checagem, exit 0, sem erros nem warnings de depreciação | ✅ build (Linux/Xvfb) |
| UI renderiza | Screenshot 1280×860 no Xvfb: foto do kit inteira, zonas e rótulos alinhados (Crash, Hi-hat, Tom 1/2, Tom de chão, Ride, Caixa, Bumbo), painel da caixa com valores do demo, status bar com latência/MIDI | ✅ screenshot |
| Acentos | 1º screenshot com mojibake (`AfinaÃ§Ã£o`); causa: `String(const char*)` no JUCE 8 é ASCII — corrigido com `CharPointer_UTF8` e conferido no 2º screenshot | ✅ screenshot |
| Zonas de clique | Medidas por visual grounding nos centros reais da foto (ex.: tom 1 em 0.29/0.36, não 0.37/0.18 como estava) | ✅ grounding |
| Diálogo de nome de kit | `runModalLoop()` não existe no JUCE 8 — refeito com `enterModalState` assíncrono | ✅ build |
| MainComponent (fiação Windows) | Revisado contra `AudioStream.h` / `MidiMonitor.h` / `DrumEngine.h` (todos os métodos conferidos); **não compilado aqui** — só o VS valida | ⚠️ só no PC dele |
| Zig + JUCE (tentado) | Smoke test JUCE mínimo: MinGW → `#error "MinGW is not supported"` (juce_TargetPlatform.h:113); MSVC → `vcruntime_exception.h` / `new.h` não encontrados (Zig não traz os headers C++ do MSVC) | ❌ bloqueado |
| Standalone/diagnóstico | Reconstruídos com Zig deste tree (exit 0); mesmo tamanho da v0.4.0 (1.078.272 / 615.424 bytes) | ✅ build |

**O que ele precisa fazer:** (1) no PC dele, com Visual Studio 2022,
compilar o AromaStudio (`docs/BUILD_JUCE_ZIG.md` tem os comandos —
`cmake -S . -B build -DAROMA_BUILD_JUCE_APP=ON -DJUCE_DIR=C:\dev\JUCE`
+ `cmake --build build --config Release`); (2) abrir o
`AromaStudio.exe`, ligar a Aroma e confirmar: peça acende ao tocar,
edição ao vivo, upload de sample, novo/salvar/carregar/duplicar kit.
O `AromaStandalone.exe` do ZIP continua sendo o fallback que funciona
hoje.

### 11.1 Build na nuvem (GitHub Actions, MSVC) + bug de áudio do JUCE — 2026-11-24

- Repo público `antigravitybrasil-art/aroma-low-latency-studio`, build `.github/workflows/build-windows.yml` (JUCE via FetchContent, CRT estático). Run 45316281323: Build ✅, testes ✅, **Stage deliverable ❌** — o EXE caía em `build/Release/Release/` (o MSVC anexa a subpasta da config ao `RUNTIME_OUTPUT_DIRECTORY` sem `$<CONFIG>`). Fix nos 3 CMakeLists (`"${CMAKE_BINARY_DIR}/$<CONFIG>"`).
- Run 45322074081 (com o fix): Build ✅, testes ✅, Stage ✅, Upload do artefato `AromaStudio-windows` ✅, **Publish release ❌** — `gh release create v0.5.0` falhou sem erro visível e nenhuma release foi criada. Fix: passo de release trocado para `softprops/action-gh-release@v2` (pendente o Allex colar o workflow corrigido no site; o artefato já baixa o EXE).
- **Bug de áudio no AromaStudio (reportado pelo Allex 24/11 05:36)**: kit carregado, foto visível, peça acende ao clicar — mas nenhum som, e o botão "Iniciar áudio" nunca inicia ("Áudio: fechado"). Causa: `CoInitializeEx(COINIT_MULTITHREADED)` falha com `RPC_E_CHANGED_MODE` na thread de mensagens do JUCE (COM já inicializado como STA por ele). Fix em `src/standalone/AudioStream.cpp` (`ComInit` aceita o modo já ativo; `CoUninitialize` só no `close()` se fomos nós que inicializamos) + `MainComponent` passa a mostrar o motivo real na barra ("Áudio: fechado — <motivo>") em vez de apagá-lo a cada 100 ms. Build Zig do standalone OK, testes unitários 144/144. Validação final: próximo build na nuvem + teste do Allex no PC.
- **MIDI + interface (reportado pelo Allex 24/11 18:43)**: (1) com a bateria ligada depois do app aberto, "nenhum dispositivo encontrado" para sempre — `openMidi()` só rodava no construtor; fix: o timer tenta reconectar o MIDI a cada ~2 s até achar. (2) Lado direito cortado sem rolagem — a janela 1280×820 era mais alta que a tela 1366×768 dele e o painel da peça (~350 px) não cabia; fix: janela limitada à área útil da tela + painel da peça dentro de um `Viewport` com rolagem vertical. APIs conferidas nos headers do JUCE 8.0.10; fiação Windows validada só pelo build na nuvem (MSVC), como documentado.

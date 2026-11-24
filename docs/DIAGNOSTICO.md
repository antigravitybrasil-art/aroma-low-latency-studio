# DIAGNOSTICO — como executar os testes (Etapa 2)

Aplicativo: `AromaDiagnostico.exe` — janela única, sem instalação.

## Preparação (5 minutos)

1. Ligue a Aroma TDX-18S via USB **antes** de abrir o app.
2. Ligue a interface USB Audio CODEC via USB.
3. Feche o Reaper e qualquer outro programa usando a interface
   (o teste Exclusive precisa do dispositivo livre).
4. Rode `AromaDiagnostico.exe`.

## Teste 1 — Sistema

A tela já mostra ao abrir: CPU, nº de núcleos, RAM total/livre, versão
do Windows e arquitetura. Confira se bate com o seu PC. Se a RAM livre
estiver abaixo de 2 GB com tudo fechado, feche programas antes de continuar.

## Teste 2 — Dispositivos MIDI

1. Clique em **Atualizar MIDI**.
2. A lista mostra todos os dispositivos de entrada MIDI.
3. A Aroma deve aparecer com nome contendo **AROMA**, **TDX** ou
   **USB Audio Device**. O app marca a linha dela com `[AROMA?]`.
4. Se ela **não** aparecer: troque a porta USB e o cabo, confirme que o
   módulo está ligado, e clique em Atualizar de novo. Anote o que tentou.

## Teste 3 — Monitor MIDI (o mais importante)

1. Selecione a Aroma na lista e clique em **Iniciar monitoramento**.
2. Toque **cada pad e prato uma vez, devagar**, nesta ordem, e anote:
   bumbo, caixa (centro), aro da caixa, tom 1, tom 2, tom de chão,
   chimbal fechado, chimbal aberto, pedal do chimbal, crash, ride,
   bell do ride (se tiver).
3. Para cada toque, o log mostra: hora, `Note On`, nota (número e nome),
   velocity (0–127) e canal.
4. Toque o **bumbo bem fraco e depois bem forte**: a velocity deve mudar
   (ex.: ~30 fraco, ~110 forte). Se ficar sempre igual, anote — é dado
   de sensibilidade do módulo.
5. Segure o chimbal fechado e bata no crash: veja se aparece alguma
   mensagem de choke (alguns módulos enviam Note Off do crash ao
   apertar o prato — anote se acontecer).
6. Clique em **Parar** e confira a tabela **contagem por nota**: cada pad
   deve ter sua nota própria. Se dois pads mostrarem a **mesma nota**,
   anote — o mapa precisa de ajuste manual no módulo ou no kit.

## Teste 4 — Dispositivos de áudio e taxas

1. Clique em **Atualizar áudio**.
2. Selecione a sua interface (USB Audio CODEC) na lista.
3. O app mostra o formato de mixagem atual e testa quais taxas o
   dispositivo aceita: 44100, 48000, 88200, 96000.
4. Anote as taxas aceitas — a taxa do kit vai ser uma delas
   (preferência: 48000, se aceita).

## Teste 5 — WASAPI Exclusive e Shared (latência)

1. Com a interface selecionada, clique em **Testar áudio**.
2. O app testa, para cada modo (Exclusive, Shared) e cada buffer
   (64, 128, 256, 512, 1024), 2 segundos de um tom de 440 Hz (audível — a v0.2.1 renderizava silêncio, e "não ouvir nada" era o comportamento esperado):
   - intervalo médio entre callbacks e jitter;
   - nº de underruns (buffer esvaziou = estalo);
   - **latência estimada de saída** = buffer + período do dispositivo.
3. O resultado aparece na grade, com `OK` ou `FALHOU` por linha.
4. **O que escolher depois:** o menor buffer com zero underrun em
   Exclusive; se o Exclusive falhar em todos, o menor buffer com zero
   underrun em Shared. Não escolha buffer com underrun "porque a
   latência é menor" — estalo é pior que 3 ms a mais.

## Exportar o relatório

Clique em **Exportar TXT/CSV**. São gerados dois arquivos na mesma pasta
do EXE, por exemplo:

```
diagnostico_aroma_20261122_170500.txt
diagnostico_aroma_20261122_170500.csv
```

O TXT tem tudo (sistema, MIDI, áudio, testes, log de notas).
O CSV tem o log de eventos MIDI, uma linha por evento:
`timestamp_ms,tipo,canal,nota,nota_nome,velocity`.

**Me envie os dois arquivos.** Eles definem as Etapas 3 em diante.

## Medição física de latência (ida e volta) — procedimento, não promessa

O app **não** mede latência física. Para medi-la de verdade:

1. No Reaper (ou gravador), crie uma pista de áudio com entrada do
   microfone/interface apontado para a caixa de som.
2. Bata no pad e grave: o áudio do impacto (som mecânico do pad) e o som
   da caixa saindo da interface aparecem na mesma gravação.
3. Meça a distância entre os dois picos no editor — esse é o atraso real
   de ponta a ponta (MIDI + processamento + buffer + conversor).
4. Repita 5 vezes e anote a média. Esse número é o que vale; o número
   "estimado" do app é só o pedaço do buffer.

## O que anotar à mão (se algo fugir do app)

- Nome exato da Aroma como aparece no Gerenciador de Dispositivos.
- Nome exato da interface de áudio.
- Qualquer mensagem de erro do Windows ao ligar os dispositivos.
- Se algum pad não gerou evento nenhum no Teste 3.

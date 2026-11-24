# INSTALL — AROMA LOW LATENCY STUDIO

## Requisitos (PC Windows 11, 64 bits)

| Item | Requisito |
|---|---|
| SO | Windows 11 64 bits (21H2 ou superior) |
| Compilador | Visual Studio 2022 Community (gratuito) com **Desktop development with C++** |
| Build | CMake 3.20 ou superior (vem com o VS 2022, ou baixe em cmake.org) |
| MIDI | Aroma TDX-18S ligada via USB (driver USB MIDI class-compliant do Windows — sem instalar nada) |
| Áudio | Interface USB Audio CODEC (a sua mesa/interface genérica) ligada via USB |
| DAW (Etapa 6) | Reaper (qualquer versão recente, 64 bits) |

> Não é preciso instalar ASIO, ASIO4ALL, driver da Aroma nem driver da
> interface para a Etapa 2. O diagnóstico usa o que o Windows já tem
> (WinMM + WASAPI). Se no futuro você quiser ASIO nativo, ele entra como
> opção, nunca como pré-requisito.

## Passo 1 — Instalar as ferramentas

1. Baixe e instale o **Visual Studio 2022 Community**:
   <https://visualstudio.microsoft.com/downloads/>
   - Na instalação, marque **"Desenvolvimento para desktop com C++"**.
   - Confirme que o componente **MSVC v143** e o **Windows 11 SDK** estão marcados.
2. Baixe e instale o **CMake** (se não vier com o VS):
   <https://cmake.org/download/>
   - Na instalação, marque **"Add CMake to the system PATH"**.
3. Reinicie o PC (o VS pede isso às vezes).

## Passo 2 — Baixar o projeto

Copie a pasta `AROMA_LOW_LATENCY_STUDIO` para onde quiser, por exemplo:

```
C:\Users\Allex\projetos\AROMA_LOW_LATENCY_STUDIO
```

Não precisa estar no Laragon — isto é C++, não PHP.

## Passo 3 — Compilar (Etapa 2: diagnóstico)

Duplo clique em **`build-windows.bat`** (ou, no PowerShell, `./build-windows.ps1`).

O que ele faz:

```
cmake -S . -B builds\cmake -G "Visual Studio 17 2022" -A x64
cmake --build builds\cmake --config Release
```

Saída esperada:

```
builds\cmake\Release\AromaDiagnostico.exe
```

Se der erro, **não avance**: copie a mensagem de erro inteira e me envie.
O projeto é mantido compilável a cada etapa — erro de build é bug meu,
não "detalhe".

> Alternativa sem script: abra o **"x64 Native Tools Command Prompt for VS 2022"**,
> entre na pasta do projeto e rode os dois comandos acima.

## Passo 4 — Rodar o diagnóstico

1. Ligue a Aroma TDX-18S no USB e espere o Windows reconhecê-la.
   (Em **Configurações → Bluetooth e dispositivos → Dispositivos**, ela deve
   aparecer; no **Gerenciador de Dispositivos → Dispositivos de som, vídeo e jogo**
   ou **Controladores USB**, procure por algo com "Aroma", "TDX" ou "USB Audio Device".)
2. Ligue a interface USB Audio CODEC.
3. Rode `builds\cmake\Release\AromaDiagnostico.exe`.
4. Siga `docs/DIAGNOSTICO.md` — são 5 testes guiados, uns 10 minutos.
5. Exporte o relatório (botão **Exportar TXT/CSV**) e me envie o arquivo.

## Passo 5 — O que vem depois

Com o relatório na mão, eu fixo:

- o mapa MIDI real da sua Aroma (notas de cada pad);
- o buffer e a taxa de amostragem que o seu PC aguenta sem estalo;
- se WASAPI Exclusive funciona com a sua interface ou se o Shared é o caminho;
- as metas de latência reais (não chutadas).

Aí começam as Etapas 3–5 (motor + standalone + GUI). O VST3 (Etapa 6)
precisa do JUCE — as instruções de instalação dele entram no `INSTALL.md`
quando essa etapa começar, para não instalar nada antes da hora.

## Solução de problemas

| Sintoma | O que fazer |
|---|---|
| `cmake não é reconhecido` | Reinstale o CMake marcando "Add to PATH", ou use o CMake que vem no VS (`C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin`) |
| `MSVC não encontrado` | Abra o "x64 Native Tools Command Prompt" em vez do cmd normal; ou reinstale o VS com "Desktop development with C++" |
| Aroma não aparece na lista MIDI | Troque a porta USB; teste outro cabo; veja se o módulo da bateria está ligado antes de abrir o app; rode o app como Administrador uma vez para descartar permissão |
| WASAPI Exclusive falha | Normal em algumas interfaces genéricas — o app testa o Shared logo depois e registra os dois resultados; não é erro, é dado |
| Som com estalos no teste | Aumente o buffer (256 → 512) e repita; anote em qual buffer os estalos somem — esse é o seu piso real |

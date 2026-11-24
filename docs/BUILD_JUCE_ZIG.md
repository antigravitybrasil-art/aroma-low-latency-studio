# Build do AromaStudio (JUCE) — Windows (Visual Studio)

> ⚠️ **Veredito 23/11/2026: o AromaStudio NÃO compila com o Zig nesta VM.**
> O caminho é o Visual Studio no PC do Allex (abaixo). O Zig continua
> valendo para o standalone Win32 e o diagnóstico (target MinGW) — só
> o app JUCE precisa do MSVC. Detalhes do bloqueio no fim do documento.

## Pré-requisitos (no PC do Allex)

## Pré-requisitos

| Item | Onde | Observação |
|---|---|---|
| Visual Studio 2022 | PC do Allex | workload "Desktop development with C++" (MSVC v143) |
| CMake 3.28+ | PC do Allex | `cmake --version` |
| Git | PC do Allex | para baixar o JUCE (ou use o FetchContent) |
| JUCE 8.0.10 | `C:\dev\JUCE` | `git clone --branch 8.0.10 --depth 1 https://github.com/juce-framework/JUCE.git` — **fora** do ZIP (dependência de terceiros) |

## Build (no PC do Allex)

```bat
cd aroma-low-latency-studio
cmake -S . -B build -DAROMA_BUILD_JUCE_APP=ON -DJUCE_DIR=C:\dev\JUCE
cmake --build build --config Release
rem Saída: build\Release\AromaStudio.exe
```

Sem `JUCE_DIR`, o CMake baixa o JUCE sozinho via FetchContent (precisa
de internet + git). O `AromaStudio.exe` procura os kits em `..\kits`
(ao lado da pasta do EXE) e a imagem em `..\assets\drumkit_topdown.png`
— copie o EXE para a pasta do projeto (junto de `kits\` e `assets\`)
ou rode a partir de `build\Release\` com essas pastas ao lado.

O standalone Win32 (`AromaStandalone.exe`) e o diagnóstico continuam
sendo compilados do mesmo jeito que antes (Zig, MinGW); o target JUCE é
adicional (`AROMA_BUILD_JUCE_APP=ON`, só em Windows, só com MSVC).

## Por que o Zig não compila o JUCE (tentado de verdade, 23/11/2026)

Duas tentativas com app JUCE mínimo (smoke test), ambas com configure
OK e falha no compile — erros exatos:

1. **Target MinGW (`x86_64-windows-gnu`)** — o JUCE 8 recusa
   explicitamente:
   `juce_core/system/juce_TargetPlatform.h:113: #error "MinGW is not
   supported. Please use an alternative compiler."` (o `#ifdef
   __MINGW32__` dispara; sem ele, `JUCE_64BIT` fica indefinido e
   surgem erros de `pointer_sized_int` como `int` em `juce_HashMap.h`
   e `juce_RelativeCoordinatePositioner.cpp`).
2. **Target MSVC (`x86_64-windows-msvc`)** — o JUCE aceita o compilador,
   mas o libc++ do Zig (ABI vcruntime) exige headers do CRT da Microsoft
   que o Zig 0.17.1 **não traz**: `fatal error: 'new.h' file not found`
   e `fatal error: 'vcruntime_exception.h' file not found` (só existem
   os `.def` de link; `vcruntime_exception.h` não existe em
   `zig/lib`). Sem o Visual Studio instalado, não há como obter esses
   headers no Linux.

Ou seja: não é falta de tentativa nem de flag — o Zig 0.17.1 não tem o
que o JUCE 8 precisa para Windows. Se um Zig futuro trouxer os headers
do vcruntime, o target MSVC pode voltar a ser tentado (o toolchain
`toolchain-zig-msvc.cmake` do smoke test está guardado em
`~/workspace/tmp-juce-smoke/`).

## Armadilhas encontradas no caminho (23/11/2026) — não reabrir

1. **`LANGUAGES C CXX` é obrigatório no `project()` raiz.** O JUCE exige
   C no projeto de nível superior (`JUCEModuleSupport.cmake:48`). Com
   só `CXX`, o configure passa e o generate falha com
   `CMAKE_C_COMPILE_OBJECT not set`. O `project(JUCE ...)` interno do
   JUCE não basta — o erro é no escopo do projeto de cima.
2. **Compilador de recursos: `windres` não existe no Linux.** O JUCE
   gera um `*_resources.rc` para o app Windows e o CMake tenta compilar
   com `windres` → `No such file or directory`. Solução: `zig rc`
   (drop-in do `rc.exe`) via wrapper `~/workspace/tools/bin/zig-rc`,
   apontado por `CMAKE_RC_COMPILER` no toolchain. A forma de lista
   (`zig;rc`) **não** funciona aqui — quebra o `CMakeDetermineRCCompiler`
   (`get_filename_component unknown component rc`); o RC precisa ser um
   caminho único.
3. **Compiladores C/C++ em forma de lista no toolchain** (não wrappers):
   `zig;cc;-target;x86_64-windows-gnu`. Com wrappers o generate falhava
   com o mesmo `CMAKE_C_COMPILE_OBJECT not set` (antes do fix 1).
4. **`juceaide` é compilado no host durante o configure** (leva ~2 min)
   e precisa dos headers X11 (`libxrandr-dev libxinerama-dev
   libxcursor-dev libfreetype6-dev libfontconfig1-dev`). **Pacotes apt
   não sobrevivem a reinício da VM** (tudo fora de `~` é efêmero —
   a VM reiniciou 3 vezes na noite de 23/11 e os pacotes sumiram a cada
   vez). Reinstalar é rápido se os `.deb` estiverem no cache
   (`/var/cache/apt/archives/` sobrevive): `dpkg -i` direto nos
   `.deb` do cache, sem `apt-get update`. Backup persistente dos
   headers + libs em `~/workspace/tools/x11-fallback/` (headers
   copiados + symlinks `libXrandr.so`/`libXinerama.so`/`libXcursor.so`
   + `.pc` do pkg-config com `libdir` ajustado).
5. **`START_JUCE_APPLICATION` define `WinMain`** — não use `-municode`
   no app JUCE (o standalone Win32 usa; o JUCE, não).
6. **JUCE 8: `String(const char*)` é ASCII, não UTF-8.** Literais com
   acento (`"Afinação"`) viram mojibake (`"AfinaÃ§Ã£o"`) — visto no
   screenshot de 23/11/2026. Usar `juce::String(juce::CharPointer_UTF8("..."))`
   em todo literal não-ASCII (e `String(const std::string&)` continua
   certo, pois trata os bytes como UTF-8). `Font(float, int)` também
   está depreciado — usar `Font(FontOptions(h, style))`. E
   `AlertWindow::runModalLoop()` não existe mais no JUCE 8 — usar
   `enterModalState` assíncrono com `ModalCallbackFunction::create`.
7. **Imagem do kit: `RectanglePlacement` com `onlyIncreaseInSize`
   NÃO encolhe** — a foto 1920×1280 ficava em tamanho nativo e cortada.
   Para "contain" (foto inteira visível) use `onlyReduceInSize`.
8. **Zonas de clique medidas por visual grounding** nos centros reais
   da foto (ver `kKitZones` em `src/app-juce/DrumKitView.h`).

## Licença

JUCE sob **GPLv3** (decisão do Allex, 23/11/2026). O AromaStudio é
código aberto; o motor (DrumEngine) e o áudio (WASAPI) são os mesmos do
standalone Win32 — só a UI é JUCE.

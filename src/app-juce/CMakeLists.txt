# AromaStudio — interface premium em JUCE (Fase B, v0.5.0).
#
# Licença: JUCE sob GPLv3 (decisão do Allex, 23/11/2026) — este app é
# código aberto. O motor (DrumEngine) e o áudio (WASAPI contínuo) são
# os mesmos do standalone Win32; só a UI é JUCE.
#
# Como obter o JUCE:
#   1. Variável JUCE_DIR apontando para o JUCE 8.x (ex.:
#      -DJUCE_DIR=C:/dev/JUCE  ou  -DJUCE_DIR=$HOME/workspace/tools/juce).
#   2. Sem JUCE_DIR, o FetchContent baixa o JUCE 8.0.10 do GitHub
#      (precisa de internet + git).
# Build no Windows (Visual Studio 2022):
#   cmake -S . -B build -DAROMA_BUILD_JUCE_APP=ON
#   cmake --build build --config Release
# Cross-compile Linux -> Windows (Zig): ver docs/BUILD_JUCE_ZIG.md
# (o host precisa dos headers X11 p/ compilar o juceaide).

if(NOT WIN32)
    message(WARNING "AromaStudio (JUCE) só compila para Windows nesta etapa.")
    return()
endif()

# JUCE 8 NÃO suporta MinGW (juce_TargetPlatform.h: #error "MinGW is not
# supported"). O Zig com -target x86_64-windows-gnu é MinGW e falha
# nesse #error; com -target x86_64-windows-msvc falta o header
# vcruntime_exception.h (o Zig 0.17.1 não traz os headers C++ do MSVC).
# Ver docs/BUILD_JUCE_ZIG.md. Sem MSVC, o target é pulado (não quebra
# o build do standalone/diagnóstico).
if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "GNU"
   OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    message(WARNING "AromaStudio (JUCE) pulado: JUCE 8 não suporta MinGW. "
                    "Compile no Windows com Visual Studio (ver docs/BUILD_JUCE_ZIG.md).")
    return()
endif()

if(DEFINED JUCE_DIR AND EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(STATUS "JUCE em ${JUCE_DIR}")
    add_subdirectory("${JUCE_DIR}" juce)
elseif(DEFINED JUCE_DIR)
    message(FATAL_ERROR "JUCE_DIR definido mas sem CMakeLists.txt: ${JUCE_DIR}")
else()
    include(FetchContent)
    message(STATUS "JUCE_DIR não definido — baixando JUCE 8.0.10 via FetchContent")
    FetchContent_Declare(juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.10
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(juce)
endif()

juce_add_gui_app(AromaStudio
    VERSION 0.5.0
    COMPANY_NAME "Aroma Studio"
    PRODUCT_NAME "AromaStudio")

target_sources(AromaStudio PRIVATE
    Main.cpp
    MainComponent.cpp
    DrumKitView.cpp
    InstrumentPanel.cpp
    KitBrowser.cpp
    StatusBar.cpp
    AppModel.cpp
    ../standalone/AudioStream.cpp
    ../diagnostic/MidiMonitor.cpp
)

target_include_directories(AromaStudio PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_definitions(AromaStudio PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_USE_WIN_WEB_BROWSER=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    # Sem loops modais (JUCE 8 removeu AlertWindow::runModalLoop): os
    # diálogos usam enterModalState assíncrono (ver KitBrowser.cpp).
)

target_link_libraries(AromaStudio PRIVATE
    aroma_core
    juce::juce_gui_basics
    juce::juce_gui_extra
    winmm ole32 uuid avrt gdi32
)

# $<CONFIG>: sem ele, o gerador multi-config do MSVC anexa a subpasta
# da config e o EXE ia para build/Release/Release/ (quebrou o "Stage
# deliverable" do build na nuvem). Esta chamada vem DEPOIS do
# juce_add_gui_app, então sobrescreve o _artefacts do JUCE.
set_target_properties(AromaStudio PROPERTIES
    OUTPUT_NAME "AromaStudio"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>"
)

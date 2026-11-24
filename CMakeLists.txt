cmake_minimum_required(VERSION 3.20)
project(AromaLowLatencyStudio VERSION 0.5.0 LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# AROMA LOW LATENCY STUDIO — build Windows 11 x64 (MSVC v143).
# Alvos:
#   AromaDiagnostico : app de diagnóstico (Etapa 2) — SEM JUCE, só Win32/WinMM/WASAPI.
#   aroma_tests      : testes unitários do motor (portáteis; rodam no Linux também).
#   AromaStandalone  : (Etapa 3/5) usa DrumEngine + WASAPI; GUI Win32 mínima.
#   AromaStudio      : (Fase B, v0.5.0) interface premium em JUCE (GPLv3).
#   AromaVST3        : (Etapa 6) requer JUCE — OFF por padrão, ver src/plugin/CMakeLists.txt.
# ---------------------------------------------------------------------------

if(NOT WIN32 AND NOT AROMA_ALLOW_NON_WINDOWS_BUILD)
    message(WARNING "Build fora do Windows: só os testes unitários serão compilados. "
                    "O app de diagnóstico e o standalone exigem Windows 11 + MSVC.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# CRT estático no MSVC: o EXE roda no PC do Allex sem instalar
# o Visual C++ Redistributable.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(MSVC)
    add_compile_options(/W4 /permissive- /EHsc)
    # Release com otimização máxima e sem checagem de runtime no áudio.
    add_compile_options($<$<CONFIG:Release>:/O2> $<$<CONFIG:Release>:/fp:fast>)
endif()

option(AROMA_BUILD_DIAGNOSTIC "Compilar o app de diagnóstico (Windows)" ON)
option(AROMA_BUILD_TESTS "Compilar os testes unitários do motor" ON)
option(AROMA_BUILD_STANDALONE "Compilar o standalone (Etapa 3, sem GUI JUCE ainda)" ON)
option(AROMA_BUILD_VST3 "Compilar o plugin VST3 via JUCE (Etapa 6)" OFF)
option(AROMA_BUILD_JUCE_APP "Compilar o AromaStudio (interface premium em JUCE, Fase B)" ON)

# --- Biblioteca comum: motor + carregador WAV + kit (portátil) ----------------
add_library(aroma_core STATIC
    src/common/WavFile.cpp
    src/common/Kit.cpp
    src/engine/DrumEngine.cpp
)
target_include_directories(aroma_core PUBLIC src)
if(WIN32)
    target_compile_definitions(aroma_core PUBLIC AROMA_WINDOWS=1)
endif()

if(AROMA_BUILD_TESTS)
    enable_testing()
    add_executable(aroma_tests
        tests/test_main.cpp
        tests/test_wav.cpp
        tests/test_engine.cpp
        tests/test_pcm_convert.cpp
        tests/test_kit.cpp
        tests/test_appmodel.cpp
        src/app-juce/AppModel.cpp
    )
    target_link_libraries(aroma_tests PRIVATE aroma_core)
    # Os testes leem kits/demo-kit/kit.txt por caminho relativo: rodar
    # a partir da raiz do projeto (sem isso o loadKit falha e, antes do
    # fix em test_kit.cpp, o teste segfaultava em findByNote nulo).
    add_test(NAME aroma_tests COMMAND aroma_tests)
    set_tests_properties(aroma_tests PROPERTIES
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()

if(WIN32 AND AROMA_BUILD_DIAGNOSTIC)
    add_executable(AromaDiagnostico WIN32
        src/diagnostic/main.cpp
        src/diagnostic/MidiMonitor.cpp
        src/diagnostic/AudioTest.cpp
        src/common/SystemInfo.cpp
    )
    target_include_directories(AromaDiagnostico PRIVATE src)
    target_link_libraries(AromaDiagnostico PRIVATE
        winmm ole32 uuid avrt comctl32
    )
    # wWinMain (não WinMain): no MinGW/Zig precisa de -municode
    # (compile + link). No MSVC o entry wide já é o padrão do projeto.
    if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "GNU"
       OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(AromaDiagnostico PRIVATE -municode)
        target_link_options(AromaDiagnostico PRIVATE -municode)
    endif()
    # $<CONFIG>: sem ele, o gerador multi-config do MSVC anexa a
    # subpasta da config e o EXE ia para build/Release/Release/
    # (foi o que quebrou o passo "Stage deliverable" do build na nuvem).
    set_target_properties(AromaDiagnostico PROPERTIES
        OUTPUT_NAME "AromaDiagnostico"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>"
    )
endif()

if(WIN32 AND AROMA_BUILD_STANDALONE)
    add_subdirectory(src/standalone)
endif()

if(AROMA_BUILD_VST3)
    add_subdirectory(src/plugin)
endif()

if(WIN32 AND AROMA_BUILD_JUCE_APP)
    add_subdirectory(src/app-juce)
endif()

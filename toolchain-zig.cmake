# Toolchain Zig — cross-compile Linux -> Windows x64 (MinGW ABI).
#
# Uso (a partir da raiz do projeto):
#   cmake -S . -B build-zig --toolchain toolchain-zig.cmake \
#         -DCMAKE_BUILD_TYPE=Release
#   cmake --build build-zig --config Release
# Saída: build-zig/Release/AromaStandalone.exe + AromaDiagnostico.exe
#
# ⚠️ NÃO serve para o app JUCE (AromaStudio): o JUCE 8 recusa MinGW
# (#error "MinGW is not supported") e o target MSVC do Zig não tem os
# headers vcruntime. O target JUCE é pulado com aviso neste toolchain
# (ver src/app-juce/CMakeLists.txt); o app JUCE só compila com o
# Visual Studio no Windows — ver docs/BUILD_JUCE_ZIG.md.
#
# Compiladores em forma de LISTA (zig + subcomando + -target). Não use
# os wrappers ~/workspace/tools/bin/zig-cc aqui: com wrapper o CMake
# falha no generate com "CMAKE_C_COMPILE_OBJECT not set" (o mesmo erro
# acontecia com a lista antes do fix LANGUAGES C CXX — ver abaixo).
# O compilador de recursos (RC) PRECISA ser wrapper (caminho único):
# a forma de lista quebra o CMakeDetermineRCCompiler.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

set(CMAKE_C_COMPILER
    "/home/hatch/workspace/tools/zig-x86_64-linux-0.17.1/zig;cc;-target;x86_64-windows-gnu")
set(CMAKE_CXX_COMPILER
    "/home/hatch/workspace/tools/zig-x86_64-linux-0.17.1/zig;c++;-target;x86_64-windows-gnu")
set(CMAKE_RC_COMPILER "/home/hatch/workspace/tools/bin/zig-rc")

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# O projeto raiz DEVE declarar LANGUAGES C CXX (não só CXX): o JUCE
# exige C no projeto de nível superior — sem isso o generate falha com
# "CMAKE_C_COMPILE_OBJECT not set" (JUCEModuleSupport.cmake:48).

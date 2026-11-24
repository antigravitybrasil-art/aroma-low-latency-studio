# AromaStandalone — Etapa 3 (sem JUCE; a GUI JUCE é a Etapa 5).
# Win32 + WinMM + WASAPI, com o mesmo DrumEngine do diagnóstico/VST3.
add_executable(AromaStandalone WIN32
    main.cpp
    AudioStream.cpp
    ../diagnostic/MidiMonitor.cpp
    ../common/WavFile.cpp
    ../common/Kit.cpp
    ../engine/DrumEngine.cpp
)
target_include_directories(AromaStandalone PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(AromaStandalone PRIVATE
    winmm ole32 uuid avrt gdi32
)
# wWinMain (não WinMain): no MinGW/Zig precisa de -municode
# (compile + link). No MSVC o entry wide já é o padrão do projeto.
# (O app JUCE NÃO usa -municode: START_JUCE_APPLICATION define WinMain.)
if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "GNU"
   OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(AromaStandalone PRIVATE -municode)
    target_link_options(AromaStandalone PRIVATE -municode)
endif()
set_target_properties(AromaStandalone PROPERTIES
    OUTPUT_NAME "AromaStandalone"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Release"
)

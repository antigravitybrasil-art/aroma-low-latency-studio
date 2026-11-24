# VST3 via JUCE — ETAPA 6. Não compilado por padrão (AROMA_BUILD_VST3=OFF).
# Pré-requisitos quando a etapa começar:
#  - decisão de licença JUCE registrada (GPLv3 ou paga — ver docs/ARQUITETURA.md);
#  - internet para o FetchContent baixar o JUCE 8;
#  - Reaper 64 bits para testar.
# O processador reutiliza o MESMO DrumEngine do standalone (aroma_core).
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(AromaVST3
    COMPANY_NAME "Aroma Low Latency Studio"
    PLUGIN_MANUFACTURER_CODE Arom
    PLUGIN_CODE Arom
    FORMATS VST3 Standalone
    PRODUCT_NAME "Aroma Low Latency Studio"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
)

target_sources(AromaVST3 PRIVATE
    PluginProcessor.cpp
    PluginEditor.cpp
)
target_link_libraries(AromaVST3 PRIVATE
    aroma_core
    juce::juce_audio_utils
    juce::juce_audio_processors
)
target_compile_definitions(AromaVST3 PRIVATE JUCE_VST3_CAN_REPLACE_VST2=0)

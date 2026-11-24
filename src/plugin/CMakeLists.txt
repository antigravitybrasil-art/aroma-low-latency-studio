# VST3 via JUCE — ETAPA 6 (v0.5.0).
#
# Licença JUCE: GPLv3 (decisão do Allex, 23/11/2026).
# O processador reutiliza o MESMO DrumEngine do standalone (aroma_core).
# O JUCE é buscado uma única vez por configure: se o app JUCE já o
# trouxe (mesmo nome `juce`, mesma tag 8.0.10), reutiliza — sem isto,
# duas versões do JUCE no mesmo configure colidem nos targets juce::*.
if(NOT TARGET juce::juce_audio_processors)
    include(FetchContent)
    FetchContent_Declare(
        juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.10
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(juce)
endif()

juce_add_plugin(AromaVST3
    COMPANY_NAME "Aroma Low Latency Studio"
    PLUGIN_MANUFACTURER_CODE Arom
    PLUGIN_CODE Arom
    FORMATS VST3
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

#pragma once
// AromaVST3 — editor (Etapa 6, JUCE). Interface mínima inicial:
// ganho master + indicador de vozes ativas + nome do kit.
// A GUI completa (mapa MIDI, volumes por instrumento) é a Etapa 5
// aplicada ao plugin.
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class AromaEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit AromaEditor(AromaProcessor& processor);
    ~AromaEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    AromaProcessor& processor_;
    juce::Slider masterGain_;
    juce::Label voicesLabel_;
    juce::Label kitLabel_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AromaEditor)
};

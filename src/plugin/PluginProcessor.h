#pragma once
// AromaVST3 — processador VST3 (Etapa 6, JUCE).
// Reutiliza o MESMO DrumEngine do standalone: o host (Reaper) entrega
// MIDI no processBlock() e o motor mistura no buffer de saída do host.
// Nenhuma thread própria de áudio aqui — o Reaper é dono do callback.
#include <juce_audio_processors/juce_audio_processors.h>
#include "../engine/DrumEngine.h"

class AromaProcessor : public juce::AudioProcessor {
public:
    AromaProcessor();
    ~AromaProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Aroma Low Latency Studio"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Demo Kit"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    aroma::DrumEngine& engine() { return engine_; }

private:
    aroma::DrumEngine engine_;
    bool kitLoaded_ = false;
};

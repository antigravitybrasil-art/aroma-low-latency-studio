// AromaVST3 — processador (Etapa 6).
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "../common/Kit.h"

AromaProcessor::AromaProcessor()
    : AudioProcessor(BusesProperties()
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)) {}

AromaProcessor::~AromaProcessor() = default;

void AromaProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/) {
    // O kit demo é 48 kHz. Se o projeto do Reaper estiver noutra taxa,
    // o ideal é um kit na mesma taxa (ou reamostragem — fora do escopo
    // da Etapa 6 inicial; o diagnóstico define a taxa a usar).
    juce::ignoreUnused(sampleRate);
    if (!kitLoaded_) {
        // O kit viaja junto ao plugin (pasta "kits/demo-kit" ao lado do .vst3).
        // Caminho resolvido a partir do arquivo do plugin.
        auto kitFile = juce::File::getSpecialLocation(
                           juce::File::currentApplicationFile)
                           .getParentDirectory()
                           .getChildFile("kits")
                           .getChildFile("demo-kit")
                           .getChildFile("kit.txt");
        aroma::Kit kit;
        std::string error;
        if (aroma::loadKit(kitFile.getFullPathName().toStdString(), kit, error)) {
            kitLoaded_ = engine_.setKit(std::move(kit));
        }
    }
}

void AromaProcessor::releaseResources() {
    engine_.allNotesOff();
}

void AromaProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    // 1) MIDI do Reaper -> motor (noteOn/noteOff enfileiram; o render consome).
    for (const auto metadata : midi) {
        const auto msg = metadata.getMessage();
        if (msg.isNoteOn())
            engine_.noteOn(msg.getNoteNumber(), msg.getVelocity());
        else if (msg.isNoteOff())
            engine_.noteOff(msg.getNoteNumber());
    }
    // 2) Motor mistura no buffer do host. O host já zerou o buffer;
    //    o render preenche (não soma por cima de outra fonte aqui).
    const int channels = buffer.getNumChannels();
    if (channels >= 2) {
        // Renderiza intercalado num buffer temporário e copia por canal.
        // (Alocação aqui é do stack via juce::HeapBlock — fora do motor.)
        juce::HeapBlock<float> tmp(static_cast<size_t>(numSamples) * 2);
        engine_.render(tmp.get(), numSamples, 2);
        auto* left = buffer.getWritePointer(0);
        auto* right = buffer.getWritePointer(1);
        for (int i = 0; i < numSamples; ++i) {
            left[i] = tmp[i * 2];
            right[i] = tmp[i * 2 + 1];
        }
    } else if (channels == 1) {
        engine_.render(buffer.getWritePointer(0), numSamples, 1);
    }
    midi.clear(); // eventos consumidos
}

juce::AudioProcessorEditor* AromaProcessor::createEditor() {
    return new AromaEditor(*this);
}

void AromaProcessor::getStateInformation(juce::MemoryBlock& destData) {
    // Estado mínimo v1: ganho master. Kits/mapeamento virão na Etapa 6 final.
    float gain = engine_.masterGain();
    destData.append(&gain, sizeof(gain));
}

void AromaProcessor::setStateInformation(const void* data, int sizeInBytes) {
    if (data && sizeInBytes >= (int)sizeof(float)) {
        float gain = 0.0f;
        std::memcpy(&gain, data, sizeof(gain));
        engine_.setMasterGain(gain);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new AromaProcessor();
}

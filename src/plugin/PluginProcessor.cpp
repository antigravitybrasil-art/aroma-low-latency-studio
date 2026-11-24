// AromaVST3 — processador (Etapa 6).
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "../common/Kit.h"
#ifdef _WIN32
#include <windows.h>
#endif

namespace {

// O .vst3 é um pacote (bundle) e o host (Reaper) é outro EXE — nem
// currentApplicationFile nem currentExecutableFile apontam para o
// plugin. O caminho do próprio módulo (DLL) vem do handle do módulo
// que contém um símbolo deste arquivo.
juce::File pluginModuleFile() {
#ifdef _WIN32
    static int anchor = 0;
    HMODULE h = nullptr;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(&anchor), &h) &&
        h != nullptr) {
        wchar_t path[MAX_PATH];
        if (GetModuleFileNameW(h, path, MAX_PATH) > 0)
            return juce::File(juce::String(path));
    }
#endif
    return {};
}

} // namespace

AromaProcessor::AromaProcessor()
    : AudioProcessor(BusesProperties()
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)) {}

AromaProcessor::~AromaProcessor() = default;

void AromaProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/) {
    hostSampleRate_ = sampleRate;
    if (kitLoaded_) return;
    // O kit demo viaja junto ao plugin: a pasta "kits" vai ao lado do
    // pacote .vst3 (C:\Program Files\Common Files\VST3\kits\...). O DLL
    // do plugin fica em <pacote>\Contents\x86_64-win\ — 3 níveis acima
    // é o pacote, e ao lado dele fica a pasta "kits".
    const juce::File module = pluginModuleFile();
    std::vector<juce::File> candidates;
    if (module.existsAsFile()) {
        const juce::File bundleDir = module.getParentDirectory()
                                         .getParentDirectory()
                                         .getParentDirectory();
        candidates.push_back(bundleDir.getParentDirectory()
                                 .getChildFile("kits")
                                 .getChildFile("demo-kit")
                                 .getChildFile("kit.txt")); // ao lado do pacote
        candidates.push_back(bundleDir.getChildFile("Contents")
                                 .getChildFile("kits")
                                 .getChildFile("demo-kit")
                                 .getChildFile("kit.txt")); // dentro do pacote
    }
    candidates.push_back(juce::File::getSpecialLocation(
                             juce::File::currentApplicationFile)
                             .getParentDirectory()
                             .getChildFile("kits")
                             .getChildFile("demo-kit")
                             .getChildFile("kit.txt"));
    candidates.push_back(juce::File::getCurrentWorkingDirectory()
                             .getChildFile("kits")
                             .getChildFile("demo-kit")
                             .getChildFile("kit.txt"));
    aroma::Kit kit;
    std::string error;
    for (const auto& f : candidates) {
        if (aroma::loadKit(f.getFullPathName().toStdString(), kit, error)) {
            kitLoaded_ = engine_.setKit(std::move(kit));
            kitError_.clear();
            return;
        }
    }
    kitError_ = error.empty() ? "kit demo nao encontrado" : error;
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

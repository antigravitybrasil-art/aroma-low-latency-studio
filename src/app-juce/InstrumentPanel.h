#pragma once
// InstrumentPanel — editor da peça selecionada (Fase B, v0.5.0).
//
// Volume, pan, afinação, curva de velocity e EQ de 3 bandas (com a
// frequência do médio). Tudo ao vivo: mexer num slider aplica no motor
// na hora (updateInstrument) e vale para o próximo hit — sem parar o
// áudio. "Trocar sample…" abre o seletor de arquivo nativo; a troca
// em si é feita pela MainComponent (para o áudio, troca, recomeça).
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "../engine/DrumEngine.h"

namespace aroma {

class InstrumentPanel : public juce::Component {
public:
    using SampleCallback = std::function<void()>; // "Trocar sample…" clicado
    using ParamsCallback = std::function<void(const InstrumentParams&)>;

    InstrumentPanel();

    void setSampleCallback(SampleCallback cb) { onSample_ = std::move(cb); }
    void setParamsCallback(ParamsCallback cb) { onParams_ = std::move(cb); }

    // Mostra a peça (nome, nota, sample, choke) e os parâmetros atuais.
    void showInstrument(const KitInstrument& inst);
    // Sem peça selecionada (kit vazio).
    void clearInstrument();

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void addSlider(juce::Label& label, juce::Slider& slider, const juce::String& text,
                   double min, double max, double step, double defaultValue,
                   bool logScale = false);
    InstrumentParams currentParams() const;
    void paramsChanged();

    juce::Label titleLabel_, sampleLabel_, chokeLabel_;
    juce::TextButton sampleButton_{
        juce::String(juce::CharPointer_UTF8("Trocar sample…"))};
    juce::Label volumeLabel_, panLabel_, tuneLabel_, gammaLabel_;
    juce::Label eqLowLabel_, eqMidLabel_, eqMidFreqLabel_, eqHighLabel_;
    juce::Slider volumeSlider_, panSlider_, tuneSlider_, gammaSlider_;
    juce::Slider eqLowSlider_, eqMidSlider_, eqMidFreqSlider_, eqHighSlider_;

    SampleCallback onSample_;
    ParamsCallback onParams_;
    bool refreshing_ = false; // não reage aos próprios setValue
    bool hasInstrument_ = false;
};

} // namespace aroma

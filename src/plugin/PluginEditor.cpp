// AromaVST3 — editor (Etapa 6).
#include "PluginEditor.h"

AromaEditor::AromaEditor(AromaProcessor& p)
    : AudioProcessorEditor(&p), processor_(p) {
    setSize(420, 220);

    kitLabel_.setText("Kit: Demo Kit (Aroma Studio)", juce::dontSendNotification);
    kitLabel_.setFont(juce::Font(16.0f, juce::Font::bold));
    addAndMakeVisible(kitLabel_);

    masterGain_.setRange(0.0, 1.5, 0.01);
    masterGain_.setValue(processor_.engine().masterGain(), juce::dontSendNotification);
    masterGain_.setSliderStyle(juce::Slider::LinearHorizontal);
    masterGain_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    masterGain_.onValueChange = [this] {
        processor_.engine().setMasterGain(static_cast<float>(masterGain_.getValue()));
    };
    addAndMakeVisible(masterGain_);

    voicesLabel_.setText("Vozes ativas: 0", juce::dontSendNotification);
    addAndMakeVisible(voicesLabel_);

    startTimerHz(10); // atualiza o contador de vozes 10x por segundo
}

AromaEditor::~AromaEditor() { stopTimer(); }

void AromaEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff14232e)); // azul-petróleo (identidade do projeto)
    g.setColour(juce::Colours::white);
    g.setFont(15.0f);
    g.drawText("Aroma Low Latency Studio", 16, 8, 300, 20, juce::Justification::left);
    g.drawText("Ganho master", 16, 92, 120, 20, juce::Justification::left);
}

void AromaEditor::resized() {
    kitLabel_.setBounds(16, 36, 380, 24);
    masterGain_.setBounds(16, 116, 380, 28);
    voicesLabel_.setBounds(16, 160, 380, 24);
}

void AromaEditor::timerCallback() {
    voicesLabel_.setText("Vozes ativas: " + juce::String(processor_.engine().activeVoices()) +
                             (processor_.engine().clipped() ? "   CLIP!" : ""),
                         juce::dontSendNotification);
}

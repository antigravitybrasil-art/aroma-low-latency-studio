// AromaVST3 — editor (Etapa 6).
#include "PluginEditor.h"

AromaEditor::AromaEditor(AromaProcessor& p)
    : AudioProcessorEditor(&p), processor_(p) {
    setSize(460, 232);

    kitLabel_.setText("Kit: Demo Kit (Aroma Studio)", juce::dontSendNotification);
    kitLabel_.setFont(juce::Font(16.0f, juce::Font::bold));
    addAndMakeVisible(kitLabel_);

    statusLabel_.setFont(juce::Font(12.5f));
    addAndMakeVisible(statusLabel_);

    rateLabel_.setFont(juce::Font(12.5f));
    addAndMakeVisible(rateLabel_);

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
    g.drawText("Ganho master", 16, 106, 120, 20, juce::Justification::left);
}

void AromaEditor::resized() {
    kitLabel_.setBounds(16, 34, 428, 24);
    statusLabel_.setBounds(16, 58, 428, 20);
    rateLabel_.setBounds(16, 78, 428, 20);
    masterGain_.setBounds(16, 128, 428, 28);
    voicesLabel_.setBounds(16, 168, 428, 24);
}

void AromaEditor::timerCallback() {
    voicesLabel_.setText("Vozes ativas: " + juce::String(processor_.engine().activeVoices()) +
                             (processor_.engine().clipped() ? "   CLIP!" : ""),
                         juce::dontSendNotification);
    // Status do kit + taxa do projeto (o kit é 48 kHz; noutra taxa o
    // som sai fora do tom — a v1 avisa, não esconde).
    if (processor_.kitLoaded()) {
        statusLabel_.setText("Kit carregado: Demo Kit",
                             juce::dontSendNotification);
        statusLabel_.setColour(juce::Label::textColourId,
                               juce::Colour(0xff9fd8a0));
    } else {
        statusLabel_.setText("Kit NAO carregado: " +
                                 juce::String(processor_.kitError()),
                             juce::dontSendNotification);
        statusLabel_.setColour(juce::Label::textColourId,
                               juce::Colour(0xffff8080));
    }
    const double rate = processor_.hostSampleRate();
    if (rate == 48000.0) {
        rateLabel_.setText("Projeto: 48000 Hz",
                           juce::dontSendNotification);
        rateLabel_.setColour(juce::Label::textColourId,
                             juce::Colour(0xffc8c8d0));
    } else {
        rateLabel_.setText("Projeto: " + juce::String(rate, 0) +
                               juce::String(juce::CharPointer_UTF8(
                                   " Hz — use 48 kHz (o kit e 48 kHz)")),
                           juce::dontSendNotification);
        rateLabel_.setColour(juce::Label::textColourId,
                             juce::Colour(0xffffc040));
    }
}

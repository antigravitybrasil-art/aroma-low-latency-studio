#include "StatusBar.h"

namespace aroma {

StatusBar::StatusBar() {
    masterLabel_.setFont(juce::Font(juce::FontOptions(12.5f)));
    masterLabel_.setColour(juce::Label::textColourId, juce::Colour(0xffd8d8de));
    addAndMakeVisible(masterLabel_);
    masterSlider_.setRange(0.0, 1.5, 0.01);
    masterSlider_.setValue(1.0, juce::dontSendNotification);
    masterSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
    masterSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 52, 20);
    masterSlider_.onValueChange = [this] {
        if (onGain_) onGain_((float)masterSlider_.getValue());
    };
    addAndMakeVisible(masterSlider_);

    toggleButton_.onClick = [this] { if (onToggle_) onToggle_(); };
    restartButton_.onClick = [this] { if (onRestart_) onRestart_(); };
    addAndMakeVisible(toggleButton_);
    addAndMakeVisible(restartButton_);

    for (auto* l : {&audioLabel_, &midiLabel_, &countersLabel_}) {
        l->setFont(juce::Font(juce::FontOptions(12.0f)));
        l->setColour(juce::Label::textColourId, juce::Colour(0xffc8c8d0));
        addAndMakeVisible(l);
    }
}

void StatusBar::setAudioRunning(bool running) {
    toggleButton_.setButtonText(
        juce::CharPointer_UTF8(running ? "Parar áudio" : "Iniciar áudio"));
}

void StatusBar::setAudioInfo(const juce::String& info) {
    audioLabel_.setText(info, juce::dontSendNotification);
}

void StatusBar::setMidiInfo(const juce::String& info) {
    midiLabel_.setText(info, juce::dontSendNotification);
}

void StatusBar::setCounters(uint64_t underruns, uint64_t callbacks, int voices,
                            uint64_t noteOns) {
    countersLabel_.setText(
        "Underruns: " + juce::String((juce::int64)underruns) +
            "   Callbacks: " + juce::String((juce::int64)callbacks) +
            "   Vozes: " + juce::String(voices) +
            "   Note Ons: " + juce::String((juce::int64)noteOns) +
            (clipped_ ? "   CLIP!" : ""),
        juce::dontSendNotification);
}

void StatusBar::setClipped(bool clipped) {
    if (clipped_ != clipped) {
        clipped_ = clipped;
        repaint();
    }
}

void StatusBar::setMasterGain(float g) {
    masterSlider_.setValue(g, juce::dontSendNotification);
}

void StatusBar::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff16161b));
    if (clipped_) {
        g.setColour(juce::Colour(0xffff4040));
        g.fillEllipse(getLocalBounds().getWidth() - 22.0f, 8.0f, 10.0f, 10.0f);
    }
}

void StatusBar::resized() {
    auto area = getLocalBounds().reduced(8, 6);
    masterLabel_.setBounds(area.removeFromLeft(52));
    masterSlider_.setBounds(area.removeFromLeft(170));
    area.removeFromLeft(10);
    toggleButton_.setBounds(area.removeFromLeft(110));
    area.removeFromLeft(6);
    restartButton_.setBounds(area.removeFromLeft(120));
    area.removeFromLeft(12);
    // O resto é dividido em 3 rótulos.
    const int w = area.getWidth() / 3;
    audioLabel_.setBounds(area.removeFromLeft(w));
    midiLabel_.setBounds(area.removeFromLeft(w));
    countersLabel_.setBounds(area);
}

} // namespace aroma

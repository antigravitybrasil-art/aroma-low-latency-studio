#include "InstrumentPanel.h"

namespace aroma {

void InstrumentPanel::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff1a1a20));
}

InstrumentPanel::InstrumentPanel() {
    titleLabel_.setFont(juce::Font(juce::FontOptions(17.0f, juce::Font::bold)));
    titleLabel_.setColour(juce::Label::textColourId, juce::Colour(0xfff0a030));
    addAndMakeVisible(titleLabel_);
    sampleLabel_.setFont(juce::Font(juce::FontOptions(12.5f)));
    sampleLabel_.setColour(juce::Label::textColourId, juce::Colour(0xffc8c8d0));
    addAndMakeVisible(sampleLabel_);
    chokeLabel_.setFont(juce::Font(juce::FontOptions(12.5f)));
    chokeLabel_.setColour(juce::Label::textColourId, juce::Colour(0xff9a9aa6));
    addAndMakeVisible(chokeLabel_);

    sampleButton_.onClick = [this] { if (onSample_) onSample_(); };
    addAndMakeVisible(sampleButton_);

    addSlider(volumeLabel_, volumeSlider_, "Volume", 0.0, 2.0, 0.01, 1.0);
    addSlider(panLabel_, panSlider_, "Pan (E < > D)", -1.0, 1.0, 0.01, 0.0);
    addSlider(tuneLabel_, tuneSlider_,
              juce::String(juce::CharPointer_UTF8("Afinação (semitons)")),
              -24.0, 24.0, 0.5, 0.0);
    addSlider(gammaLabel_, gammaSlider_, "Velocity (curva)", 0.1, 4.0, 0.05, 1.0);
    addSlider(eqLowLabel_, eqLowSlider_, "EQ grave (dB)", -18.0, 18.0, 0.5, 0.0);
    addSlider(eqMidLabel_, eqMidSlider_,
              juce::String(juce::CharPointer_UTF8("EQ médio (dB)")),
              -18.0, 18.0, 0.5, 0.0);
    addSlider(eqMidFreqLabel_, eqMidFreqSlider_,
              juce::String(juce::CharPointer_UTF8("EQ médio (freq Hz)")),
              50.0, 16000.0, 1.0, 1000.0, true);
    addSlider(eqHighLabel_, eqHighSlider_, "EQ agudo (dB)", -18.0, 18.0, 0.5, 0.0);
    clearInstrument();
}

void InstrumentPanel::addSlider(juce::Label& label, juce::Slider& slider,
                                const juce::String& text, double min, double max,
                                double step, double defaultValue, bool logScale) {
    label.setText(text, juce::dontSendNotification);
    label.setFont(juce::Font(juce::FontOptions(12.5f)));
    label.setColour(juce::Label::textColourId, juce::Colour(0xffd8d8de));
    addAndMakeVisible(label);
    slider.setRange(min, max, step);
    slider.setValue(defaultValue, juce::dontSendNotification);
    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 64, 20);
    if (logScale) slider.setSkewFactorFromMidPoint(1000.0);
    slider.onValueChange = [this] { paramsChanged(); };
    addAndMakeVisible(slider);
}

InstrumentParams InstrumentPanel::currentParams() const {
    InstrumentParams p;
    p.volume = (float)volumeSlider_.getValue();
    p.pan = (float)panSlider_.getValue();
    p.tune = (float)tuneSlider_.getValue();
    p.gamma = (float)gammaSlider_.getValue();
    p.eqLowDb = (float)eqLowSlider_.getValue();
    p.eqMidDb = (float)eqMidSlider_.getValue();
    p.eqMidFreq = (float)eqMidFreqSlider_.getValue();
    p.eqHighDb = (float)eqHighSlider_.getValue();
    return p;
}

void InstrumentPanel::paramsChanged() {
    if (refreshing_ || !hasInstrument_ || !onParams_) return;
    onParams_(currentParams());
}

void InstrumentPanel::showInstrument(const KitInstrument& inst) {
    hasInstrument_ = true;
    refreshing_ = true;
    titleLabel_.setText(juce::String(inst.name) +
                            juce::String(juce::CharPointer_UTF8("  —  nota ")) +
                            juce::String(inst.note) + " (" +
                            juce::String(aroma::noteName((uint8_t)inst.note)) + ")",
                        juce::dontSendNotification);
    sampleLabel_.setText("Sample: " + juce::String(inst.file), juce::dontSendNotification);
    chokeLabel_.setText(
        juce::String("Grupo de choke: ") + juce::String(inst.chokeGroup) +
            (inst.hasLayers
                 ? juce::String(juce::CharPointer_UTF8("  •  camadas soft/hard"))
                 : juce::String()),
        juce::dontSendNotification);
    volumeSlider_.setValue(inst.volume, juce::dontSendNotification);
    panSlider_.setValue(inst.pan, juce::dontSendNotification);
    tuneSlider_.setValue(inst.tune, juce::dontSendNotification);
    gammaSlider_.setValue(inst.gamma, juce::dontSendNotification);
    eqLowSlider_.setValue(inst.eqLowDb, juce::dontSendNotification);
    eqMidSlider_.setValue(inst.eqMidDb, juce::dontSendNotification);
    eqMidFreqSlider_.setValue(inst.eqMidFreq, juce::dontSendNotification);
    eqHighSlider_.setValue(inst.eqHighDb, juce::dontSendNotification);
    refreshing_ = false;
    setEnabled(true);
    resized();
}

void InstrumentPanel::clearInstrument() {
    hasInstrument_ = false;
    titleLabel_.setText(
        juce::String(juce::CharPointer_UTF8("(nenhuma peça selecionada)")),
        juce::dontSendNotification);
    sampleLabel_.setText("", juce::dontSendNotification);
    chokeLabel_.setText("", juce::dontSendNotification);
    setEnabled(false);
}

void InstrumentPanel::resized() {
    auto area = getLocalBounds().reduced(10);
    titleLabel_.setBounds(area.removeFromTop(24));
    sampleLabel_.setBounds(area.removeFromTop(18));
    chokeLabel_.setBounds(area.removeFromTop(18));
    area.removeFromTop(6);
    sampleButton_.setBounds(area.removeFromTop(28));
    area.removeFromTop(8);
    juce::Label* labels[] = {&volumeLabel_, &panLabel_, &tuneLabel_, &gammaLabel_,
                             &eqLowLabel_, &eqMidLabel_, &eqMidFreqLabel_, &eqHighLabel_};
    juce::Slider* sliders[] = {&volumeSlider_, &panSlider_, &tuneSlider_, &gammaSlider_,
                               &eqLowSlider_, &eqMidSlider_, &eqMidFreqSlider_, &eqHighSlider_};
    for (int i = 0; i < 8; ++i) {
        auto row = area.removeFromTop(24);
        labels[i]->setBounds(row.removeFromLeft(150));
        sliders[i]->setBounds(row);
        area.removeFromTop(4);
    }
}

} // namespace aroma

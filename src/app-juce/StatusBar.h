#pragma once
// StatusBar — volume master + estado do áudio/MIDI, sempre visíveis
// (Fase B, v0.5.0). Atualizada pelo timer da MainComponent; não toca
// em nada de áudio por conta própria.
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace aroma {

class StatusBar : public juce::Component {
public:
    using ToggleCallback = std::function<void()>;
    using RestartCallback = std::function<void()>;
    using GainCallback = std::function<void(float)>;

    StatusBar();

    void setToggleCallback(ToggleCallback cb) { onToggle_ = std::move(cb); }
    void setRestartCallback(RestartCallback cb) { onRestart_ = std::move(cb); }
    void setGainCallback(GainCallback cb) { onGain_ = std::move(cb); }

    void setAudioRunning(bool running);
    void setAudioInfo(const juce::String& info);
    void setMidiInfo(const juce::String& info);
    void setCounters(uint64_t underruns, uint64_t callbacks, int voices,
                     uint64_t noteOns);
    void setClipped(bool clipped);
    void setMasterGain(float g);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::Label masterLabel_{{}, "Master"};
    juce::Slider masterSlider_;
    juce::TextButton toggleButton_{
        juce::String(juce::CharPointer_UTF8("Parar áudio"))};
    juce::TextButton restartButton_{
        juce::String(juce::CharPointer_UTF8("Reiniciar áudio"))};
    juce::Label audioLabel_, midiLabel_, countersLabel_;
    bool clipped_ = false;

    ToggleCallback onToggle_;
    RestartCallback onRestart_;
    GainCallback onGain_;
};

} // namespace aroma

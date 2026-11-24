#pragma once
// DrumKitView — a bateria vista de cima (Fase B, v0.5.0).
//
// Desenha a imagem do kit (assets/drumkit_topdown.png, ao lado do EXE)
// e acende cada peça no instante do toque (flash que decai em ~300 ms).
// Clicar numa peça toca o sample dela (audição) e a seleciona no editor.
// Sem a imagem, desenha o kit com formas vetoriais (círculos + rótulos)
// — a tela nunca fica sem a bateria.
//
// A UI nunca é tocada pela thread de áudio/MIDI: MainComponent chama
// hit() no timer (message thread) a partir do último hit do AppModel.
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <functional>

namespace aroma {

struct KitZone {
    int note;               // nota MIDI da peça (-1 = sem nota)
    float x, y, w, h;       // retângulo normalizado (0..1) sobre a imagem
    const char* label;      // rótulo curto
};

// Zonas sobre a imagem top-down (1920x1280). Medidas com visual
// grounding em 23/11/2026 (centros reais: crash 0.19/0.10, hi-hat
// 0.07/0.41, tom1 0.29/0.36, tom2 0.50/0.30, chão 0.72/0.36, ride
// 0.92/0.27, caixa 0.21/0.77, bumbo 0.50/0.60). O flash é um anel
// sobre a zona, não um recorte exato.
inline constexpr KitZone kKitZones[] = {
    {49, 0.05f, 0.02f, 0.27f, 0.22f, "Crash"},
    {42, 0.00f, 0.33f, 0.15f, 0.24f, "Hi-hat"},
    {50, 0.21f, 0.26f, 0.15f, 0.22f, "Tom 1"},
    {47, 0.42f, 0.19f, 0.15f, 0.22f, "Tom 2"},
    {43, 0.64f, 0.25f, 0.16f, 0.22f, "Tom de chão"},
    {51, 0.79f, 0.09f, 0.21f, 0.35f, "Ride"},
    {38, 0.10f, 0.64f, 0.20f, 0.26f, "Caixa"},
    {36, 0.36f, 0.47f, 0.28f, 0.30f, "Bumbo"},
};
// Peças sem zona própria acendem a zona da peça "mãe":
// aro (37) -> caixa, bell (59) -> ride, pedal (44) e aberto (46) -> hi-hat.
inline int zoneNoteForFlash(int note) {
    switch (note) {
        case 37: return 38;
        case 59: return 51;
        case 44: case 46: return 42;
        default: return note;
    }
}

class DrumKitView : public juce::Component, private juce::Timer {
public:
    using HitCallback = std::function<void(int note)>; // clique numa peça

    DrumKitView();
    // Tenta carregar a imagem do kit (procura em vários caminhos).
    // Devolve true se achou; senão o desenho é vetorial.
    bool loadKitImage(const juce::File& exeDir);
    void setHitCallback(HitCallback cb) { onHit_ = std::move(cb); }
    // Acende a peça (chamado na message thread, pelo timer da janela).
    void hit(int note, int velocity);
    void setSelectedNote(int note) { selectedNote_ = note; repaint(); }

    void paint(juce::Graphics& g) override;
    void resized() override {}
    void mouseDown(const juce::MouseEvent& e) override;

private:
    void timerCallback() override; // decai o flash e repinta
    // Retângulo onde a imagem é desenhada (ou a área toda, no fallback).
    juce::Rectangle<float> imageBounds() const;

    juce::Image kitImage_;
    std::array<float, 128> flash_{};
    int selectedNote_ = 36;
    HitCallback onHit_;
};

} // namespace aroma

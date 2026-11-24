#include "DrumKitView.h"

namespace aroma {

DrumKitView::DrumKitView() {
    flash_.fill(0.0f);
    startTimerHz(30); // decaimento suave do flash
}

bool DrumKitView::loadKitImage(const juce::File& exeDir) {
    juce::File candidates[] = {
        exeDir.getChildFile("../assets/drumkit_topdown.png"),
        exeDir.getChildFile("assets/drumkit_topdown.png"),
        juce::File::getCurrentWorkingDirectory().getChildFile("assets/drumkit_topdown.png"),
        juce::File::getCurrentWorkingDirectory().getChildFile("../assets/drumkit_topdown.png"),
    };
    for (const auto& f : candidates) {
        if (f.existsAsFile()) {
            kitImage_ = juce::ImageFileFormat::loadFrom(f);
            if (kitImage_.isValid()) {
                repaint();
                return true;
            }
        }
    }
    return false;
}

void DrumKitView::hit(int note, int velocity) {
    if (note < 0 || note > 127) return;
    const int zoneNote = zoneNoteForFlash(note);
    flash_[static_cast<size_t>(zoneNote)] = 0.35f + 0.65f * (velocity / 127.0f);
    selectedNote_ = note;
    repaint();
}

void DrumKitView::timerCallback() {
    bool any = false;
    for (auto& f : flash_) {
        if (f > 0.0f) {
            f -= 0.11f; // ~300 ms até apagar (30 Hz)
            if (f < 0.0f) f = 0.0f;
            any = true;
        }
    }
    if (any) repaint();
}

juce::Rectangle<float> DrumKitView::imageBounds() const {
    const auto area = getLocalBounds().toFloat();
    if (!kitImage_.isValid()) return area;
    // "contain": a foto inteira cabe na área (só reduz, nunca amplia).
    // onlyIncreaseInSize estava errado — a foto 1920×1280 ficava em
    // tamanho nativo e cortada (visto no screenshot de 23/11/2026).
    return juce::RectanglePlacement(
        juce::RectanglePlacement::centred |
        juce::RectanglePlacement::onlyReduceInSize).appliedTo(
            juce::Rectangle<float>(0, 0, (float)kitImage_.getWidth(),
                                   (float)kitImage_.getHeight()),
            area);
}

void DrumKitView::mouseDown(const juce::MouseEvent& e) {
    const auto bounds = imageBounds();
    const float nx = (e.position.x - bounds.getX()) / bounds.getWidth();
    const float ny = (e.position.y - bounds.getY()) / bounds.getHeight();
    // As zonas são normalizadas sobre a imagem desenhada.
    for (const auto& z : kKitZones) {
        if (nx >= z.x && nx <= z.x + z.w && ny >= z.y && ny <= z.y + z.h) {
            if (onHit_) onHit_(z.note);
            return;
        }
    }
}

void DrumKitView::paint(juce::Graphics& g) {
    const auto area = getLocalBounds().toFloat();
    g.fillAll(juce::Colour(0xff121216));

    if (kitImage_.isValid()) {
        g.drawImage(kitImage_, imageBounds());
        // Vinheta leve para o flash aparecer sobre a foto.
        g.setColour(juce::Colour(0x20000000));
        g.fillRect(area);
    } else {
        // Fallback vetorial: círculos/elipses no lugar de cada peça.
        g.setColour(juce::Colour(0xff1c1c22));
        g.fillRoundedRectangle(area, 8.0f);
        for (const auto& z : kKitZones) {
            const auto r = juce::Rectangle<float>(
                area.getX() + z.x * area.getWidth(),
                area.getY() + z.y * area.getHeight(),
                z.w * area.getWidth(), z.h * area.getHeight());
            g.setColour(juce::Colour(0xff2a2a32));
            g.fillEllipse(r);
            g.setColour(juce::Colour(0xff8a8a96));
            g.drawEllipse(r, 1.5f);
        }
    }

    // Flash + rótulos das zonas (sobre a imagem desenhada).
    const auto imgArea = imageBounds();
    for (const auto& z : kKitZones) {
        const auto r = juce::Rectangle<float>(
            imgArea.getX() + z.x * imgArea.getWidth(),
            imgArea.getY() + z.y * imgArea.getHeight(),
            z.w * imgArea.getWidth(), z.h * imgArea.getHeight());
        const float f = flash_[static_cast<size_t>(z.note)];
        if (f > 0.01f) {
            const auto glow = juce::Colour(0xfff0a030).withAlpha(f);
            g.setColour(glow);
            g.drawEllipse(r.expanded(3.0f), 3.0f);
            g.setColour(glow.withAlpha(f * 0.35f));
            g.fillEllipse(r);
        }
        // Rótulo da peça (sempre visível, discreto).
        g.setColour(z.note == selectedNote_
                        ? juce::Colour(0xfff0a030)
                        : juce::Colour(0xffd8d8de));
        g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
        g.drawText(juce::String(juce::CharPointer_UTF8(z.label)),
                   r.withY(r.getBottom() - 18).withHeight(16),
                   juce::Justification::centred, false);
        if (z.note == selectedNote_) {
            g.setColour(juce::Colour(0xfff0a030));
            g.drawRoundedRectangle(r, 6.0f, 1.5f);
        }
    }
}

} // namespace aroma

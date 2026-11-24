#include "KitBrowser.h"
#include <memory>
#include <utility>

namespace aroma {

KitBrowser::KitBrowser() {
    titleLabel_.setFont(juce::Font(juce::FontOptions(15.0f, juce::Font::bold)));
    titleLabel_.setColour(juce::Label::textColourId, juce::Colour(0xfff0a030));
    addAndMakeVisible(titleLabel_);
    listBox_.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff16161b));
    listBox_.setColour(juce::ListBox::textColourId, juce::Colour(0xffd8d8de));
    listBox_.setRowHeight(22);
    addAndMakeVisible(listBox_);

    newButton_.onClick = [this] {
        askName("Novo kit", "Meu Kit", [this](const std::string& n) { if (onNew_) onNew_(n); });
    };
    saveButton_.onClick = [this] { if (onSave_) onSave_(); };
    loadButton_.onClick = [this] { if (onLoad_) onLoad_(); };
    duplicateButton_.onClick = [this] {
        askName("Duplicar kit", selectedKit() + " (copia)",
                [this](const std::string& n) { if (onDuplicate_) onDuplicate_(n); });
    };
    addAndMakeVisible(newButton_);
    addAndMakeVisible(saveButton_);
    addAndMakeVisible(loadButton_);
    addAndMakeVisible(duplicateButton_);
}

void KitBrowser::setKits(const std::vector<std::string>& names, const std::string& current) {
    kits_ = names;
    listBox_.updateContent();
    for (size_t i = 0; i < kits_.size(); ++i) {
        if (kits_[i] == current) {
            listBox_.selectRow((int)i, false, false);
            break;
        }
    }
    repaint();
}

std::string KitBrowser::selectedKit() const {
    const int row = listBox_.getSelectedRow();
    if (row < 0 || row >= (int)kits_.size()) return {};
    return kits_[(size_t)row];
}

void KitBrowser::askName(const juce::String& title, const juce::String& initial,
                         NameCallback cb) {
    // JUCE 8 não tem mais AlertWindow::runModalLoop: o diálogo é
    // assíncrono (enterModalState). A janela vive num shared_ptr até o
    // usuário responder; o callback só dispara com o nome preenchido.
    auto w = std::make_shared<juce::AlertWindow>(
        title, "Nome do kit:", juce::MessageBoxIconType::QuestionIcon);
    w->addTextEditor("name", initial, "");
    w->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
    w->addButton("Cancelar", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    w->enterModalState(
        true,
        juce::ModalCallbackFunction::create(
            [w, cb = std::move(cb)](int result) mutable {
                if (result == 1) {
                    const auto text = w->getTextEditorContents("name").trim();
                    if (text.isNotEmpty() && cb) cb(text.toStdString());
                }
            }));
}

void KitBrowser::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff1a1a20));
}

void KitBrowser::resized() {
    auto area = getLocalBounds().reduced(10);
    titleLabel_.setBounds(area.removeFromTop(22));
    area.removeFromTop(4);
    auto buttons = area.removeFromBottom(30);
    const int bw = buttons.getWidth() / 4 - 3;
    newButton_.setBounds(buttons.removeFromLeft(bw));
    buttons.removeFromLeft(4);
    saveButton_.setBounds(buttons.removeFromLeft(bw));
    buttons.removeFromLeft(4);
    loadButton_.setBounds(buttons.removeFromLeft(bw));
    buttons.removeFromLeft(4);
    duplicateButton_.setBounds(buttons);
    area.removeFromBottom(6);
    listBox_.setBounds(area);
}

int KitBrowser::getNumRows() { return (int)kits_.size(); }

void KitBrowser::paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height,
                                  bool rowIsSelected) {
    if (rowNumber < 0 || rowNumber >= (int)kits_.size()) return;
    if (rowIsSelected) {
        g.fillAll(juce::Colour(0xff3a2a12));
        g.setColour(juce::Colour(0xfff0a030));
    } else {
        g.setColour(juce::Colour(0xffd8d8de));
    }
    g.setFont(juce::Font(juce::FontOptions(13.0f)));
    g.drawText(juce::String(kits_[(size_t)rowNumber]), 6, 0, width - 12, height,
               juce::Justification::centredLeft, true);
}

void KitBrowser::listBoxItemClicked(int row, const juce::MouseEvent&) {
    (void)row;
}

} // namespace aroma

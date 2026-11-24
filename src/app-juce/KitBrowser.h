#pragma once
// KitBrowser — lista de kits + Novo / Salvar / Carregar / Duplicar
// (Fase B, v0.5.0). A lógica vive no AppModel; aqui só há botões e
// a lista. Novo e Duplicar pedem o nome numa janelinha de texto.
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <string>
#include <vector>

namespace aroma {

class KitBrowser : public juce::Component, private juce::ListBoxModel {
public:
    using NameCallback = std::function<void(const std::string& name)>;
    using VoidCallback = std::function<void()>;

    KitBrowser();

    void setKits(const std::vector<std::string>& names, const std::string& current);
    void setNewCallback(NameCallback cb) { onNew_ = std::move(cb); }
    void setSaveCallback(VoidCallback cb) { onSave_ = std::move(cb); }
    void setLoadCallback(VoidCallback cb) { onLoad_ = std::move(cb); }
    void setDuplicateCallback(NameCallback cb) { onDuplicate_ = std::move(cb); }
    std::string selectedKit() const;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // ListBoxModel
    int getNumRows() override;
    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height,
                          bool rowIsSelected) override;
    void listBoxItemClicked(int row, const juce::MouseEvent&) override;

private:
    void askName(const juce::String& title, const juce::String& initial,
                 NameCallback cb);

    juce::Label titleLabel_{{}, "Kits"};
    juce::ListBox listBox_{"kits", this};
    juce::TextButton newButton_{"Novo"};
    juce::TextButton saveButton_{"Salvar"};
    juce::TextButton loadButton_{"Carregar"};
    juce::TextButton duplicateButton_{"Duplicar"};

    std::vector<std::string> kits_;
    NameCallback onNew_, onDuplicate_;
    VoidCallback onSave_, onLoad_;
};

} // namespace aroma

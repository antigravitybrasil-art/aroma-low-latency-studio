#include "MainComponent.h"

namespace aroma {

MainComponent::MainComponent() : startTime_(std::chrono::steady_clock::now()) {
    // Paleta escura premium (global do app).
    auto& lf = juce::LookAndFeel::getDefaultLookAndFeel();
    lf.setColour(juce::Slider::trackColourId, juce::Colour(0xff3a3a44));
    lf.setColour(juce::Slider::thumbColourId, juce::Colour(0xfff0a030));
    lf.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffd8d8de));
    lf.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff1c1c22));
    lf.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a2a32));
    lf.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff3a2a12));
    lf.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffd8d8de));
    lf.setColour(juce::TextButton::textColourOnId, juce::Colour(0xfff0a030));
    lf.setColour(juce::Label::textColourId, juce::Colour(0xffd8d8de));

    headerLabel_.setFont(juce::Font(juce::FontOptions(18.0f, juce::Font::bold)));
    headerLabel_.setColour(juce::Label::textColourId, juce::Colour(0xfff0a030));
    addAndMakeVisible(headerLabel_);

    kitView_.setHitCallback([this](int note) { onKitViewHit(note); });
    addAndMakeVisible(kitView_);

    kitBrowser_.setNewCallback([this](const std::string& n) { onKitNew(n); });
    kitBrowser_.setSaveCallback([this] { onKitSave(); });
    kitBrowser_.setLoadCallback([this] { onKitLoad(); });
    kitBrowser_.setDuplicateCallback([this](const std::string& n) { onKitDuplicate(n); });
    addAndMakeVisible(kitBrowser_);

    addAndMakeVisible(instrumentList_);

    instrumentPanel_.setSampleCallback([this] { onUploadSample(); });
    instrumentPanel_.setParamsCallback(
        [this](const InstrumentParams& p) { model_.updateSelectedParams(p); });
    // O painel da peça tem ~350 px de conteúdo e a janela pode ser mais
    // baixa que isso (notebook 768 px): sem rolagem, os últimos sliders
    // de EQ ficavam cortados e inalcançáveis. O Viewport rola o painel
    // (só vertical) quando o espaço não basta.
    instrumentPanel_.setSize(340, 350);
    panelViewport_.setViewedComponent(&instrumentPanel_, false);
    panelViewport_.setScrollBarsShown(true, false);
    addAndMakeVisible(panelViewport_);

    statusBar_.setToggleCallback([this] {
        if (audioOn_) stopAudio();
        else if (!stream_.isOpen()) openAudio();
        else if (stream_.start(model_.engine())) {
            audioOn_ = true;
            statusBar_.setAudioRunning(true);
        } else {
            showError("Audio: " + juce::String(stream_.lastError()));
        }
    });
    statusBar_.setRestartCallback([this] { restartAudio(); });
    statusBar_.setGainCallback([this](float g) { model_.setMasterGain(g); });
    addAndMakeVisible(statusBar_);

    // Imagem da bateria (ao lado do EXE, em assets/).
    const auto exeDir = juce::File::getSpecialLocation(
        juce::File::currentExecutableFile).getParentDirectory();
    kitView_.loadKitImage(exeDir);

    kitsDir_ = findKitsDir();
    loadDemoKit();
    openMidi();
    openAudio();
    refreshKitUi();
    startTimer(100);
}

MainComponent::~MainComponent() {
    stopTimer();
    stream_.close();
    midi_.close();
}

uint64_t MainComponent::nowMs() const {
    return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startTime_).count();
}

std::string MainComponent::findKitsDir() const {
    const auto exeDir = juce::File::getSpecialLocation(
        juce::File::currentExecutableFile).getParentDirectory();
    const juce::File candidates[] = {
        exeDir.getChildFile("../kits"),
        exeDir.getChildFile("kits"),
        juce::File::getCurrentWorkingDirectory().getChildFile("kits"),
        juce::File::getCurrentWorkingDirectory().getChildFile("../kits"),
    };
    for (const auto& d : candidates) {
        if (d.getChildFile("demo-kit/kit.txt").existsAsFile())
            return d.getFullPathName().toStdString();
    }
    return exeDir.getChildFile("../kits").getFullPathName().toStdString();
}

void MainComponent::loadDemoKit() {
    std::string err;
    if (!model_.loadKit(kitsDir_ + "/demo-kit/kit.txt", err)) {
        headerLabel_.setText(
            juce::String(juce::CharPointer_UTF8(
                "AROMA STUDIO  —  kit demo não encontrado (")) +
                juce::String(err) + ")",
            juce::dontSendNotification);
    }
}

void MainComponent::openMidi() {
    const auto devices = midi_.enumerateDevices();
    if (devices.empty()) {
        statusBar_.setMidiInfo("MIDI: nenhum dispositivo encontrado");
        return;
    }
    int chosen = 0;
    for (const auto& d : devices) {
        if (d.looksLikeAroma) { chosen = d.index; break; }
    }
    std::string err;
    if (!midi_.open(chosen, [this](const MidiEvent& ev) {
            if (ev.isNoteOn()) model_.noteOn(ev.data1, ev.data2, nowMs());
            else if (ev.isNoteOff()) model_.noteOff(ev.data1);
        }, err)) {
        statusBar_.setMidiInfo("MIDI: " + juce::String(err));
        return;
    }
    statusBar_.setMidiInfo("MIDI: " + juce::String(midi_.deviceName()) + " (canal 10)");
}

void MainComponent::openAudio() {
    std::string err;
    if (!stream_.open(256, err)) {
        audioError_ = err;
        statusBar_.setAudioInfo(juce::String(juce::CharPointer_UTF8("Áudio: ")) +
                                juce::String(err));
        audioOn_ = false;
        statusBar_.setAudioRunning(false);
        return;
    }
    if (!stream_.start(model_.engine())) {
        audioError_ = stream_.lastError();
        statusBar_.setAudioInfo(juce::String(juce::CharPointer_UTF8("Áudio: ")) +
                                juce::String(stream_.lastError()));
        stream_.close();
        audioOn_ = false;
        statusBar_.setAudioRunning(false);
        return;
    }
    audioError_.clear();
    audioOn_ = true;
    invalidatedShown_ = false;
    statusBar_.setAudioRunning(true);
}

void MainComponent::stopAudio() {
    stream_.stop();
    audioOn_ = false;
    statusBar_.setAudioRunning(false);
}

void MainComponent::restartAudio() {
    stream_.close();
    openAudio();
}

template <typename Fn> void MainComponent::withAudioStopped(Fn&& fn) {
    const bool wasRunning = audioOn_;
    if (wasRunning) stopAudio();
    fn();
    if (wasRunning) {
        if (!stream_.isOpen()) openAudio();
        else if (stream_.start(model_.engine())) {
            audioOn_ = true;
            statusBar_.setAudioRunning(true);
        }
    }
}

void MainComponent::refreshKitUi() {
    // Cabeçalho: nome do kit + "*" se houver edição não salva.
    headerLabel_.setText(
        juce::String(juce::CharPointer_UTF8("AROMA STUDIO  —  ")) +
            juce::String(model_.kitName()) + (model_.isDirty() ? "  *" : ""),
        juce::dontSendNotification);

    // Navegador de kits.
    const auto kits = AppModel::listKits(kitsDir_);
    std::string current;
    const std::string path = model_.kitPath();
    const size_t slash = path.find_last_of("/\\");
    if (slash != std::string::npos) {
        const std::string dir = path.substr(0, slash);
        const size_t slash2 = dir.find_last_of("/\\");
        current = slash2 == std::string::npos ? dir : dir.substr(slash2 + 1);
    }
    kitBrowser_.setKits(kits, current);

    // Lista de peças.
    instrumentButtons_.clear();
    instrumentNotes_.clear();
    instrumentList_.removeAllChildren();
    for (const auto& inst : model_.kit().instruments) {
        auto btn = std::make_unique<juce::TextButton>(juce::String(inst.name));
        btn->setClickingTogglesState(true);
        btn->setToggleState(inst.note == model_.selectedNote(), juce::dontSendNotification);
        const int note = inst.note;
        btn->onClick = [this, note] { selectInstrument(note); };
        instrumentList_.addAndMakeVisible(btn.get());
        instrumentButtons_.push_back(std::move(btn));
        instrumentNotes_.push_back(note);
    }

    // Painel da peça selecionada.
    if (const auto* inst = model_.kit().findByNote(model_.selectedNote()))
        instrumentPanel_.showInstrument(*inst);
    else
        instrumentPanel_.clearInstrument();
    kitView_.setSelectedNote(model_.selectedNote());
    resized();
}

void MainComponent::selectInstrument(int note) {
    if (!model_.selectInstrument(note)) return;
    if (const auto* inst = model_.kit().findByNote(note))
        instrumentPanel_.showInstrument(*inst);
    kitView_.setSelectedNote(note);
    // Só atualiza o destaque dos botões (sem reconstruir a lista — a
    // seleção automática acontece a cada hit e não pode piscar a UI).
    for (size_t i = 0; i < instrumentButtons_.size(); ++i)
        instrumentButtons_[i]->setToggleState(instrumentNotes_[i] == note,
                                              juce::dontSendNotification);
}

void MainComponent::onKitViewHit(int note) {
    // Clicar na peça: toca o sample (audição) e seleciona no editor.
    model_.noteOn(note, 100, nowMs());
    selectInstrument(note);
}

void MainComponent::onUploadSample() {
    const int note = model_.selectedNote();
    auto chooser = std::make_shared<juce::FileChooser>(
        juce::String(juce::CharPointer_UTF8("Escolher sample WAV para a peça")),
        juce::File(), "*.wav;*.wave");
    chooser->launchAsync(juce::FileBrowserComponent::openMode |
                             juce::FileBrowserComponent::canSelectFiles,
                         [this, note, chooser](const juce::FileChooser& fc) {
                             const auto file = fc.getResult();
                             if (file == juce::File()) return;
                             std::string err;
                             bool ok = false;
                             withAudioStopped([&] {
                                 ok = model_.uploadSample(
                                     note, file.getFullPathName().toStdString(), err);
                             });
                             if (!ok) {
                                 showError(
                                     juce::String(juce::CharPointer_UTF8(
                                         "Sample inválido: ")) +
                                     juce::String(err));
                                 return;
                             }
                             refreshKitUi();
                         });
}

void MainComponent::onKitNew(const std::string& name) {
    std::string err;
    withAudioStopped([&] {
        if (!model_.newKit(name, kitsDir_, err)) showError("Novo kit: " + juce::String(err));
    });
    refreshKitUi();
}

void MainComponent::onKitSave() {
    std::string err;
    if (!model_.saveKit(err)) {
        showError("Salvar kit: " + juce::String(err));
        return;
    }
    refreshKitUi();
}

void MainComponent::onKitLoad() {
    const std::string sel = kitBrowser_.selectedKit();
    if (sel.empty()) {
        showError("Escolha um kit na lista para carregar.");
        return;
    }
    std::string err;
    withAudioStopped([&] {
        if (!model_.loadKit(kitsDir_ + "/" + sel + "/kit.txt", err))
            showError("Carregar kit: " + juce::String(err));
    });
    refreshKitUi();
}

void MainComponent::onKitDuplicate(const std::string& name) {
    std::string err;
    if (!model_.duplicateKit(name, err)) {
        showError("Duplicar kit: " + juce::String(err));
        return;
    }
    refreshKitUi();
}

void MainComponent::showError(const juce::String& message) {
    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                           "Aroma Studio", message);
}

void MainComponent::timerCallback() {
    // Último hit: flash na peça + seleção automática (como módulo Roland).
    const LastHit hit = model_.lastHit();
    if (hit.note >= 0 && hit.tickMs != lastSeenHitTick_) {
        lastSeenHitTick_ = hit.tickMs;
        kitView_.hit(hit.note, hit.velocity);
        if (hit.note != model_.selectedNote()) selectInstrument(hit.note);
    }
    // Status do áudio.
    if (stream_.isOpen()) {
        statusBar_.setAudioInfo(
            juce::String(stream_.deviceName()) + "  |  " +
            juce::String(stream_.mode()) + "  |  " +
            juce::String(stream_.formatDescription()) + "  |  buffer " +
            juce::String(stream_.bufferFrames()) + "  |  lat. est. " +
            juce::String(stream_.estimatedLatencyMs(), 1) + " ms" +
            (stream_.rateMismatch()
                 ? juce::String(juce::CharPointer_UTF8(
                       "  |  ATENÇÃO: taxa fora de 48 kHz"))
                 : ""));
    } else {
        // Mostra o motivo real quando o áudio não abriu, em vez de só
        // "Áudio: fechado" (era o que escondia o erro de COM do JUCE).
        juce::String info = juce::String(juce::CharPointer_UTF8("Áudio: fechado"));
        if (!audioError_.empty())
            info += " — " + juce::String(audioError_);
        statusBar_.setAudioInfo(info);
    }
    statusBar_.setCounters(stream_.underruns(), stream_.callbacks(),
                           model_.engine().activeVoices(), model_.noteOnCount());
    statusBar_.setClipped(model_.engine().clipped());
    if (model_.engine().clipped()) model_.engine().clearClipFlag();
    // Dispositivo desplugado: avisa uma vez.
    if (stream_.deviceInvalidated() && !invalidatedShown_) {
        invalidatedShown_ = true;
        audioOn_ = false;
        statusBar_.setAudioRunning(false);
        showError(juce::String(juce::CharPointer_UTF8(
            "Dispositivo de áudio desplugado ou inválido — reconecte e clique em Reiniciar áudio.")));
    }
    // Cabeçalho sujo (edição sem salvar).
    headerLabel_.setText(
        juce::String(juce::CharPointer_UTF8("AROMA STUDIO  —  ")) +
            juce::String(model_.kitName()) + (model_.isDirty() ? "  *" : ""),
        juce::dontSendNotification);
    // MIDI: se a bateria foi ligada DEPOIS do app abrir, a enumeração
    // inicial não a viu ("nenhum dispositivo encontrado") e não havia
    // nova tentativa. Tenta de novo a cada ~2 s até conectar.
    if (!midi_.isOpen() && ++midiRetryTick_ >= 20) {
        midiRetryTick_ = 0;
        openMidi();
    }
}

void MainComponent::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff121216));
}

void MainComponent::resized() {
    auto area = getLocalBounds();
    headerLabel_.setBounds(area.removeFromTop(44).reduced(12, 8));
    statusBar_.setBounds(area.removeFromBottom(56));
    auto right = area.removeFromRight(360);
    kitBrowser_.setBounds(right.removeFromTop(176));
    instrumentList_.setBounds(right.removeFromTop(210));
    panelViewport_.setBounds(right);
    kitView_.setBounds(area);
    // Botões da lista de peças: 2 colunas.
    const auto listArea = instrumentList_.getLocalBounds().reduced(8);
    const int bw = (listArea.getWidth() - 8) / 2;
    const int bh = 26;
    int x = listArea.getX(), y = listArea.getY(), col = 0;
    for (auto& b : instrumentButtons_) {
        b->setBounds(x + col * (bw + 8), y, bw, bh);
        if (++col == 2) { col = 0; y += bh + 6; }
    }
}

} // namespace aroma

// Testes do AppModel (Fase B): kits, seleção, parâmetros ao vivo,
// upload de sample, novo/duplicar kit — sem JUCE, sem Windows.
#include <cstdio>
#include <cmath>
#include <filesystem>
#include <fstream>
#include "../src/app-juce/AppModel.h"

namespace {

int failures = 0;
void check(bool ok, const char* name) {
    std::printf("  [%s] %s\n", ok ? "OK" : "FALHOU", name);
    if (!ok) ++failures;
}

bool near(float a, float b, float eps = 0.001f) {
    return std::fabs(a - b) < eps;
}

std::filesystem::path tempKitsDir() {
    return std::filesystem::temp_directory_path() / "aroma_appmodel_test";
}

} // namespace

int testAppModel() {
    std::printf("test_appmodel:\n");
    std::string err;

    // Carrega o kit demo de verdade.
    aroma::AppModel model;
    check(model.loadKit("kits/demo-kit/kit.txt", err), "loadKit demo");
    if (!model.hasKit()) { std::printf("  sem kit, abortando resto\n"); return failures + 1; }
    check(model.kit().instruments.size() == 12, "demo tem 12 instrumentos");
    check(model.selectedNote() == 36, "selecao inicial = bumbo (36)");

    // Seleção e parâmetros ao vivo.
    check(model.selectInstrument(38), "select snare (38)");
    check(!model.selectInstrument(99), "select nota sem peca falha");
    check(model.selectedNote() == 38, "selecao continua 38 apos falha");
    aroma::InstrumentParams p;
    check(model.selectedParams(p), "le parametros da snare");
    p.volume = 0.5f; p.tune = 2.0f; p.eqLowDb = 3.0f; p.eqMidFreq = 800.0f;
    check(model.updateSelectedParams(p), "update params da snare");
    aroma::InstrumentParams back;
    check(model.selectedParams(back) && near(back.volume, 0.5f) && near(back.tune, 2.0f) &&
          near(back.eqLowDb, 3.0f) && near(back.eqMidFreq, 800.0f),
          "params lidos de volta (volume/tune/EQ)");
    check(model.isDirty(), "editar marca o kit como sujo");

    // Upload de sample: usa o snare.wav do demo como "novo" sample do bumbo.
    check(model.uploadSample(36, "kits/demo-kit/samples/snare.wav", err),
          "uploadSample aceita WAV valido");
    const auto* kick = model.kit().findByNote(36);
    check(kick != nullptr && kick->file == "snare.wav", "sample do bumbo trocado");
    check(!model.uploadSample(36, "kits/demo-kit/kit.txt", err),
          "uploadSample rejeita arquivo nao-WAV");

    // noteOn registra o hit para a UI.
    model.noteOn(38, 90, 12345);
    const aroma::LastHit hit = model.lastHit();
    check(hit.note == 38 && hit.velocity == 90 && hit.tickMs == 12345,
          "noteOn registra ultimo hit");
    check(model.noteOnCount() == 1, "contador de Note Ons");

    // Kits: prepara uma pasta temporária com cópia do demo.
    const auto tmp = tempKitsDir();
    std::error_code ec;
    std::filesystem::remove_all(tmp, ec);
    std::filesystem::create_directories(tmp / "demo-kit", ec);
    std::filesystem::copy("kits/demo-kit", tmp / "demo-kit",
                          std::filesystem::copy_options::recursive, ec);
    check(!ec, "copia do demo para pasta temporaria");
    check(aroma::AppModel::listKits(tmp.string()).size() == 1, "listKits acha o demo");
    check(aroma::AppModel::listKits("/caminho/que/nao/existe").empty(),
          "listKits em pasta inexistente = vazio");

    // Novo kit (copia do demo) + duplicar + salvar/carregar.
    aroma::AppModel m2;
    check(m2.newKit("Meu Kit", tmp.string(), err), "newKit cria a partir do demo");
    check(m2.kitName() == "Meu Kit", "novo kit tem o nome pedido");
    check(m2.kit().instruments.size() == 12, "novo kit tem 12 pecas");
    check(!m2.newKit("Meu Kit", tmp.string(), err), "newKit com nome repetido falha");
    check(m2.duplicateKit("Copia do Meu Kit", err), "duplicateKit");
    const auto names = aroma::AppModel::listKits(tmp.string());
    check(names.size() == 3, "listKits acha demo + novo + copia");
    check(!m2.isDirty(), "kit salvo nao esta sujo");

    // Round-trip: edita, salva, recarrega, confere.
    aroma::InstrumentParams q;
    check(m2.selectInstrument(38) && m2.selectedParams(q), "seleciona snare do novo kit");
    q.volume = 0.77f;
    check(m2.updateSelectedParams(q), "edita volume no novo kit");
    check(m2.saveKit(err), "saveKit no caminho atual");
    aroma::AppModel m3;
    check(m3.loadKit((tmp / "Meu Kit" / "kit.txt").string(), err), "recarrega o kit salvo");
    aroma::InstrumentParams r;
    check(m3.selectInstrument(38) && m3.selectedParams(r) && near(r.volume, 0.77f),
          "volume editado persistiu no arquivo");

    std::filesystem::remove_all(tmp, ec);
    return failures;
}

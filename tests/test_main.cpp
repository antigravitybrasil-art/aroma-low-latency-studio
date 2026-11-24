// Testes unitários do motor — sem Windows, sem áudio real.
// Roda no Linux (g++) e no Windows (MSVC): ./aroma_tests
#include <cstdio>
#include <cstdlib>

int testWav();
int testEngine();
int testPcmConvert();
int testKit();
int testAppModel();

int main() {
    int failures = 0;
    failures += testWav();
    failures += testEngine();
    failures += testPcmConvert();
    failures += testKit();
    failures += testAppModel();
    if (failures == 0) {
        std::printf("\nTODOS OS TESTES PASSARAM\n");
        return 0;
    }
    std::printf("\n%d TESTE(S) FALHARAM\n", failures);
    return 1;
}

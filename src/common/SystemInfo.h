#pragma once
// Informações do sistema (Windows). Usado pelo diagnóstico, Etapa 1.
#include <cstdint>
#include <string>

namespace aroma {

struct SystemInfo {
    std::string cpuBrand;      // ex.: "Intel(R) Core(TM) i7-12700H"
    int logicalCores = 0;
    int physicalCores = 0;
    uint64_t ramTotalMB = 0;
    uint64_t ramFreeMB = 0;
    std::string windowsVersion; // ex.: "Windows 11 Pro (10.0.22631)"
    std::string architecture;   // "x64"
    std::string computerName;
};

SystemInfo collectSystemInfo();
std::string currentTimestampForFilename(); // AAAAMMDD_HHMMSS
std::string currentTimestampHuman();       // AAAA-MM-DD HH:MM:SS

} // namespace aroma

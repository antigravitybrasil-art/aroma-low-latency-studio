#include "SystemInfo.h"
#include <windows.h>
#include <intrin.h>
#include <cstdio>
#include <cstring>
#include <vector>

namespace aroma {
namespace {

// RtlGetVersion evita o GetVersionEx "mentiroso" (que retorna 6.2 sem manifesto).
typedef LONG(WINAPI* RtlGetVersionFn)(PRTL_OSVERSIONINFOW);

std::string cpuBrandString() {
    int regs[4] = {0};
    __cpuid(regs, 0x80000000);
    if (static_cast<unsigned>(regs[0]) < 0x80000004u) return "desconhecida";
    char brand[49] = {0};
    for (int i = 0; i < 3; ++i) {
        __cpuid(regs, 0x80000002 + i);
        std::memcpy(brand + i * 16, regs, sizeof(regs));
    }
    // Remove espaços duplos / trailing.
    std::string s(brand);
    const size_t end = s.find_last_not_of(" \t\r\n");
    return end == std::string::npos ? s : s.substr(0, end + 1);
}

int countPhysicalCores() {
    DWORD len = 0;
    GetLogicalProcessorInformation(nullptr, &len);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || len == 0) return 0;
    std::vector<char> buf(len);
    auto* info = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION>(buf.data());
    if (!GetLogicalProcessorInformation(info, &len)) return 0;
    int cores = 0;
    const DWORD count = len / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
    for (DWORD i = 0; i < count; ++i)
        if (info[i].Relationship == RelationProcessorCore) ++cores;
    return cores;
}

} // namespace

SystemInfo collectSystemInfo() {
    SystemInfo si;
    si.cpuBrand = cpuBrandString();
    SYSTEM_INFO sysInfo{};
    GetSystemInfo(&sysInfo);
    si.logicalCores = static_cast<int>(sysInfo.dwNumberOfProcessors);
    si.physicalCores = countPhysicalCores();
    MEMORYSTATUSEX mem{};
    mem.dwLength = sizeof(mem);
    if (GlobalMemoryStatusEx(&mem)) {
        si.ramTotalMB = mem.ullTotalPhys / (1024 * 1024);
        si.ramFreeMB = mem.ullAvailPhys / (1024 * 1024);
    }
    // Versão do Windows via RtlGetVersion (ntdll).
    if (auto mod = GetModuleHandleA("ntdll.dll")) {
        auto fn = reinterpret_cast<RtlGetVersionFn>(
            GetProcAddress(mod, "RtlGetVersion"));
        if (fn) {
            RTL_OSVERSIONINFOW vi{};
            vi.dwOSVersionInfoSize = sizeof(vi);
            if (fn(&vi) == 0) {
                char buf[128];
                const char* edition = vi.dwMajorVersion >= 10 && vi.dwBuildNumber >= 22000
                    ? "Windows 11" : "Windows 10";
                std::snprintf(buf, sizeof(buf), "%s (%lu.%lu.%lu)",
                              edition, vi.dwMajorVersion, vi.dwMinorVersion,
                              vi.dwBuildNumber);
                si.windowsVersion = buf;
            }
        }
    }
    if (si.windowsVersion.empty()) si.windowsVersion = "Windows (versao nao detectada)";
    si.architecture = sizeof(void*) == 8 ? "x64" : "x86";
    char name[MAX_COMPUTERNAME_LENGTH + 1] = {0};
    DWORD nameLen = sizeof(name);
    if (GetComputerNameA(name, &nameLen)) si.computerName = name;
    return si;
}

std::string currentTimestampForFilename() {
    SYSTEMTIME st{};
    GetLocalTime(&st);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d%02d%02d_%02d%02d%02d",
                  st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return buf;
}

std::string currentTimestampHuman() {
    SYSTEMTIME st{};
    GetLocalTime(&st);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
                  st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return buf;
}

} // namespace aroma

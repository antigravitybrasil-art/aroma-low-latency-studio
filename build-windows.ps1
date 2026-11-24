# AROMA LOW LATENCY STUDIO — build Windows 11 x64 (Visual Studio 2022)
# Uso: .\build-windows.ps1
# Saida: builds\cmake\Release\AromaDiagnostico.exe
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw 'CMake nao encontrado no PATH. Instale o CMake marcando "Add CMake to the system PATH".'
}
cmake -S . -B builds\cmake -G "Visual Studio 17 2022" -A x64
cmake --build builds\cmake --config Release
Write-Host ''
Write-Host '[OK] Compilado: builds\cmake\Release\AromaDiagnostico.exe'

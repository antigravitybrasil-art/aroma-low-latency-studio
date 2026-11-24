@echo off
REM AROMA LOW LATENCY STUDIO — build Windows 11 x64 (Visual Studio 2022)
REM Uso: duplo clique, ou: build-windows.bat
REM Saida: builds\cmake\Release\AromaDiagnostico.exe
setlocal
cd /d "%~dp0"
where cmake >nul 2>nul
if errorlevel 1 (
  echo [ERRO] CMake nao encontrado no PATH.
  echo Instale o CMake marcando "Add CMake to the system PATH",
  echo ou rode pelo "x64 Native Tools Command Prompt for VS 2022".
  pause
  exit /b 1
)
cmake -S . -B builds\cmake -G "Visual Studio 17 2022" -A x64
if errorlevel 1 (
  echo [ERRO] Falha ao configurar o projeto. Veja a mensagem acima.
  pause
  exit /b 1
)
cmake --build builds\cmake --config Release
if errorlevel 1 (
  echo [ERRO] Falha na compilacao. Copie o erro e envie ao Muse.
  pause
  exit /b 1
)
echo.
echo [OK] Compilado: builds\cmake\Release\AromaDiagnostico.exe
pause

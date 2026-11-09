@echo off
setlocal enabledelayedexpansion

echo ==========================================================
echo  Building SpectraCross for Windows x64 (VST3)
echo ==========================================================

cd /d "%~dp0\.."

where cmake >nul 2>nul
if %errorlevel% neq 0 (
    echo Error: CMake is not installed or not in PATH.
    pause
    exit /b 1
)

if not exist "JUCE" if not exist "..\JUCE" (
    echo Cloning JUCE 8.0.6...
    git clone --depth 1 --branch 8.0.6 https://github.com/juce-framework/JUCE.git JUCE
)

echo Configuring CMake for Visual Studio 2022 x64...
cmake -B build_win -S . -G "Visual Studio 17 2022" -A x64
if %errorlevel% neq 0 (
    echo CMake configuration failed.
    pause
    exit /b 1
)

echo Compiling SpectraCross VST3 (Release)...
cmake --build build_win --config Release --target SpectraCross_VST3 --parallel
if %errorlevel% neq 0 (
    echo Compilation failed.
    pause
    exit /b 1
)

echo.
echo Build succeeded!
echo Binary located at: build_win\SpectraCross_artefacts\Release\VST3\SpectraCross.vst3
pause

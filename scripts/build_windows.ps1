# ==============================================================================
# SpectraCross - Native Windows Build Script (PowerShell)
# Compiles 64-bit VST3 plugin using Visual Studio 2022 (MSVC)
# ==============================================================================

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host " Building SpectraCross for Windows x64 (VST3)            " -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RootDir = Split-Path -Parent $ScriptDir
Set-Location $RootDir

# 1. Check for CMake
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Error "CMake is not found in PATH. Please install CMake from https://cmake.org/download/"
    exit 1
}

# 2. Check for JUCE
if (-not (Test-Path "JUCE") -and -not (Test-Path "..\JUCE")) {
    Write-Host "Cloning JUCE 8.0.6..." -ForegroundColor Yellow
    git clone --depth 1 --branch 8.0.6 https://github.com/juce-framework/JUCE.git JUCE
}

# 3. Configure CMake with Visual Studio 2022
Write-Host "Configuring CMake for Visual Studio 2022 x64..." -ForegroundColor Green
cmake -B build_win -S . -G "Visual Studio 17 2022" -A x64

# 4. Build VST3 Target
Write-Host "Compiling SpectraCross VST3 (Release)..." -ForegroundColor Green
cmake --build build_win --config Release --target SpectraCross_VST3 --parallel

Write-Host "`nBuild complete!" -ForegroundColor Cyan
$VST3Path = "build_win\SpectraCross_artefacts\Release\VST3\SpectraCross.vst3"

if (Test-Path $VST3Path) {
    Write-Host "Binary created successfully at: $VST3Path" -ForegroundColor Green
} else {
    Write-Warning "VST3 binary not found at expected path."
}

# 5. Optional Installation into system VST3 folder
$InstallDir = "C:\Program Files\Common Files\VST3"
$Prompt = Read-Host "Do you want to install SpectraCross into $InstallDir? (Y/N)"
if ($Prompt -match "^[Yy]$") {
    if (-not (Test-Path $InstallDir)) {
        New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null
    }
    Copy-Item -Path $VST3Path -Destination $InstallDir -Recurse -Force
    Write-Host "Successfully installed to $InstallDir\SpectraCross.vst3" -ForegroundColor Green
}

Write-Host "Done!" -ForegroundColor Cyan

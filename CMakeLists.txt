cmake_minimum_required(VERSION 3.22)
project(SpectraCross VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# macOS specific deployment settings
if(APPLE)
    if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version" FORCE)
    endif()
    # Default to Universal Binary (Intel x86_64 + Apple Silicon arm64)
    if(NOT CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Target architectures" FORCE)
    endif()
endif()

# Check for local JUCE install or fetch via FetchContent
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE/CMakeLists.txt")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../JUCE" JUCE)
elseif(EXISTS "/home/antigravity/JUCE/CMakeLists.txt")
    add_subdirectory("/home/antigravity/JUCE" JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.6
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(JUCE)
endif()

# Create JUCE Audio Plugin Target (supporting VST3 and AU)
juce_add_plugin(SpectraCross
    COMPANY_NAME "AcroSonic DSP"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Acro
    PLUGIN_CODE SpCr
    FORMATS VST3 AU
    PRODUCT_NAME "SpectraCross"
    BUNDLE_ID "com.acrosonic.spectracross"
    PLUGIN_DESCRIPTION "Surgical Multi-Instance Spectral Analyzer & Masking Engine"
    VST3_CATEGORIES "Fx" "Analyzer"
    AU_MAIN_TYPE "'aufx'"
    AU_EXPORT_PREFIX SpectraCrossAU
)

target_sources(SpectraCross PRIVATE
    Source/Common/SpectraCommon.h
    Source/Common/SpectralData.h
    Source/DSP/LockFreeFIFO.h
    Source/DSP/MaskingAnalyzer.h
    Source/DSP/MaskingAnalyzer.cpp
    Source/DSP/SpectrumProcessor.h
    Source/DSP/SpectrumProcessor.cpp
    Source/Hub/InstanceHub.h
    Source/Hub/InstanceHub.cpp
    Source/Host/HostMetadataFetcher.h
    Source/Host/HostMetadataFetcher.cpp
    Source/UI/ColorPickerComponent.h
    Source/UI/ColorPickerComponent.cpp
    Source/UI/PianoRollComponent.h
    Source/UI/InstanceListOverlay.h
    Source/UI/InstanceListOverlay.cpp
    Source/UI/SpectrumComponent.h
    Source/UI/SpectrumComponent.cpp
    Source/UI/TopBarComponent.h
    Source/UI/TopBarComponent.cpp
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/PluginEditor.h
    Source/PluginEditor.cpp
)

target_include_directories(SpectraCross PRIVATE
    Source
    Source/Common
    Source/DSP
    Source/Hub
    Source/Host
    Source/UI
)

target_compile_definitions(SpectraCross PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    $<$<PLATFORM_ID:Linux>:JUCE_ALSA=0>
    $<$<PLATFORM_ID:Linux>:JUCE_JACK=0>
)

target_link_libraries(SpectraCross PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# Explicit SIMD and optimization flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(SpectraCross PRIVATE
        -O3
        -fvisibility=hidden
        -fvisibility-inlines-hidden
        $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<NOT:$<PLATFORM_ID:Darwin>>>:-msse4.2>
    )
elseif(MSVC)
    target_compile_options(SpectraCross PRIVATE
        /O2
        /arch:AVX2
    )
    # Statically link runtime on Windows to avoid missing MSVCP DLL dependencies
    set_property(TARGET SpectraCross PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Test runner executable
add_executable(SpectraCross_Tests
    Tests/DSPTests.cpp
    Source/DSP/MaskingAnalyzer.cpp
    Source/Hub/InstanceHub.cpp
)

target_include_directories(SpectraCross_Tests PRIVATE
    Source
    Source/Common
    Source/DSP
    Source/Hub
)

target_compile_definitions(SpectraCross_Tests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    $<$<PLATFORM_ID:Linux>:JUCE_ALSA=0>
    $<$<PLATFORM_ID:Linux>:JUCE_JACK=0>
)

target_link_libraries(SpectraCross_Tests PRIVATE
    juce::juce_core
    juce::juce_graphics
    juce::juce_audio_basics
    juce::juce_dsp
)

#pragma once

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <array>
#include <cmath>
#include <string>
#include <string_view>

namespace spectracross
{

constexpr int kNumDisplayBins = 512;
constexpr int kNumBarkBands = 24;

constexpr float kMinFreqHz = 10.0f;
constexpr float kMaxFreqHz = 30000.0f;
constexpr float kLogRatioTotal = 3.4771212547196624f; // log10(30000.0f / 10.0f) = log10(3000.0f)

enum class SummationMode : int
{
    Mono = 0,
    Stereo,
    Mid,
    Side,
    MaxPeak
};

enum class FFTSize : int
{
    Size2048 = 2048,
    Size4096 = 4096,
    Size8192 = 8192
};

enum class WindowType : int
{
    BlackmanHarris7 = 0,
    Hann
};

enum class TiltSlope : int
{
    Flat = 0,    // 0 dB/oct
    Slope3dB,    // +3.0 dB/oct (pink noise balance)
    Slope4_5dB,  // +4.5 dB/oct (psychoacoustic pinking slope, e.g. Pro-Q style)
    Slope6dB     // +6.0 dB/oct (white noise slope)
};

enum class DBRange : int
{
    Range48dB = 0, // -36 to +12 dB
    Range72dB,     // -60 to +12 dB
    Range96dB      // -90 to +6 dB
};

enum class HoldTime : int
{
    Off = 0,
    Hold1s,
    Hold2s,
    Hold5s,
    Infinite
};

struct DBRangeBounds
{
    float minDb;
    float maxDb;
};

inline DBRangeBounds getDBRangeBounds(DBRange range) noexcept
{
    switch (range)
    {
        case DBRange::Range48dB: return { -36.0f, +12.0f };
        case DBRange::Range72dB: return { -60.0f, +12.0f };
        case DBRange::Range96dB: return { -90.0f, +6.0f };
    }
    return { -60.0f, +12.0f };
}

inline float getTiltDbPerOctave(TiltSlope slope) noexcept
{
    switch (slope)
    {
        case TiltSlope::Flat:       return 0.0f;
        case TiltSlope::Slope3dB:   return 3.0f;
        case TiltSlope::Slope4_5dB: return 4.5f;
        case TiltSlope::Slope6dB:   return 6.0f;
    }
    return 0.0f;
}

// Frequency to X mapping (exact logarithmic scale 10 Hz to 30 kHz)
inline float freqToLogX(float freqHz, float width) noexcept
{
    if (width <= 0.0f) return 0.0f;
    const float clampedFreq = juce::jlimit(kMinFreqHz, kMaxFreqHz, freqHz);
    const float logVal = std::log10(clampedFreq / kMinFreqHz);
    return width * (logVal / kLogRatioTotal);
}

// X to Frequency inverse mapping
inline float logXToFreq(float x, float width) noexcept
{
    if (width <= 0.0f) return kMinFreqHz;
    const float norm = juce::jlimit(0.0f, 1.0f, x / width);
    return kMinFreqHz * std::pow(10.0f, norm * kLogRatioTotal);
}

// dB to Y mapping
inline float dbToY(float db, float height, float minDb, float maxDb) noexcept
{
    if (height <= 0.0f || maxDb <= minDb) return 0.0f;
    const float norm = (maxDb - db) / (maxDb - minDb);
    return height * norm;
}

// Y to dB inverse mapping
inline float yToDb(float y, float height, float minDb, float maxDb) noexcept
{
    if (height <= 0.0f || maxDb <= minDb) return minDb;
    const float norm = juce::jlimit(0.0f, 1.0f, y / height);
    return maxDb - norm * (maxDb - minDb);
}

// Traunmüller frequency-to-Bark conversion
// z = 26.81 * (f / (1960 + f)) - 0.53
inline float freqToBark(float freqHz) noexcept
{
    if (freqHz <= 0.0f) return 0.0f;
    const float z = (26.81f * freqHz / (1960.0f + freqHz)) - 0.53f;
    return juce::jlimit(0.0f, 24.0f, z);
}

// Inverse Traunmüller Bark-to-frequency conversion
inline float barkToFreq(float bark) noexcept
{
    const float clampedZ = juce::jlimit(0.0f, 23.99f, bark);
    const float term = clampedZ + 0.53f;
    const float denom = 26.81f - term;
    if (denom <= 0.001f) return kMaxFreqHz;
    return (1960.0f * term) / denom;
}

// Frequency to MIDI note conversion: MIDI = 69 + 12 * log2(f / 440)
inline void freqToMidiInfo(float freqHz, int& outMidiNote, int& outCents, juce::String& outNoteName)
{
    if (freqHz <= 8.0f)
    {
        outMidiNote = 0;
        outCents = 0;
        outNoteName = "--";
        return;
    }

    const double semitonesExact = 69.0 + 12.0 * (std::log(freqHz / 440.0) / std::log(2.0));
    outMidiNote = static_cast<int>(std::round(semitonesExact));
    outCents = static_cast<int>(std::round((semitonesExact - static_cast<double>(outMidiNote)) * 100.0));

    static const char* const kNoteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int noteIndex = (outMidiNote % 12 + 12) % 12;
    const int octave = (outMidiNote / 12) - 1;

    outNoteName = juce::String(kNoteNames[noteIndex]) + juce::String(octave);
}

// Problematic Frequency Band Definitions
struct ProblemBand
{
    float lowFreq;
    float highFreq;
    const char* id;
    const char* name;
    const char* issueDescription;
    uint32_t colorArgb;
};

inline const std::array<ProblemBand, 7>& getProblemBands() noexcept
{
    static const std::array<ProblemBand, 7> bands {{
        { 10.0f,   30.0f,   "sub",       "Sub / Infrasound", "DC rumble, power waste, sub-bass mud",       0x40E05030 },
        { 30.0f,   100.0f,  "bottom",    "Bottom / Low End", "Kick fundamental, sub-bass masking",        0x40E08020 },
        { 200.0f,  400.0f,  "mud",       "Warmth / Mud",     "Boxiness, acoustic build-up, clutter",      0x40D4A020 },
        { 500.0f,  1000.0f, "honk",      "Honk / Boxiness",  "Cardboard snare, hollow/nasal midrange",    0x4040A040 },
        { 2500.0f, 4500.0f, "harshness", "Harshness / Bite", "Ear canal resonance (3kHz-4kHz), fatigue", 0x402090C0 },
        { 5000.0f, 8500.0f, "sibilance", "Sibilance",        "De-ess target, vocal splash ('s', 't')",    0x403060E0 },
        { 10000.0f,20000.0f,"air",       "Hiss / Air",       "Tape/mic hiss, top-end sparkle & air",      0x409040D0 }
    }};
    return bands;
}

// Standard modern DAW palette for manual assignment
inline const std::array<uint32_t, 12>& getPresetTrackColors() noexcept
{
    static const std::array<uint32_t, 12> colors {{
        0xFF3A86FF, // Electric Blue (Pro Tools / Studio One)
        0xFFFF006E, // Vivid Magenta (Ableton / Bitwig)
        0xFF8338EC, // Deep Violet (Cubase / FL Studio)
        0xFFFFBE0B, // Solar Amber (Logic / Ableton)
        0xFFFB5607, // Flame Orange (FL Studio / Reaper)
        0xFF06D6A0, // Emerald Cyan (Ableton / Logic)
        0xFF118AB2, // Ocean Teal
        0xFF073B4C, // Midnight Navy
        0xFFEF476F, // Coral Pink
        0xFF70E000, // Lime Green
        0xFF9D4EDD, // Orchid Purple
        0xFFE0E1DD  // Platinum White
    }};
    return colors;
}

} // namespace spectracross

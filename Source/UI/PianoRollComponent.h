#pragma once

#include "../Common/SpectraCommon.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <cmath>

namespace spectracross
{

class PianoRollComponent : public juce::Component
{
public:
    PianoRollComponent()
    {
        setOpaque(true);
        clashIntensityBins.fill(0.0f);
    }
    ~PianoRollComponent() override = default;

    void setHoveredMidiNote(int midiNote)
    {
        if (hoveredNote != midiNote)
        {
            hoveredNote = midiNote;
            repaint();
        }
    }

    void setClashIntensityProfile(const std::array<float, kNumDisplayBins>& profile)
    {
        clashIntensityBins = profile;
        repaint();
    }

    static juce::Colour getClashSeverityColour(float severity) noexcept
    {
        const float s = juce::jlimit(0.0f, 1.0f, severity);
        const juce::Colour bgCol(0xFF11151E);
        const juce::Colour lowCol(0xFF06D6A0);      // Lieve: Emerald Green / Teal
        const juce::Colour medCol(0xFFFFD166);      // Moderato: Warm Yellow / Amber
        const juce::Colour highCol(0xFFFF6B00);     // Alto: Deep Orange
        const juce::Colour severeCol(0xFFFF1E42);   // Grave: Crimson Alert Red

        if (s <= 0.02f)
            return bgCol;
        if (s < 0.25f)
            return bgCol.interpolatedWith(lowCol, (s - 0.02f) / 0.23f);
        if (s < 0.50f)
            return lowCol.interpolatedWith(medCol, (s - 0.25f) / 0.25f);
        if (s < 0.75f)
            return medCol.interpolatedWith(highCol, (s - 0.50f) / 0.25f);

        return highCol.interpolatedWith(severeCol, (s - 0.75f) / 0.25f);
    }

    void paint(juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        const float w = bounds.getWidth();
        const float h = bounds.getHeight();

        g.fillAll(juce::Colour(0xFF10131B));

        // Top row above the keyboard: Energy Clash Heatmap Strip
        const float clashRowHeight = std::min(18.0f, h * 0.43f);
        const float keysTop = clashRowHeight;
        const float keysHeight = std::max(10.0f, h - clashRowHeight);

        drawEnergyClashRow(g, juce::Rectangle<float>(0.0f, 0.0f, w, clashRowHeight));

        // MIDI note range: C0 (note 12, ~16.35 Hz) to B8 (note 119, ~7902 Hz)
        constexpr int minNote = 12;  // C0
        constexpr int maxNote = 119; // B8

        static const bool isBlackKey[12] = {
            false, true, false, true, false, false, true, false, true, false, true, false
        };

        // Draw White Keys first
        for (int note = minNote; note <= maxNote; ++note)
        {
            const int noteMod = (note % 12 + 12) % 12;
            if (isBlackKey[noteMod])
                continue;

            // Semitone edges in frequency
            const float freqLow = 440.0f * std::pow(2.0f, (static_cast<float>(note) - 0.5f - 69.0f) / 12.0f);
            const float freqHigh = 440.0f * std::pow(2.0f, (static_cast<float>(note) + 0.5f - 69.0f) / 12.0f);

            const float x1 = freqToLogX(freqLow, w);
            const float x2 = freqToLogX(freqHigh, w);
            const float keyWidth = std::max(1.0f, x2 - x1);

            juce::Colour keyCol = (note == hoveredNote) ? juce::Colour(0xFF4A6572) : juce::Colour(0xFFD6DBDF);
            g.setColour(keyCol);
            g.fillRect(x1, keysTop, keyWidth, keysHeight);

            g.setColour(juce::Colour(0xFF1E232F));
            g.drawVerticalLine(static_cast<int>(x2), keysTop, h);

            // Draw Octave label on C notes
            if (noteMod == 0)
            {
                const int octave = (note / 12) - 1;
                g.setColour(juce::Colour(0xFF1B2631));
                g.setFont(juce::FontOptions(9.0f).withStyle("Bold"));
                g.drawText("C" + juce::String(octave),
                           juce::Rectangle<float>(x1, h - 11.0f, keyWidth, 10.0f),
                           juce::Justification::centred);
            }
        }

        // Draw Black Keys on top
        const float blackKeyHeight = keysHeight * 0.65f;
        for (int note = minNote; note <= maxNote; ++note)
        {
            const int noteMod = (note % 12 + 12) % 12;
            if (!isBlackKey[noteMod])
                continue;

            const float freqLow = 440.0f * std::pow(2.0f, (static_cast<float>(note) - 0.45f - 69.0f) / 12.0f);
            const float freqHigh = 440.0f * std::pow(2.0f, (static_cast<float>(note) + 0.45f - 69.0f) / 12.0f);

            const float x1 = freqToLogX(freqLow, w);
            const float x2 = freqToLogX(freqHigh, w);
            const float keyWidth = std::max(1.0f, x2 - x1);

            juce::Colour keyCol = (note == hoveredNote) ? juce::Colour(0xFFFFBE0B) : juce::Colour(0xFF212529);
            g.setColour(keyCol);
            g.fillRect(x1, keysTop, keyWidth, blackKeyHeight);

            g.setColour(juce::Colour(0xFF10131B));
            g.drawRect(x1, keysTop, keyWidth, blackKeyHeight, 0.5f);
        }

        // Separator line between Energy Clash Row and Piano Keys
        g.setColour(juce::Colour(0xFF282E3D));
        g.drawHorizontalLine(static_cast<int>(keysTop), 0.0f, w);
        g.drawHorizontalLine(0, 0.0f, w);
    }

private:
    void drawEnergyClashRow(juce::Graphics& g, const juce::Rectangle<float>& area)
    {
        const float w = area.getWidth();
        const float h = area.getHeight();

        // Dark background for the clash strip
        g.setColour(juce::Colour(0xFF11151E));
        g.fillRect(area);

        // Paint each of the 512 logarithmic frequency bins
        const float binWidth = w / static_cast<float>(kNumDisplayBins - 1);
        for (int i = 0; i < kNumDisplayBins; ++i)
        {
            const float severity = clashIntensityBins[static_cast<size_t>(i)];
            if (severity <= 0.02f)
                continue;

            const float x = (static_cast<float>(i) / static_cast<float>(kNumDisplayBins - 1)) * w;
            const juce::Colour col = getClashSeverityColour(severity);

            // Fill vertical bar strip for this frequency bin
            g.setColour(col.withAlpha(0.88f));
            g.fillRect(x, area.getY() + 1.0f, std::max(1.5f, binWidth + 0.8f), h - 2.0f);

            // Bright top highlight proportional to clash severity
            if (severity > 0.20f)
            {
                g.setColour(col.brighter(0.35f));
                g.fillRect(x, area.getY() + 1.0f, std::max(1.5f, binWidth + 0.8f), 2.0f);
            }
        }

        // Subtle octave alignment ticks on the clash bar
        g.setColour(juce::Colour(0x26FFFFFF));
        for (int note = 12; note <= 119; note += 12)
        {
            const float freqLow = 440.0f * std::pow(2.0f, (static_cast<float>(note) - 0.5f - 69.0f) / 12.0f);
            const float x = freqToLogX(freqLow, w);
            g.drawVerticalLine(static_cast<int>(x), area.getY(), area.getBottom());
        }

        // Compact Legend Overlay Pill on the far-left (10-25 Hz region)
        const auto legendBox = juce::Rectangle<float>(6.0f, area.getY() + 2.0f, 238.0f, h - 4.0f);
        g.setColour(juce::Colour(0xD80E121A));
        g.fillRoundedRectangle(legendBox, 3.0f);
        g.setColour(juce::Colour(0xFF2B3245));
        g.drawRoundedRectangle(legendBox, 3.0f, 0.8f);

        g.setColour(juce::Colour(0xFFD0D7E2));
        g.setFont(juce::FontOptions(8.5f).withStyle("Bold"));
        g.drawText("ENERGY CLASH:", juce::Rectangle<float>(11.0f, area.getY() + 2.0f, 70.0f, h - 4.0f),
                   juce::Justification::centredLeft);

        struct LegendStop { const char* name; juce::uint32 col; float xOffset; };
        static const LegendStop stops[] = {
            { "Lieve",  0xFF06D6A0, 82.0f  },
            { "Medio",  0xFFFFD166, 121.0f },
            { "Alto",   0xFFFF6B00, 162.0f },
            { "Grave",  0xFFFF1E42, 198.0f }
        };

        g.setFont(juce::FontOptions(8.0f));
        for (const auto& st : stops)
        {
            g.setColour(juce::Colour(st.col));
            g.fillEllipse(st.xOffset, area.getCentreY() - 3.0f, 6.0f, 6.0f);
            g.setColour(juce::Colour(0xFFB8C2D1));
            g.drawText(st.name,
                       juce::Rectangle<float>(st.xOffset + 8.0f, area.getY() + 2.0f, 32.0f, h - 4.0f),
                       juce::Justification::centredLeft);
        }
    }

    int hoveredNote = -1;
    std::array<float, kNumDisplayBins> clashIntensityBins {};
};

} // namespace spectracross

#pragma once

#include "../Common/SpectraCommon.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace spectracross
{

class PianoRollComponent : public juce::Component
{
public:
    PianoRollComponent()
    {
        setOpaque(true);
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

    void paint(juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        const float w = bounds.getWidth();
        const float h = bounds.getHeight();

        g.fillAll(juce::Colour(0xFF10131B));

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
            g.fillRect(x1, 0.0f, keyWidth, h);

            g.setColour(juce::Colour(0xFF1E232F));
            g.drawVerticalLine(static_cast<int>(x2), 0.0f, h);

            // Draw Octave label on C notes
            if (noteMod == 0)
            {
                const int octave = (note / 12) - 1;
                g.setColour(juce::Colour(0xFF1B2631));
                g.setFont(juce::FontOptions(9.0f).withStyle("Bold"));
                g.drawText("C" + juce::String(octave), juce::Rectangle<float>(x1, h - 11.0f, keyWidth, 10.0f), juce::Justification::centred);
            }
        }

        // Draw Black Keys on top
        const float blackKeyHeight = h * 0.65f;
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
            g.fillRect(x1, 0.0f, keyWidth, blackKeyHeight);

            g.setColour(juce::Colour(0xFF10131B));
            g.drawRect(x1, 0.0f, keyWidth, blackKeyHeight, 0.5f);
        }

        // Top border line
        g.setColour(juce::Colour(0xFF282E3D));
        g.drawHorizontalLine(0, 0.0f, w);
    }

private:
    int hoveredNote = -1;
};

} // namespace spectracross

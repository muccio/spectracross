#pragma once

#include "../Common/SpectraCommon.h"
#include "../Common/SpectralData.h"
#include "../DSP/SpectrumProcessor.h"
#include "../Hub/InstanceHub.h"
#include "PianoRollComponent.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <vector>

namespace spectracross
{

class SpectrumComponent : public juce::Component, private juce::Timer
{
public:
    SpectrumComponent(SpectrumProcessor& processor, const juce::Uuid& localInstanceId);
    ~SpectrumComponent() override;

    void setDBRange(DBRange range);
    [[nodiscard]] DBRange getDBRange() const noexcept { return currentRange; }

    void setDisplayMode(DisplayMode mode);
    [[nodiscard]] DisplayMode getDisplayMode() const noexcept { return currentDisplayMode; }

    void setReferenceBandsVisible(bool visible);
    [[nodiscard]] bool areReferenceBandsVisible() const noexcept { return showReferenceBands; }

    void setPianoRollVisible(bool visible);
    [[nodiscard]] bool isPianoRollVisible() const noexcept { return showPianoRoll; }

    void setCollisionOverlayVisible(bool visible);
    [[nodiscard]] bool isCollisionOverlayVisible() const noexcept { return showCollisionOverlay; }

    void setLocalTrackColour(juce::Colour colour);

    // Callbacks
    std::function<void(int)> onHoveredMidiNoteChanged;
    std::function<void(const std::array<float, kNumDisplayBins>&)> onCollisionProfileUpdated;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    void timerCallback() override;

    void drawGridAndScales(juce::Graphics& g, const juce::Rectangle<float>& plotArea, float minDb, float maxDb);
    void drawReferenceZones(juce::Graphics& g, const juce::Rectangle<float>& plotArea);
    void drawCurves(juce::Graphics& g, const juce::Rectangle<float>& plotArea, float minDb, float maxDb);
    void drawSpectrogram2D(juce::Graphics& g, const juce::Rectangle<float>& plotArea, float minDb, float maxDb);
    void drawWaterfall3D(juce::Graphics& g, const juce::Rectangle<float>& plotArea, float minDb, float maxDb);
    void drawCollisionVisualizer(juce::Graphics& g, const juce::Rectangle<float>& plotArea, float minDb, float maxDb);
    void drawCrosshairAndReadout(juce::Graphics& g, const juce::Rectangle<float>& plotArea, float minDb, float maxDb);

    void pushHistorySlice(const std::array<float, kNumDisplayBins>& magsDb,
                          const std::array<float, kNumDisplayBins>& collision);
    void rebuildSpectrogramImage();

    SpectrumProcessor& processor;
    juce::Uuid localId;
    juce::Colour localColour { 0xFF3A86FF };

    DBRange currentRange = DBRange::Range72dB;
    DisplayMode currentDisplayMode = DisplayMode::SpectrumRTA;
    bool showReferenceBands = true;
    bool showPianoRoll = true;
    bool showCollisionOverlay = true;

    // Temporal history buffer for 2D Spectrogram & 3D Waterfall (~3 seconds at 60 FPS)
    static constexpr int kHistoryDepth = 180;
    struct HistorySlice
    {
        std::array<float, kNumDisplayBins> magnitudesDb {};
        std::array<float, kNumDisplayBins> collision {};
    };
    std::array<HistorySlice, kHistoryDepth> historyRing {};
    int historyHead = 0;
    int historyCount = 0;
    juce::Image spectrogramImage;

    // Mouse crosshair state
    bool isMouseOverPlot = false;
    juce::Point<float> mousePos;
    float hoveredFreq = 1000.0f;
    float hoveredDb = -18.0f;
    int hoveredMidiNote = 69;
    int hoveredCents = 0;
    juce::String hoveredNoteName = "A4";

    // Reusable paths for 60 FPS vector painting
    juce::Path localCurvePath;
    juce::Path localFillPath;
    juce::Path localPeakPath;
    juce::Path collisionAreaPath;

    // Collision metrics
    std::array<float, kNumDisplayBins> localCollisionIntensity {};
    float maxCurrentCollision = 0.0f;

    // Animation phase for collision pulse
    float pulsePhase = 0.0f;
};

} // namespace spectracross

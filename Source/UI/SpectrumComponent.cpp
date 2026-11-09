#include "SpectrumComponent.h"
#include <iomanip>
#include <sstream>

namespace spectracross
{

SpectrumComponent::SpectrumComponent(SpectrumProcessor& proc, const juce::Uuid& localInstanceId)
    : processor(proc), localId(localInstanceId)
{
    setOpaque(true);
    startTimerHz(60); // 60 FPS ultra-smooth vector rendering
}

SpectrumComponent::~SpectrumComponent()
{
    stopTimer();
}

void SpectrumComponent::setDBRange(DBRange range)
{
    currentRange = range;
    repaint();
}

void SpectrumComponent::setReferenceBandsVisible(bool visible)
{
    showReferenceBands = visible;
    repaint();
}

void SpectrumComponent::setPianoRollVisible(bool visible)
{
    showPianoRoll = visible;
    repaint();
}

void SpectrumComponent::setCollisionOverlayVisible(bool visible)
{
    showCollisionOverlay = visible;
    repaint();
}

void SpectrumComponent::setLocalTrackColour(juce::Colour colour)
{
    localColour = colour;
    repaint();
}

void SpectrumComponent::timerCallback()
{
    // Update collision pulse animation phase
    pulsePhase += 0.05f;
    if (pulsePhase > juce::MathConstants<float>::twoPi)
        pulsePhase -= juce::MathConstants<float>::twoPi;

    // Fetch latest local frame and publish to InstanceHub
    auto localFrame = processor.getLatestLocalFrame();
    if (localFrame)
    {
        // Compute collision against all active remote instances
        auto snapshot = InstanceHub::getInstance().getSnapshot();
        if (snapshot)
        {
            localCollisionIntensity.fill(0.0f);
            float totalMaxCollision = 0.0f;

            for (const auto& remote : *snapshot)
            {
                if (!remote || remote->instanceId == localId || remote->isBypassed || !remote->isVisibleInDisplay)
                    continue;

                std::array<float, kNumDisplayBins> remoteCollision512 {};
                float remoteCollisionPct = 0.0f;

                processor.getMaskingAnalyzer().computeInterTrackCollisions(
                    localFrame->barkEnergyDb,
                    remote->barkExcitationDb,
                    0.0f, // SMR threshold
                    remoteCollision512,
                    remoteCollisionPct
                );

                for (int i = 0; i < kNumDisplayBins; ++i)
                {
                    localCollisionIntensity[static_cast<size_t>(i)] = std::max(
                        localCollisionIntensity[static_cast<size_t>(i)],
                        remoteCollision512[static_cast<size_t>(i)]
                    );
                }

                totalMaxCollision = std::max(totalMaxCollision, remoteCollisionPct);
            }

            maxCurrentCollision = totalMaxCollision;

            // Update hub with latest frame
            InstanceSpectralFrame published = *localFrame;
            published.instanceId = localId;
            published.trackColour = localColour;
            published.collisionPercent = maxCurrentCollision;
            published.collisionMaskDb = localCollisionIntensity;
            InstanceHub::getInstance().publishSpectralFrame(localId, published);
        }
    }

    repaint();
}

void SpectrumComponent::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto boundsRange = getDBRangeBounds(currentRange);

    // Dark sleek background
    g.fillAll(juce::Colour(0xFF0D1017));

    // Plot area (reserve margin for dB scale and labels)
    auto plotArea = bounds;

    // Layer 1: Problematic Frequency Band Reference Zones (if enabled)
    if (showReferenceBands)
    {
        drawReferenceZones(g, plotArea);
    }

    // Layer 2: Frequency & dB Grid Lines + Scale Labels
    drawGridAndScales(g, plotArea, boundsRange.minDb, boundsRange.maxDb);

    // Layer 3: Inter-Track Masking Collision Overlay Shader
    if (showCollisionOverlay)
    {
        drawCollisionVisualizer(g, plotArea, boundsRange.minDb, boundsRange.maxDb);
    }

    // Layer 4: Multi-Track Curves & Local Track Path
    drawCurves(g, plotArea, boundsRange.minDb, boundsRange.maxDb);

    // Layer 5: Interactive Crosshair & Information Badge
    if (isMouseOverPlot)
    {
        drawCrosshairAndReadout(g, plotArea, boundsRange.minDb, boundsRange.maxDb);
    }
}

void SpectrumComponent::resized()
{
}

void SpectrumComponent::drawGridAndScales(juce::Graphics& g, const juce::Rectangle<float>& plotArea, float minDb, float maxDb)
{
    const float w = plotArea.getWidth();
    const float h = plotArea.getHeight();

    // Secondary subtle log ticks
    static const float secondaryFreqs[] = {
        30.0f, 40.0f, 60.0f, 70.0f, 80.0f, 90.0f,
        300.0f, 400.0f, 600.0f, 700.0f, 800.0f, 900.0f,
        3000.0f, 4000.0f, 6000.0f, 7000.0f, 8000.0f, 9000.0f
    };

    g.setColour(juce::Colour(0x18FFFFFF));
    for (float f : secondaryFreqs)
    {
        const float x = freqToLogX(f, w);
        g.drawVerticalLine(static_cast<int>(x), 0.0f, h);
    }

    // Primary log grid lines with numerical labels
    struct PrimaryFreq { float freq; const char* label; };
    static const PrimaryFreq primaryFreqs[] = {
        { 20.0f, "20" }, { 50.0f, "50" }, { 100.0f, "100" }, { 200.0f, "200" }, { 500.0f, "500" },
        { 1000.0f, "1k" }, { 2000.0f, "2k" }, { 5000.0f, "5k" }, { 10000.0f, "10k" }, { 20000.0f, "20k" }
    };

    for (const auto& pf : primaryFreqs)
    {
        const float x = freqToLogX(pf.freq, w);

        // Vertical line
        g.setColour(juce::Colour(0x28FFFFFF));
        g.drawVerticalLine(static_cast<int>(x), 0.0f, h);

        // Frequency Label at bottom
        g.setColour(juce::Colour(0x80FFFFFF));
        g.setFont(juce::FontOptions(10.0f).withStyle("Regular"));
        g.drawText(pf.label, x - 18.0f, h - 18.0f, 36.0f, 16.0f, juce::Justification::centred);
    }

    // Horizontal dB lines
    static const float dbLines[] = { 12.0f, 6.0f, 0.0f, -6.0f, -12.0f, -18.0f, -24.0f, -36.0f, -48.0f, -60.0f, -72.0f, -84.0f };

    for (float db : dbLines)
    {
        if (db < minDb || db > maxDb)
            continue;

        const float y = dbToY(db, h, minDb, maxDb);

        if (std::abs(db) < 0.01f)
        {
            // 0 dB reference line highlighted
            g.setColour(juce::Colour(0x60FFFFFF));
            g.drawHorizontalLine(static_cast<int>(y), 0.0f, w);
        }
        else
        {
            g.setColour(juce::Colour(0x18FFFFFF));
            g.drawHorizontalLine(static_cast<int>(y), 0.0f, w);
        }

        // dB Label along right edge
        g.setColour(juce::Colour(0x70FFFFFF));
        g.setFont(juce::FontOptions(9.0f));
        const juce::String dbText = (db > 0.0f ? "+" : "") + juce::String(static_cast<int>(db)) + " dB";
        g.drawText(dbText, w - 44.0f, y - 8.0f, 40.0f, 16.0f, juce::Justification::centredRight);
    }
}

void SpectrumComponent::drawReferenceZones(juce::Graphics& g, const juce::Rectangle<float>& plotArea)
{
    const float w = plotArea.getWidth();
    const float h = plotArea.getHeight();
    const auto& zones = getProblemBands();

    for (const auto& zone : zones)
    {
        const float x1 = freqToLogX(zone.lowFreq, w);
        const float x2 = freqToLogX(zone.highFreq, w);
        const float zoneWidth = std::max(2.0f, x2 - x1);

        // Fill vertical band with subtle color
        juce::Colour baseCol(zone.colorArgb);
        g.setColour(baseCol.withAlpha(0.06f));
        g.fillRect(x1, 0.0f, zoneWidth, h - 22.0f);

        // Bracket outline at top
        g.setColour(baseCol.withAlpha(0.25f));
        g.drawVerticalLine(static_cast<int>(x1), 0.0f, 16.0f);
        g.drawVerticalLine(static_cast<int>(x2), 0.0f, 16.0f);
        g.drawHorizontalLine(2, x1, x2);

        // Bracketed text label
        g.setFont(juce::FontOptions(9.0f).withStyle("Bold"));
        g.setColour(baseCol.withAlpha(0.7f));
        g.drawText(juce::String(zone.name).toUpperCase(),
                   x1 + 2.0f, 3.0f, zoneWidth - 4.0f, 14.0f,
                   juce::Justification::centred, true);
    }
}

void SpectrumComponent::drawCollisionVisualizer(juce::Graphics& g, const juce::Rectangle<float>& plotArea, float minDb, float maxDb)
{
    const float w = plotArea.getWidth();
    const float h = plotArea.getHeight();

    // Check if there is active collision intensity
    bool hasCollision = false;
    for (float val : localCollisionIntensity)
    {
        if (val > 0.05f) { hasCollision = true; break; }
    }

    if (!hasCollision)
        return;

    collisionAreaPath.clear();
    bool inCollisionRegion = false;

    // Pulsing glow alpha
    const float pulseAlpha = 0.25f + 0.15f * std::sin(pulsePhase * 2.0f);

    for (int i = 0; i < kNumDisplayBins; ++i)
    {
        const float intensity = localCollisionIntensity[static_cast<size_t>(i)];
        const float normX = static_cast<float>(i) / static_cast<float>(kNumDisplayBins - 1);
        const float x = normX * w;

        if (intensity > 0.05f)
        {
            // Height of collision shading corresponds to violation intensity
            const float colTopDb = juce::jlimit(minDb, maxDb, maxDb - (1.0f - intensity) * 36.0f);
            const float yTop = dbToY(colTopDb, h, minDb, maxDb);

            if (!inCollisionRegion)
            {
                collisionAreaPath.startNewSubPath(x, h);
                collisionAreaPath.lineTo(x, yTop);
                inCollisionRegion = true;
            }
            else
            {
                collisionAreaPath.lineTo(x, yTop);
            }
        }
        else if (inCollisionRegion)
        {
            collisionAreaPath.lineTo(x, h);
            collisionAreaPath.closeSubPath();
            inCollisionRegion = false;
        }
    }

    if (inCollisionRegion)
    {
        collisionAreaPath.lineTo(w, h);
        collisionAreaPath.closeSubPath();
    }

    if (!collisionAreaPath.isEmpty())
    {
        // Glowing alert red-orange gradient fill
        juce::ColourGradient grad(
            juce::Colour(0xFFFF3B30).withAlpha(pulseAlpha * 1.2f),
            0.0f, 0.0f,
            juce::Colour(0xFFFF9500).withAlpha(pulseAlpha * 0.3f),
            0.0f, h,
            false
        );
        g.setGradientFill(grad);
        g.fillPath(collisionAreaPath);

        // Top alert edge stroke
        g.setColour(juce::Colour(0xFFFF3B30).withAlpha(0.8f));
        g.strokePath(collisionAreaPath, juce::PathStrokeType(1.5f));
    }
}

void SpectrumComponent::drawCurves(juce::Graphics& g, const juce::Rectangle<float>& plotArea, float minDb, float maxDb)
{
    const float w = plotArea.getWidth();
    const float h = plotArea.getHeight();

    auto snapshot = InstanceHub::getInstance().getSnapshot();

    // 1. Draw Remote Tracks first (semi-transparent stroke curves)
    if (snapshot)
    {
        for (const auto& inst : *snapshot)
        {
            if (!inst || inst->instanceId == localId || inst->isBypassed || !inst->isVisibleInDisplay)
                continue;

            juce::Path remotePath;
            bool started = false;

            for (int i = 0; i < kNumDisplayBins; ++i)
            {
                const float db = inst->magnitudesDb[static_cast<size_t>(i)];
                const float normX = static_cast<float>(i) / static_cast<float>(kNumDisplayBins - 1);
                const float x = normX * w;
                const float y = juce::jlimit(0.0f, h, dbToY(db, h, minDb, maxDb));

                if (!started)
                {
                    remotePath.startNewSubPath(x, y);
                    started = true;
                }
                else
                {
                    remotePath.lineTo(x, y);
                }
            }

            g.setColour(inst->trackColour.withAlpha(0.7f));
            g.strokePath(remotePath, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    // 2. Draw Local Track (solid glowing path + vertical gradient fill)
    auto localFrame = processor.getLatestLocalFrame();
    if (!localFrame)
        return;

    localCurvePath.clear();
    localFillPath.clear();
    localPeakPath.clear();

    bool started = false;
    for (int i = 0; i < kNumDisplayBins; ++i)
    {
        const float db = localFrame->magnitudesDb[static_cast<size_t>(i)];
        const float normX = static_cast<float>(i) / static_cast<float>(kNumDisplayBins - 1);
        const float x = normX * w;
        const float y = juce::jlimit(0.0f, h, dbToY(db, h, minDb, maxDb));

        const float peakDb = localFrame->peakHoldDb[static_cast<size_t>(i)];
        const float peakY = juce::jlimit(0.0f, h, dbToY(peakDb, h, minDb, maxDb));

        if (!started)
        {
            localCurvePath.startNewSubPath(x, y);
            localPeakPath.startNewSubPath(x, peakY);
            started = true;
        }
        else
        {
            localCurvePath.lineTo(x, y);
            localPeakPath.lineTo(x, peakY);
        }
    }

    // Create fill path
    localFillPath = localCurvePath;
    localFillPath.lineTo(w, h);
    localFillPath.lineTo(0.0f, h);
    localFillPath.closeSubPath();

    // Soft vertical gradient fill to baseline
    juce::ColourGradient fillGrad(
        localColour.withAlpha(0.35f),
        0.0f, 0.0f,
        localColour.withAlpha(0.02f),
        0.0f, h,
        false
    );
    g.setGradientFill(fillGrad);
    g.fillPath(localFillPath);

    // Glowing main curve outline
    g.setColour(localColour.withAlpha(0.3f));
    g.strokePath(localCurvePath, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour(localColour);
    g.strokePath(localCurvePath, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Peak-Hold hairpin curve
    g.setColour(localColour.brighter(0.5f).withAlpha(0.85f));
    g.strokePath(localPeakPath, juce::PathStrokeType(1.0f));
}

void SpectrumComponent::drawCrosshairAndReadout(juce::Graphics& g, const juce::Rectangle<float>& plotArea, float minDb, float maxDb)
{
    juce::ignoreUnused(minDb, maxDb);
    const float w = plotArea.getWidth();
    const float h = plotArea.getHeight();

    // Crosshair lines
    g.setColour(juce::Colour(0x60FFFFFF));
    g.drawVerticalLine(static_cast<int>(mousePos.x), 0.0f, h);
    g.drawHorizontalLine(static_cast<int>(mousePos.y), 0.0f, w);

    // Readout Badge text formatting
    juce::String freqStr;
    if (hoveredFreq >= 1000.0f)
        freqStr = juce::String(hoveredFreq / 1000.0f, 2) + " kHz";
    else
        freqStr = juce::String(hoveredFreq, 1) + " Hz";

    juce::String centsStr = (hoveredCents >= 0 ? "+" : "") + juce::String(hoveredCents) + " ct";
    juce::String dbStr = (hoveredDb >= 0.0f ? "+" : "") + juce::String(hoveredDb, 1) + " dB";

    juce::String fullInfo = freqStr + "   " + hoveredNoteName + " (" + centsStr + ")   " + dbStr;

    // Draw floating pill badge
    const float badgeW = 210.0f;
    const float badgeH = 26.0f;
    float badgeX = mousePos.x + 12.0f;
    float badgeY = mousePos.y - 32.0f;

    if (badgeX + badgeW > w - 8.0f)
        badgeX = mousePos.x - badgeW - 12.0f;
    if (badgeY < 8.0f)
        badgeY = mousePos.y + 12.0f;

    auto badgeRect = juce::Rectangle<float>(badgeX, badgeY, badgeW, badgeH);

    g.setColour(juce::Colour(0xEE161A24));
    g.fillRoundedRectangle(badgeRect, 5.0f);

    g.setColour(localColour.withAlpha(0.8f));
    g.drawRoundedRectangle(badgeRect, 5.0f, 1.2f);

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(11.0f).withStyle("Bold"));
    g.drawText(fullInfo, badgeRect, juce::Justification::centred);
}

void SpectrumComponent::mouseMove(const juce::MouseEvent& e)
{
    isMouseOverPlot = true;
    mousePos = e.position;

    const float w = static_cast<float>(getWidth());
    const float h = static_cast<float>(getHeight());
    const auto boundsRange = getDBRangeBounds(currentRange);

    hoveredFreq = logXToFreq(mousePos.x, w);
    hoveredDb = yToDb(mousePos.y, h, boundsRange.minDb, boundsRange.maxDb);

    freqToMidiInfo(hoveredFreq, hoveredMidiNote, hoveredCents, hoveredNoteName);

    if (onHoveredMidiNoteChanged)
        onHoveredMidiNoteChanged(hoveredMidiNote);

    repaint();
}

void SpectrumComponent::mouseExit(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);
    isMouseOverPlot = false;
    if (onHoveredMidiNoteChanged)
        onHoveredMidiNoteChanged(-1);
    repaint();
}

void SpectrumComponent::mouseDown(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);
}

} // namespace spectracross

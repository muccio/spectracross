#include "SpectrumComponent.h"
#include <iomanip>
#include <sstream>

namespace spectracross
{

SpectrumComponent::SpectrumComponent(SpectrumProcessor& proc, const juce::Uuid& localInstanceId)
    : processor(proc),
      localId(localInstanceId),
      spectrogramImage(juce::Image::ARGB, kNumDisplayBins, kHistoryDepth, true)
{
    setOpaque(true);

    for (auto& slice : historyRing)
    {
        slice.magnitudesDb.fill(-120.0f);
        slice.collision.fill(0.0f);
    }
    spectrogramImage.clear(spectrogramImage.getBounds(), juce::Colour(0xFF0A0D14));

    startTimerHz(60); // 60 FPS ultra-smooth vector rendering
}

SpectrumComponent::~SpectrumComponent()
{
    stopTimer();
}

void SpectrumComponent::setDBRange(DBRange range)
{
    currentRange = range;
    rebuildSpectrogramImage();
    repaint();
}

void SpectrumComponent::setDisplayMode(DisplayMode mode)
{
    if (currentDisplayMode != mode)
    {
        currentDisplayMode = mode;
        repaint();
    }
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

    // Compute collision against all active remote instances for visualization
    auto localFrame = processor.getLatestLocalFrame();
    if (localFrame)
    {
        auto snapshot = InstanceHub::getInstance().getSnapshot();
        if (snapshot)
        {
            localCollisionIntensity.fill(0.0f);
            float totalMaxCollision = 0.0f;
            const float threshold = processor.getSmrThreshold();

            for (const auto& remote : *snapshot)
            {
                if (!remote || remote->instanceId == localId || remote->isBypassed || !remote->isVisibleInDisplay)
                    continue;

                std::array<float, kNumDisplayBins> remoteCollision512 {};
                float remoteCollisionPct = 0.0f;

                processor.getMaskingAnalyzer().computeInterTrackCollisions(
                    localFrame->barkEnergyDb,
                    remote->barkExcitationDb,
                    threshold,
                    remoteCollision512,
                    remoteCollisionPct
                );

                for (int i = 0; i < kNumDisplayBins; ++i)
                {
                    const size_t idx = static_cast<size_t>(i);
                    float binSeverity = remoteCollision512[idx];

                    // Combine Bark masking with fine 512-bin spectral energy overlap
                    const float localDb = localFrame->magnitudesDb[idx];
                    const float remoteDb = remote->magnitudesDb[idx];
                    if (localDb > -68.0f && remoteDb > -68.0f)
                    {
                        const float jointLevel = std::min(localDb, remoteDb);
                        const float prominence = juce::jlimit(0.0f, 1.0f, (jointLevel - (-68.0f)) / 44.0f);
                        const float maskRatio = juce::jlimit(0.0f, 1.25f, (remoteDb - localDb + 12.0f - threshold) / 22.0f);
                        const float fineOverlap = juce::jlimit(0.0f, 1.0f, prominence * maskRatio);
                        binSeverity = std::max(binSeverity, fineOverlap * 0.9f + binSeverity * 0.1f);
                    }

                    localCollisionIntensity[idx] = std::max(
                        localCollisionIntensity[idx],
                        binSeverity
                    );
                }

                totalMaxCollision = std::max(totalMaxCollision, remoteCollisionPct);
            }

            maxCurrentCollision = totalMaxCollision;

            pushHistorySlice(localFrame->magnitudesDb, localCollisionIntensity);

            if (onCollisionProfileUpdated)
                onCollisionProfileUpdated(localCollisionIntensity);
        }
    }

    repaint();
}

void SpectrumComponent::pushHistorySlice(const std::array<float, kNumDisplayBins>& magsDb,
                                         const std::array<float, kNumDisplayBins>& collision)
{
    historyRing[static_cast<size_t>(historyHead)].magnitudesDb = magsDb;
    historyRing[static_cast<size_t>(historyHead)].collision = collision;
    historyHead = (historyHead + 1) % kHistoryDepth;
    if (historyCount < kHistoryDepth)
        ++historyCount;

    // Scroll 2D spectrogram image up by 1 row (newest frame at the bottom right above Piano Roll)
    spectrogramImage.moveImageSection(0, 0, 0, 1, kNumDisplayBins, kHistoryDepth - 1);

    const auto boundsRange = getDBRangeBounds(currentRange);
    const float spanDb = std::max(1.0f, boundsRange.maxDb - boundsRange.minDb);
    const int bottomY = kHistoryDepth - 1;

    juce::Image::BitmapData bmp(spectrogramImage, juce::Image::BitmapData::writeOnly);
    for (int x = 0; x < kNumDisplayBins; ++x)
    {
        const float db = magsDb[static_cast<size_t>(x)];
        const float norm = juce::jlimit(0.0f, 1.0f, (db - boundsRange.minDb) / spanDb);
        juce::Colour col = mapNormalizedDbToSpectrogramColour(norm);

        // If collision overlay is enabled and there is severe clash at this frequency, add a subtle magenta-red rim
        const float clash = collision[static_cast<size_t>(x)];
        if (showCollisionOverlay && clash > 0.35f)
        {
            col = col.interpolatedWith(juce::Colour(0xFFFF1E56), (clash - 0.35f) * 0.45f);
        }

        bmp.setPixelColour(x, bottomY, col);
    }
}

void SpectrumComponent::rebuildSpectrogramImage()
{
    if (!spectrogramImage.isValid())
        return;

    const auto boundsRange = getDBRangeBounds(currentRange);
    const float spanDb = std::max(1.0f, boundsRange.maxDb - boundsRange.minDb);

    spectrogramImage.clear(spectrogramImage.getBounds(), juce::Colour(0xFF0A0D14));
    juce::Image::BitmapData bmp(spectrogramImage, juce::Image::BitmapData::writeOnly);

    for (int age = 0; age < historyCount; ++age)
    {
        const int ringIdx = (historyHead - 1 - age + kHistoryDepth) % kHistoryDepth;
        const int y = (kHistoryDepth - 1) - age;
        const auto& slice = historyRing[static_cast<size_t>(ringIdx)];

        for (int x = 0; x < kNumDisplayBins; ++x)
        {
            const float db = slice.magnitudesDb[static_cast<size_t>(x)];
            const float norm = juce::jlimit(0.0f, 1.0f, (db - boundsRange.minDb) / spanDb);
            juce::Colour col = mapNormalizedDbToSpectrogramColour(norm);

            const float clash = slice.collision[static_cast<size_t>(x)];
            if (showCollisionOverlay && clash > 0.35f)
            {
                col = col.interpolatedWith(juce::Colour(0xFFFF1E56), (clash - 0.35f) * 0.45f);
            }

            bmp.setPixelColour(x, y, col);
        }
    }
}

void SpectrumComponent::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto boundsRange = getDBRangeBounds(currentRange);

    // Dark sleek background
    g.fillAll(juce::Colour(0xFF0D1017));

    auto plotArea = bounds;

    if (currentDisplayMode == DisplayMode::Spectrogram2D)
    {
        drawSpectrogram2D(g, plotArea, boundsRange.minDb, boundsRange.maxDb);
    }
    else if (currentDisplayMode == DisplayMode::Waterfall3D)
    {
        if (showReferenceBands)
            drawReferenceZones(g, plotArea);
        drawGridAndScales(g, plotArea, boundsRange.minDb, boundsRange.maxDb);
        drawWaterfall3D(g, plotArea, boundsRange.minDb, boundsRange.maxDb);
    }
    else
    {
        // Default: Instantaneous Real-Time Frequency Spectrum (RTA)
        if (showReferenceBands)
            drawReferenceZones(g, plotArea);

        drawGridAndScales(g, plotArea, boundsRange.minDb, boundsRange.maxDb);

        if (showCollisionOverlay)
            drawCollisionVisualizer(g, plotArea, boundsRange.minDb, boundsRange.maxDb);

        drawCurves(g, plotArea, boundsRange.minDb, boundsRange.maxDb);
    }

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
        g.drawText(pf.label, juce::Rectangle<float>(x - 18.0f, h - 18.0f, 36.0f, 16.0f), juce::Justification::centred);
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
        g.drawText(dbText, juce::Rectangle<float>(w - 44.0f, y - 8.0f, 40.0f, 16.0f), juce::Justification::centredRight);
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
                   juce::Rectangle<float>(x1 + 2.0f, 3.0f, zoneWidth - 4.0f, 14.0f),
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

void SpectrumComponent::drawSpectrogram2D(juce::Graphics& g, const juce::Rectangle<float>& plotArea, float minDb, float maxDb)
{
    const float w = plotArea.getWidth();
    const float h = plotArea.getHeight();

    // 1. Render 2D Temporal Spectrogram Heatmap Image (512 log-freq bins x 180 time slices)
    if (spectrogramImage.isValid())
    {
        g.setImageResamplingQuality(juce::Graphics::mediumResamplingQuality);
        g.drawImage(spectrogramImage, plotArea);
    }

    // 2. Overlay Reference Zone Brackets at the top (if enabled)
    if (showReferenceBands)
    {
        const auto& zones = getProblemBands();
        for (const auto& zone : zones)
        {
            const float x1 = freqToLogX(zone.lowFreq, w);
            const float x2 = freqToLogX(zone.highFreq, w);
            const float zoneWidth = std::max(2.0f, x2 - x1);

            juce::Colour baseCol(zone.colorArgb);
            g.setColour(baseCol.withAlpha(0.35f));
            g.drawVerticalLine(static_cast<int>(x1), 0.0f, 16.0f);
            g.drawVerticalLine(static_cast<int>(x2), 0.0f, 16.0f);
            g.drawHorizontalLine(2, x1, x2);

            g.setFont(juce::FontOptions(9.0f).withStyle("Bold"));
            g.setColour(baseCol.brighter(0.2f).withAlpha(0.85f));
            g.drawText(juce::String(zone.name).toUpperCase(),
                       juce::Rectangle<float>(x1 + 2.0f, 3.0f, zoneWidth - 4.0f, 14.0f),
                       juce::Justification::centred, true);
        }
    }

    // 3. Vertical Logarithmic Frequency Grid Lines & Bottom Labels
    struct PrimaryFreq { float freq; const char* label; };
    static const PrimaryFreq primaryFreqs[] = {
        { 20.0f, "20" }, { 50.0f, "50" }, { 100.0f, "100" }, { 200.0f, "200" }, { 500.0f, "500" },
        { 1000.0f, "1k" }, { 2000.0f, "2k" }, { 5000.0f, "5k" }, { 10000.0f, "10k" }, { 20000.0f, "20k" }
    };

    for (const auto& pf : primaryFreqs)
    {
        const float x = freqToLogX(pf.freq, w);
        g.setColour(juce::Colour(0x30FFFFFF));
        g.drawVerticalLine(static_cast<int>(x), 0.0f, h);

        g.setColour(juce::Colour(0xB0FFFFFF));
        g.setFont(juce::FontOptions(10.0f).withStyle("Bold"));
        g.drawText(pf.label, juce::Rectangle<float>(x - 18.0f, h - 18.0f, 36.0f, 16.0f), juce::Justification::centred);
    }

    // 4. Horizontal Temporal Grid Lines (0.0s at bottom -> -3.0s at top)
    static const float timeMarksSec[] = { 0.0f, -0.5f, -1.0f, -1.5f, -2.0f, -2.5f };
    constexpr float maxHistorySec = 3.0f;

    for (float tSec : timeMarksSec)
    {
        const float normAge = std::abs(tSec) / maxHistorySec;
        const float y = h * (1.0f - normAge);

        g.setColour(juce::Colour(0x22FFFFFF));
        g.drawHorizontalLine(static_cast<int>(y), 0.0f, w);

        g.setColour(juce::Colour(0x90FFFFFF));
        g.setFont(juce::FontOptions(9.0f));
        const juce::String timeLabel = (tSec == 0.0f) ? "NOW (0.0s)" : (juce::String(tSec, 1) + " s");
        g.drawText(timeLabel, juce::Rectangle<float>(w - 66.0f, y - 15.0f, 60.0f, 14.0f), juce::Justification::centredRight);
    }

    // 5. Thermal Color Intensity Scale Legend Bar (Top-Left overlay pill)
    const auto legendRect = juce::Rectangle<float>(10.0f, 22.0f, 176.0f, 34.0f);
    g.setColour(juce::Colour(0xD810141E));
    g.fillRoundedRectangle(legendRect, 5.0f);
    g.setColour(juce::Colour(0xFF2B3348));
    g.drawRoundedRectangle(legendRect, 5.0f, 1.0f);

    g.setColour(juce::Colours::white.withAlpha(0.9f));
    g.setFont(juce::FontOptions(9.0f).withStyle("Bold"));
    g.drawText("SPECTROGRAM 2D INTENSITY",
               juce::Rectangle<float>(legendRect.getX() + 8.0f, legendRect.getY() + 3.0f, legendRect.getWidth() - 16.0f, 12.0f),
               juce::Justification::centredLeft);

    const float barX = legendRect.getX() + 38.0f;
    const float barY = legendRect.getY() + 18.0f;
    const float barW = 100.0f;
    const float barH = 9.0f;

    for (int i = 0; i < static_cast<int>(barW); ++i)
    {
        const float norm = static_cast<float>(i) / (barW - 1.0f);
        g.setColour(mapNormalizedDbToSpectrogramColour(norm));
        g.fillRect(barX + static_cast<float>(i), barY, 1.5f, barH);
    }
    g.setColour(juce::Colour(0x60FFFFFF));
    g.drawRect(barX, barY, barW, barH, 0.8f);

    g.setColour(juce::Colour(0xFFAAB4C8));
    g.setFont(juce::FontOptions(8.5f));
    g.drawText(juce::String(static_cast<int>(minDb)),
               juce::Rectangle<float>(legendRect.getX() + 6.0f, barY - 2.0f, 28.0f, 13.0f),
               juce::Justification::centredRight);
    g.drawText("+" + juce::String(static_cast<int>(maxDb)) + "dB",
               juce::Rectangle<float>(barX + barW + 3.0f, barY - 2.0f, 34.0f, 13.0f),
               juce::Justification::centredLeft);
}

void SpectrumComponent::drawWaterfall3D(juce::Graphics& g, const juce::Rectangle<float>& plotArea, float minDb, float maxDb)
{
    const float w = plotArea.getWidth();
    const float h = plotArea.getHeight();
    const float spanDb = std::max(1.0f, maxDb - minDb);

    constexpr int kNumWaterfallSlices = 36;
    constexpr int kSliceStride = 4; // Sample every 4th history frame (~2.4s total 3D depth)
    constexpr int kPointsPerSlice = 171; // Every 3rd bin for crisp 60 FPS 3D vector ridges

    const float maxDepthOffsetY = h * 0.44f;
    const float frontPlotHeight = h * 0.56f;

    // Render from oldest (back/top) to newest (front/bottom) using Painter's Occlusion Algorithm
    for (int s = kNumWaterfallSlices - 1; s >= 0; --s)
    {
        const int historyAge = s * kSliceStride;
        if (historyAge >= historyCount)
            continue;

        const int ringIdx = (historyHead - 1 - historyAge + kHistoryDepth) % kHistoryDepth;
        const auto& slice = historyRing[static_cast<size_t>(ringIdx)];

        const float depth = static_cast<float>(s) / static_cast<float>(kNumWaterfallSlices - 1); // 0 = front, 1 = back
        const float baselineY = h - depth * maxDepthOffsetY;
        const float sliceScaleH = frontPlotHeight * (1.0f - depth * 0.28f);

        juce::Path ridgePath;
        juce::Path occlusionFill;
        bool started = false;
        float peakNormInSlice = 0.0f;
        float maxClashInSlice = 0.0f;

        for (int p = 0; p < kPointsPerSlice; ++p)
        {
            const int binIdx = std::min(kNumDisplayBins - 1, p * 3);
            const float db = slice.magnitudesDb[static_cast<size_t>(binIdx)];
            const float normDb = juce::jlimit(0.0f, 1.0f, (db - minDb) / spanDb);
            peakNormInSlice = std::max(peakNormInSlice, normDb);
            maxClashInSlice = std::max(maxClashInSlice, slice.collision[static_cast<size_t>(binIdx)]);

            const float normX = static_cast<float>(binIdx) / static_cast<float>(kNumDisplayBins - 1);
            const float x = normX * w;
            const float y = baselineY - normDb * sliceScaleH;

            if (!started)
            {
                ridgePath.startNewSubPath(x, y);
                started = true;
            }
            else
            {
                ridgePath.lineTo(x, y);
            }
        }

        // Occlude older slices behind this ridge
        occlusionFill = ridgePath;
        occlusionFill.lineTo(w, baselineY + 2.0f);
        occlusionFill.lineTo(0.0f, baselineY + 2.0f);
        occlusionFill.closeSubPath();

        const float distanceFade = 1.0f - depth * 0.75f;
        const juce::Colour ridgeThermalCol = mapNormalizedDbToSpectrogramColour(peakNormInSlice);
        juce::Colour sliceStrokeCol = localColour.interpolatedWith(ridgeThermalCol, 0.55f).withAlpha(distanceFade);

        if (showCollisionOverlay && maxClashInSlice > 0.30f)
        {
            sliceStrokeCol = sliceStrokeCol.interpolatedWith(juce::Colour(0xFFFF2A4B), maxClashInSlice * 0.5f);
        }

        juce::ColourGradient sliceFillGrad(
            sliceStrokeCol.withAlpha(0.22f * distanceFade),
            0.0f, baselineY - sliceScaleH,
            juce::Colour(0xF20D1017),
            0.0f, baselineY,
            false
        );
        g.setGradientFill(sliceFillGrad);
        g.fillPath(occlusionFill);

        const float strokeWidth = (s == 0) ? 2.2f : (1.5f - depth * 0.7f);
        g.setColour(sliceStrokeCol);
        g.strokePath(ridgePath, juce::PathStrokeType(strokeWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Draw visible remote tracks on the front-most plane (depth = 0) for instant multi-track comparison
    auto snapshot = InstanceHub::getInstance().getSnapshot();
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
                const float normDb = juce::jlimit(0.0f, 1.0f, (db - minDb) / spanDb);
                const float normX = static_cast<float>(i) / static_cast<float>(kNumDisplayBins - 1);
                const float x = normX * w;
                const float y = h - normDb * frontPlotHeight;

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

            g.setColour(inst->trackColour.withAlpha(0.80f));
            g.strokePath(remotePath, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }
}

void SpectrumComponent::drawCrosshairAndReadout(juce::Graphics& g, const juce::Rectangle<float>& plotArea, float minDb, float maxDb)
{
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
    juce::String dbStr;

    if (currentDisplayMode == DisplayMode::Spectrogram2D && historyCount > 0)
    {
        const float normY = juce::jlimit(0.0f, 1.0f, mousePos.y / std::max(1.0f, h));
        const int age = juce::jlimit(0, kHistoryDepth - 1, static_cast<int>((1.0f - normY) * (kHistoryDepth - 1)));
        const int binIdx = juce::jlimit(0, kNumDisplayBins - 1,
                                        static_cast<int>((mousePos.x / std::max(1.0f, w)) * (kNumDisplayBins - 1)));
        float sampleDb = minDb;
        if (age < historyCount)
        {
            const int ringIdx = (historyHead - 1 - age + kHistoryDepth) % kHistoryDepth;
            sampleDb = historyRing[static_cast<size_t>(ringIdx)].magnitudesDb[static_cast<size_t>(binIdx)];
        }
        const float timeOffsetSec = -(static_cast<float>(age) / 60.0f);
        dbStr = juce::String(sampleDb, 1) + " dB @ " + juce::String(timeOffsetSec, 2) + "s";
    }
    else
    {
        juce::ignoreUnused(minDb, maxDb);
        dbStr = (hoveredDb >= 0.0f ? "+" : "") + juce::String(hoveredDb, 1) + " dB";
    }

    juce::String fullInfo = freqStr + "   " + hoveredNoteName + " (" + centsStr + ")   " + dbStr;

    // Draw floating pill badge
    const float badgeW = (currentDisplayMode == DisplayMode::Spectrogram2D) ? 245.0f : 210.0f;
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

#include "InstanceListOverlay.h"

namespace spectracross
{

InstanceRowComponent::InstanceRowComponent(const juce::Uuid& instanceId, bool isLocalInstance)
    : id(instanceId), isLocal(isLocalInstance)
{
    showToggle.setToggleState(true, juce::dontSendNotification);
    showToggle.onClick = [this]() {
        if (onVisibilityToggled) onVisibilityToggled(id, showToggle.getToggleState());
    };
    addAndMakeVisible(showToggle);

    soloButton.setClickingTogglesState(true);
    soloButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF232733));
    soloButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xFFFFBE0B));
    soloButton.setColour(juce::TextButton::textColourOnId, juce::Colours::black);
    soloButton.onClick = [this]() {
        if (onSoloToggled) onSoloToggled(id, soloButton.getToggleState());
    };
    addAndMakeVisible(soloButton);

    muteButton.setClickingTogglesState(true);
    muteButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF232733));
    muteButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xFFE63946));
    muteButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    muteButton.onClick = [this]() {
        if (onMuteToggled) onMuteToggled(id, muteButton.getToggleState());
    };
    addAndMakeVisible(muteButton);
}

void InstanceRowComponent::updateData(const InstanceSpectralFrame& frame, float collisionPercent)
{
    trackColour = frame.trackColour;
    trackName = frame.trackName;
    currentCollisionPercent = collisionPercent;
    showToggle.setToggleState(frame.isVisibleInDisplay, juce::dontSendNotification);
    soloButton.setToggleState(frame.isSoloed, juce::dontSendNotification);
    muteButton.setToggleState(frame.isMuted, juce::dontSendNotification);
    repaint();
}

void InstanceRowComponent::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    // Subtle row background
    if (isLocal)
        g.fillAll(juce::Colour(0xFF1B2232));
    else
        g.fillAll(juce::Colour(0xFF141822));

    g.setColour(juce::Colour(0xFF252A3A));
    g.drawHorizontalLine(getHeight() - 1, 0.0f, bounds.getWidth());

    // Interactive Color Swatch Button (Click to change instance color)
    const auto swatchArea = getSwatchBounds();
    g.setColour(trackColour);
    g.fillRoundedRectangle(swatchArea, 4.0f);

    if (isHoveringSwatch)
    {
        g.setColour(juce::Colours::white);
        g.drawRoundedRectangle(swatchArea.expanded(1.2f), 4.5f, 1.6f);
    }
    else
    {
        g.setColour(juce::Colours::white.withAlpha(0.35f));
        g.drawRoundedRectangle(swatchArea, 4.0f, 1.0f);
    }

    // Track Name Text
    g.setColour(isLocal ? juce::Colours::white : juce::Colour(0xFFD6DBDF));
    g.setFont(juce::FontOptions(12.0f).withStyle(isLocal ? "Bold" : "Regular"));
    juce::String displayName = trackName;
    if (isLocal) displayName += " (Local)";
    const int nameMaxWidth = std::max(60, static_cast<int>(bounds.getWidth()) - 180);
    g.drawText(displayName, 48, 6, nameMaxWidth, 20, juce::Justification::centredLeft, true);

    // Masking Clash % Badge (for remote tracks) or Reference Target Badge (for local track)
    const auto badgeArea = juce::Rectangle<float>(bounds.getWidth() - 126.0f, 7.0f, 68.0f, 18.0f);

    if (!isLocal)
    {
        juce::Colour badgeCol;
        if (currentCollisionPercent < 15.0f)
            badgeCol = juce::Colour(0xFF06D6A0); // Calm green (Low overlap)
        else if (currentCollisionPercent < 35.0f)
            badgeCol = juce::Colour(0xFFFFBE0B); // Caution amber (Moderate overlap)
        else
            badgeCol = juce::Colour(0xFFFF3333); // Severe alert red (High masking clash)

        g.setColour(badgeCol.withAlpha(0.20f));
        g.fillRoundedRectangle(badgeArea, 3.5f);
        g.setColour(badgeCol.withAlpha(0.90f));
        g.drawRoundedRectangle(badgeArea, 3.5f, 1.0f);

        g.setFont(juce::FontOptions(9.5f).withStyle("Bold"));
        const int pct = static_cast<int>(std::round(currentCollisionPercent));
        const juce::String badgeText = "Clash " + juce::String(pct) + "%";
        g.drawText(badgeText, badgeArea, juce::Justification::centred);
    }
    else
    {
        g.setColour(juce::Colour(0xFF283246));
        g.fillRoundedRectangle(badgeArea, 3.5f);
        g.setColour(juce::Colour(0xFF4C6085));
        g.drawRoundedRectangle(badgeArea, 3.5f, 1.0f);
        g.setColour(juce::Colour(0xFFAAB8D0));
        g.setFont(juce::FontOptions(9.0f).withStyle("Bold"));
        g.drawText("REF TARGET", badgeArea, juce::Justification::centred);
    }
}

void InstanceRowComponent::mouseMove(const juce::MouseEvent& e)
{
    const bool hovering = getSwatchBounds().expanded(3.0f).contains(e.position);
    if (hovering != isHoveringSwatch)
    {
        isHoveringSwatch = hovering;
        setMouseCursor(hovering ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void InstanceRowComponent::mouseExit(const juce::MouseEvent&)
{
    if (isHoveringSwatch)
    {
        isHoveringSwatch = false;
        setMouseCursor(juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void InstanceRowComponent::mouseDown(const juce::MouseEvent& e)
{
    if (getSwatchBounds().expanded(3.0f).contains(e.position))
    {
        if (e.mods.isPopupMenu())
        {
            // Right-click: quickly cycle through preset palette colours
            const auto& palette = getPresetTrackColors();
            size_t nextIdx = 0;
            for (size_t i = 0; i < palette.size(); ++i)
            {
                if (juce::Colour(palette[i]).getARGB() == trackColour.getARGB())
                {
                    nextIdx = (i + 1) % palette.size();
                    break;
                }
            }
            const juce::Colour newCol(palette[nextIdx]);
            trackColour = newCol;
            InstanceHub::getInstance().setInstanceColour(id, newCol);
            repaint();
        }
        else if (onColourSwatchClicked)
        {
            onColourSwatchClicked(id, trackName, trackColour);
        }
    }
}

void InstanceRowComponent::resized()
{
    const int h = getHeight();
    showToggle.setBounds(6, (h - 20) / 2, 20, 20);

    const int right = getWidth() - 6;
    muteButton.setBounds(right - 22, (h - 20) / 2, 22, 20);
    soloButton.setBounds(right - 48, (h - 20) / 2, 22, 20);
}

//==============================================================================

InstanceListOverlay::InstanceListOverlay(const juce::Uuid& localInstanceId)
    : localId(localInstanceId)
{
    setOpaque(true);

    headerLabel.setText("Active Instances & Unmasking", juce::dontSendNotification);
    headerLabel.setFont(juce::FontOptions(13.0f).withStyle("Bold"));
    headerLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(headerLabel);

    thresholdLabel.setText("SMR Sensitivity:", juce::dontSendNotification);
    thresholdLabel.setFont(juce::FontOptions(11.0f));
    thresholdLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF9AA0A6));
    addAndMakeVisible(thresholdLabel);

    thresholdSlider.setRange(-12.0, 6.0, 0.5);
    thresholdSlider.setValue(0.0, juce::dontSendNotification);
    thresholdSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    thresholdSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 48, 18);
    thresholdSlider.setTextValueSuffix(" dB");
    thresholdSlider.onValueChange = [this]() {
        currentSMRThreshold = static_cast<float>(thresholdSlider.getValue());
        if (onSMRThresholdChanged) onSMRThresholdChanged(currentSMRThreshold);
    };
    addAndMakeVisible(thresholdSlider);

    viewport.setViewedComponent(&listContainer, false);
    viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport);

    startTimerHz(15); // 15 Hz refresh for instance list
}

InstanceListOverlay::~InstanceListOverlay()
{
    stopTimer();
}

void InstanceListOverlay::setSMRThreshold(float thresholdDb)
{
    currentSMRThreshold = thresholdDb;
    thresholdSlider.setValue(thresholdDb, juce::dontSendNotification);
}

void InstanceListOverlay::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF10131B));
    g.setColour(juce::Colour(0xFF282E3D));
    g.drawRect(getLocalBounds(), 1);

    const float w = static_cast<float>(getWidth());

    // Explanation subtitle & color legend
    g.setColour(juce::Colour(0xFF161B26));
    g.fillRect(1.0f, 54.0f, w - 2.0f, 38.0f);

    g.setColour(juce::Colour(0xFF8E99AD));
    g.setFont(juce::FontOptions(9.5f));
    g.drawText("Clash % = quota spettro Local coperta (clicca colore per cambiare)",
               10, 55, getWidth() - 16, 15, juce::Justification::centredLeft, true);

    // Column Header Row
    g.setColour(juce::Colour(0xFF23293A));
    g.drawHorizontalLine(72, 1.0f, w - 1.0f);

    g.setColour(juce::Colour(0xFF7C889E));
    g.setFont(juce::FontOptions(9.0f).withStyle("Bold"));
    g.drawText("COL / TRACCIA", 26, 74, 110, 15, juce::Justification::centredLeft);
    g.drawText("CLASH vs LOCAL", getWidth() - 130, 74, 76, 15, juce::Justification::centred);
    g.drawText("S / M", getWidth() - 52, 74, 46, 15, juce::Justification::centred);

    // Section separator line
    g.setColour(juce::Colour(0xFF282E3D));
    g.drawHorizontalLine(92, 0.0f, w);
}

void InstanceListOverlay::resized()
{
    headerLabel.setBounds(12, 6, getWidth() - 24, 20);
    thresholdLabel.setBounds(12, 30, 95, 20);
    thresholdSlider.setBounds(110, 30, getWidth() - 120, 20);

    viewport.setBounds(1, 93, getWidth() - 2, getHeight() - 94);
    refreshInstances();
}

void InstanceListOverlay::timerCallback()
{
    refreshInstances();
}

void InstanceListOverlay::refreshInstances()
{
    auto snapshot = InstanceHub::getInstance().getSnapshot();
    if (!snapshot) return;

    // Find local frame for collision reference
    InstanceFramePtr localFrame = nullptr;
    for (const auto& item : *snapshot)
    {
        if (item && item->instanceId == localId)
        {
            localFrame = item;
            break;
        }
    }

    const size_t numInstances = snapshot->size();

    // Reallocate row components if count or instance IDs changed
    bool needsRebuild = (rows.size() != numInstances);
    if (!needsRebuild)
    {
        for (size_t i = 0; i < numInstances; ++i)
        {
            if ((*snapshot)[i] && rows[i]->getInstanceId() != (*snapshot)[i]->instanceId)
            {
                needsRebuild = true;
                break;
            }
        }
    }

    if (needsRebuild)
    {
        rows.clear();
        for (const auto& item : *snapshot)
        {
            if (!item) continue;
            const bool isLocal = (item->instanceId == localId);
            auto row = std::make_unique<InstanceRowComponent>(item->instanceId, isLocal);

            row->onVisibilityToggled = [](const juce::Uuid& instId, bool visible) {
                InstanceHub::getInstance().setInstanceVisibility(instId, visible);
            };
            row->onSoloToggled = [](const juce::Uuid& instId, bool solo) {
                InstanceHub::getInstance().setInstanceSolo(instId, solo);
            };
            row->onMuteToggled = [](const juce::Uuid& instId, bool mute) {
                InstanceHub::getInstance().setInstanceMute(instId, mute);
            };
            row->onColourSwatchClicked = [this](const juce::Uuid& instId, const juce::String& name, juce::Colour col) {
                if (onInstanceColourSwatchClicked)
                    onInstanceColourSwatchClicked(instId, name, col);
            };

            listContainer.addAndMakeVisible(row.get());
            rows.push_back(std::move(row));
        }
    }

    constexpr int rowHeight = 32;
    int y = 0;
    const int rowWidth = viewport.getWidth() > 0 ? viewport.getWidth() : (getWidth() - 2);

    for (size_t i = 0; i < snapshot->size() && i < rows.size(); ++i)
    {
        const auto& item = (*snapshot)[i];
        if (!item) continue;

        float collisionScore = 0.0f;
        if (item->instanceId != localId && localFrame)
        {
            std::array<float, kNumDisplayBins> dummyMask {};
            maskingAnalyzer.computeInterTrackCollisions(
                localFrame->barkEnergyDb,
                item->barkExcitationDb,
                currentSMRThreshold,
                dummyMask,
                collisionScore
            );
        }
        else if (item->instanceId == localId)
        {
            collisionScore = item->collisionPercent;
        }

        rows[i]->updateData(*item, collisionScore);
        rows[i]->setBounds(0, y, rowWidth, rowHeight);
        y += rowHeight;
    }

    listContainer.setSize(rowWidth, std::max(y, viewport.getHeight()));
}

} // namespace spectracross

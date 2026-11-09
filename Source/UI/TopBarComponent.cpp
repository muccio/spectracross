#include "TopBarComponent.h"

namespace spectracross
{

TopBarComponent::TopBarComponent()
{
    setOpaque(true);

    // Track identity swatch
    colourSwatchButton.setButtonText("");
    colourSwatchButton.onClick = [this]() {
        if (onColourSwatchClicked) onColourSwatchClicked();
    };
    addAndMakeVisible(colourSwatchButton);

    // Track name editable field
    trackNameEditor.setText("SpectraCross", juce::dontSendNotification);
    trackNameEditor.setFont(juce::FontOptions(13.0f).withStyle("Bold"));
    trackNameEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF1E2433));
    trackNameEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    trackNameEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xFF2E374D));
    trackNameEditor.onReturnKey = [this]() {
        if (onTrackNameChanged) onTrackNameChanged(trackNameEditor.getText());
    };
    trackNameEditor.onFocusLost = [this]() {
        if (onTrackNameChanged) onTrackNameChanged(trackNameEditor.getText());
    };
    addAndMakeVisible(trackNameEditor);

    auto setupCombo = [this](juce::ComboBox& cb, const std::vector<juce::String>& items, int selectedId) {
        for (size_t i = 0; i < items.size(); ++i)
            cb.addItem(items[i], static_cast<int>(i + 1));
        cb.setSelectedId(selectedId, juce::dontSendNotification);
        cb.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xFF191D28));
        cb.setColour(juce::ComboBox::textColourId, juce::Colours::white);
        cb.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xFF282F40));
        addAndMakeVisible(cb);
    };

    // Summation Mode
    setupCombo(modeCombo, { "Stereo", "Mono", "Mid", "Side", "Max-Peak" }, 1);
    modeCombo.onChange = [this]() {
        if (onSummationModeChanged)
        {
            switch (modeCombo.getSelectedId())
            {
                case 1: onSummationModeChanged(SummationMode::Stereo); break;
                case 2: onSummationModeChanged(SummationMode::Mono); break;
                case 3: onSummationModeChanged(SummationMode::Mid); break;
                case 4: onSummationModeChanged(SummationMode::Side); break;
                case 5: onSummationModeChanged(SummationMode::MaxPeak); break;
            }
        }
    };

    // FFT Size
    setupCombo(fftSizeCombo, { "2048 pts", "4096 pts", "8192 pts" }, 2);
    fftSizeCombo.onChange = [this]() {
        if (onFFTSizeChanged)
        {
            switch (fftSizeCombo.getSelectedId())
            {
                case 1: onFFTSizeChanged(FFTSize::Size2048); break;
                case 2: onFFTSizeChanged(FFTSize::Size4096); break;
                case 3: onFFTSizeChanged(FFTSize::Size8192); break;
            }
        }
    };

    // Windowing
    setupCombo(windowCombo, { "Blackman-Harris 7T", "Hann" }, 1);
    windowCombo.onChange = [this]() {
        if (onWindowTypeChanged)
        {
            switch (windowCombo.getSelectedId())
            {
                case 1: onWindowTypeChanged(WindowType::BlackmanHarris7); break;
                case 2: onWindowTypeChanged(WindowType::Hann); break;
            }
        }
    };

    // Tilt Slope
    setupCombo(tiltCombo, { "Flat (0 dB)", "+3.0 dB/oct", "+4.5 dB (Pink)", "+6.0 dB/oct" }, 3);
    tiltCombo.onChange = [this]() {
        if (onTiltSlopeChanged)
        {
            switch (tiltCombo.getSelectedId())
            {
                case 1: onTiltSlopeChanged(TiltSlope::Flat); break;
                case 2: onTiltSlopeChanged(TiltSlope::Slope3dB); break;
                case 3: onTiltSlopeChanged(TiltSlope::Slope4_5dB); break;
                case 4: onTiltSlopeChanged(TiltSlope::Slope6dB); break;
            }
        }
    };

    // dB Range
    setupCombo(rangeCombo, { "48 dB Range", "72 dB Range", "96 dB Range" }, 2);
    rangeCombo.onChange = [this]() {
        if (onDBRangeChanged)
        {
            switch (rangeCombo.getSelectedId())
            {
                case 1: onDBRangeChanged(DBRange::Range48dB); break;
                case 2: onDBRangeChanged(DBRange::Range72dB); break;
                case 3: onDBRangeChanged(DBRange::Range96dB); break;
            }
        }
    };

    // Peak Hold
    setupCombo(holdCombo, { "Hold Off", "Hold 1s", "Hold 2s", "Hold 5s", "Hold Infinite" }, 3);
    holdCombo.onChange = [this]() {
        if (onHoldTimeChanged)
        {
            switch (holdCombo.getSelectedId())
            {
                case 1: onHoldTimeChanged(HoldTime::Off); break;
                case 2: onHoldTimeChanged(HoldTime::Hold1s); break;
                case 3: onHoldTimeChanged(HoldTime::Hold2s); break;
                case 4: onHoldTimeChanged(HoldTime::Hold5s); break;
                case 5: onHoldTimeChanged(HoldTime::Infinite); break;
            }
        }
    };

    // Toggles
    refBandsToggle.setToggleState(true, juce::dontSendNotification);
    refBandsToggle.onClick = [this]() {
        if (onReferenceBandsToggled) onReferenceBandsToggled(refBandsToggle.getToggleState());
    };
    addAndMakeVisible(refBandsToggle);

    pianoToggle.setToggleState(true, juce::dontSendNotification);
    pianoToggle.onClick = [this]() {
        if (onPianoRollToggled) onPianoRollToggled(pianoToggle.getToggleState());
    };
    addAndMakeVisible(pianoToggle);

    instanceListButton.setClickingTogglesState(true);
    instanceListButton.setToggleState(false, juce::dontSendNotification);
    instanceListButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF202635));
    instanceListButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xFF3A86FF));
    instanceListButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    instanceListButton.onClick = [this]() {
        if (onInstanceListToggled) onInstanceListToggled(instanceListButton.getToggleState());
    };
    addAndMakeVisible(instanceListButton);

    freezeButton.setClickingTogglesState(true);
    freezeButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF202635));
    freezeButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xFF00B4D8));
    freezeButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    freezeButton.onClick = [this]() {
        if (onFreezeToggled) onFreezeToggled(freezeButton.getToggleState());
    };
    addAndMakeVisible(freezeButton);
}

void TopBarComponent::setTrackName(const juce::String& name)
{
    if (!trackNameEditor.hasKeyboardFocus(true))
        trackNameEditor.setText(name, juce::dontSendNotification);
}

void TopBarComponent::setTrackColour(juce::Colour colour)
{
    currentTrackColour = colour;
    repaint();
}

void TopBarComponent::updateInstanceCountBadge(int count)
{
    instanceListButton.setButtonText("Instances (" + juce::String(count) + ")");
}

void TopBarComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF131722));
    g.setColour(juce::Colour(0xFF252D3D));
    g.drawHorizontalLine(getHeight() - 1, 0.0f, static_cast<float>(getWidth()));

    // Paint Color Swatch Button
    const auto swatchBounds = colourSwatchButton.getBounds().toFloat();
    g.setColour(currentTrackColour);
    g.fillRoundedRectangle(swatchBounds, 4.0f);
    g.setColour(juce::Colours::white.withAlpha(0.6f));
    g.drawRoundedRectangle(swatchBounds, 4.0f, 1.0f);
}

void TopBarComponent::resized()
{
    const int pad = 8;
    const int h = 26;
    const int y = (getHeight() - h) / 2;

    int x = pad;

    colourSwatchButton.setBounds(x, y + 2, 22, 22);
    x += 28;

    trackNameEditor.setBounds(x, y, 140, h);
    x += 148;

    modeCombo.setBounds(x, y, 92, h);
    x += 98;

    fftSizeCombo.setBounds(x, y, 92, h);
    x += 98;

    windowCombo.setBounds(x, y, 138, h);
    x += 144;

    tiltCombo.setBounds(x, y, 115, h);
    x += 121;

    rangeCombo.setBounds(x, y, 105, h);
    x += 111;

    holdCombo.setBounds(x, y, 98, h);
    x += 104;

    refBandsToggle.setBounds(x, y, 84, h);
    x += 88;

    pianoToggle.setBounds(x, y, 62, h);
    x += 66;

    // Right-aligned controls
    int rightX = getWidth() - pad;

    instanceListButton.setBounds(rightX - 105, y, 105, h);
    rightX -= 112;

    freezeButton.setBounds(rightX - 65, y, 65, h);
}

} // namespace spectracross

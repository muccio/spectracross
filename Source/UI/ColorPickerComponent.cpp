#include "ColorPickerComponent.h"

namespace spectracross
{

ColorPickerComponent::ColorPickerComponent()
{
    setOpaque(true);

    auto setupSlider = [this](juce::Slider& s, const juce::String&, juce::Colour col) {
        s.setRange(0, 255, 1);
        s.setSliderStyle(juce::Slider::LinearHorizontal);
        s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
        s.setColour(juce::Slider::thumbColourId, col);
        s.setColour(juce::Slider::trackColourId, col.withAlpha(0.4f));
        s.onValueChange = [this]() { updateFromSliders(); };
        addAndMakeVisible(s);
    };

    setupSlider(rSlider, "R", juce::Colours::red);
    setupSlider(gSlider, "G", juce::Colours::green);
    setupSlider(bSlider, "B", juce::Colours::dodgerblue);

    hexLabel.setText("Hex #:", juce::dontSendNotification);
    hexLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(hexLabel);

    hexEditor.setText("3A86FF", juce::dontSendNotification);
    hexEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF222634));
    hexEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    hexEditor.onReturnKey = [this]() { updateFromHexEditor(); };
    hexEditor.onFocusLost = [this]() { updateFromHexEditor(); };
    addAndMakeVisible(hexEditor);

    closeButton.onClick = [this]() {
        if (onCloseRequested) onCloseRequested();
    };
    closeButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF2A2E3D));
    closeButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    addAndMakeVisible(closeButton);

    syncSlidersToColour();
    setSize(260, 280);
}

void ColorPickerComponent::setCurrentColour(juce::Colour colour)
{
    selectedColour = colour;
    syncSlidersToColour();
    repaint();
}

void ColorPickerComponent::paint(juce::Graphics& g)
{
    // Background and border
    g.fillAll(juce::Colour(0xFF141721));
    g.setColour(juce::Colour(0xFF2E3448));
    g.drawRect(getLocalBounds(), 1);

    // Header
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(13.0f).withStyle("Bold"));
    g.drawText("Track Color Palette", 12, 8, getWidth() - 24, 20, juce::Justification::left);

    // Current Color Preview Pill
    auto previewArea = juce::Rectangle<int>(getWidth() - 48, 8, 36, 18);
    g.setColour(selectedColour);
    g.fillRoundedRectangle(previewArea.toFloat(), 4.0f);
    g.setColour(juce::Colours::white.withAlpha(0.6f));
    g.drawRoundedRectangle(previewArea.toFloat(), 4.0f, 1.0f);

    // Swatches grid (2 rows of 6)
    const auto& palette = getPresetTrackColors();
    for (size_t i = 0; i < palette.size(); ++i)
    {
        const auto bounds = swatchBounds[i];
        const auto col = juce::Colour(palette[i]);

        g.setColour(col);
        g.fillRoundedRectangle(bounds.toFloat(), 4.0f);

        // Selection ring
        if (selectedColour.getARGB() == col.getARGB())
        {
            g.setColour(juce::Colours::white);
            g.drawRoundedRectangle(bounds.toFloat().expanded(2.0f), 5.0f, 2.0f);
        }
        else
        {
            g.setColour(juce::Colours::black.withAlpha(0.3f));
            g.drawRoundedRectangle(bounds.toFloat(), 4.0f, 1.0f);
        }
    }
}

void ColorPickerComponent::resized()
{
    const int pad = 12;
    const int swatchSize = 30;
    const int swatchGap = 8;
    int top = 34;

    for (int row = 0; row < 2; ++row)
    {
        for (int col = 0; col < 6; ++col)
        {
            const size_t idx = static_cast<size_t>(row * 6 + col);
            swatchBounds[idx] = juce::Rectangle<int>(
                pad + col * (swatchSize + swatchGap),
                top + row * (swatchSize + swatchGap),
                swatchSize,
                swatchSize
            );
        }
    }

    top += 2 * (swatchSize + swatchGap) + 8;

    rSlider.setBounds(pad, top, getWidth() - 2 * pad, 22);
    top += 26;
    gSlider.setBounds(pad, top, getWidth() - 2 * pad, 22);
    top += 26;
    bSlider.setBounds(pad, top, getWidth() - 2 * pad, 22);
    top += 30;

    hexLabel.setBounds(pad, top, 50, 24);
    hexEditor.setBounds(pad + 54, top, 80, 24);
    closeButton.setBounds(getWidth() - pad - 60, top, 60, 24);
}

void ColorPickerComponent::mouseDown(const juce::MouseEvent& e)
{
    const auto& palette = getPresetTrackColors();
    for (size_t i = 0; i < palette.size(); ++i)
    {
        if (swatchBounds[i].contains(e.getPosition()))
        {
            selectedColour = juce::Colour(palette[i]);
            syncSlidersToColour();
            if (onColourChanged) onColourChanged(selectedColour);
            repaint();
            return;
        }
    }
}

void ColorPickerComponent::updateFromSliders()
{
    selectedColour = juce::Colour(
        static_cast<juce::uint8>(rSlider.getValue()),
        static_cast<juce::uint8>(gSlider.getValue()),
        static_cast<juce::uint8>(bSlider.getValue())
    );

    hexEditor.setText(selectedColour.toDisplayString(false), juce::dontSendNotification);

    if (onColourChanged)
        onColourChanged(selectedColour);

    repaint();
}

void ColorPickerComponent::updateFromHexEditor()
{
    auto text = hexEditor.getText().trim();
    if (!text.startsWith("#") && text.length() <= 6)
        text = "FF" + text; // Prepend alpha if missing
    else if (text.startsWith("#"))
        text = text.substring(1);

    auto parsed = juce::Colour::fromString(text);
    if (parsed != juce::Colour())
    {
        selectedColour = parsed.withAlpha(1.0f);
        syncSlidersToColour();
        if (onColourChanged)
            onColourChanged(selectedColour);
        repaint();
    }
}

void ColorPickerComponent::syncSlidersToColour()
{
    rSlider.setValue(selectedColour.getRed(), juce::dontSendNotification);
    gSlider.setValue(selectedColour.getGreen(), juce::dontSendNotification);
    bSlider.setValue(selectedColour.getBlue(), juce::dontSendNotification);
    hexEditor.setText(selectedColour.toDisplayString(false), juce::dontSendNotification);
}

} // namespace spectracross

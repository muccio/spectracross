#pragma once

#include "../Common/SpectraCommon.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace spectracross
{

class ColorPickerComponent : public juce::Component
{
public:
    ColorPickerComponent();
    ~ColorPickerComponent() override = default;

    void setCurrentColour(juce::Colour colour);
    [[nodiscard]] juce::Colour getCurrentColour() const noexcept { return selectedColour; }

    std::function<void(juce::Colour)> onColourChanged;
    std::function<void()> onCloseRequested;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    juce::Colour selectedColour { 0xFF3A86FF };

    juce::Slider rSlider;
    juce::Slider gSlider;
    juce::Slider bSlider;
    juce::Label hexLabel;
    juce::TextEditor hexEditor;
    juce::TextButton closeButton { "Close" };

    std::array<juce::Rectangle<int>, 12> swatchBounds;

    void updateFromSliders();
    void updateFromHexEditor();
    void syncSlidersToColour();
};

} // namespace spectracross

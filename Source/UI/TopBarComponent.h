#pragma once

#include "../Common/SpectraCommon.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace spectracross
{

class TopBarComponent : public juce::Component
{
public:
    TopBarComponent();
    ~TopBarComponent() override = default;

    void setTrackName(const juce::String& name);
    void setTrackColour(juce::Colour colour);

    // Callbacks
    std::function<void()> onColourSwatchClicked;
    std::function<void(const juce::String&)> onTrackNameChanged;
    std::function<void(SummationMode)> onSummationModeChanged;
    std::function<void(FFTSize)> onFFTSizeChanged;
    std::function<void(WindowType)> onWindowTypeChanged;
    std::function<void(TiltSlope)> onTiltSlopeChanged;
    std::function<void(DBRange)> onDBRangeChanged;
    std::function<void(HoldTime)> onHoldTimeChanged;
    std::function<void(bool)> onReferenceBandsToggled;
    std::function<void(bool)> onPianoRollToggled;
    std::function<void(bool)> onInstanceListToggled;
    std::function<void(bool)> onFreezeToggled;

    void updateInstanceCountBadge(int count);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::Colour currentTrackColour { 0xFF3A86FF };
    juce::TextButton colourSwatchButton;
    juce::TextEditor trackNameEditor;

    juce::ComboBox modeCombo;
    juce::ComboBox fftSizeCombo;
    juce::ComboBox windowCombo;
    juce::ComboBox tiltCombo;
    juce::ComboBox rangeCombo;
    juce::ComboBox holdCombo;

    juce::ToggleButton refBandsToggle { "Mix Zones" };
    juce::ToggleButton pianoToggle { "Piano" };
    juce::TextButton instanceListButton { "Instances (1)" };
    juce::TextButton freezeButton { "Freeze" };
};

} // namespace spectracross

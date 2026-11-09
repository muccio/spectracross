#pragma once

#include "Common/SpectraCommon.h"
#include "PluginProcessor.h"
#include "UI/ColorPickerComponent.h"
#include "UI/InstanceListOverlay.h"
#include "UI/PianoRollComponent.h"
#include "UI/SpectrumComponent.h"
#include "UI/TopBarComponent.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace spectracross
{

class SpectraCrossAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SpectraCrossAudioProcessorEditor(SpectraCrossAudioProcessor&);
    ~SpectraCrossAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    void setupCallbacks();
    void showColorPickerModal(bool show);

    SpectraCrossAudioProcessor& processorRef;

    TopBarComponent topBar;
    SpectrumComponent spectrumView;
    PianoRollComponent pianoRoll;
    InstanceListOverlay instanceSidebar;
    ColorPickerComponent colorPickerModal;

    bool isSidebarVisible = false;
    bool isColorPickerVisible = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectraCrossAudioProcessorEditor)
};

} // namespace spectracross

#pragma once

#include "../Common/SpectraCommon.h"
#include "../Common/SpectralData.h"
#include "../DSP/MaskingAnalyzer.h"
#include "../Hub/InstanceHub.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <vector>

namespace spectracross
{

class InstanceRowComponent : public juce::Component
{
public:
    InstanceRowComponent(const juce::Uuid& instanceId, bool isLocalInstance);
    ~InstanceRowComponent() override = default;

    const juce::Uuid& getInstanceId() const noexcept { return id; }

    void updateData(const InstanceSpectralFrame& frame, float collisionPercent);

    void paint(juce::Graphics& g) override;
    void resized() override;

    std::function<void(const juce::Uuid&, bool)> onVisibilityToggled;
    std::function<void(const juce::Uuid&, bool)> onSoloToggled;
    std::function<void(const juce::Uuid&, bool)> onMuteToggled;

private:
    juce::Uuid id;
    bool isLocal = false;
    juce::Colour trackColour { 0xFF3A86FF };
    juce::String trackName;
    float currentCollisionPercent = 0.0f;

    juce::ToggleButton showToggle;
    juce::TextButton soloButton { "S" };
    juce::TextButton muteButton { "M" };
};

class InstanceListOverlay : public juce::Component, private juce::Timer
{
public:
    InstanceListOverlay(const juce::Uuid& localInstanceId);
    ~InstanceListOverlay() override;

    void setSMRThreshold(float thresholdDb);
    float getSMRThreshold() const noexcept { return currentSMRThreshold; }

    std::function<void(float)> onSMRThresholdChanged;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void refreshInstances();

private:
    void timerCallback() override;

    juce::Uuid localId;
    float currentSMRThreshold = 0.0f;

    juce::Label headerLabel;
    juce::Label thresholdLabel;
    juce::Slider thresholdSlider;
    juce::Viewport viewport;
    juce::Component listContainer;

    MaskingAnalyzer maskingAnalyzer;
    std::vector<std::unique_ptr<InstanceRowComponent>> rows;
};

} // namespace spectracross

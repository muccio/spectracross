#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>

namespace spectracross
{

class HostMetadataFetcher
{
public:
    HostMetadataFetcher();
    ~HostMetadataFetcher() = default;

    /** Updates track metadata from JUCE / DAW callback. */
    void updateFromTrackProperties(const juce::AudioProcessor::TrackProperties& properties);

    /** Probes host context for VST3 / Reaper specific metadata. */
    void probeHostContext(juce::AudioProcessor& processor);

    // Manual overrides
    void setCustomTrackName(const juce::String& name);
    void setCustomTrackColour(juce::Colour colour);
    void clearCustomNameOverride();
    void clearCustomColourOverride();

    // Query effective metadata
    [[nodiscard]] juce::String getEffectiveTrackName() const;
    [[nodiscard]] juce::Colour getEffectiveTrackColour() const;
    [[nodiscard]] int getTrackIndex() const noexcept { return dawTrackIndex; }
    [[nodiscard]] bool hasDawMetadata() const noexcept { return dawMetadataDetected; }
    [[nodiscard]] bool isNameOverridden() const noexcept { return isCustomNameActive; }
    [[nodiscard]] bool isColourOverridden() const noexcept { return isCustomColourActive; }

    // State persistence
    juce::ValueTree exportToValueTree() const;
    void importFromValueTree(const juce::ValueTree& vt);

private:
    // Host-reported metadata
    juce::String dawTrackName;
    juce::Colour dawTrackColour { 0xFF3A86FF };
    int dawTrackIndex = -1;
    bool dawMetadataDetected = false;

    // Manual overrides
    juce::String customTrackName;
    juce::Colour customTrackColour { 0xFF3A86FF };
    bool isCustomNameActive = false;
    bool isCustomColourActive = false;
};

} // namespace spectracross

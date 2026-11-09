#include "HostMetadataFetcher.h"

namespace spectracross
{

HostMetadataFetcher::HostMetadataFetcher()
    : customTrackColour(0xFF3A86FF)
{
}

void HostMetadataFetcher::updateFromTrackProperties(const juce::AudioProcessor::TrackProperties& properties)
{
    if (properties.name.has_value() && properties.name->isNotEmpty())
    {
        dawTrackName = *properties.name;
        dawMetadataDetected = true;
    }

    if (properties.colour.has_value())
    {
        dawTrackColour = *properties.colour;
        dawMetadataDetected = true;
    }
}

void HostMetadataFetcher::probeHostContext(juce::AudioProcessor& processor)
{
    // If JUCE wrapper already populated metadata, keep it
    if (dawMetadataDetected && dawTrackName.isNotEmpty())
        return;

    // Check processor name or parent container where available
    const juce::String procName = processor.getName();
    if (dawTrackName.isEmpty() && procName.isNotEmpty())
    {
        dawTrackName = procName;
    }
}

void HostMetadataFetcher::setCustomTrackName(const juce::String& name)
{
    customTrackName = name.trim();
    isCustomNameActive = customTrackName.isNotEmpty();
}

void HostMetadataFetcher::setCustomTrackColour(juce::Colour colour)
{
    customTrackColour = colour;
    isCustomColourActive = true;
}

void HostMetadataFetcher::clearCustomNameOverride()
{
    isCustomNameActive = false;
    customTrackName.clear();
}

void HostMetadataFetcher::clearCustomColourOverride()
{
    isCustomColourActive = false;
}

juce::String HostMetadataFetcher::getEffectiveTrackName() const
{
    if (isCustomNameActive && customTrackName.isNotEmpty())
        return customTrackName;

    if (dawTrackName.isNotEmpty())
        return dawTrackName;

    return "SpectraCross Track";
}

juce::Colour HostMetadataFetcher::getEffectiveTrackColour() const
{
    if (isCustomColourActive)
        return customTrackColour;

    if (dawMetadataDetected)
        return dawTrackColour;

    return customTrackColour;
}

juce::ValueTree HostMetadataFetcher::exportToValueTree() const
{
    juce::ValueTree vt("TrackMetadataOverrides");
    vt.setProperty("isCustomNameActive", isCustomNameActive, nullptr);
    vt.setProperty("customTrackName", customTrackName, nullptr);
    vt.setProperty("isCustomColourActive", isCustomColourActive, nullptr);
    vt.setProperty("customTrackColour", customTrackColour.toString(), nullptr);
    return vt;
}

void HostMetadataFetcher::importFromValueTree(const juce::ValueTree& vt)
{
    if (!vt.isValid())
        return;

    if (vt.hasProperty("isCustomNameActive"))
        isCustomNameActive = vt.getProperty("isCustomNameActive");

    if (vt.hasProperty("customTrackName"))
        customTrackName = vt.getProperty("customTrackName").toString();

    if (vt.hasProperty("isCustomColourActive"))
        isCustomColourActive = vt.getProperty("isCustomColourActive");

    if (vt.hasProperty("customTrackColour"))
        customTrackColour = juce::Colour::fromString(vt.getProperty("customTrackColour").toString());
}

} // namespace spectracross

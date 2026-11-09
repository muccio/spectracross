#pragma once

#include "SpectraCommon.h"
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <array>
#include <memory>
#include <vector>

namespace spectracross
{

struct InstanceSpectralFrame
{
    juce::Uuid instanceId;
    juce::String trackName;
    juce::Colour trackColour { 0xFF3A86FF };
    int trackIndex = -1;
    bool isBypassed = false;
    bool isSoloed = false;
    bool isMuted = false;
    bool isVisibleInDisplay = true;

    // 512 log-spaced display bins for 60 FPS graphics
    std::array<float, kNumDisplayBins> magnitudesDb {};
    std::array<float, kNumDisplayBins> peakHoldDb {};

    // 24 Bark critical bands for psychoacoustic masking analysis
    std::array<float, kNumBarkBands> barkEnergyDb {};
    std::array<float, kNumBarkBands> barkExcitationDb {};

    // Calculated collision against the current local instance
    std::array<float, kNumDisplayBins> collisionMaskDb {};
    float collisionPercent = 0.0f;

    int64_t lastPublishTimeMs = 0;

    InstanceSpectralFrame()
    {
        magnitudesDb.fill(-120.0f);
        peakHoldDb.fill(-120.0f);
        barkEnergyDb.fill(-120.0f);
        barkExcitationDb.fill(-120.0f);
        collisionMaskDb.fill(0.0f);
    }
};

using InstanceFramePtr = std::shared_ptr<InstanceSpectralFrame>;
using InstanceSnapshotMap = std::vector<InstanceFramePtr>;

} // namespace spectracross

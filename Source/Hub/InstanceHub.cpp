#include "InstanceHub.h"

namespace spectracross
{

InstanceHub& InstanceHub::getInstance()
{
    static InstanceHub instance;
    return instance;
}

InstanceHub::InstanceHub()
{
    auto initialMap = std::make_shared<InstanceSnapshotMap>();
    std::atomic_store_explicit(&currentSnapshot, std::shared_ptr<const InstanceSnapshotMap>(initialMap), std::memory_order_relaxed);
}

void InstanceHub::registerInstance(const juce::Uuid& id,
                                   const juce::String& trackName,
                                   juce::Colour colour,
                                   int trackIndex)
{
    std::lock_guard<std::mutex> lock(mutateMutex);

    auto oldMap = std::atomic_load_explicit(&currentSnapshot, std::memory_order_relaxed);
    auto newMap = std::make_shared<InstanceSnapshotMap>(*oldMap);

    // Check if already registered
    auto it = std::find_if(newMap->begin(), newMap->end(),
                           [&id](const InstanceFramePtr& ptr) { return ptr && ptr->instanceId == id; });

    if (it == newMap->end())
    {
        auto newFrame = std::make_shared<InstanceSpectralFrame>();
        newFrame->instanceId = id;
        newFrame->trackName = trackName.isNotEmpty() ? trackName : "Track " + juce::String(newMap->size() + 1);
        newFrame->trackColour = colour;
        newFrame->trackIndex = trackIndex;
        newFrame->lastPublishTimeMs = juce::Time::currentTimeMillis();
        newMap->push_back(newFrame);
    }
    else
    {
        // Clone existing entry and update
        auto updated = std::make_shared<InstanceSpectralFrame>(**it);
        if (trackName.isNotEmpty()) updated->trackName = trackName;
        updated->trackColour = colour;
        updated->trackIndex = trackIndex;
        updated->lastPublishTimeMs = juce::Time::currentTimeMillis();
        *it = updated;
    }

    std::atomic_store_explicit(&currentSnapshot, std::shared_ptr<const InstanceSnapshotMap>(newMap), std::memory_order_release);
}

void InstanceHub::unregisterInstance(const juce::Uuid& id)
{
    std::lock_guard<std::mutex> lock(mutateMutex);

    auto oldMap = std::atomic_load_explicit(&currentSnapshot, std::memory_order_relaxed);
    auto newMap = std::make_shared<InstanceSnapshotMap>();
    newMap->reserve(oldMap->size());

    for (const auto& item : *oldMap)
    {
        if (item && item->instanceId != id)
        {
            newMap->push_back(item);
        }
    }

    std::atomic_store_explicit(&currentSnapshot, std::shared_ptr<const InstanceSnapshotMap>(newMap), std::memory_order_release);
}

void InstanceHub::updateInstanceMetadata(const juce::Uuid& id,
                                        const juce::String& trackName,
                                        juce::Colour colour,
                                        int trackIndex,
                                        bool isBypassed)
{
    std::lock_guard<std::mutex> lock(mutateMutex);

    auto oldMap = std::atomic_load_explicit(&currentSnapshot, std::memory_order_relaxed);
    auto newMap = std::make_shared<InstanceSnapshotMap>(*oldMap);

    auto it = std::find_if(newMap->begin(), newMap->end(),
                           [&id](const InstanceFramePtr& ptr) { return ptr && ptr->instanceId == id; });

    if (it != newMap->end())
    {
        auto updated = std::make_shared<InstanceSpectralFrame>(**it);
        if (trackName.isNotEmpty()) updated->trackName = trackName;
        updated->trackColour = colour;
        updated->trackIndex = trackIndex;
        updated->isBypassed = isBypassed;
        *it = updated;
        std::atomic_store_explicit(&currentSnapshot, std::shared_ptr<const InstanceSnapshotMap>(newMap), std::memory_order_release);
    }
}

void InstanceHub::setInstanceVisibility(const juce::Uuid& id, bool isVisible)
{
    std::lock_guard<std::mutex> lock(mutateMutex);

    auto oldMap = std::atomic_load_explicit(&currentSnapshot, std::memory_order_relaxed);
    auto newMap = std::make_shared<InstanceSnapshotMap>(*oldMap);

    auto it = std::find_if(newMap->begin(), newMap->end(),
                           [&id](const InstanceFramePtr& ptr) { return ptr && ptr->instanceId == id; });

    if (it != newMap->end())
    {
        auto updated = std::make_shared<InstanceSpectralFrame>(**it);
        updated->isVisibleInDisplay = isVisible;
        *it = updated;
        std::atomic_store_explicit(&currentSnapshot, std::shared_ptr<const InstanceSnapshotMap>(newMap), std::memory_order_release);
    }
}

void InstanceHub::setInstanceSolo(const juce::Uuid& id, bool isSoloed)
{
    std::lock_guard<std::mutex> lock(mutateMutex);

    auto oldMap = std::atomic_load_explicit(&currentSnapshot, std::memory_order_relaxed);
    auto newMap = std::make_shared<InstanceSnapshotMap>(*oldMap);

    auto it = std::find_if(newMap->begin(), newMap->end(),
                           [&id](const InstanceFramePtr& ptr) { return ptr && ptr->instanceId == id; });

    if (it != newMap->end())
    {
        auto updated = std::make_shared<InstanceSpectralFrame>(**it);
        updated->isSoloed = isSoloed;
        *it = updated;
        std::atomic_store_explicit(&currentSnapshot, std::shared_ptr<const InstanceSnapshotMap>(newMap), std::memory_order_release);
    }
}

void InstanceHub::setInstanceMute(const juce::Uuid& id, bool isMuted)
{
    std::lock_guard<std::mutex> lock(mutateMutex);

    auto oldMap = std::atomic_load_explicit(&currentSnapshot, std::memory_order_relaxed);
    auto newMap = std::make_shared<InstanceSnapshotMap>(*oldMap);

    auto it = std::find_if(newMap->begin(), newMap->end(),
                           [&id](const InstanceFramePtr& ptr) { return ptr && ptr->instanceId == id; });

    if (it != newMap->end())
    {
        auto updated = std::make_shared<InstanceSpectralFrame>(**it);
        updated->isMuted = isMuted;
        *it = updated;
        std::atomic_store_explicit(&currentSnapshot, std::shared_ptr<const InstanceSnapshotMap>(newMap), std::memory_order_release);
    }
}

void InstanceHub::publishSpectralFrame(const juce::Uuid& id, const InstanceSpectralFrame& frame)
{
    std::lock_guard<std::mutex> lock(mutateMutex);

    const int64_t nowMs = juce::Time::currentTimeMillis();
    auto oldMap = std::atomic_load_explicit(&currentSnapshot, std::memory_order_relaxed);
    auto newMap = std::make_shared<InstanceSnapshotMap>();
    newMap->reserve(oldMap->size() + 1);

    bool found = false;
    for (const auto& item : *oldMap)
    {
        if (!item) continue;

        if (item->instanceId == id)
        {
            auto updated = std::make_shared<InstanceSpectralFrame>(frame);
            updated->instanceId = id;
            updated->lastPublishTimeMs = nowMs;
            // Retain user display states and track metadata
            updated->isVisibleInDisplay = item->isVisibleInDisplay;
            updated->isSoloed = item->isSoloed;
            updated->isMuted = item->isMuted;
            updated->isBypassed = item->isBypassed;

            if (frame.trackName.isNotEmpty())
                updated->trackName = frame.trackName;
            else
                updated->trackName = item->trackName;

            if (frame.trackColour != juce::Colour())
                updated->trackColour = frame.trackColour;
            else
                updated->trackColour = item->trackColour;

            if (frame.trackIndex >= 0)
                updated->trackIndex = frame.trackIndex;
            else
                updated->trackIndex = item->trackIndex;

            newMap->push_back(updated);
            found = true;
        }
        else
        {
            newMap->push_back(item);
        }
    }

    if (!found)
    {
        auto created = std::make_shared<InstanceSpectralFrame>(frame);
        created->instanceId = id;
        created->lastPublishTimeMs = nowMs;
        newMap->push_back(created);
    }

    std::atomic_store_explicit(&currentSnapshot, std::shared_ptr<const InstanceSnapshotMap>(newMap), std::memory_order_release);
}

std::shared_ptr<const InstanceSnapshotMap> InstanceHub::getSnapshot() const noexcept
{
    return std::atomic_load_explicit(&currentSnapshot, std::memory_order_acquire);
}

InstanceFramePtr InstanceHub::getInstanceFrame(const juce::Uuid& id) const noexcept
{
    auto snap = std::atomic_load_explicit(&currentSnapshot, std::memory_order_acquire);
    if (!snap) return nullptr;

    for (const auto& item : *snap)
    {
        if (item && item->instanceId == id)
            return item;
    }
    return nullptr;
}

int InstanceHub::getNumInstances() const noexcept
{
    auto snap = std::atomic_load_explicit(&currentSnapshot, std::memory_order_acquire);
    return snap ? static_cast<int>(snap->size()) : 0;
}

} // namespace spectracross

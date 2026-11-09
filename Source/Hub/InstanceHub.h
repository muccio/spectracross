#pragma once

#include "../Common/SpectraCommon.h"
#include "../Common/SpectralData.h"
#include <juce_core/juce_core.h>
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

namespace spectracross
{

class InstanceHub
{
public:
    static InstanceHub& getInstance();

    // Instance lifecycle
    void registerInstance(const juce::Uuid& id,
                          const juce::String& trackName,
                          juce::Colour colour,
                          int trackIndex);

    void unregisterInstance(const juce::Uuid& id);

    void updateInstanceMetadata(const juce::Uuid& id,
                                const juce::String& trackName,
                                juce::Colour colour,
                                int trackIndex,
                                bool isBypassed);

    void setInstanceVisibility(const juce::Uuid& id, bool isVisible);
    void setInstanceSolo(const juce::Uuid& id, bool isSoloed);
    void setInstanceMute(const juce::Uuid& id, bool isMuted);

    /** Publishes a downsampled spectral frame (512 display points + Bark data).
        Thread-safe and fast.
    */
    void publishSpectralFrame(const juce::Uuid& id, const InstanceSpectralFrame& frame);

    /** Returns current RCU immutable snapshot of all registered instances.
        Lock-free, zero blocking for reader threads.
    */
    std::shared_ptr<const InstanceSnapshotMap> getSnapshot() const noexcept;

    /** Helper to find a specific instance from current snapshot. */
    InstanceFramePtr getInstanceFrame(const juce::Uuid& id) const noexcept;

    /** Returns total number of registered instances. */
    int getNumInstances() const noexcept;

private:
    InstanceHub();
    ~InstanceHub() = default;

    InstanceHub(const InstanceHub&) = delete;
    InstanceHub& operator=(const InstanceHub&) = delete;

    void pruneStaleInstances(InstanceSnapshotMap& map, int64_t nowMs);

    std::mutex mutateMutex;
    std::shared_ptr<const InstanceSnapshotMap> currentSnapshot;
};

} // namespace spectracross

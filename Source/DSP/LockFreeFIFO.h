#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <algorithm>
#include <cstring>

namespace spectracross
{

class LockFreeAudioFIFO
{
public:
    explicit LockFreeAudioFIFO(int initialCapacity = 65536)
        : abstractFifo(initialCapacity)
    {
        buffer.resize(static_cast<size_t>(initialCapacity), 0.0f);
    }

    ~LockFreeAudioFIFO() = default;

    // Non-copyable, non-movable for real-time safety
    LockFreeAudioFIFO(const LockFreeAudioFIFO&) = delete;
    LockFreeAudioFIFO& operator=(const LockFreeAudioFIFO&) = delete;
    LockFreeAudioFIFO(LockFreeAudioFIFO&&) = delete;
    LockFreeAudioFIFO& operator=(LockFreeAudioFIFO&&) = delete;

    /** Pre-allocates buffer. Must be called from non-realtime thread (e.g. prepareToPlay). */
    void prepare(int capacity)
    {
        capacity = std::max(capacity, 1024);
        abstractFifo.setTotalSize(capacity);
        buffer.assign(static_cast<size_t>(capacity), 0.0f);
        abstractFifo.reset();
    }

    /** Resets read and write pointers. */
    void reset() noexcept
    {
        abstractFifo.reset();
    }

    /** Pushes samples from the real-time audio thread.
        Zero heap allocations, zero mutexes, zero syscalls.
        Returns number of samples actually written.
    */
    int write(const float* data, int numSamples) noexcept
    {
        if (data == nullptr || numSamples <= 0)
            return 0;

        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        abstractFifo.prepareToWrite(numSamples, start1, size1, start2, size2);

        if (size1 > 0)
            std::memcpy(buffer.data() + start1, data, static_cast<size_t>(size1) * sizeof(float));

        if (size2 > 0)
            std::memcpy(buffer.data() + start2, data + size1, static_cast<size_t>(size2) * sizeof(float));

        const int totalWritten = size1 + size2;
        abstractFifo.finishedWrite(totalWritten);
        return totalWritten;
    }

    /** Pulls samples into destination buffer from the background worker thread.
        Returns number of samples actually read.
    */
    int read(float* dest, int numSamples) noexcept
    {
        if (dest == nullptr || numSamples <= 0)
            return 0;

        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        abstractFifo.prepareToRead(numSamples, start1, size1, start2, size2);

        if (size1 > 0)
            std::memcpy(dest, buffer.data() + start1, static_cast<size_t>(size1) * sizeof(float));

        if (size2 > 0)
            std::memcpy(dest + size1, buffer.data() + start2, static_cast<size_t>(size2) * sizeof(float));

        const int totalRead = size1 + size2;
        abstractFifo.finishedRead(totalRead);
        return totalRead;
    }

    [[nodiscard]] int getNumReady() const noexcept
    {
        return abstractFifo.getNumReady();
    }

    [[nodiscard]] int getFreeSpace() const noexcept
    {
        return abstractFifo.getFreeSpace();
    }

    [[nodiscard]] int getTotalSize() const noexcept
    {
        return abstractFifo.getTotalSize();
    }

private:
    juce::AbstractFifo abstractFifo;
    std::vector<float> buffer;
};

} // namespace spectracross

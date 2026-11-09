#pragma once

#include "../Common/SpectraCommon.h"
#include "../Common/SpectralData.h"
#include "LockFreeFIFO.h"
#include "MaskingAnalyzer.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <memory>
#include <vector>

namespace spectracross
{

class SpectrumProcessor : private juce::Thread
{
public:
    SpectrumProcessor();
    ~SpectrumProcessor() override;

    void prepare(double sampleRate, int maxBlockSize);
    void reset();

    /** Pushes incoming audio samples from real-time processBlock().
        Lock-free, zero heap allocation, zero syscalls.
    */
    void pushAudioBlock(const juce::AudioBuffer<float>& buffer, SummationMode mode) noexcept;

    // Parameter configuration (thread-safe atomics)
    void setFFTSize(FFTSize newSize) noexcept;
    void setWindowType(WindowType newType) noexcept;
    void setTiltSlope(TiltSlope newSlope) noexcept;
    void setAttackDecay(float attackMs, float decayMs) noexcept;
    void setPeakHoldTime(HoldTime holdTime) noexcept;
    void setSmrThreshold(float thresholdDb) noexcept;
    void setFrozen(bool isFrozen) noexcept;

    // Snapshot retrieval for UI thread
    std::shared_ptr<const InstanceSpectralFrame> getLatestLocalFrame() const noexcept;

    LockFreeAudioFIFO& getFIFO() noexcept { return audioFifo; }

    const MaskingAnalyzer& getMaskingAnalyzer() const noexcept { return maskingAnalyzer; }

private:
    void run() override;

    void rebuildFFTAndWindows(int fftSize, WindowType winType);
    void processSpectralFrame();
    void applyBallistics(float deltaSeconds);
    void downsampleToLogBins();

    // Audio FIFO
    LockFreeAudioFIFO audioFifo { 65536 };

    // DSP Engine components
    MaskingAnalyzer maskingAnalyzer;
    std::unique_ptr<juce::dsp::FFT> fft;

    // Settings (atomic)
    std::atomic<int> targetFFTSize { 4096 };
    std::atomic<int> activeFFTSize { 4096 };
    std::atomic<int> targetWindowType { static_cast<int>(WindowType::BlackmanHarris7) };
    std::atomic<int> targetTiltSlope { static_cast<int>(TiltSlope::Slope4_5dB) };
    std::atomic<float> attackTimeMs { 10.0f };
    std::atomic<float> decayTimeMs { 800.0f };
    std::atomic<int> targetHoldTime { static_cast<int>(HoldTime::Hold2s) };
    std::atomic<float> smrThresholdDb { 0.0f };
    std::atomic<bool> freezeAnalyzer { false };

    // Runtime buffers (only touched on worker thread)
    double currentSampleRate = 44100.0;
    int currentOrder = 12; // 2^12 = 4096
    int currentHopSize = 1024; // 75% overlap: 4096 / 4 = 1024
    float coherentGain = 1.0f;

    std::vector<float> windowCoeffs;
    std::vector<float> fifoReadBuffer;
    std::vector<float> timeDomainAccumulator;
    std::vector<float> fftWorkBuffer; // Size 2 * N for complex FFT

    std::vector<float> rawMagnitudesDb;
    std::vector<float> smoothedMagnitudesDb;
    std::vector<float> peakHoldDb;
    std::vector<float> peakHoldTimersSec;

    // Double-buffered local frame snapshot for UI thread
    std::shared_ptr<const InstanceSpectralFrame> publishedLocalFrame;

    // Working frame
    InstanceSpectralFrame workingFrame;

    juce::WaitableEvent waitEvent;
};

} // namespace spectracross

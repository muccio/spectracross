#include "SpectrumProcessor.h"
#include <cmath>

namespace spectracross
{

SpectrumProcessor::SpectrumProcessor()
    : juce::Thread("SpectraCross_Worker")
{
    // Start with 4096 points FFT
    rebuildFFTAndWindows(4096, WindowType::BlackmanHarris7);
    startThread(juce::Thread::Priority::normal);
}

SpectrumProcessor::~SpectrumProcessor()
{
    signalThreadShouldExit();
    waitEvent.signal();
    stopThread(3000);
}

void SpectrumProcessor::prepare(double sampleRate, int maxBlockSize)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    juce::ignoreUnused(maxBlockSize);

    audioFifo.prepare(131072); // 128k samples capacity
    maskingAnalyzer.prepare(currentSampleRate, activeFFTSize.load(std::memory_order_relaxed));

    rebuildFFTAndWindows(targetFFTSize.load(std::memory_order_relaxed),
                         static_cast<WindowType>(targetWindowType.load(std::memory_order_relaxed)));
}

void SpectrumProcessor::reset()
{
    audioFifo.reset();
}

void SpectrumProcessor::setFFTSize(FFTSize newSize) noexcept
{
    targetFFTSize.store(static_cast<int>(newSize), std::memory_order_relaxed);
    waitEvent.signal();
}

void SpectrumProcessor::setWindowType(WindowType newType) noexcept
{
    targetWindowType.store(static_cast<int>(newType), std::memory_order_relaxed);
    waitEvent.signal();
}

void SpectrumProcessor::setTiltSlope(TiltSlope newSlope) noexcept
{
    targetTiltSlope.store(static_cast<int>(newSlope), std::memory_order_relaxed);
}

void SpectrumProcessor::setAttackDecay(float attackMs, float decayMs) noexcept
{
    attackTimeMs.store(juce::jlimit(0.0f, 100.0f, attackMs), std::memory_order_relaxed);
    decayTimeMs.store(juce::jlimit(50.0f, 5000.0f, decayMs), std::memory_order_relaxed);
}

void SpectrumProcessor::setPeakHoldTime(HoldTime holdTime) noexcept
{
    targetHoldTime.store(static_cast<int>(holdTime), std::memory_order_relaxed);
}

void SpectrumProcessor::setSmrThreshold(float thresholdDb) noexcept
{
    smrThresholdDb.store(juce::jlimit(-18.0f, 12.0f, thresholdDb), std::memory_order_relaxed);
}

void SpectrumProcessor::setFrozen(bool isFrozen) noexcept
{
    freezeAnalyzer.store(isFrozen, std::memory_order_relaxed);
}

std::shared_ptr<const InstanceSpectralFrame> SpectrumProcessor::getLatestLocalFrame() const noexcept
{
    return publishedLocalFrame.load(std::memory_order_acquire);
}

void SpectrumProcessor::pushAudioBlock(const juce::AudioBuffer<float>& buffer, SummationMode mode) noexcept
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    if (numChannels == 0 || numSamples == 0)
        return;

    constexpr int kChunkSize = 512;
    std::array<float, kChunkSize> chunk {};

    int remaining = numSamples;
    int offset = 0;

    const float* const* channelData = buffer.getArrayOfReadPointers();

    while (remaining > 0)
    {
        const int samplesToProcess = std::min(remaining, kChunkSize);

        switch (mode)
        {
            case SummationMode::Mono:
            {
                if (numChannels == 1)
                {
                    std::memcpy(chunk.data(), channelData[0] + offset, static_cast<size_t>(samplesToProcess) * sizeof(float));
                }
                else
                {
                    const float invCh = 1.0f / static_cast<float>(numChannels);
                    for (int i = 0; i < samplesToProcess; ++i)
                    {
                        float sum = 0.0f;
                        for (int ch = 0; ch < numChannels; ++ch)
                            sum += channelData[ch][offset + i];
                        chunk[static_cast<size_t>(i)] = sum * invCh;
                    }
                }
                break;
            }

            case SummationMode::Stereo:
            case SummationMode::Mid:
            {
                if (numChannels >= 2)
                {
                    const float* l = channelData[0] + offset;
                    const float* r = channelData[1] + offset;
                    for (int i = 0; i < samplesToProcess; ++i)
                        chunk[static_cast<size_t>(i)] = (l[i] + r[i]) * 0.5f;
                }
                else
                {
                    std::memcpy(chunk.data(), channelData[0] + offset, static_cast<size_t>(samplesToProcess) * sizeof(float));
                }
                break;
            }

            case SummationMode::Side:
            {
                if (numChannels >= 2)
                {
                    const float* l = channelData[0] + offset;
                    const float* r = channelData[1] + offset;
                    for (int i = 0; i < samplesToProcess; ++i)
                        chunk[static_cast<size_t>(i)] = (l[i] - r[i]) * 0.5f;
                }
                else
                {
                    std::fill_n(chunk.data(), samplesToProcess, 0.0f);
                }
                break;
            }

            case SummationMode::MaxPeak:
            {
                if (numChannels >= 2)
                {
                    const float* l = channelData[0] + offset;
                    const float* r = channelData[1] + offset;
                    for (int i = 0; i < samplesToProcess; ++i)
                    {
                        const float absL = std::abs(l[i]);
                        const float absR = std::abs(r[i]);
                        chunk[static_cast<size_t>(i)] = (absL >= absR) ? l[i] : r[i];
                    }
                }
                else
                {
                    std::memcpy(chunk.data(), channelData[0] + offset, static_cast<size_t>(samplesToProcess) * sizeof(float));
                }
                break;
            }
        }

        audioFifo.write(chunk.data(), samplesToProcess);
        offset += samplesToProcess;
        remaining -= samplesToProcess;
    }

    waitEvent.signal();
}

void SpectrumProcessor::rebuildFFTAndWindows(int newFFTSize, WindowType winType)
{
    int order = 12; // 4096 default
    if (newFFTSize <= 2048) { newFFTSize = 2048; order = 11; }
    else if (newFFTSize >= 8192) { newFFTSize = 8192; order = 13; }
    else { newFFTSize = 4096; order = 12; }

    currentOrder = order;
    activeFFTSize.store(newFFTSize, std::memory_order_relaxed);
    currentHopSize = newFFTSize / 4; // 75% overlap

    fft = std::make_unique<juce::dsp::FFT>(currentOrder);

    // Resize runtime buffers
    windowCoeffs.resize(static_cast<size_t>(newFFTSize));
    fifoReadBuffer.resize(static_cast<size_t>(currentHopSize));
    timeDomainAccumulator.assign(static_cast<size_t>(newFFTSize), 0.0f);
    fftWorkBuffer.assign(static_cast<size_t>(newFFTSize * 2), 0.0f);

    const int numBins = newFFTSize / 2;
    rawMagnitudesDb.assign(static_cast<size_t>(numBins), -120.0f);
    smoothedMagnitudesDb.assign(static_cast<size_t>(numBins), -120.0f);
    peakHoldDb.assign(static_cast<size_t>(numBins), -120.0f);
    peakHoldTimersSec.assign(static_cast<size_t>(numBins), 0.0f);

    // Window calculation and coherent gain normalization
    double sumWindow = 0.0;
    const double N = static_cast<double>(newFFTSize);
    constexpr double twoPi = 6.28318530717958647692;

    if (winType == WindowType::BlackmanHarris7)
    {
        // 7-term Blackman-Harris coefficients
        constexpr double a0 = 0.27105140069342;
        constexpr double a1 = 0.43329793923448;
        constexpr double a2 = 0.21812299954311;
        constexpr double a3 = 0.06592544638803;
        constexpr double a4 = 0.01081174209837;
        constexpr double a5 = 0.00077658482522;
        constexpr double a6 = 0.00001388721735;

        for (int n = 0; n < newFFTSize; ++n)
        {
            const double phase = twoPi * static_cast<double>(n) / N;
            const double w = a0 - a1 * std::cos(phase)
                                + a2 * std::cos(2.0 * phase)
                                - a3 * std::cos(3.0 * phase)
                                + a4 * std::cos(4.0 * phase)
                                - a5 * std::cos(5.0 * phase)
                                + a6 * std::cos(6.0 * phase);
            windowCoeffs[static_cast<size_t>(n)] = static_cast<float>(w);
            sumWindow += w;
        }
    }
    else
    {
        // Standard Hann window
        for (int n = 0; n < newFFTSize; ++n)
        {
            const double phase = twoPi * static_cast<double>(n) / N;
            const double w = 0.5 - 0.5 * std::cos(phase);
            windowCoeffs[static_cast<size_t>(n)] = static_cast<float>(w);
            sumWindow += w;
        }
    }

    // Coherent gain normalization factor
    coherentGain = static_cast<float>(sumWindow / N);
    if (coherentGain <= 0.00001f) coherentGain = 1.0f;

    maskingAnalyzer.prepare(currentSampleRate, newFFTSize);
}

void SpectrumProcessor::run()
{
    while (!threadShouldExit())
    {
        // Check if FFT configuration has been modified
        const int targetSize = targetFFTSize.load(std::memory_order_relaxed);
        const auto targetWin = static_cast<WindowType>(targetWindowType.load(std::memory_order_relaxed));
        if (targetSize != activeFFTSize.load(std::memory_order_relaxed))
        {
            rebuildFFTAndWindows(targetSize, targetWin);
        }

        bool hasProcessedFrame = false;

        // Drain audio FIFO by hopSize chunks
        while (audioFifo.getNumReady() >= currentHopSize && !threadShouldExit())
        {
            const int readCount = audioFifo.read(fifoReadBuffer.data(), currentHopSize);
            if (readCount == currentHopSize)
            {
                // Shift existing history left by hopSize
                const int remainingSamples = activeFFTSize.load(std::memory_order_relaxed) - currentHopSize;
                if (remainingSamples > 0)
                {
                    std::memmove(timeDomainAccumulator.data(),
                                 timeDomainAccumulator.data() + currentHopSize,
                                 static_cast<size_t>(remainingSamples) * sizeof(float));
                }

                // Append new samples at the end
                std::memcpy(timeDomainAccumulator.data() + remainingSamples,
                            fifoReadBuffer.data(),
                            static_cast<size_t>(currentHopSize) * sizeof(float));

                if (!freezeAnalyzer.load(std::memory_order_relaxed))
                {
                    processSpectralFrame();
                    hasProcessedFrame = true;
                }
            }
        }

        if (hasProcessedFrame)
        {
            downsampleToLogBins();
        }

        // Sleep briefly or wait for new incoming audio (target ~60-120 updates/sec)
        waitEvent.wait(8);
    }
}

void SpectrumProcessor::processSpectralFrame()
{
    const int fftSize = activeFFTSize.load(std::memory_order_relaxed);
    const int numBins = fftSize / 2;

    // Apply window and coherent gain normalization directly into complex work buffer
    const float normGainFactor = 1.0f / (coherentGain * static_cast<float>(fftSize));

    for (int n = 0; n < fftSize; ++n)
    {
        fftWorkBuffer[static_cast<size_t>(n)] = timeDomainAccumulator[static_cast<size_t>(n)] * windowCoeffs[static_cast<size_t>(n)] * normGainFactor;
    }
    // Clear imaginary half for real-to-complex FFT
    std::fill(fftWorkBuffer.begin() + fftSize, fftWorkBuffer.begin() + (fftSize * 2), 0.0f);

    fft->performRealOnlyForwardTransform(fftWorkBuffer.data());

    // Extract magnitudes and compute dB + tilt slope
    const float tiltPerOct = getTiltDbPerOctave(static_cast<TiltSlope>(targetTiltSlope.load(std::memory_order_relaxed)));
    const float binWidthHz = static_cast<float>(currentSampleRate) / static_cast<float>(fftSize);

    for (int k = 0; k < numBins; ++k)
    {
        const float real = fftWorkBuffer[static_cast<size_t>(k * 2)];
        const float imag = fftWorkBuffer[static_cast<size_t>(k * 2 + 1)];

        // Single-sided spectrum factor: DC and Nyquist = 1x, positive bins = 2x
        const float scale = (k == 0) ? 1.0f : 2.0f;
        const float magLinear = std::sqrt(real * real + imag * imag) * scale;

        // dBFS value
        float db = 20.0f * std::log10(std::max(magLinear, 1e-6f));

        // Tilt EQ Slope adjustment relative to 1000 Hz reference
        if (std::abs(tiltPerOct) > 1e-4f)
        {
            const float freq = std::max(10.0f, static_cast<float>(k) * binWidthHz);
            const float octavesFrom1k = std::log2(freq / 1000.0f);
            db += tiltPerOct * octavesFrom1k;
        }

        rawMagnitudesDb[static_cast<size_t>(k)] = db;
    }

    const float deltaSec = static_cast<float>(currentHopSize) / static_cast<float>(currentSampleRate);
    applyBallistics(deltaSec);

    // Pass smoothed spectrum to Bark critical band psychoacoustic analyzer
    maskingAnalyzer.analyzeLocalSpectrum(smoothedMagnitudesDb.data(),
                                         numBins,
                                         workingFrame.barkEnergyDb,
                                         workingFrame.barkExcitationDb);
}

void SpectrumProcessor::applyBallistics(float deltaSeconds)
{
    const int numBins = static_cast<int>(smoothedMagnitudesDb.size());

    const float attackMs = attackTimeMs.load(std::memory_order_relaxed);
    const float decayMs = decayTimeMs.load(std::memory_order_relaxed);

    const float alphaAttack = (attackMs <= 0.1f) ? 1.0f : (1.0f - std::exp(-deltaSeconds / (attackMs * 0.001f)));
    const float alphaDecay  = (decayMs <= 0.1f)  ? 1.0f : (1.0f - std::exp(-deltaSeconds / (decayMs * 0.001f)));

    const auto holdSetting = static_cast<HoldTime>(targetHoldTime.load(std::memory_order_relaxed));
    float holdDurationSec = 0.0f;
    switch (holdSetting)
    {
        case HoldTime::Off:      holdDurationSec = 0.0f;   break;
        case HoldTime::Hold1s:   holdDurationSec = 1.0f;   break;
        case HoldTime::Hold2s:   holdDurationSec = 2.0f;   break;
        case HoldTime::Hold5s:   holdDurationSec = 5.0f;   break;
        case HoldTime::Infinite: holdDurationSec = 1e9f;   break;
    }

    constexpr float peakDecayRateDbPerSec = 25.0f;

    for (int k = 0; k < numBins; ++k)
    {
        const float raw = rawMagnitudesDb[static_cast<size_t>(k)];
        float& smoothed = smoothedMagnitudesDb[static_cast<size_t>(k)];

        // Dynamic smoothing filter
        if (raw >= smoothed)
            smoothed += alphaAttack * (raw - smoothed);
        else
            smoothed += alphaDecay * (raw - smoothed);

        // Peak & Hold buffer
        float& peak = peakHoldDb[static_cast<size_t>(k)];
        float& timer = peakHoldTimersSec[static_cast<size_t>(k)];

        if (holdSetting == HoldTime::Off)
        {
            peak = smoothed;
            timer = 0.0f;
        }
        else
        {
            if (smoothed >= peak)
            {
                peak = smoothed;
                timer = holdDurationSec;
            }
            else if (timer > 0.0f)
            {
                timer -= deltaSeconds;
            }
            else
            {
                peak -= peakDecayRateDbPerSec * deltaSeconds;
                if (peak < smoothed)
                    peak = smoothed;
            }
        }
    }
}

void SpectrumProcessor::downsampleToLogBins()
{
    const int numBins = static_cast<int>(smoothedMagnitudesDb.size());
    const float binWidthHz = static_cast<float>(currentSampleRate) / static_cast<float>(activeFFTSize.load(std::memory_order_relaxed));

    for (int i = 0; i < kNumDisplayBins; ++i)
    {
        const float normX = static_cast<float>(i) / static_cast<float>(kNumDisplayBins - 1);
        const float centerFreq = kMinFreqHz * std::pow(10.0f, normX * kLogRatioTotal);

        // Determine frequency bounds for this display point
        const float leftFreq  = kMinFreqHz * std::pow(10.0f, (normX - 0.5f / kNumDisplayBins) * kLogRatioTotal);
        const float rightFreq = kMinFreqHz * std::pow(10.0f, (normX + 0.5f / kNumDisplayBins) * kLogRatioTotal);

        const float exactBin = centerFreq / binWidthHz;
        const int lowBin = std::max(0, static_cast<int>(leftFreq / binWidthHz));
        const int highBin = std::min(numBins - 1, static_cast<int>(std::ceil(rightFreq / binWidthHz)));

        float magVal = -120.0f;
        float peakVal = -120.0f;

        if (highBin <= lowBin + 1)
        {
            // Low frequencies: interpolate between adjacent bins
            const int b0 = juce::jlimit(0, numBins - 1, static_cast<int>(exactBin));
            const int b1 = juce::jlimit(0, numBins - 1, b0 + 1);
            const float frac = exactBin - static_cast<float>(b0);

            magVal = smoothedMagnitudesDb[static_cast<size_t>(b0)] + frac * (smoothedMagnitudesDb[static_cast<size_t>(b1)] - smoothedMagnitudesDb[static_cast<size_t>(b0)]);
            peakVal = peakHoldDb[static_cast<size_t>(b0)] + frac * (peakHoldDb[static_cast<size_t>(b1)] - peakHoldDb[static_cast<size_t>(b0)]);
        }
        else
        {
            // Higher frequencies: take max peak across covered bins for crisp transients
            for (int b = lowBin; b <= highBin; ++b)
            {
                magVal = std::max(magVal, smoothedMagnitudesDb[static_cast<size_t>(b)]);
                peakVal = std::max(peakVal, peakHoldDb[static_cast<size_t>(b)]);
            }
        }

        workingFrame.magnitudesDb[static_cast<size_t>(i)] = magVal;
        workingFrame.peakHoldDb[static_cast<size_t>(i)] = peakVal;
    }

    workingFrame.lastPublishTimeMs = juce::Time::currentTimeMillis();

    // Publish immutable snapshot via atomic pointer
    auto snapshot = std::make_shared<InstanceSpectralFrame>(workingFrame);
    publishedLocalFrame.store(snapshot, std::memory_order_release);
}

} // namespace spectracross

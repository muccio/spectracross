#include "MaskingAnalyzer.h"
#include <algorithm>
#include <cmath>

namespace spectracross
{

MaskingAnalyzer::MaskingAnalyzer()
{
    initializeBarkBands();
    prepare(44100.0, 4096);
}

void MaskingAnalyzer::initializeBarkBands()
{
    // Compute exact frequency edges for each Bark band using inverse Traunmüller
    for (int b = 0; b <= kNumBarkBands; ++b)
    {
        barkEdgeFreqs[static_cast<size_t>(b)] = barkToFreq(static_cast<float>(b));
    }

    for (int b = 0; b < kNumBarkBands; ++b)
    {
        barkCenterFreqs[static_cast<size_t>(b)] = barkToFreq(static_cast<float>(b) + 0.5f);
    }
}

void MaskingAnalyzer::prepare(double sampleRate, int fftSize)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    currentFFTSize = std::max(512, fftSize);

    const int numBins = currentFFTSize / 2;
    binToBarkBand.resize(static_cast<size_t>(numBins));
    binFrequencies.resize(static_cast<size_t>(numBins));

    const float binWidthHz = static_cast<float>(currentSampleRate) / static_cast<float>(currentFFTSize);

    for (int i = 0; i < numBins; ++i)
    {
        const float freq = static_cast<float>(i) * binWidthHz;
        binFrequencies[static_cast<size_t>(i)] = freq;
        const float bark = freqToBark(freq);
        const int band = juce::jlimit(0, kNumBarkBands - 1, static_cast<int>(bark));
        binToBarkBand[static_cast<size_t>(i)] = band;
    }
}

void MaskingAnalyzer::analyzeLocalSpectrum(const float* magnitudesDb,
                                          int numBins,
                                          std::array<float, kNumBarkBands>& outBarkEnergyDb,
                                          std::array<float, kNumBarkBands>& outBarkExcitationDb) const
{
    if (magnitudesDb == nullptr || numBins <= 0)
    {
        outBarkEnergyDb.fill(-120.0f);
        outBarkExcitationDb.fill(-120.0f);
        return;
    }

    // Step 1: Accumulate acoustic power per Bark band
    std::array<double, kNumBarkBands> bandPowerLinear {};
    bandPowerLinear.fill(1e-12);

    const int count = std::min(numBins, static_cast<int>(binToBarkBand.size()));
    for (int i = 0; i < count; ++i)
    {
        const float db = magnitudesDb[i];
        if (db > -110.0f)
        {
            const int band = binToBarkBand[static_cast<size_t>(i)];
            const double linearPwr = std::pow(10.0, static_cast<double>(db) / 10.0);
            bandPowerLinear[static_cast<size_t>(band)] += linearPwr;
        }
    }

    // Convert back to dB
    for (size_t b = 0; b < kNumBarkBands; ++b)
    {
        outBarkEnergyDb[b] = static_cast<float>(10.0 * std::log10(bandPowerLinear[b]));
    }

    // Step 2: Compute psychoacoustic excitation pattern across critical bands
    // using Traunmüller / Schroeder spreading function:
    // Low-frequency slope = +27 dB/Bark
    // Level-dependent high-frequency slope = -(24 + 0.230 * L_i) dB/Bark
    // Self-masking tone/noise offset = -6.0 dB
    std::array<double, kNumBarkBands> excitationPowerLinear {};
    excitationPowerLinear.fill(1e-12);

    for (int i = 0; i < kNumBarkBands; ++i)
    {
        const float li = outBarkEnergyDb[static_cast<size_t>(i)];
        if (li < -90.0f)
            continue;

        const float selfMaskedLevel = li - 6.0f;
        const float highSlope = -(24.0f + 0.230f * li); // dB per Bark

        for (int j = 0; j < kNumBarkBands; ++j)
        {
            const float deltaZ = static_cast<float>(j - i);
            float spreadDb = 0.0f;

            if (deltaZ <= 0.0f)
            {
                // Low-frequency spread: steep upward slope
                spreadDb = 27.0f * deltaZ;
            }
            else
            {
                // High-frequency spread: level-dependent downward slope
                spreadDb = highSlope * deltaZ;
            }

            const float componentDb = selfMaskedLevel + spreadDb;
            if (componentDb > -100.0f)
            {
                excitationPowerLinear[static_cast<size_t>(j)] +=
                    std::pow(10.0, static_cast<double>(componentDb) / 10.0);
            }
        }
    }

    for (size_t b = 0; b < kNumBarkBands; ++b)
    {
        outBarkExcitationDb[b] = static_cast<float>(10.0 * std::log10(excitationPowerLinear[b]));
    }
}

void MaskingAnalyzer::computeInterTrackCollisions(const std::array<float, kNumBarkBands>& localBarkEnergy,
                                                 const std::array<float, kNumBarkBands>& remoteBarkExcitation,
                                                 float smrThresholdDb,
                                                 std::array<float, kNumDisplayBins>& outCollisionMask512,
                                                 float& outCollisionPercent) const
{
    std::array<float, kNumBarkBands> barkCollisionIntensity {};
    barkCollisionIntensity.fill(0.0f);

    float totalCollisionScore = 0.0f;
    float activeBandsCount = 0.0f;

    for (size_t b = 0; b < kNumBarkBands; ++b)
    {
        const float localEnergy = localBarkEnergy[b];
        const float remoteExcitation = remoteBarkExcitation[b];

        // Masking is significant when both local signal and remote masker are audible
        if (localEnergy > -75.0f && remoteExcitation > -75.0f)
        {
            activeBandsCount += 1.0f;
            const float smr = localEnergy - remoteExcitation;

            if (smr < smrThresholdDb)
            {
                const float violationDb = smrThresholdDb - smr;
                // Normalize violation: 0 to 18 dB mapped to [0.0 .. 1.0]
                const float intensity = juce::jlimit(0.0f, 1.0f, violationDb / 18.0f);
                barkCollisionIntensity[b] = intensity;
                totalCollisionScore += intensity;
            }
        }
    }

    if (activeBandsCount > 0.0f)
    {
        outCollisionPercent = juce::jlimit(0.0f, 100.0f, (totalCollisionScore / activeBandsCount) * 100.0f);
    }
    else
    {
        outCollisionPercent = 0.0f;
    }

    // Map 24 Bark band collision intensities into 512 log-spaced display bins
    for (int i = 0; i < kNumDisplayBins; ++i)
    {
        const float normX = static_cast<float>(i) / static_cast<float>(kNumDisplayBins - 1);
        const float freq = kMinFreqHz * std::pow(10.0f, normX * kLogRatioTotal);
        const float bark = freqToBark(freq);

        const int b0 = juce::jlimit(0, kNumBarkBands - 1, static_cast<int>(bark));
        const int b1 = juce::jlimit(0, kNumBarkBands - 1, b0 + 1);
        const float frac = bark - static_cast<float>(b0);

        const float val0 = barkCollisionIntensity[static_cast<size_t>(b0)];
        const float val1 = barkCollisionIntensity[static_cast<size_t>(b1)];
        outCollisionMask512[static_cast<size_t>(i)] = val0 + frac * (val1 - val0);
    }
}

void MaskingAnalyzer::mapBarkToDisplayBins(const std::array<float, kNumBarkBands>& barkData,
                                          std::array<float, kNumDisplayBins>& outDisplayBins) noexcept
{
    for (int i = 0; i < kNumDisplayBins; ++i)
    {
        const float normX = static_cast<float>(i) / static_cast<float>(kNumDisplayBins - 1);
        const float freq = kMinFreqHz * std::pow(10.0f, normX * kLogRatioTotal);
        const float bark = freqToBark(freq);

        const int b0 = juce::jlimit(0, kNumBarkBands - 1, static_cast<int>(bark));
        const int b1 = juce::jlimit(0, kNumBarkBands - 1, b0 + 1);
        const float frac = bark - static_cast<float>(b0);

        const float val0 = barkData[static_cast<size_t>(b0)];
        const float val1 = barkData[static_cast<size_t>(b1)];
        outDisplayBins[static_cast<size_t>(i)] = val0 + frac * (val1 - val0);
    }
}

} // namespace spectracross

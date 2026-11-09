#pragma once

#include "../Common/SpectraCommon.h"
#include <array>
#include <vector>

namespace spectracross
{

class MaskingAnalyzer
{
public:
    MaskingAnalyzer();
    ~MaskingAnalyzer() = default;

    /** Precomputes Bark band mappings for a given sample rate and FFT size. */
    void prepare(double sampleRate, int fftSize);

    /** Computes Bark critical band energies from linear FFT magnitudes (or dB magnitudes).
        @param magnitudesDb Array of FFT bin magnitudes in dB (size: fftSize / 2)
        @param outBarkEnergyDb Output array of 24 Bark critical band energies in dB
        @param outBarkExcitationDb Output array of 24 Bark excitation pattern levels in dB
    */
    void analyzeLocalSpectrum(const float* magnitudesDb,
                              int numBins,
                              std::array<float, kNumBarkBands>& outBarkEnergyDb,
                              std::array<float, kNumBarkBands>& outBarkExcitationDb) const;

    /** Computes Signal-to-Mask Ratio (SMR) and collision profile against an external track.
        @param localBarkEnergy Local track critical band energies
        @param remoteBarkExcitation External track excitation pattern M(z)
        @param smrThresholdDb User-adjustable SMR threshold (e.g. -6 dB to +3 dB)
        @param outCollisionMask512 Output 512 log-spaced collision intensity values [0.0f .. 1.0f]
        @param outCollisionPercent Output overall cumulative collision percentage [0% .. 100%]
    */
    void computeInterTrackCollisions(const std::array<float, kNumBarkBands>& localBarkEnergy,
                                     const std::array<float, kNumBarkBands>& remoteBarkExcitation,
                                     float smrThresholdDb,
                                     std::array<float, kNumDisplayBins>& outCollisionMask512,
                                     float& outCollisionPercent) const;

    /** Maps a Bark band excitation pattern to 512 display bins for visualization. */
    static void mapBarkToDisplayBins(const std::array<float, kNumBarkBands>& barkData,
                                     std::array<float, kNumDisplayBins>& outDisplayBins) noexcept;

private:
    double currentSampleRate = 44100.0;
    int currentFFTSize = 4096;

    // Precomputed bin-to-Bark mapping table
    std::vector<int> binToBarkBand;
    std::vector<float> binFrequencies;

    // Bark band center and edge frequencies
    std::array<float, kNumBarkBands + 1> barkEdgeFreqs {};
    std::array<float, kNumBarkBands> barkCenterFreqs {};

    void initializeBarkBands();
};

} // namespace spectracross

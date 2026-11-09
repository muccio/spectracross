#include "../Source/Common/SpectraCommon.h"
#include "../Source/Common/SpectralData.h"
#include "../Source/DSP/LockFreeFIFO.h"
#include "../Source/DSP/MaskingAnalyzer.h"
#include "../Source/Hub/InstanceHub.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

using namespace spectracross;

void testFIFO()
{
    std::cout << "[TEST] Running LockFreeAudioFIFO test..." << std::endl;
    LockFreeAudioFIFO fifo(1024);

    std::vector<float> input(600);
    for (size_t i = 0; i < input.size(); ++i)
        input[i] = static_cast<float>(i + 1);

    int written = fifo.write(input.data(), 600);
    assert(written == 600);
    assert(fifo.getNumReady() == 600);

    std::vector<float> output1(400);
    int read1 = fifo.read(output1.data(), 400);
    assert(read1 == 400);
    assert(fifo.getNumReady() == 200);

    for (int i = 0; i < 400; ++i)
        assert(std::abs(output1[static_cast<size_t>(i)] - static_cast<float>(i + 1)) < 1e-5f);

    // Write another 500 samples to test wrap-around
    std::vector<float> input2(500);
    for (size_t i = 0; i < input2.size(); ++i)
        input2[i] = static_cast<float>(1000 + i);

    int written2 = fifo.write(input2.data(), 500);
    assert(written2 == 500);
    assert(fifo.getNumReady() == 700);

    std::vector<float> output2(700);
    int read2 = fifo.read(output2.data(), 700);
    assert(read2 == 700);
    assert(fifo.getNumReady() == 0);

    // First 200 should be 401..600
    for (int i = 0; i < 200; ++i)
        assert(std::abs(output2[static_cast<size_t>(i)] - static_cast<float>(401 + i)) < 1e-5f);
    // Next 500 should be 1000..1499
    for (int i = 0; i < 500; ++i)
        assert(std::abs(output2[static_cast<size_t>(200 + i)] - static_cast<float>(1000 + i)) < 1e-5f);

    std::cout << "  -> LockFreeAudioFIFO passed!" << std::endl;
}

void testBarkConversion()
{
    std::cout << "[TEST] Running Bark Conversion test..." << std::endl;

    // Traunmüller formula test:
    // At 1000 Hz: z = 26.81 * (1000 / 2960) - 0.53 = 26.81 * 0.337838 - 0.53 = 9.057 - 0.53 = 8.527 Bark
    const float bark1k = freqToBark(1000.0f);
    assert(std::abs(bark1k - 8.527f) < 0.05f);

    // Round-trip test from 50 Hz to 18 kHz
    for (float f = 50.0f; f <= 18000.0f; f *= 1.3f)
    {
        const float b = freqToBark(f);
        const float fRoundTrip = barkToFreq(b);
        const float errorRatio = std::abs(f - fRoundTrip) / f;
        assert(errorRatio < 0.01f);
    }

    std::cout << "  -> Bark conversion & round-trip passed!" << std::endl;
}

void testMaskingAnalyzer()
{
    std::cout << "[TEST] Running MaskingAnalyzer psychoacoustic test..." << std::endl;
    MaskingAnalyzer analyzer;
    analyzer.prepare(44100.0, 4096);

    const int numBins = 2048;
    std::vector<float> spectrum(numBins, -120.0f);

    // Put a strong 1000 Hz tone at 0 dBFS
    const float binWidth = 44100.0f / 4096.0f;
    const int toneBin = static_cast<int>(std::round(1000.0f / binWidth));
    spectrum[static_cast<size_t>(toneBin)] = 0.0f;

    std::array<float, kNumBarkBands> barkEnergy {};
    std::array<float, kNumBarkBands> barkExcitation {};

    analyzer.analyzeLocalSpectrum(spectrum.data(), numBins, barkEnergy, barkExcitation);

    // The tone is at ~8.5 Bark (band 8)
    assert(barkEnergy[8] > -3.0f);
    assert(barkExcitation[8] > -10.0f); // Self-masked level (~ -6 dB)

    // Verify upward spreading slope (+27 dB/Bark below masker, so lower bands drop sharply)
    assert(barkExcitation[7] < barkExcitation[8]);
    assert(barkExcitation[6] < barkExcitation[7]);

    // Verify high frequency spreading (downward slope)
    assert(barkExcitation[9] < barkExcitation[8]);
    assert(barkExcitation[10] < barkExcitation[9]);

    // Now test inter-track collision:
    // Create remote track with loud masker at 1000 Hz, local track with weak signal at 1000 Hz
    std::array<float, kNumBarkBands> weakLocalEnergy {};
    weakLocalEnergy.fill(-120.0f);
    weakLocalEnergy[8] = -25.0f; // Weak local signal

    std::array<float, kNumDisplayBins> collisionMask {};
    float collisionPct = 0.0f;

    analyzer.computeInterTrackCollisions(weakLocalEnergy, barkExcitation, 0.0f, collisionMask, collisionPct);

    // SMR is -25 - (-6) = -19 dB, well below 0 dB threshold -> collision should be flagged!
    assert(collisionPct > 0.0f);

    std::cout << "  -> MaskingAnalyzer SMR collision detected correctly (Collision: "
              << collisionPct << "%)!" << std::endl;
}

void testInstanceHub()
{
    std::cout << "[TEST] Running InstanceHub central registry test..." << std::endl;
    auto& hub = InstanceHub::getInstance();

    juce::Uuid id1;
    juce::Uuid id2;

    hub.registerInstance(id1, "Kick Drum", juce::Colour(0xFFFF5533), 1);
    hub.registerInstance(id2, "Bass Synth", juce::Colour(0xFF3399FF), 2);

    auto snap = hub.getSnapshot();
    assert(snap != nullptr);
    assert(snap->size() >= 2);

    auto frame1 = hub.getInstanceFrame(id1);
    assert(frame1 != nullptr);
    assert(frame1->trackName == "Kick Drum");

    auto frame2 = hub.getInstanceFrame(id2);
    assert(frame2 != nullptr);
    assert(frame2->trackName == "Bass Synth");

    hub.unregisterInstance(id1);
    auto snapAfter = hub.getSnapshot();
    assert(hub.getInstanceFrame(id1) == nullptr);

    hub.unregisterInstance(id2);
    std::cout << "  -> InstanceHub RCU registry passed!" << std::endl;
}

void testMIDIAndMath()
{
    std::cout << "[TEST] Running Musical Note / MIDI conversion test..." << std::endl;

    int midiNote = 0;
    int cents = 0;
    juce::String noteName;

    // A4 = 440 Hz -> MIDI 69, 0 cents, "A4"
    freqToMidiInfo(440.0f, midiNote, cents, noteName);
    assert(midiNote == 69);
    assert(cents == 0);
    assert(noteName == "A4");

    // Middle C = ~261.63 Hz -> MIDI 60, "C4"
    freqToMidiInfo(261.63f, midiNote, cents, noteName);
    assert(midiNote == 60);
    assert(noteName == "C4");

    // Coordinate conversion test: 10 Hz should map to x = 0, 30 kHz to x = width
    const float w = 1000.0f;
    assert(std::abs(freqToLogX(10.0f, w) - 0.0f) < 0.01f);
    assert(std::abs(freqToLogX(30000.0f, w) - w) < 0.01f);

    // Round-trip x -> freq -> x
    for (float x = 0.0f; x <= 1000.0f; x += 50.0f)
    {
        float f = logXToFreq(x, w);
        float xRoundTrip = freqToLogX(f, w);
        assert(std::abs(x - xRoundTrip) < 0.1f);
    }

    std::cout << "  -> MIDI conversion & coordinate mapping passed!" << std::endl;
}

int main()
{
    std::cout << "========================================" << std::endl;
    std::cout << " SpectraCross DSP & Engine Verification " << std::endl;
    std::cout << "========================================" << std::endl;

    testFIFO();
    testBarkConversion();
    testMaskingAnalyzer();
    testInstanceHub();
    testMIDIAndMath();

    std::cout << "========================================" << std::endl;
    std::cout << " ALL SPECTACROSS ENGINE TESTS PASSED!   " << std::endl;
    std::cout << "========================================" << std::endl;
    return 0;
}

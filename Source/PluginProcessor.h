#pragma once

#include "Common/SpectraCommon.h"
#include "Common/SpectralData.h"
#include "DSP/SpectrumProcessor.h"
#include "Host/HostMetadataFetcher.h"
#include "Hub/InstanceHub.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace spectracross
{

class SpectraCrossAudioProcessor : public juce::AudioProcessor
{
public:
    SpectraCrossAudioProcessor();
    ~SpectraCrossAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    using AudioProcessor::processBlock;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    //==============================================================================
    const juce::String getName() const override { return "SpectraCross"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    //==============================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    //==============================================================================
    void updateTrackProperties(const TrackProperties& properties) override;

    //==============================================================================
    const juce::Uuid& getInstanceId() const noexcept { return instanceId; }
    SpectrumProcessor& getSpectrumProcessor() noexcept { return spectrumProcessor; }
    HostMetadataFetcher& getMetadataFetcher() noexcept { return metadataFetcher; }
    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }

    SummationMode getSummationMode() const noexcept { return currentSummationMode.load(std::memory_order_relaxed); }
    void setSummationMode(SummationMode mode) noexcept;

private:
    juce::Uuid instanceId;

    SpectrumProcessor spectrumProcessor;
    HostMetadataFetcher metadataFetcher;

    juce::AudioProcessorValueTreeState apvts;
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    std::atomic<SummationMode> currentSummationMode { SummationMode::Stereo };

    void syncParametersToDSP();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectraCrossAudioProcessor)
};

} // namespace spectracross

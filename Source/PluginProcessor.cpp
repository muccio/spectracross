#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace spectracross
{

SpectraCrossAudioProcessor::SpectraCrossAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    spectrumProcessor.setInstanceId(instanceId);
    spectrumProcessor.setTrackMetadata(
        metadataFetcher.getEffectiveTrackName(),
        metadataFetcher.getEffectiveTrackColour(),
        metadataFetcher.getTrackIndex()
    );

    // Register with in-process central hub
    InstanceHub::getInstance().registerInstance(
        instanceId,
        metadataFetcher.getEffectiveTrackName(),
        metadataFetcher.getEffectiveTrackColour(),
        metadataFetcher.getTrackIndex()
    );
}

SpectraCrossAudioProcessor::~SpectraCrossAudioProcessor()
{
    InstanceHub::getInstance().unregisterInstance(instanceId);
}

juce::AudioProcessorValueTreeState::ParameterLayout SpectraCrossAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "summationMode", "Summation Mode",
        juce::StringArray { "Stereo", "Mono", "Mid", "Side", "Max-Peak" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "fftSize", "FFT Size",
        juce::StringArray { "2048", "4096", "8192" }, 1));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "windowType", "Window Type",
        juce::StringArray { "Blackman-Harris 7T", "Hann" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "tiltSlope", "Tilt Slope",
        juce::StringArray { "Flat", "+3.0 dB/oct", "+4.5 dB/oct", "+6.0 dB/oct" }, 2));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "dbRange", "dB Range",
        juce::StringArray { "48 dB", "72 dB", "96 dB" }, 1));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "attackMs", "Attack Time",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.5f, 0.5f), 10.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "decayMs", "Decay Time",
        juce::NormalisableRange<float>(50.0f, 3000.0f, 1.0f, 0.5f), 800.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "holdTime", "Peak Hold Time",
        juce::StringArray { "Off", "1s", "2s", "5s", "Infinite" }, 2));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "smrThreshold", "SMR Collision Threshold",
        juce::NormalisableRange<float>(-12.0f, 6.0f, 0.5f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    return { params.begin(), params.end() };
}

void SpectraCrossAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    spectrumProcessor.prepare(sampleRate, samplesPerBlock);
    spectrumProcessor.setInstanceId(instanceId);
    spectrumProcessor.setTrackMetadata(
        metadataFetcher.getEffectiveTrackName(),
        metadataFetcher.getEffectiveTrackColour(),
        metadataFetcher.getTrackIndex()
    );
    metadataFetcher.probeHostContext(*this);

    InstanceHub::getInstance().updateInstanceMetadata(
        instanceId,
        metadataFetcher.getEffectiveTrackName(),
        metadataFetcher.getEffectiveTrackColour(),
        metadataFetcher.getTrackIndex(),
        false
    );

    syncParametersToDSP();
}

void SpectraCrossAudioProcessor::releaseResources()
{
    spectrumProcessor.reset();
}

void SpectraCrossAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;

    // Real-Time Audio Process Thread:
    // Zero heap allocations, zero mutexes, zero syscalls.
    spectrumProcessor.pushAudioBlock(buffer, currentSummationMode.load(std::memory_order_relaxed));
}

void SpectraCrossAudioProcessor::setSummationMode(SummationMode mode) noexcept
{
    currentSummationMode.store(mode, std::memory_order_relaxed);
    if (auto* param = apvts.getRawParameterValue("summationMode"))
        *param = static_cast<float>(mode);
}

void SpectraCrossAudioProcessor::syncParametersToDSP()
{
    if (auto* p = apvts.getRawParameterValue("fftSize"))
    {
        const int idx = static_cast<int>(p->load());
        const FFTSize sizes[] = { FFTSize::Size2048, FFTSize::Size4096, FFTSize::Size8192 };
        spectrumProcessor.setFFTSize(sizes[juce::jlimit(0, 2, idx)]);
    }

    if (auto* p = apvts.getRawParameterValue("windowType"))
    {
        const int idx = static_cast<int>(p->load());
        spectrumProcessor.setWindowType(idx == 0 ? WindowType::BlackmanHarris7 : WindowType::Hann);
    }

    if (auto* p = apvts.getRawParameterValue("tiltSlope"))
    {
        const int idx = static_cast<int>(p->load());
        const TiltSlope slopes[] = { TiltSlope::Flat, TiltSlope::Slope3dB, TiltSlope::Slope4_5dB, TiltSlope::Slope6dB };
        spectrumProcessor.setTiltSlope(slopes[juce::jlimit(0, 3, idx)]);
    }

    float att = 10.0f;
    float dec = 800.0f;
    if (auto* p = apvts.getRawParameterValue("attackMs")) att = p->load();
    if (auto* p = apvts.getRawParameterValue("decayMs")) dec = p->load();
    spectrumProcessor.setAttackDecay(att, dec);

    if (auto* p = apvts.getRawParameterValue("holdTime"))
    {
        const int idx = static_cast<int>(p->load());
        const HoldTime holds[] = { HoldTime::Off, HoldTime::Hold1s, HoldTime::Hold2s, HoldTime::Hold5s, HoldTime::Infinite };
        spectrumProcessor.setPeakHoldTime(holds[juce::jlimit(0, 4, idx)]);
    }

    if (auto* p = apvts.getRawParameterValue("smrThreshold"))
    {
        spectrumProcessor.setSmrThreshold(p->load());
    }

    if (auto* p = apvts.getRawParameterValue("summationMode"))
    {
        const int idx = static_cast<int>(p->load());
        const SummationMode modes[] = { SummationMode::Stereo, SummationMode::Mono, SummationMode::Mid, SummationMode::Side, SummationMode::MaxPeak };
        currentSummationMode.store(modes[juce::jlimit(0, 4, idx)], std::memory_order_relaxed);
    }
}

void SpectraCrossAudioProcessor::updateTrackProperties(const TrackProperties& properties)
{
    metadataFetcher.updateFromTrackProperties(properties);
    spectrumProcessor.setTrackMetadata(
        metadataFetcher.getEffectiveTrackName(),
        metadataFetcher.getEffectiveTrackColour(),
        metadataFetcher.getTrackIndex()
    );

    InstanceHub::getInstance().updateInstanceMetadata(
        instanceId,
        metadataFetcher.getEffectiveTrackName(),
        metadataFetcher.getEffectiveTrackColour(),
        metadataFetcher.getTrackIndex(),
        false
    );
}

void SpectraCrossAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    auto metaTree = metadataFetcher.exportToValueTree();
    state.appendChild(metaTree, nullptr);

    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void SpectraCrossAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
    {
        auto state = juce::ValueTree::fromXml(*xmlState);
        if (state.isValid())
        {
            auto metaTree = state.getChildWithName("TrackMetadataOverrides");
            if (metaTree.isValid())
                metadataFetcher.importFromValueTree(metaTree);

            apvts.replaceState(state);
            syncParametersToDSP();

            spectrumProcessor.setTrackMetadata(
                metadataFetcher.getEffectiveTrackName(),
                metadataFetcher.getEffectiveTrackColour(),
                metadataFetcher.getTrackIndex()
            );

            InstanceHub::getInstance().updateInstanceMetadata(
                instanceId,
                metadataFetcher.getEffectiveTrackName(),
                metadataFetcher.getEffectiveTrackColour(),
                metadataFetcher.getTrackIndex(),
                false
            );
        }
    }
}

juce::AudioProcessorEditor* SpectraCrossAudioProcessor::createEditor()
{
    return new SpectraCrossAudioProcessorEditor(*this);
}

} // namespace spectracross

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new spectracross::SpectraCrossAudioProcessor();
}

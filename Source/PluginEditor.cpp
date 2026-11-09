#include "PluginEditor.h"

namespace spectracross
{

SpectraCrossAudioProcessorEditor::SpectraCrossAudioProcessorEditor(SpectraCrossAudioProcessor& p)
    : AudioProcessorEditor(&p),
      processorRef(p),
      spectrumView(p.getSpectrumProcessor(), p.getInstanceId()),
      instanceSidebar(p.getInstanceId())
{
    // Configure components
    topBar.setTrackName(processorRef.getMetadataFetcher().getEffectiveTrackName());
    topBar.setTrackColour(processorRef.getMetadataFetcher().getEffectiveTrackColour());
    spectrumView.setLocalTrackColour(processorRef.getMetadataFetcher().getEffectiveTrackColour());
    colorPickerModal.setCurrentColour(processorRef.getMetadataFetcher().getEffectiveTrackColour());

    addAndMakeVisible(topBar);
    addAndMakeVisible(spectrumView);
    addAndMakeVisible(pianoRoll);
    addChildComponent(instanceSidebar);
    addChildComponent(colorPickerModal);

    setupCallbacks();

    setResizable(true, true);
    setResizeLimits(800, 480, 1920, 1200);
    setSize(1040, 620);

    startTimerHz(10); // 10 Hz refresh for metadata and instance count sync
}

SpectraCrossAudioProcessorEditor::~SpectraCrossAudioProcessorEditor()
{
    stopTimer();
}

void SpectraCrossAudioProcessorEditor::setupCallbacks()
{
    // Color Swatch and Name overrides
    topBar.onColourSwatchClicked = [this]() {
        showColorPickerModal(!isColorPickerVisible);
    };

    topBar.onTrackNameChanged = [this](const juce::String& newName) {
        processorRef.getMetadataFetcher().setCustomTrackName(newName);
        InstanceHub::getInstance().updateInstanceMetadata(
            processorRef.getInstanceId(),
            processorRef.getMetadataFetcher().getEffectiveTrackName(),
            processorRef.getMetadataFetcher().getEffectiveTrackColour(),
            processorRef.getMetadataFetcher().getTrackIndex(),
            false
        );
    };

    colorPickerModal.onColourChanged = [this](juce::Colour col) {
        processorRef.getMetadataFetcher().setCustomTrackColour(col);
        topBar.setTrackColour(col);
        spectrumView.setLocalTrackColour(col);
        InstanceHub::getInstance().updateInstanceMetadata(
            processorRef.getInstanceId(),
            processorRef.getMetadataFetcher().getEffectiveTrackName(),
            col,
            processorRef.getMetadataFetcher().getTrackIndex(),
            false
        );
    };

    colorPickerModal.onCloseRequested = [this]() {
        showColorPickerModal(false);
    };

    // Ribbon DSP controls
    topBar.onSummationModeChanged = [this](SummationMode mode) {
        processorRef.setSummationMode(mode);
    };

    topBar.onFFTSizeChanged = [this](FFTSize size) {
        processorRef.getSpectrumProcessor().setFFTSize(size);
        if (auto* p = processorRef.getAPVTS().getRawParameterValue("fftSize"))
        {
            int idx = 1;
            if (size == FFTSize::Size2048) idx = 0;
            else if (size == FFTSize::Size8192) idx = 2;
            *p = static_cast<float>(idx);
        }
    };

    topBar.onWindowTypeChanged = [this](WindowType win) {
        processorRef.getSpectrumProcessor().setWindowType(win);
        if (auto* p = processorRef.getAPVTS().getRawParameterValue("windowType"))
            *p = static_cast<float>(win == WindowType::BlackmanHarris7 ? 0 : 1);
    };

    topBar.onTiltSlopeChanged = [this](TiltSlope slope) {
        processorRef.getSpectrumProcessor().setTiltSlope(slope);
        if (auto* p = processorRef.getAPVTS().getRawParameterValue("tiltSlope"))
            *p = static_cast<float>(slope);
    };

    topBar.onDBRangeChanged = [this](DBRange range) {
        spectrumView.setDBRange(range);
        if (auto* p = processorRef.getAPVTS().getRawParameterValue("dbRange"))
            *p = static_cast<float>(range);
    };

    topBar.onHoldTimeChanged = [this](HoldTime hold) {
        processorRef.getSpectrumProcessor().setPeakHoldTime(hold);
        if (auto* p = processorRef.getAPVTS().getRawParameterValue("holdTime"))
            *p = static_cast<float>(hold);
    };

    topBar.onReferenceBandsToggled = [this](bool visible) {
        spectrumView.setReferenceBandsVisible(visible);
    };

    topBar.onPianoRollToggled = [this](bool visible) {
        pianoRoll.setVisible(visible);
        resized();
    };

    topBar.onInstanceListToggled = [this](bool show) {
        isSidebarVisible = show;
        instanceSidebar.setVisible(show);
        resized();
    };

    topBar.onFreezeToggled = [this](bool frozen) {
        processorRef.getSpectrumProcessor().setFrozen(frozen);
    };

    // Crosshair hover synchronization with Piano Roll
    spectrumView.onHoveredMidiNoteChanged = [this](int note) {
        pianoRoll.setHoveredMidiNote(note);
    };

    // SMR threshold synchronization
    instanceSidebar.onSMRThresholdChanged = [this](float thresh) {
        processorRef.getSpectrumProcessor().setSmrThreshold(thresh);
        if (auto* p = processorRef.getAPVTS().getRawParameterValue("smrThreshold"))
            *p = thresh;
    };
}

void SpectraCrossAudioProcessorEditor::showColorPickerModal(bool show)
{
    isColorPickerVisible = show;
    colorPickerModal.setVisible(show);
    if (show)
    {
        colorPickerModal.toFront(true);
        colorPickerModal.setBounds(10, 42, 260, 280);
    }
}

void SpectraCrossAudioProcessorEditor::timerCallback()
{
    // Keep instance count badge updated
    const int count = InstanceHub::getInstance().getNumInstances();
    topBar.updateInstanceCountBadge(count);

    // Sync DAW track metadata if host updated it dynamically
    const auto name = processorRef.getMetadataFetcher().getEffectiveTrackName();
    const auto col = processorRef.getMetadataFetcher().getEffectiveTrackColour();
    topBar.setTrackName(name);
    topBar.setTrackColour(col);
    spectrumView.setLocalTrackColour(col);
}

void SpectraCrossAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF0E1117));
}

void SpectraCrossAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    constexpr int topBarHeight = 40;
    topBar.setBounds(bounds.removeFromTop(topBarHeight));

    // Sidebar on the right
    if (isSidebarVisible)
    {
        const int sidebarWidth = std::min(280, bounds.getWidth() / 3);
        instanceSidebar.setBounds(bounds.removeFromRight(sidebarWidth));
    }

    // Mini Piano Roll along the bottom of the analyzer
    if (pianoRoll.isVisible())
    {
        constexpr int pianoHeight = 24;
        pianoRoll.setBounds(bounds.removeFromBottom(pianoHeight));
    }

    // Full remaining area for main spectrum display
    spectrumView.setBounds(bounds);

    if (isColorPickerVisible)
    {
        colorPickerModal.setBounds(10, 42, 260, 280);
    }
}

} // namespace spectracross

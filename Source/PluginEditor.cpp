/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "GitVersion.h"
#include "FactoryPresets.h"

//Setup Slider in Constructor Here

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), oscilloscope(p), gainReductionMeter(p), phaseCorrelationMeter(p)
{
    // paint() fills the full bounds with black before drawing — tell JUCE so it
    // skips the pre-paint clear. On X11 this removes a brief flash of the parent
    // window's background that can appear between size-change and child repaints
    // during the fold animation.
    setOpaque (true);

    // Font scale factor (672/960 = 0.7)
    const float fs = 672.0f / 960.0f;

    addAndMakeVisible(oscilloscope);

    // XY Morph Pad overlay (invisible, on top of oscilloscope)
    addAndMakeVisible(xyMorphPad);
    xyMorphPad.onPositionChanged = [this](float x, float y)
    {
        morphDistortionParameters(x, y);
    };

    // Free-draw Graphic EQ overlay — shares the scope slot with the XY pad; the
    // Settings "Overlay" selector decides which one is interactive. Reads/writes
    // the eqBand0..N APVTS params so the curve automates and saves with presets.
    addChildComponent(graphicEqOverlay);
    graphicEqOverlay.getBandGainDb = [this](int i) -> float
    {
        if (auto* p = audioProcessor.parameters.getParameter("eqBand" + juce::String(i)))
            return p->convertFrom0to1(p->getValue());
        return 0.0f;
    };
    graphicEqOverlay.onBandChanged = [this](int i, float db)
    {
        if (auto* p = audioProcessor.parameters.getParameter("eqBand" + juce::String(i)))
            p->setValueNotifyingHost(p->convertTo0to1(db));
    };

    addAndMakeVisible(gainReductionMeter);
    addAndMakeVisible(phaseCorrelationMeter);

    // Setup logo title
    addAndMakeVisible(logoTitle);

    // Setup version label (bottom left corner)
    addAndMakeVisible(versionLabel);
    versionLabel.setText(GIT_VERSION_STRING, juce::dontSendNotification);
    versionLabel.setFont(juce::Font(10.0f * fs));
    versionLabel.setColour(juce::Label::textColourId, juce::Colour(0x88, 0x88, 0x88));  // Gray text
    versionLabel.setJustificationType(juce::Justification::left);

    // Note: setSize moved to end of constructor so all components exist when resized() fires
    setResizable(false, false);

    // Load background image from Resources folder
    backgroundImage = juce::ImageCache::getFromMemory(
        BinaryData::background_png,
        BinaryData::background_pngSize
    );


    setupSlider(inputGainSlider, inputGainLabel, "Input Gain",
        inputGainAttachment, "inputGain");
    setupSlider(distortionAmountSlider, distortionAmountLabel, "Distortion Amount",
        distortionAmountAttachment, "distortionAmount");
    setupSlider(distMixSlider, distMixLabel, "Dist Mix",
        distMixAttachment, "distMix");
    setupSlider(toneSlider, toneLabel, "Tone",
        toneAttachment, "tone");
    setupSlider(outputGainSlider, outputGainLabel, "Output Gain",
        outputGainAttachment, "outputGain");
    setupSlider(highPassFreqSlider, highPassFreqLabel, "Hi-Pass Filter",
        highPassFreqAttachment, "highPassFreq");
    // Filter mode dropdown takes the knob's label slot, so hide the text label.
    highPassFreqLabel.setVisible(false);
    addAndMakeVisible(filterModeComboBox);
    filterModeComboBox.setLookAndFeel(&comboBoxLookAndFeel);
    filterModeComboBox.addItem("High Pass", 1);
    filterModeComboBox.addItem("Low Pass", 2);
    filterModeComboBox.addItem("Band Pass", 3);
    filterModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.parameters, "filterMode", filterModeComboBox);
    setupSlider(lfoRateSlider, lfoRateLabel, "LFO Rate",
        lfoRateAttachment, "lfoRate");
    setupSlider(lfoDepthSlider, lfoDepthLabel, "LFO Depth",
        lfoDepthAttachment, "lfoDepth");

    // Setup LFO waveform selector
    addAndMakeVisible(lfoWaveformComboBox);
    lfoWaveformComboBox.setLookAndFeel(&comboBoxLookAndFeel);  // Apply neon red styling
    lfoWaveformComboBox.addItem("Sine", 1);
    lfoWaveformComboBox.addItem("Triangle", 2);
    lfoWaveformComboBox.addItem("Square", 3);
    lfoWaveformComboBox.addItem("Saw", 4);
    lfoWaveformComboBox.addItem("Random", 5);

    lfoWaveformAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.parameters, "lfoWaveform", lfoWaveformComboBox);

    addAndMakeVisible(lfoWaveformLabel);
    lfoWaveformLabel.setText("Wave", juce::dontSendNotification);
    lfoWaveformLabel.setJustificationType(juce::Justification::centred);
    lfoWaveformLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    lfoWaveformLabel.setFont(juce::Font(10.0f * fs, juce::Font::bold));

    // LFO Destination Selector
    addAndMakeVisible(lfoDestinationComboBox);
    lfoDestinationComboBox.setLookAndFeel(&comboBoxLookAndFeel);
    lfoDestinationComboBox.addItem("Distortion", 1);
    lfoDestinationComboBox.addItem("Tone", 2);
    lfoDestinationComboBox.addItem("Hi-Pass", 3);
    lfoDestinationComboBox.addItem("Dist Mix", 4);
    lfoDestinationComboBox.addItem("Out Gain", 5);

    lfoDestinationAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.parameters, "lfoDestination", lfoDestinationComboBox);

    addAndMakeVisible(lfoDestinationLabel);
    lfoDestinationLabel.setText("Target", juce::dontSendNotification);
    lfoDestinationLabel.setJustificationType(juce::Justification::centred);
    lfoDestinationLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    lfoDestinationLabel.setFont(juce::Font(10.0f * fs, juce::Font::bold));

    // Add destination lock icon
    addAndMakeVisible(lfoDestinationLock);

    // BPM sync toggle button
    addAndMakeVisible(lfoBpmSyncButton);
    lfoBpmSyncButton.setButtonText("SYNC");
    lfoBpmSyncButton.setClickingTogglesState(true);
    lfoBpmSyncButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF1A1A1A));
    lfoBpmSyncButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xFFFF0044));
    lfoBpmSyncButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFFFF0044));
    lfoBpmSyncButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    lfoBpmSyncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.parameters, "lfoBpmSync", lfoBpmSyncButton);
    lfoBpmSyncButton.onStateChange = [this]() { updateLFOVisibility(); };

    // LFO polarity invert button
    addAndMakeVisible(lfoInvertButton);
    lfoInvertButton.setButtonText("INV");
    lfoInvertButton.setClickingTogglesState(true);
    lfoInvertButton.setColour(juce::TextButton::buttonColourId,  juce::Colour(0xFF1A1A1A));
    lfoInvertButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xFFFF0044));
    lfoInvertButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFFFF0044));
    lfoInvertButton.setColour(juce::TextButton::textColourOnId,  juce::Colours::white);
    lfoInvertAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.parameters, "lfoInvert", lfoInvertButton);

    // BPM division dropdown
    addAndMakeVisible(lfoBpmDivisionComboBox);
    lfoBpmDivisionComboBox.setLookAndFeel(&comboBoxLookAndFeel);
    lfoBpmDivisionComboBox.addItem("1/1",   1);
    lfoBpmDivisionComboBox.addItem("1/2",   2);
    lfoBpmDivisionComboBox.addItem("1/4",   3);
    lfoBpmDivisionComboBox.addItem("1/8",   4);
    lfoBpmDivisionComboBox.addItem("1/16",  5);
    lfoBpmDivisionComboBox.addItem("1/32",  6);
    lfoBpmDivisionComboBox.addItem("1/4T",  7);
    lfoBpmDivisionComboBox.addItem("1/8T",  8);
    lfoBpmDivisionComboBox.addItem("1/16T", 9);
    lfoBpmDivisionAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.parameters, "lfoBpmDivision", lfoBpmDivisionComboBox);

    addAndMakeVisible(lfoBpmDivisionLabel);
    lfoBpmDivisionLabel.setText("Division", juce::dontSendNotification);
    lfoBpmDivisionLabel.setJustificationType(juce::Justification::centred);
    lfoBpmDivisionLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    lfoBpmDivisionLabel.setFont(juce::Font(10.0f * fs, juce::Font::bold));

    // Setup waveshaper mix knob
    setupSlider(waveshaperSlider, waveshaperLabel, "Wave Mix",
        waveshaperAttachment, "waveshaperMix");

    // Setup compressor knobs
    setupSlider(compPeakReductionSlider, compPeakReductionLabel, "Peak Reduction",
        compPeakReductionAttachment, "compPeakReduction");
    setupSlider(compMakeupGainSlider, compMakeupGainLabel, "Makeup Gain",
        compMakeupGainAttachment, "compMakeupGain");

    // Setup Compress/Limit dropdown
    addAndMakeVisible(compRatioComboBox);
    compRatioComboBox.setLookAndFeel(&comboBoxLookAndFeel);  // Apply neon red styling
    compRatioComboBox.addItem("Compress", 1);  // 3:1 ratio
    compRatioComboBox.addItem("Limit", 2);     // 12:1 ratio

    compRatioAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.parameters, "compRatio", compRatioComboBox);

    addAndMakeVisible(compRatioLabel);
    compRatioLabel.setText("Mode", juce::dontSendNotification);
    compRatioLabel.setJustificationType(juce::Justification::centred);
    compRatioLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    compRatioLabel.setFont(juce::Font(10.0f * fs, juce::Font::bold));

    // Setup compression enable toggle
    addAndMakeVisible(compEnableToggle);
    compEnableToggle.setButtonText("COMP");
    compEnableToggle.setLookAndFeel(&checkboxLookAndFeel);

    compEnableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.parameters, "compEnabled", compEnableToggle);

    // Setup collapsible compression tab header
    addAndMakeVisible(compressionTabHeader);
    compressionTabHeader.onToggle = [this](bool expanded)
    {
        isCompressionExpanded = expanded;
        updateCompressionVisibility();
        resized();
        updateExpansionBackdrop();
    };

    // Setup LFO enable toggle
    addAndMakeVisible(lfoEnableToggle);
    lfoEnableToggle.setButtonText("LFO");
    lfoEnableToggle.setLookAndFeel(&checkboxLookAndFeel);

    lfoEnableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.parameters, "lfoEnabled", lfoEnableToggle);

    // Setup collapsible LFO tab header
    addAndMakeVisible(lfoTabHeader);
    lfoTabHeader.onToggle = [this](bool expanded)
    {
        isLFOExpanded = expanded;
        updateLFOVisibility();
        resized();
        updateExpansionBackdrop();
    };

    // Add LFO lock icon
    addAndMakeVisible(lfoEnableLock);

    // Setup Sub Guard knob with snap behavior
    setupSlider(subGuardSlider, subGuardLabel, "Sub Guard", subGuardAttachment, "subGuardFreq");

    // Custom text display with mode indicators
    subGuardSlider.textFromValueFunction = [](double v) {
        // Check for OFF position first
        if (v <= 1.0) return juce::String("OFF");

        // Normal frequency display with snap points
        juce::String text = juce::String(static_cast<int>(v)) + " Hz";
        if (std::abs(v - 60.0) <= 8.0) text = "60Hz PRESERVE";
        else if (std::abs(v - 100.0) <= 8.0) text = "100Hz CONTROL";
        else if (std::abs(v - 150.0) <= 8.0) text = "150Hz AGGRO";
        return text;
    };

    // Snap to nearest point when value changes
    subGuardSlider.onValueChange = [this]() {
        double v = subGuardSlider.getValue();

        // Snap to OFF if very low (creates discrete OFF position)
        if (v <= 25.0) {
            subGuardSlider.setValue(0.0);
            return;
        }

        // Snap to preset frequencies
        if (std::abs(v - 60.0) <= 8.0) subGuardSlider.setValue(60.0);
        else if (std::abs(v - 100.0) <= 8.0) subGuardSlider.setValue(100.0);
        else if (std::abs(v - 150.0) <= 8.0) subGuardSlider.setValue(150.0);
    };

    addAndMakeVisible(clipTypeComboBox);
    clipTypeComboBox.setLookAndFeel(&comboBoxLookAndFeel);  // Apply neon red styling
    clipTypeComboBox.addItem("Brutal Fuzz", 1);
    clipTypeComboBox.addItem("Tube Overdrive", 2);
    clipTypeComboBox.addItem("Bit Crusher", 3);
    clipTypeComboBox.addItem("Tape Saturation", 4);
    clipTypeComboBox.addItem("Transformer", 5);
    clipTypeComboBox.addItem("Diode Clipper", 6);
    clipTypeComboBox.addItem("Decimator", 7);

    clipTypeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.parameters, "clipType", clipTypeComboBox);

    addAndMakeVisible(extremeButton);
    extremeButton.setButtonText("EXTREME");
    extremeButton.setClickingTogglesState(true);
    extremeButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF1A1A1A));
    extremeButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xFFFF0044));
    extremeButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFFFF0044));
    extremeButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    extremeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.parameters, "extremeEnabled", extremeButton);

    addAndMakeVisible(cleanBoostButton);
    cleanBoostButton.setButtonText("BOOST");
    cleanBoostButton.setClickingTogglesState(true);
    cleanBoostButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF1A1A1A));
    cleanBoostButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xFFFF0044));
    cleanBoostButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFFFF0044));
    cleanBoostButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    cleanBoostAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.parameters, "cleanBoost", cleanBoostButton);

    // Setup preset selector
    // Note: presetLabel removed - no "Preset:" text shown
    presetLabel.setText("", juce::dontSendNotification);
    presetLabel.setJustificationType(juce::Justification::centredRight);
    presetLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    presetLabel.setFont(juce::Font(12.0f * fs, juce::Font::bold));

    addAndMakeVisible(presetSelector);
    presetSelector.setLookAndFeel(&comboBoxLookAndFeel);  // Apply neon red styling
    presetSelector.setTextWhenNothingSelected("Select Preset...");
    presetSelector.onChange = [this]()
    {
        int selectedId = presetSelector.getSelectedId();

        // Handle Save action item
        if (selectedId == 9990)
        {
            presetSelector.setSelectedId(0, juce::dontSendNotification);

            auto* w = new juce::AlertWindow("Save Preset", "Enter preset name:", juce::AlertWindow::NoIcon);
            w->addTextEditor("presetName", "", "Preset Name:");
            w->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
            w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

            juce::Component::SafePointer<PluginEditor> safeThis (this);
            w->enterModalState(true, juce::ModalCallbackFunction::create([safeThis, w](int result)
            {
                // Read the editor contents from w (always valid until we delete it),
                // but only touch editor state if the editor still exists — the user
                // may have closed the plugin while this modal was open.
                if (result == 1 && safeThis != nullptr)
                {
                    juce::String presetName = w->getTextEditorContents("presetName");
                    if (presetName.isNotEmpty())
                    {
                        safeThis->savePreset(presetName);
                        safeThis->audioProcessor.parameters.state.setProperty("currentPresetName", presetName, nullptr);
                        safeThis->refreshPresetList();
                    }
                }
                delete w;
            }));
            return;
        }

        // Handle Delete action item
        if (selectedId == 9991)
        {
            presetSelector.setSelectedId(0, juce::dontSendNotification);

            // Build a popup of deletable user presets
            auto presetDir = getPresetDirectory();
            auto presetFiles = presetDir.findChildFiles(juce::File::findFiles, false, "*.xml");

            if (presetFiles.isEmpty()) return;

            juce::PopupMenu deleteMenu;
            for (int i = 0; i < presetFiles.size(); ++i)
                deleteMenu.addItem(i + 1, presetFiles[i].getFileNameWithoutExtension());

            juce::Component::SafePointer<PluginEditor> safeThis (this);
            deleteMenu.showMenuAsync(juce::PopupMenu::Options(), [safeThis, presetFiles](int result)
            {
                if (result > 0 && safeThis != nullptr)
                {
                    juce::String presetName = presetFiles[result - 1].getFileNameWithoutExtension();

                    auto options = juce::MessageBoxOptions()
                        .withIconType(juce::MessageBoxIconType::WarningIcon)
                        .withTitle("Delete Preset")
                        .withMessage("Are you sure you want to delete '" + presetName + "'?")
                        .withButton("OK")
                        .withButton("Cancel");

                    juce::AlertWindow::showAsync(options, [safeThis, presetName](int r)
                    {
                        if (r == 1 && safeThis != nullptr)
                        {
                            safeThis->deletePreset(presetName);
                            safeThis->refreshPresetList();
                        }
                    });
                }
            });
            return;
        }

        // Normal preset selection
        if (selectedId > 0)
        {
            juce::String presetName = presetSelector.getText();

            if (FactoryPresets::isFactory(presetName))
                loadFactoryPreset(presetName);
            else
                loadPreset(presetName);

            // Persist the selected preset name so the DAW session can restore it
            audioProcessor.parameters.state.setProperty("currentPresetName", presetName, nullptr);
        }
    };

    // Load presets
    refreshPresetList();

    // Setup global mix slider (plugin wet/dry)
    addAndMakeVisible(globalMixSlider);
    globalMixSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    globalMixSlider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    globalMixSlider.setColour(juce::Slider::trackColourId, juce::Colour(0xFFFF0044));
    globalMixSlider.setColour(juce::Slider::thumbColourId, juce::Colours::white);
    globalMixSlider.setColour(juce::Slider::backgroundColourId, juce::Colour(0xFF333333));
    globalMixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.parameters, "globalMix", globalMixSlider);

    addAndMakeVisible(globalMixLabel);
    globalMixLabel.setText("MIX", juce::dontSendNotification);
    globalMixLabel.setJustificationType(juce::Justification::centredRight);
    globalMixLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFAAAAAA));

    // Setup randomize button
    addAndMakeVisible(randomizeButton);
    randomizeButton.setButtonText("Randomize");
    randomizeButton.onClick = [this]() { randomizeAllParameters(); };

    // Apply neon red styling
    randomizeButton.setColour(juce::TextButton::buttonColourId, juce::Colours::black);
    randomizeButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFF, 0x00, 0x44));  // Neon red text
    randomizeButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);  // White when pressed
    randomizeButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xFF, 0x00, 0x44));  // Neon red when pressed
    randomizeButton.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xFF, 0x00, 0x44));  // Neon red outline

    // Add right-click menu for lock management
    randomizeButton.setMouseCursor(juce::MouseCursor::PointingHandCursor);
    randomizeButton.onRightClick = [this]()
    {
        juce::PopupMenu menu;

        // Add lock/unlock options for each parameter
        menu.addSectionHeader("Lock/Unlock Parameters");
        menu.addSeparator();

        // Main parameters
        menu.addItem("Input Gain", true, isParameterLocked("inputGain"),
                     [this]() { toggleParameterLock("inputGain"); });
        menu.addItem("Output Gain", true, isParameterLocked("outputGain"),
                     [this]() { toggleParameterLock("outputGain"); });
        menu.addItem("Distortion Amount", true, isParameterLocked("distortionAmount"),
                     [this]() { toggleParameterLock("distortionAmount"); });
        menu.addItem("Filter Frequency", true, isParameterLocked("highPassFreq"),
                     [this]() { toggleParameterLock("highPassFreq"); });
        menu.addItem("Filter Mode", true, isParameterLocked("filterMode"),
                     [this]() { toggleParameterLock("filterMode"); });
        menu.addItem("Sub Guard", true, isParameterLocked("subGuardFreq"),
                     [this]() { toggleParameterLock("subGuardFreq"); });
        menu.addItem("Clip Type", true, isParameterLocked("clipType"),
                     [this]() { toggleParameterLock("clipType"); });
        menu.addItem("Dist Mix", true, isParameterLocked("distMix"),
                     [this]() { toggleParameterLock("distMix"); });
        menu.addItem("Tone", true, isParameterLocked("tone"),
                     [this]() { toggleParameterLock("tone"); });
        menu.addItem("Waveshaper Mix", true, isParameterLocked("waveshaperMix"),
                     [this]() { toggleParameterLock("waveshaperMix"); });

        menu.addSeparator();
        menu.addSectionHeader("LFO");
        menu.addItem("LFO Rate", true, isParameterLocked("lfoRate"),
                     [this]() { toggleParameterLock("lfoRate"); });
        menu.addItem("LFO Depth", true, isParameterLocked("lfoDepth"),
                     [this]() { toggleParameterLock("lfoDepth"); });
        menu.addItem("LFO Waveform", true, isParameterLocked("lfoWaveform"),
                     [this]() { toggleParameterLock("lfoWaveform"); });
        menu.addItem("LFO Destination", true, isParameterLocked("lfoDestination"),
                     [this]() { toggleParameterLock("lfoDestination"); });
        menu.addItem("LFO Enable", true, isParameterLocked("lfoEnabled"),
                     [this]() { toggleParameterLock("lfoEnabled"); });
        menu.addItem("LFO Invert", true, isParameterLocked("lfoInvert"),
                     [this]() { toggleParameterLock("lfoInvert"); });

        menu.addSeparator();
        menu.addSectionHeader("Compression");
        menu.addItem("Peak Reduction", true, isParameterLocked("compPeakReduction"),
                     [this]() { toggleParameterLock("compPeakReduction"); });
        menu.addItem("Makeup Gain", true, isParameterLocked("compMakeupGain"),
                     [this]() { toggleParameterLock("compMakeupGain"); });
        menu.addItem("Comp Ratio", true, isParameterLocked("compRatio"),
                     [this]() { toggleParameterLock("compRatio"); });
        menu.addItem("Comp Enable", true, isParameterLocked("compEnabled"),
                     [this]() { toggleParameterLock("compEnabled"); });

        menu.addSeparator();
        menu.addSectionHeader("Character");
        menu.addItem("Extreme", true, isParameterLocked("extremeEnabled"),
                     [this]() { toggleParameterLock("extremeEnabled"); });
        menu.addItem("Clean Boost", true, isParameterLocked("cleanBoost"),
                     [this]() { toggleParameterLock("cleanBoost"); });

        menu.addSeparator();
        menu.addItem("Lock All", [this]()
        {
            for (auto& pair : parameterLocks)
                pair.second = true;
        });
        menu.addItem("Unlock All", [this]()
        {
            for (auto& pair : parameterLocks)
                pair.second = false;
        });

        menu.showMenuAsync(juce::PopupMenu::Options());
    };

    // Initialize all parameter locks to unlocked (false)
    parameterLocks["inputGain"] = false;
    parameterLocks["outputGain"] = false;
    parameterLocks["distortionAmount"] = false;
    parameterLocks["highPassFreq"] = false;
    parameterLocks["filterMode"] = false;
    parameterLocks["subGuardFreq"] = false;
    parameterLocks["clipType"] = false;
    parameterLocks["distMix"] = false;
    parameterLocks["tone"] = false;
    parameterLocks["waveshaperMix"] = false;
    parameterLocks["lfoRate"] = false;
    parameterLocks["lfoDepth"] = false;
    parameterLocks["lfoEnabled"] = false;
    parameterLocks["lfoInvert"] = false;
    parameterLocks["lfoWaveform"] = false;
    parameterLocks["lfoDestination"] = false;
    parameterLocks["compPeakReduction"] = false;
    parameterLocks["compMakeupGain"] = false;
    parameterLocks["compRatio"] = false;
    parameterLocks["compEnabled"] = false;
    parameterLocks["extremeEnabled"] = false;
    parameterLocks["cleanBoost"] = false;

    // Add lock icons
    addAndMakeVisible(inputGainLock);
    addAndMakeVisible(outputGainLock);
    addAndMakeVisible(distortionAmountLock);
    addAndMakeVisible(highPassFreqLock);
    addAndMakeVisible(distMixLock);
    addAndMakeVisible(lfoRateLock);
    addAndMakeVisible(lfoDepthLock);
    addAndMakeVisible(compPeakReductionLock);
    addAndMakeVisible(compMakeupGainLock);
    addAndMakeVisible(subGuardLock);
    addAndMakeVisible(clipTypeLock);
    addAndMakeVisible(compRatioLock);
    addAndMakeVisible(compEnableLock);

    // Setup right-click handlers for all controls
    setupKnobRightClick(inputGainSlider, "inputGain");
    setupKnobRightClick(outputGainSlider, "outputGain");
    setupKnobRightClick(distortionAmountSlider, "distortionAmount");
    setupKnobRightClick(highPassFreqSlider, "highPassFreq");
    setupKnobRightClick(distMixSlider, "distMix");
    setupKnobRightClick(lfoRateSlider, "lfoRate");
    setupKnobRightClick(lfoDepthSlider, "lfoDepth");
    setupKnobRightClick(compPeakReductionSlider, "compPeakReduction");
    setupKnobRightClick(compMakeupGainSlider, "compMakeupGain");
    setupKnobRightClick(subGuardSlider, "subGuardFreq");
    setupKnobRightClick(clipTypeComboBox, "clipType");
    setupKnobRightClick(compRatioComboBox, "compRatio");
    setupKnobRightClick(compEnableToggle, "compEnabled");
    setupKnobRightClick(lfoEnableToggle, "lfoEnabled");
    setupKnobRightClick(lfoDestinationComboBox, "lfoDestination");

    // Initialize section visibility
    updateCompressionVisibility();
    updateLFOVisibility();

    // Expansion backdrop — starts hidden, appears when a tab is expanded in compact mode
    addChildComponent(expansionBackdrop);

    // Setup settings (gear) button
    addAndMakeVisible(settingsButton);
    settingsButton.onClick = [this]() { showSettingsOverlay(); };

    // Setup oscilloscope view toggle (compact <-> full) — duplicates the Settings toggle
    addAndMakeVisible(scopeButton);
    scopeButton.onClick = [this]() { toggleOscilloscopeMode(); };

    // Toolbar quick-toggle for the Graphic EQ overlay — mirrors the Settings
    // "Overlay" selector's EQ position.
    addAndMakeVisible(eqButton);
    eqButton.onClick = [this]() { toggleGraphicEqOverlay(); };

    // Load UI settings — capture first-run state before loading
    const bool settingsFileExisted = getSettingsFile().existsAsFile();
    loadSettings();
    // Sync oversampling setting to processor (in case it was saved as non-default)
    audioProcessor.requestOversamplingRebuild(settingsState.oversamplingMode);
    applyOscilloscopeEnabled(settingsState.oscilloscopeEnabled);
    applyScopeOverlayMode(settingsState.scopeOverlayMode);
    scopeButton.setFullMode(settingsState.oscilloscopeEnabled);
    oscilloscope.setStereoMode(settingsState.oscilloscopeStereo);
    oscilloscope.setScopeLength(settingsState.scopeLength);

    // Set window size AFTER all components are added so resized() can position them all
    setSize(672, 395);
    applyWindowScale(settingsState.windowScalePercent);

    // Start timer for LFO modulation visual feedback (60Hz)
    startTimerHz(60);

    // Construction-time sizing is done; allow animated folds from now on
    allowFoldAnimation = true;

    // First-run: settings file didn't exist before this session — show a one-time
    // consent notice and write the file so it won't show again.
    if (!settingsFileExisted)
    {
        showFirstRunNotice();
        saveSettings();
    }
}

PluginEditor::~PluginEditor()
{
    stopTimer();
    foldAnimTimer.stopTimer();

    // Destroy settings overlay before LookAndFeel instances
    settingsOverlay.reset();
    firstRunNotice.reset();

    // Reset LookAndFeel to nullptr before destruction to prevent crash
    // Components must not reference a LookAndFeel that may be destroyed before them
    compEnableToggle.setLookAndFeel(nullptr);
    lfoEnableToggle.setLookAndFeel(nullptr);
    clipTypeComboBox.setLookAndFeel(nullptr);
    filterModeComboBox.setLookAndFeel(nullptr);
    compRatioComboBox.setLookAndFeel(nullptr);
    lfoWaveformComboBox.setLookAndFeel(nullptr);
    lfoDestinationComboBox.setLookAndFeel(nullptr);
    lfoBpmDivisionComboBox.setLookAndFeel(nullptr);
    presetSelector.setLookAndFeel(nullptr);
}

//==============================================================================
void PluginEditor::setupSlider(CustomKnob& slider,
    juce::Label& label,
    const juce::String& text,
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& attachment,
    const juce::String& paramID)
{

    // Add mouse wheel and double-click sensitivity
    slider.setMouseDragSensitivity(150);
    slider.setDoubleClickReturnValue(true, 50.0f); // Double-click returns to 50

    addAndMakeVisible(slider);


    if (audioProcessor.parameters.getParameter(paramID) != nullptr)
    {
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            audioProcessor.parameters, paramID, slider);
        slider.textFromValueFunction = [](double value)
            {
                return juce::String(static_cast<int>(value));
            };
        slider.updateText();
    }
    else
    {
        jassertfalse; // Parameter not found - check your parameter IDs
    }

    // Configure label with neon red color
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, juce::Colours::white);  // White text
    label.setColour(juce::Label::backgroundColourId, juce::Colours::transparentWhite);
    // Use base scale factor (fonts get repositioned in resized() anyway)
    const float fontScale = std::max(getWidth() / 960.0f, 672.0f / 960.0f);
    label.setFont(juce::Font(12.0f * fontScale, juce::Font::bold));

    addAndMakeVisible(label);
}

//==============================================================================
void PluginEditor::paint(juce::Graphics& g)
{
    // Black background
    g.fillAll(juce::Colours::black);

    // Draw solid neon red frame (consistent width on all 4 sides)
    g.setColour(juce::Colour(0xFF, 0x00, 0x44));  // Fully opaque neon red
    g.drawRect(getLocalBounds().toFloat(), 1.5f);  // 1.5px solid frame
}

void PluginEditor::resized()
{
    // ========== PROPORTIONAL SCALING ==========
    // Base design is 960x564, scale everything proportionally to actual size
    const float s = getWidth() / 960.0f;
    auto S = [s](int v) -> int { return juce::roundToInt(v * s); };

    // ========== LAYOUT CONSTANTS (scaled) ==========
    const int titleHeight = S(50);
    const int bottomControlsHeight = S(130);
    const int margin = S(20);
    const float borderWidth = 1.5f * s;

    // ========== LOGO TITLE ==========
    logoTitle.setBounds(0, 0, getWidth(), titleHeight);

    // ========== VERSION LABEL (bottom left corner) ==========
    versionLabel.setBounds(margin, getHeight() - S(20), S(100), S(16));
    versionLabel.setFont(juce::Font(10.0f * s));

    // ========== OSCILLOSCOPE (below title, above controls, inset by border) ==========
    const int phaseH = S(16);
    const int scopeH = juce::jmax(0, getHeight() - titleHeight - bottomControlsHeight - phaseH);
    oscilloscope.setBounds((int)borderWidth, titleHeight,
                          getWidth() - (int)(borderWidth * 2), scopeH);
    xyMorphPad.setBounds(oscilloscope.getBounds());
    graphicEqOverlay.setBounds(oscilloscope.getBounds());
    phaseCorrelationMeter.setBounds((int)borderWidth, oscilloscope.getBottom(),
                                    getWidth() - (int)(borderWidth * 2), phaseH);

    const int labelHeight = S(20);

    // Calculate available space for controls (overlaid on oscilloscope)
    auto bounds = getLocalBounds().reduced(margin);
    auto titleArea = bounds.removeFromTop(titleHeight);

    // ========== PRESET SELECTOR + RANDOMIZE + SETTINGS (TOP-LEFT) ==========
    const int presetSelectorWidth = S(150);
    const int presetSpacing = S(5);
    const int presetHeight = S(24);

    const int presetY = titleArea.getY() + (titleHeight - presetHeight) / 2;
    const int presetX = titleArea.getX();

    // Hide the label (set to zero width)
    presetLabel.setBounds(0, 0, 0, 0);

    // Preset selector (Save/Delete are inside the dropdown)
    presetSelector.setBounds(presetX, presetY, presetSelectorWidth, presetHeight);

    // Randomize + Settings buttons - right after preset selector
    const int randomizeX = presetX + presetSelectorWidth + presetSpacing;
    const int randomizeButtonWidth = S(80);
    randomizeButton.setBounds(randomizeX, presetY, randomizeButtonWidth, presetHeight);

    const int settingsBtnSize = S(24);
    settingsButton.setBounds(randomizeX + randomizeButtonWidth + presetSpacing,
                             presetY, settingsBtnSize, settingsBtnSize);

    scopeButton.setBounds(settingsButton.getRight() + presetSpacing,
                          presetY, settingsBtnSize, settingsBtnSize);

    eqButton.setBounds(scopeButton.getRight() + presetSpacing,
                       presetY, settingsBtnSize, settingsBtnSize);

    // Global Mix slider - directly below preset selector
    const int mixLabelWidth = S(28);
    const int mixSliderY = presetY + presetHeight + S(2);
    const int mixSliderHeight = S(16);
    globalMixLabel.setBounds(presetX, mixSliderY, mixLabelWidth, mixSliderHeight);
    globalMixLabel.setFont(juce::Font(9.0f * s, juce::Font::bold));
    globalMixSlider.setBounds(presetX + mixLabelWidth, mixSliderY,
                              presetSelectorWidth - mixLabelWidth, mixSliderHeight);

    // ========== LFO & COMPRESSION TABS (TOP-RIGHT) ==========
    const int tabHeaderHeight = S(25);
    const int tabChevronWidth = S(30);
    const int toggleSize = S(24);

    // Compression tab (far right)
    const int compTabTextWidth = S(120);
    const int compTabTotalWidth = compTabTextWidth + tabChevronWidth;
    const int compTabX = getWidth() - margin - compTabTotalWidth;
    compressionTabHeader.setBounds(compTabX, presetY, compTabTotalWidth, tabHeaderHeight);
    compressionTabHeader.setExpanded(isCompressionExpanded);

    // COMP Enable Toggle - to the left of compression tab
    compEnableToggle.setBounds(compTabX - toggleSize - S(4), presetY, toggleSize, toggleSize);
    compEnableLock.setBounds(compTabX - toggleSize - S(4) + toggleSize - S(12), presetY, S(12), S(12));

    // LFO tab (left of compression toggle)
    const int lfoTabTextWidth = S(50);
    const int lfoTabTotalWidth = lfoTabTextWidth + tabChevronWidth;
    const int lfoTabX = compTabX - toggleSize - S(4) - S(10) - lfoTabTotalWidth;
    lfoTabHeader.setBounds(lfoTabX, presetY, lfoTabTotalWidth, tabHeaderHeight);
    lfoTabHeader.setExpanded(isLFOExpanded);

    // LFO Enable Toggle - to the left of LFO tab
    lfoEnableToggle.setBounds(lfoTabX - toggleSize - S(4), presetY, toggleSize, toggleSize);
    lfoEnableLock.setBounds(lfoTabX - toggleSize - S(4) + toggleSize - S(12), presetY, S(12), S(12));

    auto contentArea = bounds;

    // ========== EXPANDED LFO & COMPRESSION SECTIONS (RIGHT-ALIGNED BELOW TABS) ==========
    const int sKnob = S(45);  // Smaller knobs for sections
    const int sSpacing = S(6);
    const int expandedSectionHeight = sKnob + S(18); // knob + label
    const int actualWindowHeight = getHeight();

    const int expandedY = presetY + presetHeight + S(8);  // Below the tab headers with gap

    // Calculate section height for oscilloscope positioning
    const bool anyExpanded = isLFOExpanded || isCompressionExpanded;
    const int topSectionHeight = anyExpanded ? expandedSectionHeight : 0;
    contentArea.removeFromTop(topSectionHeight);

    // Dividing point between LFO and Compression areas (shifted right to prevent overlap)
    const int dividerX = (lfoTabX + lfoTabTotalWidth + compTabX - toggleSize - S(4)) / 2 + S(15);

    // Only layout LFO controls if expanded
    if (isLFOExpanded)
    {
        const int dropW = S(55);
        const int dropH = S(16);
        const int gap = S(8);

        const int lfoControlsWidth = sKnob + sSpacing + sKnob + sSpacing + dropW;
        int lx = dividerX - gap - lfoControlsWidth;

        lfoRateSlider.setBounds(lx, expandedY, sKnob, sKnob);
        lfoRateLabel.setBounds(lx, expandedY + sKnob, sKnob, S(13));
        lfoRateLock.setBounds(lx + sKnob - S(12) - S(2), expandedY + S(2), S(12), S(12));
        // Division dropdown overlaps the rate knob area when BPM sync is ON
        lfoBpmDivisionComboBox.setBounds(lx, expandedY + (sKnob - S(16)) / 2, sKnob, S(16));
        lfoBpmDivisionLabel.setBounds(lx, expandedY + sKnob, sKnob, S(13));
        // SYNC toggle button sits below the rate label row, always in the rate column
        lfoBpmSyncButton.setBounds(lx, expandedY + sKnob + S(13), sKnob, S(13));
        lx += sKnob + sSpacing;

        lfoDepthSlider.setBounds(lx, expandedY, sKnob, sKnob);
        lfoDepthLabel.setBounds(lx, expandedY + sKnob, sKnob, S(13));
        lfoDepthLock.setBounds(lx + sKnob - S(12) - S(2), expandedY + S(2), S(12), S(12));
        lfoInvertButton.setBounds(lx, expandedY + sKnob + S(13), sKnob, S(13));
        lx += sKnob + sSpacing;

        // Wave dropdown on top
        lfoWaveformComboBox.setBounds(lx, expandedY + S(2), dropW, dropH);
        lfoWaveformLabel.setBounds(lx, expandedY + S(2) + dropH, dropW, S(11));

        // Target dropdown directly below Wave
        lfoDestinationComboBox.setBounds(lx, expandedY + S(2) + dropH + S(12), dropW, dropH);
        lfoDestinationLabel.setBounds(lx, expandedY + S(2) + dropH + S(12) + dropH, dropW, S(11));
        lfoDestinationLock.setBounds(lx + dropW - S(10), expandedY + S(2) + dropH + S(12), S(10), S(10));
    }

    // Only layout compression controls if expanded
    if (isCompressionExpanded)
    {
        const int dropW = S(50);
        const int meterW = S(22);
        const int meterH = S(60);

        int cx = dividerX;

        compPeakReductionSlider.setBounds(cx, expandedY, sKnob, sKnob);
        compPeakReductionLabel.setBounds(cx, expandedY + sKnob, sKnob, S(13));
        compPeakReductionLock.setBounds(cx + sKnob - S(12) - S(2), expandedY + S(2), S(12), S(12));
        cx += sKnob + sSpacing;

        compMakeupGainSlider.setBounds(cx, expandedY, sKnob, sKnob);
        compMakeupGainLabel.setBounds(cx, expandedY + sKnob, sKnob, S(13));
        compMakeupGainLock.setBounds(cx + sKnob - S(12) - S(2), expandedY + S(2), S(12), S(12));
        cx += sKnob + sSpacing;

        compRatioComboBox.setBounds(cx, expandedY + S(10), dropW, S(16));
        compRatioLabel.setBounds(cx, expandedY + S(28), dropW, S(12));
        compRatioLock.setBounds(cx + dropW - S(12), expandedY + S(10), S(12), S(12));
        cx += dropW + sSpacing;

        gainReductionMeter.setBounds(cx, expandedY + S(2), meterW, meterH);
    }
    // ===========================================================

    // ========== SINGLE ROW LAYOUT AT BOTTOM ==========
    const int knobSize = S(67);
    const int controlSpacing = S(8);
    const int bottomMargin = S(20);
    const int rowY = actualWindowHeight - bottomMargin - knobSize - labelHeight - S(10) + 2;

    const int clipTypeColumnWidth = S(50);
    const int totalControlsWidth = knobSize * 8 + clipTypeColumnWidth + controlSpacing * 8;
    int currentX = (getWidth() - totalControlsWidth) / 2;

    const int lockSize = S(16);
    const int lockInset = S(5);

    // Sub Guard Knob
    subGuardSlider.setBounds(currentX, rowY, knobSize, knobSize);
    subGuardLabel.setBounds(currentX, rowY + knobSize + lockInset, knobSize, labelHeight);
    subGuardLock.setBounds(currentX + knobSize - lockSize - lockInset, rowY + lockInset, lockSize, lockSize);
    currentX += knobSize + controlSpacing;

    // Input Gain
    inputGainSlider.setBounds(currentX, rowY, knobSize, knobSize);
    inputGainLabel.setBounds(currentX, rowY + knobSize + lockInset, knobSize, labelHeight);
    inputGainLock.setBounds(currentX + knobSize - lockSize - lockInset, rowY + lockInset, lockSize, lockSize);
    currentX += knobSize + controlSpacing;

    // Filter (multimode HP/LP/BP): knob with the mode dropdown in the label slot.
    highPassFreqSlider.setBounds(currentX, rowY, knobSize, knobSize);
    filterModeComboBox.setBounds(currentX, rowY + knobSize + lockInset, knobSize, labelHeight);
    highPassFreqLock.setBounds(currentX + knobSize - lockSize - lockInset, rowY + lockInset, lockSize, lockSize);
    currentX += knobSize + controlSpacing;

    // Dist Mix
    distMixSlider.setBounds(currentX, rowY, knobSize, knobSize);
    distMixLabel.setBounds(currentX, rowY + knobSize + lockInset, knobSize, labelHeight);
    distMixLock.setBounds(currentX + knobSize - lockSize - lockInset, rowY + lockInset, lockSize, lockSize);
    currentX += knobSize + controlSpacing;

    // Distortion Amount
    distortionAmountSlider.setBounds(currentX, rowY, knobSize, knobSize);
    distortionAmountLabel.setBounds(currentX, rowY + knobSize + lockInset, knobSize, labelHeight);
    distortionAmountLock.setBounds(currentX + knobSize - lockSize - lockInset, rowY + lockInset, lockSize, lockSize);
    currentX += knobSize + controlSpacing;

    // Clip Type Dropdown
    const int comboWidth = S(45);
    const int comboHeight = S(18);
    const int comboXOffset = (clipTypeColumnWidth - comboWidth) / 2;
    const int clipButtonH = S(14);
    const int clipButtonGap = S(2);
    // Center the ClipType / EXTREME / BOOST stack on the knob centerline so EXTREME
    // (the middle element) lines up vertically between the Dist. Amount and Tone knobs.
    const int extremeY = rowY + (knobSize - clipButtonH) / 2 + 2;
    const int comboY = extremeY - clipButtonGap - comboHeight;
    const int boostY = extremeY + clipButtonH + clipButtonGap;
    clipTypeComboBox.setBounds(currentX + comboXOffset, comboY, comboWidth, comboHeight);
    clipTypeLabel.setBounds(currentX, rowY + knobSize + lockInset, clipTypeColumnWidth, labelHeight);
    clipTypeLock.setBounds(currentX + comboXOffset + comboWidth - S(12) - S(2), comboY, S(12), S(12));
    extremeButton.setBounds(currentX + comboXOffset, extremeY, comboWidth, clipButtonH);
    cleanBoostButton.setBounds(currentX + comboXOffset, boostY, comboWidth, clipButtonH);
    currentX += clipTypeColumnWidth + controlSpacing;

    // Tone
    toneSlider.setBounds(currentX, rowY, knobSize, knobSize);
    toneLabel.setBounds(currentX, rowY + knobSize + lockInset, knobSize, labelHeight);
    currentX += knobSize + controlSpacing;

    // Wave Mix
    waveshaperSlider.setBounds(currentX, rowY, knobSize, knobSize);
    waveshaperLabel.setBounds(currentX, rowY + knobSize + lockInset, knobSize, labelHeight);
    currentX += knobSize + controlSpacing;

    // Output Gain
    outputGainSlider.setBounds(currentX, rowY, knobSize, knobSize);
    outputGainLabel.setBounds(currentX, rowY + knobSize + lockInset, knobSize, labelHeight);
    outputGainLock.setBounds(currentX + knobSize - lockSize - lockInset, rowY + lockInset, lockSize, lockSize);

    // Expansion backdrop (compact mode only)
    updateExpansionBackdrop();

    // Settings overlay covers entire editor
    if (settingsOverlay)
        settingsOverlay->setBounds(getLocalBounds());

    // First-run notice also covers the entire editor
    if (firstRunNotice)
        firstRunNotice->setBounds(getLocalBounds());
}

bool PluginEditor::isParameterLocked(const juce::String& paramID) const
{
    auto it = parameterLocks.find(paramID);
    return (it != parameterLocks.end()) ? it->second : false;
}

void PluginEditor::toggleParameterLock(const juce::String& paramID)
{
    parameterLocks[paramID] = !parameterLocks[paramID];
    updateLockIcons();
}

void PluginEditor::updateLockIcons()
{
    inputGainLock.setLocked(isParameterLocked("inputGain"));
    outputGainLock.setLocked(isParameterLocked("outputGain"));
    distortionAmountLock.setLocked(isParameterLocked("distortionAmount"));
    highPassFreqLock.setLocked(isParameterLocked("highPassFreq"));
    distMixLock.setLocked(isParameterLocked("distMix"));
    lfoRateLock.setLocked(isParameterLocked("lfoRate"));
    lfoDepthLock.setLocked(isParameterLocked("lfoDepth"));
    lfoEnableLock.setLocked(isParameterLocked("lfoEnabled"));
    lfoDestinationLock.setLocked(isParameterLocked("lfoDestination"));
    compPeakReductionLock.setLocked(isParameterLocked("compPeakReduction"));
    compMakeupGainLock.setLocked(isParameterLocked("compMakeupGain"));
    subGuardLock.setLocked(isParameterLocked("subGuardFreq"));
    clipTypeLock.setLocked(isParameterLocked("clipType"));
    compRatioLock.setLocked(isParameterLocked("compRatio"));
    compEnableLock.setLocked(isParameterLocked("compEnabled"));
}

void PluginEditor::setupKnobRightClick(juce::Component& component, const juce::String& paramID)
{
    component.setMouseCursor(juce::MouseCursor::PointingHandCursor);
    component.addMouseListener(this, false);

    // Store parameter ID in component properties for later retrieval
    component.getProperties().set("paramID", paramID);
}

void PluginEditor::mouseUp(const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        // Check if the event came from a component with a paramID property
        if (auto* comp = e.eventComponent)
        {
            if (comp->getProperties().contains("paramID"))
            {
                juce::String paramID = comp->getProperties()["paramID"].toString();
                toggleParameterLock(paramID);
                e.eventComponent->repaint();
            }
        }
    }
}

void PluginEditor::updateCompressionVisibility()
{
    const bool visible = isCompressionExpanded;

    // 2 knobs + labels
    compPeakReductionSlider.setVisible(visible);
    compPeakReductionLabel.setVisible(visible);
    compMakeupGainSlider.setVisible(visible);
    compMakeupGainLabel.setVisible(visible);

    // Dropdown, meter
    compRatioComboBox.setVisible(visible);
    compRatioLabel.setVisible(visible);
    gainReductionMeter.setVisible(visible);

    // Lock icons (toggle + lock always visible, handled separately)
    compPeakReductionLock.setVisible(visible);
    compMakeupGainLock.setVisible(visible);
    compRatioLock.setVisible(visible);
}

void PluginEditor::updateLFOVisibility()
{
    static const juce::StringArray kDivNames {
        "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4T", "1/8T", "1/16T"
    };

    const bool visible = isLFOExpanded;
    const bool bpmSync = visible &&
        audioProcessor.parameters.getParameter("lfoBpmSync")->getValue() > 0.5f;

    // Rate knob always shown when expanded; division combo retired — knob controls it when sync is on.
    lfoRateSlider.setVisible(visible);
    lfoRateLabel.setVisible(visible);
    lfoBpmDivisionComboBox.setVisible(false);
    lfoBpmDivisionLabel.setVisible(false);

    // Swap attachment: division index (0-8, step 1) when sync on; Hz rate when sync off.
    lfoRateAttachment.reset();
    if (bpmSync)
    {
        lfoRateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            audioProcessor.parameters, "lfoBpmDivision", lfoRateSlider);
        lfoRateSlider.textFromValueFunction = [](double v) {
            return kDivNames[juce::jlimit(0, 8, (int)std::round(v))];
        };
        lfoRateSlider.onValueChange = [this, &kDivNames = kDivNames]() {
            const int idx = juce::jlimit(0, 8, (int)std::round(lfoRateSlider.getValue()));
            lfoRateLabel.setText(kDivNames[idx], juce::dontSendNotification);
        };
        // Show current division immediately
        const int idx = juce::jlimit(0, 8, (int)std::round(lfoRateSlider.getValue()));
        lfoRateLabel.setText(kDivNames[idx], juce::dontSendNotification);
    }
    else
    {
        lfoRateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            audioProcessor.parameters, "lfoRate", lfoRateSlider);
        lfoRateSlider.textFromValueFunction = [](double value) {
            return juce::String(static_cast<int>(value));
        };
        lfoRateSlider.onValueChange = nullptr;
        lfoRateLabel.setText("Rate", juce::dontSendNotification);
    }
    lfoRateSlider.updateText();

    lfoDepthSlider.setVisible(visible);
    lfoDepthLabel.setVisible(visible);
    lfoInvertButton.setVisible(visible);

    lfoWaveformComboBox.setVisible(visible);
    lfoWaveformLabel.setVisible(visible);
    lfoDestinationComboBox.setVisible(visible);
    lfoDestinationLabel.setVisible(visible);
    lfoDestinationLock.setVisible(visible);

    lfoBpmSyncButton.setVisible(visible);

    lfoRateLock.setVisible(visible && !bpmSync);
    lfoDepthLock.setVisible(visible);
}

void PluginEditor::updateModulationHighlight()
{
    const bool lfoEnabled = audioProcessor.parameters.getParameter("lfoEnabled")->getValue() > 0.5f;
    const float lfoDepth = audioProcessor.parameters.getParameter("lfoDepth")->getValue();
    const float lfoRate = audioProcessor.parameters.getParameter("lfoRate")->getValue();

    if (!lfoEnabled || lfoDepth < 0.01f)
    {
        // LFO disabled or no depth - reset all arcs
        distortionAmountSlider.setLFOArc(false, 0.0f, 0.0f);
        toneSlider.setLFOArc(false, 0.0f, 0.0f);
        highPassFreqSlider.setLFOArc(false, 0.0f, 0.0f);
        distMixSlider.setLFOArc(false, 0.0f, 0.0f);
        outputGainSlider.setLFOArc(false, 0.0f, 0.0f);
        return;
    }

    // Get current destination
    const int destination = static_cast<int>(
        audioProcessor.parameters.getParameter("lfoDestination")->getValue() * 4.0f + 0.5f);

    // Read LFO phase directly from audio thread
    // With 60Hz timer and 10Hz max rate, we get at least 6 frames per cycle — smooth enough
    auto* rateParam = audioProcessor.parameters.getParameter("lfoRate");
    float lfoRateHz = rateParam->convertFrom0to1(lfoRate);

    float lfoPhase;
    if (lfoRateHz < 0.01f)
        lfoPhase = 0.25f;  // Static arc at max positive swing
    else
        lfoPhase = audioProcessor.lfoPhaseForUI.load(std::memory_order_relaxed);

    const bool lfoInverted = audioProcessor.parameters.getParameter("lfoInvert")->getValue() > 0.5f;
    const float depthNorm = lfoInverted ? -lfoDepth : lfoDepth;

    // Set arc on the targeted knob, clear others
    distortionAmountSlider.setLFOArc(destination == 0, lfoPhase, depthNorm);
    toneSlider.setLFOArc(destination == 1, lfoPhase, depthNorm);
    highPassFreqSlider.setLFOArc(destination == 2, lfoPhase, depthNorm);
    distMixSlider.setLFOArc(destination == 3, lfoPhase, depthNorm);
    outputGainSlider.setLFOArc(destination == 4, lfoPhase, depthNorm);
}

void PluginEditor::timerCallback()
{
    // Fold animation runs on its own dedicated 60Hz timer (foldAnimTimer)
    // so the editor's 60Hz UI tick does not gate its frame rate.

    // Update LFO modulation indicator
    updateModulationHighlight();
}

void PluginEditor::morphDistortionParameters(float x, float y)
{
    // Helper lambda to set parameters (respecting locks)
    auto setParam = [this](const juce::String& paramID, float value)
    {
        if (isParameterLocked(paramID)) return;  // Respect locks

        if (auto* param = audioProcessor.parameters.getParameter(paramID))
        {
            param->setValueNotifyingHost(param->convertTo0to1(value));
        }
    };

    // Morph formulas based on XY position
    // X = distortion intensity/character (left=subtle, right=aggressive)
    // Y = brightness/filter (bottom=dark, top=bright)

    // Distortion Amount: X drives it heavily, Y adds slight boost
    // Range: 0-100
    setParam("distortionAmount", x * 80.0f + y * 20.0f);

    // Input Gain: X drives input harder (30-70 range for headroom)
    setParam("inputGain", 30.0f + x * 40.0f);

    // Hi-Pass Filter: Y controls brightness (20-300Hz)
    setParam("highPassFreq", 20.0f + y * 280.0f);

    // Dist Mix: Both axes contribute (50-100%)
    setParam("distMix", 50.0f + x * 25.0f + y * 25.0f);

    // Waveshaper Mix: X is main driver (0-80%)
    setParam("waveshaperMix", x * 60.0f + y * 20.0f);

    // Output Gain: Compensate for increased distortion (70-30 inverse)
    setParam("outputGain", 70.0f - x * 30.0f - y * 10.0f);
}

void PluginEditor::randomizeAllParameters()
{
    // Create random number generator
    juce::Random random(juce::Time::currentTimeMillis());

    // Helper lambda to randomize float parameters (with lock check)
    auto randomizeFloatParam = [this, &random](const juce::String& paramID, float min, float max)
    {
        if (isParameterLocked(paramID)) return;  // Skip if locked

        float randomValue = min + random.nextFloat() * (max - min);
        if (auto* param = audioProcessor.parameters.getParameter(paramID))
        {
            param->setValueNotifyingHost(param->convertTo0to1(randomValue));
        }
    };

    // Helper lambda to randomize choice parameters (with lock check)
    auto randomizeChoiceParam = [this, &random](const juce::String& paramID, int numChoices)
    {
        if (isParameterLocked(paramID)) return;  // Skip if locked

        int randomChoice = random.nextInt(numChoices);
        if (auto* param = audioProcessor.parameters.getParameter(paramID))
        {
            param->setValueNotifyingHost(randomChoice / (float)(numChoices - 1));
        }
    };

    // Helper lambda to randomize boolean parameters (with lock check)
    auto randomizeBoolParam = [this, &random](const juce::String& paramID)
    {
        if (isParameterLocked(paramID)) return;  // Skip if locked

        bool randomValue = random.nextBool();
        if (auto* param = audioProcessor.parameters.getParameter(paramID))
        {
            param->setValueNotifyingHost(randomValue ? 1.0f : 0.0f);
        }
    };

    // DISTORTION SECTION
    randomizeFloatParam("inputGain", 0.0f, 100.0f);
    randomizeFloatParam("outputGain", 0.0f, 100.0f);
    randomizeFloatParam("distortionAmount", 0.0f, 100.0f);
    // highPassFreq now drives the multimode input filter (HP/LP/BP). Keep the random
    // sweep in a musical 20-2000 Hz band rather than the full 20-20000 Hz range so
    // results stay usable across all three modes.
    randomizeFloatParam("highPassFreq", 20.0f, 2000.0f);
    randomizeChoiceParam("filterMode", 3);  // High Pass / Low Pass / Band Pass
    // subGuardFreq spans 0-200 Hz where 0 = OFF (no band-split); include the OFF end.
    randomizeFloatParam("subGuardFreq", 0.0f, DSPConstants::SUBGUARD_FREQ_MAX);
    randomizeChoiceParam("clipType", 7);
    randomizeFloatParam("distMix", 0.0f, 100.0f);
    randomizeFloatParam("tone", 2000.0f, 20000.0f);
    randomizeFloatParam("waveshaperMix", 0.0f, 100.0f);

    // LFO SECTION
    randomizeFloatParam("lfoRate", 0.0f, 10.0f);  // Matches layout range (0 = LFO off)
    randomizeFloatParam("lfoDepth", 0.0f, 100.0f);
    randomizeBoolParam("lfoEnabled");
    randomizeBoolParam("lfoInvert");
    randomizeChoiceParam("lfoWaveform", 5);  // Sine, Triangle, Square, Saw, Random
    randomizeChoiceParam("lfoDestination", 5);  // Distortion, Tone, Hi-Pass, Dist Mix, Output Gain

    // COMPRESSION SECTION
    randomizeFloatParam("compPeakReduction", 0.0f, 100.0f);
    randomizeFloatParam("compMakeupGain", 0.0f, 100.0f);
    randomizeChoiceParam("compRatio", 2);
    randomizeBoolParam("compEnabled");

    // CHARACTER
    randomizeBoolParam("extremeEnabled");
    randomizeBoolParam("cleanBoost");
    // NOTE: globalMix, autoGainEnabled, waveshaperClean, linearPhaseDry, and the LFO
    // BPM-sync transport params are intentionally left fixed (global/mode/quality/
    // transport controls the user sets deliberately, not part of a sound roll).
}

juce::File PluginEditor::getPresetDirectory()
{
    // Get user's AppData folder
    auto presetDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Monolit Beatz")
        .getChildFile("Sledge Distortion")
        .getChildFile("Presets");

    // Create directory if it doesn't exist
    if (!presetDir.exists())
        presetDir.createDirectory();

    return presetDir;
}

void PluginEditor::savePreset(const juce::String& presetName)
{
    // Create XML document
    juce::XmlElement preset("Preset");
    preset.setAttribute("name", presetName);

    // Save all parameter values
    auto state = audioProcessor.parameters.copyState();
    // Stamp the schema version so presets migrate the same way host state does.
    state.setProperty(PluginProcessor::stateVersionAttribute,
                      PluginProcessor::currentStateVersion, nullptr);
    auto stateXml = state.createXml();
    if (stateXml != nullptr)
        preset.addChildElement(stateXml.release());

    // Save lock states
    auto* locksXml = new juce::XmlElement("ParameterLocks");
    for (const auto& pair : parameterLocks)
    {
        locksXml->setAttribute(pair.first, pair.second);
    }
    preset.addChildElement(locksXml);

    // Save to file
    auto presetFile = getPresetDirectory().getChildFile(presetName + ".xml");
    preset.writeTo(presetFile);
}

void PluginEditor::loadPreset(const juce::String& presetName)
{
    auto presetFile = getPresetDirectory().getChildFile(presetName + ".xml");

    if (presetFile.existsAsFile())
    {
        auto xml = juce::parseXML(presetFile);
        if (xml != nullptr)
        {
            // Load parameter values - first child is the parameter state
            auto* stateXml = xml->getFirstChildElement();
            if (stateXml != nullptr && stateXml->getTagName() != "ParameterLocks")
            {
                juce::ValueTree state = juce::ValueTree::fromXml(*stateXml);
                audioProcessor.parameters.replaceState(state);
                audioProcessor.migrateState(*stateXml);
            }

            // Load lock states
            auto* locksXml = xml->getChildByName("ParameterLocks");
            if (locksXml != nullptr)
            {
                // Restore all lock states from preset
                for (auto& pair : parameterLocks)
                {
                    pair.second = locksXml->getBoolAttribute(pair.first, false);
                }
            }
            else
            {
                // No lock data in preset - clear all locks
                for (auto& pair : parameterLocks)
                {
                    pair.second = false;
                }
            }
            updateLockIcons();
        }
    }
}

void PluginEditor::deletePreset(const juce::String& presetName)
{
    auto presetFile = getPresetDirectory().getChildFile(presetName + ".xml");
    if (presetFile.existsAsFile())
    {
        presetFile.deleteFile();
    }
}

void PluginEditor::refreshPresetList()
{
    presetSelector.clear();

    // Add factory presets (built-in)
    int id = 1;
    presetSelector.addSectionHeading("Factory Presets");
    for (const auto& preset : FactoryPresets::all())
        presetSelector.addItem(preset.name, id++);
    presetSelector.addSeparator();

    // Add user presets
    auto presetDir = getPresetDirectory();
    auto presetFiles = presetDir.findChildFiles(juce::File::findFiles, false, "*.xml");

    if (presetFiles.size() > 0)
    {
        presetSelector.addSectionHeading("User Presets");
        for (auto& file : presetFiles)
        {
            presetSelector.addItem(file.getFileNameWithoutExtension(), id++);
        }
    }

    // Add Save/Delete actions at the bottom
    presetSelector.addSeparator();
    presetSelector.addItem("Save Preset...", 9990);
    presetSelector.addItem("Delete Preset...", 9991);

    // Restore the last selected preset name; fall back to "Default" if not found
    juce::String savedName = audioProcessor.parameters.state
        .getProperty("currentPresetName", "Default").toString();
    bool found = false;
    for (int i = 0; i < presetSelector.getNumItems(); ++i)
    {
        if (presetSelector.getItemText(i) == savedName)
        {
            presetSelector.setSelectedItemIndex(i, juce::dontSendNotification);
            found = true;
            break;
        }
    }
    if (!found)
        presetSelector.setSelectedId(1, juce::dontSendNotification);
}

void PluginEditor::loadFactoryPreset(const juce::String& presetName)
{
    // Clear all locks for factory presets
    for (auto& pair : parameterLocks)
    {
        pair.second = false;
    }
    updateLockIcons();

    // Apply the named preset from the shared factory table (single source of
    // truth — see FactoryPresets.h). The table applies a neutral baseline first,
    // then the preset's overrides, so loads are deterministic.
    FactoryPresets::apply(audioProcessor.parameters, presetName);
}

// ============================================================================
// Settings Overlay Methods
// ============================================================================

void PluginEditor::showFirstRunNotice()
{
    if (firstRunNotice) return;
    firstRunNotice = std::make_unique<FirstRunNotice>();
    addAndMakeVisible(*firstRunNotice);
    firstRunNotice->setBounds(getLocalBounds());
    firstRunNotice->onDismiss = [this]() { hideFirstRunNotice(); };
}

void PluginEditor::hideFirstRunNotice()
{
    firstRunNotice.reset();
}

void PluginEditor::showSettingsOverlay()
{
    if (settingsOverlay) return;

    settingsOverlay = std::make_unique<SettingsOverlay>(audioProcessor.parameters, settingsState, audioProcessor);
    addAndMakeVisible(*settingsOverlay);
    settingsOverlay->setBounds(getLocalBounds());

    settingsOverlay->onClose = [this]() { hideSettingsOverlay(); };
    settingsOverlay->onOscilloscopeToggled = [this](bool enabled) {
        scopeButton.setFullMode(enabled);  // keep the toolbar duplicate in sync
        applyOscilloscopeEnabled(enabled);
    };
    settingsOverlay->onOverlayModeChanged = [this](int mode) {
        applyScopeOverlayMode(mode);
    };
    settingsOverlay->onScopeChannelModeChanged = [this](bool isStereo) {
        oscilloscope.setStereoMode(isStereo);
    };
    settingsOverlay->onWindowScaleChanged = [this](int scalePercent) {
        applyWindowScale(scalePercent);
    };
    settingsOverlay->onScopeLengthChanged = [this](int length) {
        oscilloscope.setScopeLength(length);
    };
}

void PluginEditor::hideSettingsOverlay()
{
    settingsOverlay.reset();
    startFoldAnimation(); // animate fold/unfold to match the current scope setting
    juce::Component::SafePointer<PluginEditor> safeThis (this);
    juce::MessageManager::callAsync ([safeThis] { if (safeThis != nullptr) safeThis->saveSettings(); });
}

void PluginEditor::applyWindowScale(int scalePercent)
{
    const float baseH = settingsState.oscilloscopeEnabled ? 564.0f : 180.0f;
    const int w = juce::roundToInt(960.0f * scalePercent / 100.0f);
    const int h = juce::roundToInt(baseH * scalePercent / 100.0f);
    setSize(w, h);
}

void PluginEditor::applyScopeOverlayMode(int mode)
{
    // 0 = Off (scope clicks pass through), 1 = XY Morph, 2 = Graphic EQ. Only one
    // overlay is interactive/visible at a time; the others release the mouse.
    settingsState.scopeOverlayMode = juce::jlimit(0, 2, mode);
    if (settingsState.scopeOverlayMode == 2)
        graphicEqOverlay.syncFromParams();   // seed the curve from the live params
    eqButton.setActive(settingsState.scopeOverlayMode == 2);  // keep the toolbar toggle in sync
    updateScopeOverlays(oscilloscope.getAlpha());
}

void PluginEditor::updateScopeOverlays(float alpha)
{
    // Both overlays share the scope's slot. Which one is live depends on the
    // selected mode; whether ANY is shown tracks the scope's own visibility/alpha
    // (so they fade with it during the fold animation).
    const int  mode         = settingsState.scopeOverlayMode;
    const bool scopeShowing = oscilloscope.isVisible();
    const bool xyActive     = scopeShowing && mode == 1;
    const bool eqActive     = scopeShowing && mode == 2;

    xyMorphPad.setVisible(xyActive);
    xyMorphPad.setAlpha(alpha);
    xyMorphPad.setInterceptsMouseClicks(xyActive, false);
    if (xyActive && ! xyMorphPad.isTimerRunning())  xyMorphPad.startTimerHz(30);
    if (! xyActive && xyMorphPad.isTimerRunning())  xyMorphPad.stopTimer();

    graphicEqOverlay.setVisible(eqActive);
    graphicEqOverlay.setAlpha(alpha);
    graphicEqOverlay.setInterceptsMouseClicks(eqActive, false);
    if (eqActive && ! graphicEqOverlay.isTimerRunning())  graphicEqOverlay.startTimerHz(30);
    if (! eqActive && graphicEqOverlay.isTimerRunning())  graphicEqOverlay.stopTimer();
}

void PluginEditor::applyOscilloscopeEnabled(bool enabled)
{
    if (enabled)
    {
        const bool willAnimateFold = (allowFoldAnimation && settingsOverlay == nullptr);

        // For the animated path: pre-set alpha to 0 BEFORE making them visible
        // so the first paint after the upcoming snap-resize is transparent —
        // then the fade ramps alpha up to 1. Without this, components would
        // flash fully-opaque for a frame between setVisible and the first fade tick.
        if (willAnimateFold)
        {
            oscilloscope.setAlpha (0.0f);
            phaseCorrelationMeter.setAlpha (0.0f);
        }

        oscilloscope.setVisible(true);
        phaseCorrelationMeter.setVisible(true);
        // Sync the active overlay (XY / EQ) to the scope's visibility + fade alpha.
        updateScopeOverlays(willAnimateFold ? 0.0f : 1.0f);

        // Defer the scope's 30Hz repaint timer until the fade finishes; running
        // it during the fade just wastes paints on a near-transparent component.
        if (! willAnimateFold)
            oscilloscope.startTimerHz(DSPConstants::SCOPE_REFRESH_RATE_HZ);
    }
    else
    {
        // Collapse expanded sections so compact window ends clean
        if (isLFOExpanded)        { isLFOExpanded = false;        lfoTabHeader.setExpanded(false);        updateLFOVisibility(); }
        if (isCompressionExpanded){ isCompressionExpanded = false; compressionTabHeader.setExpanded(false); updateCompressionVisibility(); }

        // When not animating (overlay open), hide immediately as before.
        // When animating, the scope stays visible during the shrink and is hidden
        // once the fold completes (see finishFoldAnimation).
        if (settingsOverlay || !allowFoldAnimation)
        {
            oscilloscope.stopTimer();
            oscilloscope.setVisible(false);
            phaseCorrelationMeter.setVisible(false);
            updateScopeOverlays(1.0f);  // scope hidden → overlays hidden, timers stop
        }
    }

    // Don't resize while settings overlay is open — fold happens on close
    if (settingsOverlay)
    {
        resized(); // bounds must be recalculated when visibility changes mid-session
        return;
    }

    if (allowFoldAnimation)
        startFoldAnimation();
    else
        applyWindowScale(settingsState.windowScalePercent); // instant during construction
}

void PluginEditor::toggleOscilloscopeMode()
{
    const bool newState = !settingsState.oscilloscopeEnabled;
    settingsState.oscilloscopeEnabled = newState;
    scopeButton.setFullMode(newState);
    applyOscilloscopeEnabled(newState);
    // Defer the disk write — a synchronous XML save on the message thread here
    // would block the fold timer's first tick (~click→first-frame latency).
    juce::Component::SafePointer<PluginEditor> safeThis (this);
    juce::MessageManager::callAsync ([safeThis] { if (safeThis != nullptr) safeThis->saveSettings(); });
}

void PluginEditor::toggleGraphicEqOverlay()
{
    // Quick toggle: turn the Graphic EQ overlay on (mode 2), or off (mode 0) if
    // it is already the active overlay. applyScopeOverlayMode syncs eqButton's lit
    // state; the Settings dropdown re-reads settingsState next time it opens.
    const int newMode = (settingsState.scopeOverlayMode == 2) ? 0 : 2;
    applyScopeOverlayMode(newMode);
    juce::Component::SafePointer<PluginEditor> safeThis (this);
    juce::MessageManager::callAsync ([safeThis] { if (safeThis != nullptr) safeThis->saveSettings(); });
}

void PluginEditor::startFoldAnimation()
{
    // Snap + fade strategy:
    //  - Expand:   snap the window to full size in ONE atomic resize, then fade
    //              scope/XY/phase alpha 0 → 1.
    //  - Collapse: fade alpha 1 → 0 with window held at full size, then snap to
    //              compact in finishFoldAnimation().
    // This avoids the per-frame setSize() chain that produces occasional X11
    // resize tearing on Linux — there's at most one ConfigureNotify per toggle.
    const bool expanding = settingsState.oscilloscopeEnabled;
    const float baseH = expanding ? 564.0f : 180.0f;
    const int targetW = juce::roundToInt(960.0f * settingsState.windowScalePercent / 100.0f);
    const int targetH = juce::roundToInt(baseH  * settingsState.windowScalePercent / 100.0f);

    if (expanding)
    {
        // Single atomic resize. Components are already setVisible(true) with
        // alpha 0 from applyOscilloscopeEnabled, so this snap paints them
        // transparent — the fade ramps them in over the next ~280ms.
        setSize (targetW, targetH);
    }
    else
    {
        // Keep window at full size while the fade-out plays. Sync width only if
        // a scale change desynced it; height holds until finish.
        if (getWidth() != targetW)
            setSize (targetW, getHeight());
    }

    foldStartMs   = juce::Time::getMillisecondCounterHiRes();
    foldAnimating = true;
    foldAnimTimer.startTimerHz (60); // matches typical display refresh
}

void PluginEditor::stepFoldAnimation()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const double t = juce::jlimit (0.0, 1.0, (now - foldStartMs) / foldDurationMs);

    if (t >= 1.0)
    {
        finishFoldAnimation();
        return;
    }

    // Quintic ease-in-out: soft start, soft settle. No resize-tear risk here
    // since we're only changing alpha — pick the smoothest-looking curve.
    const double e = t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
    const float alpha = (float) (settingsState.oscilloscopeEnabled ? e : (1.0 - e));

    oscilloscope.setAlpha (alpha);
    phaseCorrelationMeter.setAlpha (alpha);
    updateScopeOverlays(alpha);
}

void PluginEditor::finishFoldAnimation()
{
    foldAnimating = false;
    foldAnimTimer.stopTimer();

    if (! settingsState.oscilloscopeEnabled)
    {
        // Collapse: hide components, reset their alpha for next expand, then
        // snap the window to compact size in a single resize.
        oscilloscope.stopTimer();
        oscilloscope.setAlpha (1.0f);
        phaseCorrelationMeter.setAlpha (1.0f);
        oscilloscope.setVisible (false);
        phaseCorrelationMeter.setVisible (false);
        updateScopeOverlays (1.0f);  // scope hidden → overlays hidden, timers stop
        applyWindowScale (settingsState.windowScalePercent);
    }
    else
    {
        // Expand done: lock alpha at 1.0 and kick off live scope repaints.
        oscilloscope.setAlpha (1.0f);
        phaseCorrelationMeter.setAlpha (1.0f);
        updateScopeOverlays (1.0f);
        if (! oscilloscope.isTimerRunning())
            oscilloscope.startTimerHz (DSPConstants::SCOPE_REFRESH_RATE_HZ);
    }
}

void PluginEditor::updateExpansionBackdrop()
{
    const bool compact = !settingsState.oscilloscopeEnabled;
    const bool anyExpanded = isLFOExpanded || isCompressionExpanded;

    if (compact && anyExpanded)
    {
        // Compute tight bounding box from the actual control positions
        juce::Rectangle<int> box;
        auto grow = [&](juce::Component& c) {
            if (c.isVisible()) box = box.isEmpty() ? c.getBounds() : box.getUnion(c.getBounds());
        };
        if (isLFOExpanded)
        {
            grow(lfoRateSlider);  grow(lfoRateLabel);
            grow(lfoDepthSlider); grow(lfoDepthLabel);
            grow(lfoWaveformComboBox); grow(lfoWaveformLabel);
            grow(lfoDestinationComboBox); grow(lfoDestinationLabel);
            grow(lfoBpmSyncButton);
            grow(lfoInvertButton);
        }
        if (isCompressionExpanded)
        {
            grow(compPeakReductionSlider); grow(compPeakReductionLabel);
            grow(compMakeupGainSlider);    grow(compMakeupGainLabel);
            grow(compRatioComboBox);       grow(compRatioLabel);
            grow(gainReductionMeter);
        }
        expansionBackdrop.setBounds(box.expanded(8, 6));
        expansionBackdrop.setVisible(true);
        expansionBackdrop.toFront(false);
        // Bring expanded controls in front of the backdrop
        if (isLFOExpanded)
        {
            lfoRateSlider.toFront(false);     lfoRateLabel.toFront(false);     lfoRateLock.toFront(false);
            lfoDepthSlider.toFront(false);    lfoDepthLabel.toFront(false);    lfoDepthLock.toFront(false);
            lfoWaveformComboBox.toFront(false); lfoWaveformLabel.toFront(false);
            lfoDestinationComboBox.toFront(false); lfoDestinationLabel.toFront(false); lfoDestinationLock.toFront(false);
            lfoBpmSyncButton.toFront(false);
            lfoInvertButton.toFront(false);
        }
        if (isCompressionExpanded)
        {
            compPeakReductionSlider.toFront(false); compPeakReductionLabel.toFront(false); compPeakReductionLock.toFront(false);
            compMakeupGainSlider.toFront(false);    compMakeupGainLabel.toFront(false);    compMakeupGainLock.toFront(false);
            compRatioComboBox.toFront(false);       compRatioLabel.toFront(false);         compRatioLock.toFront(false);
            gainReductionMeter.toFront(false);
        }
    }
    else
    {
        expansionBackdrop.setVisible(false);
    }
}

void PluginEditor::loadSettings()
{
    settingsState.loadFromFile(getSettingsFile());
}

void PluginEditor::saveSettings()
{
    settingsState.saveToFile(getSettingsFile());
}

juce::File PluginEditor::getSettingsFile()
{
    return getPresetDirectory().getParentDirectory().getChildFile("settings.xml");
}

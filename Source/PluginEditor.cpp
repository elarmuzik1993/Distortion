/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "GitVersion.h"

//Setup Slider in Constructor Here

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), oscilloscope(p), gainReductionMeter(p), phaseCorrelationMeter(p)
{
    // Font scale factor (672/960 = 0.7)
    const float fs = 672.0f / 960.0f;

    addAndMakeVisible(oscilloscope);

    // XY Morph Pad overlay (invisible, on top of oscilloscope)
    addAndMakeVisible(xyMorphPad);
    xyMorphPad.onPositionChanged = [this](float x, float y)
    {
        morphDistortionParameters(x, y);
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

            w->enterModalState(true, juce::ModalCallbackFunction::create([this, w](int result)
            {
                if (result == 1)
                {
                    juce::String presetName = w->getTextEditorContents("presetName");
                    if (presetName.isNotEmpty())
                    {
                        savePreset(presetName);
                        audioProcessor.parameters.state.setProperty("currentPresetName", presetName, nullptr);
                        refreshPresetList();
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

            deleteMenu.showMenuAsync(juce::PopupMenu::Options(), [this, presetFiles](int result)
            {
                if (result > 0)
                {
                    juce::String presetName = presetFiles[result - 1].getFileNameWithoutExtension();

                    auto options = juce::MessageBoxOptions()
                        .withIconType(juce::MessageBoxIconType::WarningIcon)
                        .withTitle("Delete Preset")
                        .withMessage("Are you sure you want to delete '" + presetName + "'?")
                        .withButton("OK")
                        .withButton("Cancel");

                    juce::AlertWindow::showAsync(options, [this, presetName](int r)
                    {
                        if (r == 1)
                        {
                            deletePreset(presetName);
                            refreshPresetList();
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

            if (presetName == "Default" || presetName == "Warm Tube" ||
                presetName == "Hard Clip" || presetName == "Soft Saturation" ||
                presetName == "808 Safe" || presetName == "Parallel Grit" ||
                presetName == "Vocal Warmth" || presetName == "EXTREME")
            {
                loadFactoryPreset(presetName);
            }
            else
            {
                loadPreset(presetName);
            }

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
        menu.addItem("Hi-Pass Filter", true, isParameterLocked("highPassFreq"),
                     [this]() { toggleParameterLock("highPassFreq"); });
        menu.addItem("Sub Guard", true, isParameterLocked("subGuardFreq"),
                     [this]() { toggleParameterLock("subGuardFreq"); });
        menu.addItem("Clip Type", true, isParameterLocked("clipType"),
                     [this]() { toggleParameterLock("clipType"); });
        menu.addItem("Dist Mix", true, isParameterLocked("distMix"),
                     [this]() { toggleParameterLock("distMix"); });

        menu.addSeparator();
        menu.addSectionHeader("LFO");
        menu.addItem("LFO Rate", true, isParameterLocked("lfoRate"),
                     [this]() { toggleParameterLock("lfoRate"); });
        menu.addItem("LFO Depth", true, isParameterLocked("lfoDepth"),
                     [this]() { toggleParameterLock("lfoDepth"); });

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
    parameterLocks["subGuardFreq"] = false;
    parameterLocks["clipType"] = false;
    parameterLocks["distMix"] = false;
    parameterLocks["lfoRate"] = false;
    parameterLocks["lfoDepth"] = false;
    parameterLocks["lfoEnabled"] = false;
    parameterLocks["lfoDestination"] = false;
    parameterLocks["compPeakReduction"] = false;
    parameterLocks["compMakeupGain"] = false;
    parameterLocks["compRatio"] = false;
    parameterLocks["compEnabled"] = false;
    parameterLocks["autoGainEnabled"] = false;
    parameterLocks["extremeEnabled"] = false;
    parameterLocks["globalMix"] = false;

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

    // Load UI settings
    loadSettings();
    // Sync oversampling setting to processor (in case it was saved as non-default)
    audioProcessor.requestOversamplingRebuild(settingsState.oversamplingMode);
    applyOscilloscopeEnabled(settingsState.oscilloscopeEnabled);
    scopeButton.setFullMode(settingsState.oscilloscopeEnabled);
    oscilloscope.setStereoMode(settingsState.oscilloscopeStereo);
    oscilloscope.setScopeLength(settingsState.scopeLength);

    // Set window size AFTER all components are added so resized() can position them all
    setSize(672, 395);
    applyWindowScale(settingsState.windowScalePercent);

    // Start timer for LFO modulation visual feedback (30Hz)
    startTimerHz(60);

    // Construction-time sizing is done; allow animated folds from now on
    allowFoldAnimation = true;
}

PluginEditor::~PluginEditor()
{
    stopTimer();

    // Destroy settings overlay before LookAndFeel instances
    settingsOverlay.reset();

    // Reset LookAndFeel to nullptr before destruction to prevent crash
    // Components must not reference a LookAndFeel that may be destroyed before them
    compEnableToggle.setLookAndFeel(nullptr);
    lfoEnableToggle.setLookAndFeel(nullptr);
    clipTypeComboBox.setLookAndFeel(nullptr);
    compRatioComboBox.setLookAndFeel(nullptr);
    lfoWaveformComboBox.setLookAndFeel(nullptr);
    lfoDestinationComboBox.setLookAndFeel(nullptr);
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
        lx += sKnob + sSpacing;

        lfoDepthSlider.setBounds(lx, expandedY, sKnob, sKnob);
        lfoDepthLabel.setBounds(lx, expandedY + sKnob, sKnob, S(13));
        lfoDepthLock.setBounds(lx + sKnob - S(12) - S(2), expandedY + S(2), S(12), S(12));
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
        const int meterW = S(16);
        const int meterH = S(45);

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

    // Hi-Pass Filter
    highPassFreqSlider.setBounds(currentX, rowY, knobSize, knobSize);
    highPassFreqLabel.setBounds(currentX, rowY + knobSize + lockInset, knobSize, labelHeight);
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
    const bool visible = isLFOExpanded;

    // Knobs + labels
    lfoRateSlider.setVisible(visible);
    lfoRateLabel.setVisible(visible);
    lfoDepthSlider.setVisible(visible);
    lfoDepthLabel.setVisible(visible);

    // Dropdowns
    lfoWaveformComboBox.setVisible(visible);
    lfoWaveformLabel.setVisible(visible);
    lfoDestinationComboBox.setVisible(visible);   // NEW
    lfoDestinationLabel.setVisible(visible);      // NEW
    lfoDestinationLock.setVisible(visible);       // NEW

    // Lock icons (toggle + lock always visible, handled separately)
    lfoRateLock.setVisible(visible);
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

    const float depthNorm = lfoDepth;

    // Set arc on the targeted knob, clear others
    distortionAmountSlider.setLFOArc(destination == 0, lfoPhase, depthNorm);
    toneSlider.setLFOArc(destination == 1, lfoPhase, depthNorm);
    highPassFreqSlider.setLFOArc(destination == 2, lfoPhase, depthNorm);
    distMixSlider.setLFOArc(destination == 3, lfoPhase, depthNorm);
    outputGainSlider.setLFOArc(destination == 4, lfoPhase, depthNorm);
}

void PluginEditor::timerCallback()
{
    if (foldAnimating)
        stepFoldAnimation();

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
    randomizeFloatParam("highPassFreq", 20.0f, 500.0f);
    randomizeFloatParam("subGuardFreq", 50.0f, 200.0f);  // Sub Guard frequency range
    randomizeChoiceParam("clipType", 7);
    randomizeFloatParam("distMix", 0.0f, 100.0f);

    // LFO SECTION
    randomizeFloatParam("lfoRate", 0.1f, 10.0f);
    randomizeFloatParam("lfoDepth", 0.0f, 100.0f);
    randomizeBoolParam("lfoEnabled");
    randomizeChoiceParam("lfoDestination", 5);  // 5 destinations: Distortion, Tone, Hi-Pass, Dist Mix, Output Gain

    // COMPRESSION SECTION
    randomizeFloatParam("compPeakReduction", 0.0f, 100.0f);
    randomizeFloatParam("compMakeupGain", 0.0f, 100.0f);
    randomizeChoiceParam("compRatio", 2);
    randomizeBoolParam("compEnabled");
    randomizeBoolParam("autoGainEnabled");
    randomizeBoolParam("extremeEnabled");
    randomizeFloatParam("globalMix", 50.0f, 100.0f);
}

juce::File PluginEditor::getPresetDirectory()
{
    // Get user's AppData folder
    auto presetDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("MonolitBeats")
        .getChildFile("Monolit Distortion")
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
    presetSelector.addItem("Default", id++);
    presetSelector.addItem("Warm Tube", id++);
    presetSelector.addItem("Hard Clip", id++);
    presetSelector.addItem("Soft Saturation", id++);
    presetSelector.addItem("808 Safe", id++);
    presetSelector.addItem("Parallel Grit", id++);
    presetSelector.addItem("Vocal Warmth", id++);
    presetSelector.addItem("EXTREME", id++);
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

    // Helper lambda to set parameter values
    auto setParam = [this](const juce::String& paramID, float value)
    {
        if (auto* param = audioProcessor.parameters.getParameter(paramID))
        {
            param->setValueNotifyingHost(param->convertTo0to1(value));
        }
    };

    // Reset every parameter to a neutral baseline so factory presets load deterministically.
    setParam("inputGain", 50.0f);
    setParam("outputGain", 50.0f);
    setParam("distortionAmount", 0.0f);
    setParam("highPassFreq", 20.0f);
    setParam("subGuardFreq", 60.0f);
    setParam("clipType", 0.0f);
    setParam("distMix", 100.0f);
    setParam("tone", 20000.0f);
    setParam("waveshaperClean", 0.0f);
    setParam("waveshaperMix", 0.0f);
    setParam("lfoRate", 0.0f);
    setParam("lfoDepth", 0.0f);
    setParam("lfoWaveform", 0.0f);
    setParam("lfoEnabled", 0.0f);
    setParam("lfoDestination", 0.0f);
    setParam("compPeakReduction", 0.0f);
    setParam("compMakeupGain", 50.0f);
    setParam("compRatio", 0.0f);
    setParam("compEnabled", 0.0f);
    setParam("autoGainEnabled", 1.0f);
    setParam("extremeEnabled", 0.0f);
    setParam("globalMix", 100.0f);

    if (presetName == "Default")
    {
        // Baseline already applied above — nothing to override.
    }
    else if (presetName == "Warm Tube")
    {
        setParam("inputGain", 60.0f);
        setParam("outputGain", 45.0f);
        setParam("distortionAmount", 30.0f);
        setParam("highPassFreq", 80.0f);
        setParam("clipType", 1.0f);  // Tube Overdrive
        setParam("distMix", 70.0f);
    }
    else if (presetName == "Hard Clip")
    {
        setParam("inputGain", 70.0f);
        setParam("outputGain", 40.0f);
        setParam("distortionAmount", 70.0f);
        setParam("highPassFreq", 100.0f);
        setParam("subGuardFreq", 150.0f);
        setParam("clipType", 0.0f);  // Brutal Fuzz (closest to hard clipping)
        setParam("compPeakReduction", 30.0f);
        setParam("compMakeupGain", 60.0f);
        setParam("compEnabled", 1.0f);
    }
    else if (presetName == "Soft Saturation")
    {
        setParam("inputGain", 55.0f);
        setParam("outputGain", 48.0f);
        setParam("distortionAmount", 20.0f);
        setParam("highPassFreq", 40.0f);
        setParam("clipType", 3.0f);  // Tape Saturation
        setParam("distMix", 50.0f);
    }
    else if (presetName == "808 Safe")
    {
        setParam("inputGain", 65.0f);
        setParam("outputGain", 45.0f);
        setParam("distortionAmount", 60.0f);
        setParam("highPassFreq", 150.0f);
        setParam("subGuardFreq", 150.0f);
        setParam("clipType", 3.0f);  // Tape Saturation (sub-friendly)
    }
    else if (presetName == "Parallel Grit")
    {
        // Heavy distortion blended in parallel via global mix
        setParam("inputGain", 58.0f);
        setParam("distortionAmount", 85.0f);
        setParam("highPassFreq", 60.0f);
        setParam("clipType", 5.0f);  // Diode Clipper
        setParam("globalMix", 35.0f);
        setParam("compPeakReduction", 20.0f);
        setParam("compEnabled", 1.0f);
    }
    else if (presetName == "Vocal Warmth")
    {
        // Tube warmth with slow LFO tone sweep
        setParam("distortionAmount", 25.0f);
        setParam("highPassFreq", 100.0f);
        setParam("clipType", 1.0f);       // Tube Overdrive
        setParam("tone", 6000.0f);
        setParam("distMix", 60.0f);
        setParam("lfoEnabled", 1.0f);
        setParam("lfoDestination", 1.0f); // Tone Filter
        setParam("lfoRate", 0.3f);
        setParam("lfoDepth", 30.0f);
    }
    else if (presetName == "EXTREME")
    {
        // Over-the-top saturation using EXTREME toggle + limiter compression
        setParam("inputGain", 75.0f);
        setParam("outputGain", 35.0f);
        setParam("distortionAmount", 90.0f);
        setParam("highPassFreq", 120.0f);
        setParam("subGuardFreq", 100.0f);
        setParam("clipType", 6.0f);      // Decimator
        setParam("extremeEnabled", 1.0f);
        setParam("compPeakReduction", 40.0f);
        setParam("compRatio", 1.0f);     // Limit mode
        setParam("compEnabled", 1.0f);
    }
}

// ============================================================================
// Settings Overlay Methods
// ============================================================================

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
    saveSettings();
    settingsOverlay.reset();
    startFoldAnimation(); // animate fold/unfold to match the current scope setting
}

void PluginEditor::applyWindowScale(int scalePercent)
{
    const float baseH = settingsState.oscilloscopeEnabled ? 564.0f : 180.0f;
    const int w = juce::roundToInt(960.0f * scalePercent / 100.0f);
    const int h = juce::roundToInt(baseH * scalePercent / 100.0f);
    setSize(w, h);
}

void PluginEditor::applyOscilloscopeEnabled(bool enabled)
{
    if (enabled)
    {
        // Reveal scope components up front so they animate into view as the window grows
        oscilloscope.setVisible(true);
        xyMorphPad.setVisible(true);
        phaseCorrelationMeter.setVisible(true);
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
            xyMorphPad.setVisible(false);
            phaseCorrelationMeter.setVisible(false);
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
    saveSettings();
}

void PluginEditor::startFoldAnimation()
{
    const float baseH = settingsState.oscilloscopeEnabled ? 564.0f : 180.0f;
    const int targetW = juce::roundToInt(960.0f * settingsState.windowScalePercent / 100.0f);
    const int targetH = juce::roundToInt(baseH  * settingsState.windowScalePercent / 100.0f);

    // Width never changes between modes; set it instantly so only height animates.
    if (getWidth() != targetW)
        setSize(targetW, getHeight());

    foldStartHeight  = getHeight();
    foldTargetHeight = targetH;

    if (foldStartHeight == foldTargetHeight)
    {
        finishFoldAnimation();
        return;
    }

    foldStartMs   = juce::Time::getMillisecondCounterHiRes();
    foldAnimating = true; // stepped from timerCallback() at 60Hz
}

void PluginEditor::stepFoldAnimation()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const double t = (now - foldStartMs) / foldDurationMs;

    if (t >= 1.0)
    {
        finishFoldAnimation();
        return;
    }

    const double e = t * t * (3.0 - 2.0 * t); // smoothstep ease in/out
    const int h = juce::roundToInt(foldStartHeight + (foldTargetHeight - foldStartHeight) * e);
    setSize(getWidth(), h);
}

void PluginEditor::finishFoldAnimation()
{
    foldAnimating = false;
    setSize(getWidth(), foldTargetHeight);

    // If we just folded to compact, hide the scope components now the shrink is done.
    if (!settingsState.oscilloscopeEnabled)
    {
        oscilloscope.stopTimer();
        oscilloscope.setVisible(false);
        xyMorphPad.setVisible(false);
        phaseCorrelationMeter.setVisible(false);
        resized();
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

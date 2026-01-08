/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginEditor.h"
#include "PluginProcessor.h"

//Setup Slider in Constructor Here

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), oscilloscope(p), gainReductionMeter(p)
{
    addAndMakeVisible(oscilloscope);

    // XY Morph Pad overlay (invisible, on top of oscilloscope)
    addAndMakeVisible(xyMorphPad);
    xyMorphPad.onPositionChanged = [this](float x, float y)
    {
        morphDistortionParameters(x, y);
    };

    addAndMakeVisible(gainReductionMeter);

    // Setup title label
    addAndMakeVisible(titleLabel);
    titleLabel.setText("MONOLIT BEATZ", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(24.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF, 0x00, 0x44));
    titleLabel.setJustificationType(juce::Justification::centred);

    // Set fixed window size - no resizing allowed (increased width to fit all controls)
    setSize(960, 564);
    setResizeLimits(960, 564, 960, 564);
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

    // Setup waveshaper mix knob
    setupSlider(waveshaperSlider, waveshaperLabel, "Wave Mix",
        waveshaperAttachment, "waveshaperMix");

    // Setup compressor knobs
    setupSlider(compPeakReductionSlider, compPeakReductionLabel, "Peak Reduction",
        compPeakReductionAttachment, "compPeakReduction");
    setupSlider(compMakeupGainSlider, compMakeupGainLabel, "Makeup Gain",
        compMakeupGainAttachment, "compMakeupGain");
    setupSlider(compWetDrySlider, compWetDryLabel, "Wet/Dry",
        compWetDryAttachment, "compWetDry");
    setupSlider(compCrossoverSlider, compCrossoverLabel, "Crossover",
        compCrossoverAttachment, "compCrossover");

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
    compRatioLabel.setFont(juce::Font(10.0f, juce::Font::bold));

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
        resized(); // Recalculate layout with new height
    };
    
    // Setup 808-Safe Mode toggle
    addAndMakeVisible(bandSplitToggle);
    bandSplitToggle.setButtonText("808-Safe");
    bandSplitToggle.setLookAndFeel(&checkboxLookAndFeel);  // Apply custom black box with green tick

    bandSplitAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.parameters, "bandSplitEnabled", bandSplitToggle);

    addAndMakeVisible(bandSplitLabel);
    bandSplitLabel.setText("Clean Sub", juce::dontSendNotification);
    bandSplitLabel.setJustificationType(juce::Justification::centred);
    bandSplitLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    bandSplitLabel.setFont(juce::Font(12.0f, juce::Font::bold));  // Bigger and bold

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

    // Setup preset selector
    // Note: presetLabel removed - no "Preset:" text shown
    presetLabel.setText("", juce::dontSendNotification);
    presetLabel.setJustificationType(juce::Justification::centredRight);
    presetLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    presetLabel.setFont(juce::Font(12.0f, juce::Font::bold));

    addAndMakeVisible(presetSelector);
    presetSelector.setLookAndFeel(&comboBoxLookAndFeel);  // Apply neon red styling
    presetSelector.setTextWhenNothingSelected("Select Preset...");
    presetSelector.onChange = [this]()
    {
        if (presetSelector.getSelectedId() > 0)
        {
            juce::String presetName = presetSelector.getText();

            // Check if it's a factory preset or user preset
            if (presetName == "Default" || presetName == "Warm Tube" ||
                presetName == "Hard Clip" || presetName == "Soft Saturation" ||
                presetName == "808 Safe")
            {
                loadFactoryPreset(presetName);
            }
            else
            {
                loadPreset(presetName);
            }
        }
    };

    addAndMakeVisible(savePresetButton);
    savePresetButton.setButtonText("Save");

    // Apply neon red styling
    savePresetButton.setColour(juce::TextButton::buttonColourId, juce::Colours::black);
    savePresetButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFF, 0x00, 0x44));  // Neon red text
    savePresetButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);  // White when pressed
    savePresetButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xFF, 0x00, 0x44));  // Neon red when pressed
    savePresetButton.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xFF, 0x00, 0x44));  // Neon red outline

    savePresetButton.onClick = [this]()
    {
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
                    refreshPresetList();
                }
            }
            delete w;
        }));
    };

    addAndMakeVisible(deletePresetButton);
    deletePresetButton.setButtonText("Delete");

    // Apply neon red styling
    deletePresetButton.setColour(juce::TextButton::buttonColourId, juce::Colours::black);
    deletePresetButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFF, 0x00, 0x44));  // Neon red text
    deletePresetButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);  // White when pressed
    deletePresetButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xFF, 0x00, 0x44));  // Neon red when pressed
    deletePresetButton.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xFF, 0x00, 0x44));  // Neon red outline

    deletePresetButton.onClick = [this]()
    {
        if (presetSelector.getSelectedId() > 0)
        {
            juce::String presetName = presetSelector.getText();

            // Create confirmation dialog
            auto options = juce::MessageBoxOptions()
                .withIconType(juce::MessageBoxIconType::WarningIcon)
                .withTitle("Delete Preset")
                .withMessage("Are you sure you want to delete '" + presetName + "'?")
                .withButton("OK")
                .withButton("Cancel");

            juce::AlertWindow::showAsync(options, [this, presetName](int result)
            {
                if (result == 1)  // OK button pressed
                {
                    deletePreset(presetName);
                    refreshPresetList();
                }
            });
        }
    };

    // Load presets
    refreshPresetList();

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
        menu.addItem("808-Safe", true, isParameterLocked("bandSplitEnabled"),
                     [this]() { toggleParameterLock("bandSplitEnabled"); });
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
        menu.addItem("Comp Wet/Dry", true, isParameterLocked("compWetDry"),
                     [this]() { toggleParameterLock("compWetDry"); });
        menu.addItem("Comp Crossover", true, isParameterLocked("compCrossover"),
                     [this]() { toggleParameterLock("compCrossover"); });

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
    parameterLocks["bandSplitEnabled"] = false;
    parameterLocks["clipType"] = false;
    parameterLocks["distMix"] = false;
    parameterLocks["lfoRate"] = false;
    parameterLocks["lfoDepth"] = false;
    parameterLocks["compPeakReduction"] = false;
    parameterLocks["compMakeupGain"] = false;
    parameterLocks["compRatio"] = false;
    parameterLocks["compEnabled"] = false;
    parameterLocks["compWetDry"] = false;
    parameterLocks["compCrossover"] = false;

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
    addAndMakeVisible(compWetDryLock);
    addAndMakeVisible(compCrossoverLock);
    addAndMakeVisible(bandSplitLock);
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
    setupKnobRightClick(compWetDrySlider, "compWetDry");
    setupKnobRightClick(compCrossoverSlider, "compCrossover");
    setupKnobRightClick(bandSplitToggle, "bandSplitEnabled");
    setupKnobRightClick(clipTypeComboBox, "clipType");
    setupKnobRightClick(compRatioComboBox, "compRatio");
    setupKnobRightClick(compEnableToggle, "compEnabled");

    // Initialize compression section visibility
    updateCompressionVisibility();
}

PluginEditor::~PluginEditor()
{
    // Reset LookAndFeel to nullptr before destruction to prevent crash
    // Components must not reference a LookAndFeel that may be destroyed before them
    bandSplitToggle.setLookAndFeel(nullptr);
    compEnableToggle.setLookAndFeel(nullptr);
    clipTypeComboBox.setLookAndFeel(nullptr);
    compRatioComboBox.setLookAndFeel(nullptr);
    lfoWaveformComboBox.setLookAndFeel(nullptr);
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

    
    if (auto* param = audioProcessor.parameters.getParameter(paramID))
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
    label.setFont(juce::Font(12.0f, juce::Font::bold));

    addAndMakeVisible(label);
}

//==============================================================================
void PluginEditor::paint(juce::Graphics& g)
{
    // Black background
    g.fillAll(juce::Colours::black);

    // Draw neon red border
    g.setColour(juce::Colour(0xFF, 0x00, 0x44).withAlpha(0.7f));
    g.drawRect(getLocalBounds(), 2);
}

void PluginEditor::resized()
{
    // ========== LAYOUT CONSTANTS ==========
    const int titleHeight = 50;
    const int bottomControlsHeight = 130;
    const int margin = 20;
    const int compressionBarHeight = 90;

    // ========== TITLE LABEL ==========
    titleLabel.setBounds(0, 0, getWidth(), titleHeight);

    // ========== OSCILLOSCOPE (below title, above controls) ==========
    oscilloscope.setBounds(0, titleHeight, getWidth(), getHeight() - titleHeight - bottomControlsHeight);

    // XY Morph Pad - same bounds as oscilloscope (invisible overlay)
    xyMorphPad.setBounds(oscilloscope.getBounds());

    const int labelHeight = 20;

    // Calculate available space for controls (overlaid on oscilloscope)
    auto bounds = getLocalBounds().reduced(margin);
    auto titleArea = bounds.removeFromTop(titleHeight);

    // ========== PRESET SELECTOR (TOP-LEFT) ==========
    // No label - preset selector starts at left edge
    const int presetSelectorWidth = 150;
    const int presetButtonWidth = 50;
    const int presetSpacing = 5;
    const int presetHeight = 24;

    const int presetY = titleArea.getY() + (titleHeight - presetHeight) / 2;
    const int presetX = titleArea.getX();

    // Hide the label (set to zero width)
    presetLabel.setBounds(0, 0, 0, 0);

    // Preset selector starts at presetX (no label offset)
    presetSelector.setBounds(presetX, presetY, presetSelectorWidth, presetHeight);
    savePresetButton.setBounds(presetX + presetSelectorWidth + presetSpacing,
                               presetY, presetButtonWidth, presetHeight);
    deletePresetButton.setBounds(presetX + presetSelectorWidth + presetSpacing + presetButtonWidth + presetSpacing,
                                 presetY, presetButtonWidth, presetHeight);

    // Randomize button top-right (opposite preset selector, same size as preset dropdown)
    const int randomizeButtonWidth = presetSelectorWidth;   // Match preset dropdown width (150px)
    const int randomizeButtonHeight = presetHeight;         // Match preset dropdown height (24px)
    const int randomizeButtonX = getWidth() - randomizeButtonWidth - 20;  // Right-aligned, 20px margin
    const int randomizeButtonY = presetY;  // Same height as preset row
    randomizeButton.setBounds(randomizeButtonX, randomizeButtonY,
                             randomizeButtonWidth, randomizeButtonHeight);

    auto contentArea = bounds;

    // ========== COMPRESSION SECTION (COLLAPSIBLE) ==========
    const int tabHeaderHeight = 25;
    const int expandedCompressionHeight = 90;

    // Dynamic height based on collapse state
    const int compressionSectionHeight = isCompressionExpanded
        ? expandedCompressionHeight
        : tabHeaderHeight;

    auto compressionArea = contentArea.removeFromTop(compressionSectionHeight);

    // Calculate actual window height for footer positioning
    const int actualWindowHeight = getHeight();

    // Tab header (always visible, clickable) - sized to fit text
    auto tabArea = compressionArea.removeFromTop(tabHeaderHeight);

    // Calculate width for "COMPRESSION" text + padding + chevron
    const int tabTextWidth = 120;  // Width for text
    const int tabChevronWidth = 30;  // Space for chevron
    const int tabTotalWidth = tabTextWidth + tabChevronWidth;
    const int tabX = (getWidth() - tabTotalWidth) / 2;  // Center horizontally

    compressionTabHeader.setBounds(tabX, tabArea.getY(), tabTotalWidth, tabHeaderHeight);
    compressionTabHeader.setExpanded(isCompressionExpanded);

    // Only layout controls if expanded
    if (isCompressionExpanded)
    {
        // ========== COMPRESSION CONTROLS LAYOUT (4 knobs + dropdown + toggle) ==========
        const int compKnobSize = 60;
        const int compSpacing = 15;
        const int compControlsWidth = (4 * compKnobSize) + (3 * compSpacing) + 60 + 30;
        const int compStartX = compressionArea.getRight() - compControlsWidth - 20;
        const int compKnobY = compressionArea.getY() + 5;

        // Peak Reduction knob
        compPeakReductionSlider.setBounds(compStartX, compKnobY, compKnobSize, compKnobSize);
        compPeakReductionLabel.setBounds(compStartX, compKnobY + compKnobSize, compKnobSize, 15);

        // Makeup Gain knob
        const int makeupX = compStartX + compKnobSize + compSpacing;
        compMakeupGainSlider.setBounds(makeupX, compKnobY, compKnobSize, compKnobSize);
        compMakeupGainLabel.setBounds(makeupX, compKnobY + compKnobSize, compKnobSize, 15);

        // Wet/Dry knob (Compression)
        const int wetDryX = makeupX + compKnobSize + compSpacing;
        compWetDrySlider.setBounds(wetDryX, compKnobY, compKnobSize, compKnobSize);
        compWetDryLabel.setBounds(wetDryX, compKnobY + compKnobSize, compKnobSize, 15);

        // Crossover knob
        const int crossoverX = wetDryX + compKnobSize + compSpacing;
        compCrossoverSlider.setBounds(crossoverX, compKnobY, compKnobSize, compKnobSize);
        compCrossoverLabel.setBounds(crossoverX, compKnobY + compKnobSize, compKnobSize, 15);

        // Compress/Limit dropdown
        const int dropdownX = crossoverX + compKnobSize + compSpacing;
        const int dropdownWidth = 60;
        compRatioComboBox.setBounds(dropdownX, compKnobY + 15, dropdownWidth, 20);
        compRatioLabel.setBounds(dropdownX, compKnobY + 37, dropdownWidth, 12);

        // ========== ENABLE TOGGLE (LEFT OF PEAK REDUCTION) ==========
        const int toggleSize = 24;
        const int toggleX = compStartX - toggleSize - 15;
        compEnableToggle.setBounds(toggleX, compKnobY + 15, toggleSize, toggleSize);

        // ========== GAIN REDUCTION METER (RIGHT, AFTER DROPDOWN - SLIM) ==========
        const int meterWidth = 20;  // Slim meter
        const int meterHeight = 55;
        const int meterX = dropdownX + dropdownWidth + 15;
        const int meterY = compKnobY + 5;
        gainReductionMeter.setBounds(meterX, meterY, meterWidth, meterHeight);

        // Position compression lock icons
        const int compLockSize = 14;
        const int compLockOffset = 3;
        compPeakReductionLock.setBounds(compStartX + compKnobSize - compLockSize - compLockOffset,
                                       compKnobY + compLockOffset, compLockSize, compLockSize);
        compMakeupGainLock.setBounds(makeupX + compKnobSize - compLockSize - compLockOffset,
                                    compKnobY + compLockOffset, compLockSize, compLockSize);
        compWetDryLock.setBounds(wetDryX + compKnobSize - compLockSize - compLockOffset,
                                compKnobY + compLockOffset, compLockSize, compLockSize);
        compCrossoverLock.setBounds(crossoverX + compKnobSize - compLockSize - compLockOffset,
                                   compKnobY + compLockOffset, compLockSize, compLockSize);
        compRatioLock.setBounds(dropdownX + dropdownWidth - compLockSize - 2,
                               compKnobY + 15, compLockSize, compLockSize);
        compEnableLock.setBounds(toggleX + toggleSize - compLockSize,
                                compKnobY + 15, compLockSize, compLockSize);
    }
    // ===========================================================

    // ========== SINGLE ROW LAYOUT AT BOTTOM ==========
    const int knobSize = 75;  // Large knob size
    const int smallKnobSize = 45;  // Small knob size
    const int controlSpacing = 8;  // Spacing between controls
    const int bottomMargin = 20;
    const int rowY = actualWindowHeight - bottomMargin - knobSize - labelHeight - 10;  // Bottom row position

    // Calculate total width to center controls
    const int totalControlsWidth = 60 + 8 + 75 + 8 + 45 + 8 + 75 + 8 + 45 + 8 + 75 + 8 + 45 + 8 + 75 + 8 + 45 + 8 + 75 + 8 + 75;  // All controls + spacing
    int currentX = (getWidth() - totalControlsWidth) / 2;  // Center horizontally

    // Clean Sub Toggle (24x24)
    const int cleanSubToggleSize = 24;
    const int toggleYOffset = (knobSize - cleanSubToggleSize) / 2;  // Center vertically with big knobs
    bandSplitToggle.setBounds(currentX, rowY + toggleYOffset, cleanSubToggleSize, cleanSubToggleSize);
    const int toggleLabelWidth = 60;  // Width for toggle area
    bandSplitLabel.setBounds(currentX - (toggleLabelWidth - cleanSubToggleSize) / 2,
                            rowY + toggleYOffset + cleanSubToggleSize + 5, toggleLabelWidth, 16);
    bandSplitLock.setBounds(currentX + cleanSubToggleSize - 12, rowY + toggleYOffset, 12, 12);
    currentX += toggleLabelWidth + controlSpacing;

    // Input Gain (100x100)
    inputGainSlider.setBounds(currentX, rowY, knobSize, knobSize);
    inputGainLabel.setBounds(currentX, rowY + knobSize + 5, knobSize, labelHeight);
    inputGainLock.setBounds(currentX + knobSize - 16 - 5, rowY + 5, 16, 16);
    currentX += knobSize + controlSpacing;

    // Clip Type Dropdown (45x18)
    const int comboWidth = 45;  // Dropdown width
    const int comboHeight = 18;
    const int comboYOffset = (knobSize - comboHeight) / 2;
    clipTypeComboBox.setBounds(currentX, rowY + comboYOffset, comboWidth, comboHeight);
    clipTypeLabel.setBounds(currentX, rowY + comboYOffset + comboHeight + 2, comboWidth, 14);
    clipTypeLock.setBounds(currentX + comboWidth - 12 - 2, rowY + comboYOffset, 12, 12);
    currentX += comboWidth + controlSpacing;

    // Hi-Pass Filter (100x100)
    highPassFreqSlider.setBounds(currentX, rowY, knobSize, knobSize);
    highPassFreqLabel.setBounds(currentX, rowY + knobSize + 5, knobSize, labelHeight);
    highPassFreqLock.setBounds(currentX + knobSize - 16 - 5, rowY + 5, 16, 16);
    currentX += knobSize + controlSpacing;

    // Dist Mix (50x50)
    const int smallKnobYOffset = (knobSize - smallKnobSize) / 2;
    distMixSlider.setBounds(currentX, rowY + smallKnobYOffset, smallKnobSize, smallKnobSize);
    distMixLabel.setBounds(currentX, rowY + smallKnobYOffset + smallKnobSize + 2, smallKnobSize, 14);
    distMixLock.setBounds(currentX + smallKnobSize - 12 - 3, rowY + smallKnobYOffset + 3, 12, 12);
    currentX += smallKnobSize + controlSpacing;

    // Distortion Amount (100x100)
    distortionAmountSlider.setBounds(currentX, rowY, knobSize, knobSize);
    distortionAmountLabel.setBounds(currentX, rowY + knobSize + 5, knobSize, labelHeight);
    distortionAmountLock.setBounds(currentX + knobSize - 16 - 5, rowY + 5, 16, 16);
    currentX += knobSize + controlSpacing;

    // Tone (50x50) - post-distortion darkness/brightness
    toneSlider.setBounds(currentX, rowY + smallKnobYOffset, smallKnobSize, smallKnobSize);
    toneLabel.setBounds(currentX, rowY + smallKnobYOffset + smallKnobSize + 2, smallKnobSize, 14);
    currentX += smallKnobSize + controlSpacing;

    // Wave Mix (50x50)
    waveshaperSlider.setBounds(currentX, rowY + smallKnobYOffset, smallKnobSize, smallKnobSize);
    waveshaperLabel.setBounds(currentX, rowY + smallKnobYOffset + smallKnobSize + 2, smallKnobSize, 14);
    currentX += smallKnobSize + controlSpacing;

    // Output Gain (100x100)
    outputGainSlider.setBounds(currentX, rowY, knobSize, knobSize);
    outputGainLabel.setBounds(currentX, rowY + knobSize + 5, knobSize, labelHeight);
    outputGainLock.setBounds(currentX + knobSize - 16 - 5, rowY + 5, 16, 16);
    currentX += knobSize + controlSpacing;

    // LFO Waveform Dropdown (50x18)
    lfoWaveformComboBox.setBounds(currentX, rowY + comboYOffset, comboWidth, comboHeight);
    lfoWaveformLabel.setBounds(currentX, rowY + comboYOffset + comboHeight + 2, comboWidth, 14);
    currentX += comboWidth + controlSpacing;

    // LFO Rate (100x100)
    lfoRateSlider.setBounds(currentX, rowY, knobSize, knobSize);
    lfoRateLabel.setBounds(currentX, rowY + knobSize + 5, knobSize, labelHeight);
    lfoRateLock.setBounds(currentX + knobSize - 16 - 5, rowY + 5, 16, 16);
    currentX += knobSize + controlSpacing;

    // LFO Depth (100x100)
    lfoDepthSlider.setBounds(currentX, rowY, knobSize, knobSize);
    lfoDepthLabel.setBounds(currentX, rowY + knobSize + 5, knobSize, labelHeight);
    lfoDepthLock.setBounds(currentX + knobSize - 16 - 5, rowY + 5, 16, 16);

    // All controls and lock icons positioned above in single row layout
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
    compPeakReductionLock.setLocked(isParameterLocked("compPeakReduction"));
    compMakeupGainLock.setLocked(isParameterLocked("compMakeupGain"));
    compWetDryLock.setLocked(isParameterLocked("compWetDry"));
    compCrossoverLock.setLocked(isParameterLocked("compCrossover"));
    bandSplitLock.setLocked(isParameterLocked("bandSplitEnabled"));
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

    // 4 knobs + labels
    compPeakReductionSlider.setVisible(visible);
    compPeakReductionLabel.setVisible(visible);
    compMakeupGainSlider.setVisible(visible);
    compMakeupGainLabel.setVisible(visible);
    compWetDrySlider.setVisible(visible);
    compWetDryLabel.setVisible(visible);
    compCrossoverSlider.setVisible(visible);
    compCrossoverLabel.setVisible(visible);

    // Dropdown, toggle, meter
    compRatioComboBox.setVisible(visible);
    compRatioLabel.setVisible(visible);
    compEnableToggle.setVisible(visible);
    gainReductionMeter.setVisible(visible);

    // 6 lock icons
    compPeakReductionLock.setVisible(visible);
    compMakeupGainLock.setVisible(visible);
    compWetDryLock.setVisible(visible);
    compCrossoverLock.setVisible(visible);
    compRatioLock.setVisible(visible);
    compEnableLock.setVisible(visible);
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
    randomizeBoolParam("bandSplitEnabled");
    randomizeChoiceParam("clipType", 7);
    randomizeFloatParam("distMix", 0.0f, 100.0f);

    // LFO SECTION
    randomizeFloatParam("lfoRate", 0.1f, 10.0f);
    randomizeFloatParam("lfoDepth", 0.0f, 100.0f);

    // COMPRESSION SECTION
    randomizeFloatParam("compPeakReduction", 0.0f, 100.0f);
    randomizeFloatParam("compMakeupGain", 0.0f, 100.0f);
    randomizeChoiceParam("compRatio", 2);
    randomizeBoolParam("compEnabled");
    randomizeFloatParam("compWetDry", 0.0f, 100.0f);
    randomizeFloatParam("compCrossover", 150.0f, 350.0f);
}

juce::File PluginEditor::getPresetDirectory()
{
    // Get user's AppData folder
    auto presetDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("ElarMusicAudio")
        .getChildFile("Distortion")
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

    // Select "Default" preset by default (ID = 1)
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

    if (presetName == "Default")
    {
        // Default preset - neutral settings
        setParam("inputGain", 50.0f);
        setParam("outputGain", 50.0f);
        setParam("distortionAmount", 0.0f);
        setParam("highPassFreq", 20.0f);
        setParam("bandSplitEnabled", 0.0f);
        setParam("clipType", 0.0f);
        setParam("distMix", 100.0f);
        setParam("lfoRate", 0.0f);
        setParam("lfoDepth", 0.0f);
        setParam("compPeakReduction", 0.0f);
        setParam("compMakeupGain", 50.0f);
        setParam("compRatio", 0.0f);
        setParam("compEnabled", 0.0f);
        setParam("compWetDry", 50.0f);
        setParam("compCrossover", 250.0f);
    }
    else if (presetName == "Warm Tube")
    {
        // Warm tube-like saturation
        setParam("inputGain", 60.0f);
        setParam("outputGain", 45.0f);
        setParam("distortionAmount", 30.0f);
        setParam("highPassFreq", 80.0f);
        setParam("bandSplitEnabled", 0.0f);
        setParam("clipType", 4.0f);  // Harmonic
        setParam("distMix", 70.0f);
        setParam("lfoRate", 0.0f);
        setParam("lfoDepth", 0.0f);
        setParam("compPeakReduction", 0.0f);
        setParam("compMakeupGain", 50.0f);
        setParam("compRatio", 0.0f);
        setParam("compEnabled", 0.0f);
        setParam("compWetDry", 50.0f);
        setParam("compCrossover", 250.0f);
    }
    else if (presetName == "Hard Clip")
    {
        // Aggressive hard clipping
        setParam("inputGain", 70.0f);
        setParam("outputGain", 40.0f);
        setParam("distortionAmount", 70.0f);
        setParam("highPassFreq", 100.0f);
        setParam("bandSplitEnabled", 1.0f);  // 808-safe enabled
        setParam("clipType", 6.0f);  // Hard Limit
        setParam("distMix", 100.0f);
        setParam("lfoRate", 0.0f);
        setParam("lfoDepth", 0.0f);
        setParam("compPeakReduction", 30.0f);
        setParam("compMakeupGain", 60.0f);
        setParam("compRatio", 0.0f);
        setParam("compEnabled", 1.0f);
        setParam("compWetDry", 70.0f);
        setParam("compCrossover", 250.0f);
    }
    else if (presetName == "Soft Saturation")
    {
        // Subtle soft saturation
        setParam("inputGain", 55.0f);
        setParam("outputGain", 48.0f);
        setParam("distortionAmount", 20.0f);
        setParam("highPassFreq", 40.0f);
        setParam("bandSplitEnabled", 0.0f);
        setParam("clipType", 1.0f);  // Soft Knee
        setParam("distMix", 50.0f);  // Parallel blend
        setParam("lfoRate", 0.0f);
        setParam("lfoDepth", 0.0f);
        setParam("compPeakReduction", 0.0f);
        setParam("compMakeupGain", 50.0f);
        setParam("compRatio", 0.0f);
        setParam("compEnabled", 0.0f);
        setParam("compWetDry", 50.0f);
        setParam("compCrossover", 250.0f);
    }
    else if (presetName == "808 Safe")
    {
        // Clean sub, aggressive highs
        setParam("inputGain", 65.0f);
        setParam("outputGain", 45.0f);
        setParam("distortionAmount", 60.0f);
        setParam("highPassFreq", 150.0f);
        setParam("bandSplitEnabled", 1.0f);  // 808-safe enabled
        setParam("clipType", 3.0f);  // Multi-Stage
        setParam("distMix", 100.0f);
        setParam("lfoRate", 0.0f);
        setParam("lfoDepth", 0.0f);
        setParam("compPeakReduction", 0.0f);
        setParam("compMakeupGain", 50.0f);
        setParam("compRatio", 0.0f);
        setParam("compEnabled", 0.0f);
        setParam("compWetDry", 50.0f);
        setParam("compCrossover", 250.0f);
    }
}
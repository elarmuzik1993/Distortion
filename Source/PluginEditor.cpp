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
    addAndMakeVisible(gainReductionMeter);

    // Set fixed window size - no resizing allowed
    setSize(802, 564);
    setResizeLimits(802, 564, 802, 564);
    setResizable(false, false);
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
    setupSlider(outputGainSlider, outputGainLabel, "Output Gain",
        outputGainAttachment, "outputGain");
    setupSlider(highPassFreqSlider, highPassFreqLabel, "Hi-Pass Filter",
        highPassFreqAttachment, "highPassFreq");
    setupSlider(lfoRateSlider, lfoRateLabel, "LFO Rate",
        lfoRateAttachment, "lfoRate");
    setupSlider(lfoDepthSlider, lfoDepthLabel, "LFO Depth",
        lfoDepthAttachment, "lfoDepth");
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
    compRatioComboBox.addItem("Compress", 1);  // 3:1 ratio
    compRatioComboBox.addItem("Limit", 2);     // 12:1 ratio

    compRatioAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.parameters, "compRatio", compRatioComboBox);

    addAndMakeVisible(compRatioLabel);
    compRatioLabel.setText("Mode", juce::dontSendNotification);
    compRatioLabel.setJustificationType(juce::Justification::centred);
    compRatioLabel.setColour(juce::Label::textColourId, juce::Colours::black);
    compRatioLabel.setFont(juce::Font(10.0f, juce::Font::bold));

    // Setup compression enable toggle
    addAndMakeVisible(compEnableToggle);
    compEnableToggle.setButtonText("COMP");
    compEnableToggle.setLookAndFeel(&checkboxLookAndFeel);

    compEnableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.parameters, "compEnabled", compEnableToggle);

    // Setup section title label
    addAndMakeVisible(compSectionLabel);
    compSectionLabel.setText("LOW-END COMPRESSION", juce::dontSendNotification);
    compSectionLabel.setJustificationType(juce::Justification::centred);
    compSectionLabel.setColour(juce::Label::textColourId, juce::Colours::darkblue);
    compSectionLabel.setFont(juce::Font(14.0f, juce::Font::bold));
    
    // Setup 808-Safe Mode toggle
    addAndMakeVisible(bandSplitToggle);
    bandSplitToggle.setButtonText("808-Safe");
    bandSplitToggle.setLookAndFeel(&checkboxLookAndFeel);  // Apply custom black box with green tick

    bandSplitAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.parameters, "bandSplitEnabled", bandSplitToggle);

    addAndMakeVisible(bandSplitLabel);
    bandSplitLabel.setText("Clean Sub", juce::dontSendNotification);
    bandSplitLabel.setJustificationType(juce::Justification::centred);
    bandSplitLabel.setColour(juce::Label::textColourId, juce::Colours::black);  // Changed to black for visibility
    bandSplitLabel.setFont(juce::Font(12.0f, juce::Font::bold));  // Bigger and bold

    addAndMakeVisible(clipTypeComboBox);
    clipTypeComboBox.addItem("Studio Tanh", 1);
    clipTypeComboBox.addItem("Soft Knee", 2);
    clipTypeComboBox.addItem("Dynamic Compress", 3);
    clipTypeComboBox.addItem("Multi-Stage", 4);
    clipTypeComboBox.addItem("Harmonic", 5);
    clipTypeComboBox.addItem("Asymmetric", 6);
    clipTypeComboBox.addItem("Hard Limit", 7);
    
    clipTypeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.parameters, "clipType", clipTypeComboBox);
    
    addAndMakeVisible(clipTypeLabel);
    clipTypeLabel.setText("Clip Type", juce::dontSendNotification);
    clipTypeLabel.setJustificationType(juce::Justification::centred);
    clipTypeLabel.setColour(juce::Label::textColourId, juce::Colours::black);
    clipTypeLabel.setFont(juce::Font(12.0f, juce::Font::bold));

    // Setup randomize button
    addAndMakeVisible(randomizeButton);
    randomizeButton.setButtonText("Randomize");
    randomizeButton.onClick = [this]() { randomizeAllParameters(); };

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

    // Configure label
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, juce::Colours::black);
    label.setColour(juce::Label::backgroundColourId, juce::Colours::transparentWhite);
    label.setFont(juce::Font(12.0f, juce::Font::plain));

    addAndMakeVisible(label);
}

//==============================================================================
void PluginEditor::paint(juce::Graphics& g)
{
    // ========== REPLACE THE EXISTING GRADIENT CODE WITH THIS ==========
    // Draw the background image
    if (backgroundImage.isValid())
    {
        g.drawImage(backgroundImage, getLocalBounds().toFloat(),
            juce::RectanglePlacement::fillDestination);
    }
    else
    {
        // Fallback if image fails to load - keep original gradient
        juce::ColourGradient gradient(
            juce::Colour::fromRGB(0x63, 0xFF, 0x2F), 0, 0,
            juce::Colour::fromRGB(0x80, 0xFF, 0x00), 0, (float)getHeight(),
            false);
        g.setGradientFill(gradient);
        g.fillAll();
    }
    // ==================================================================

    // Draw title
    g.setColour(juce::Colours::darkblue);
    g.setFont(juce::Font(18.0f, juce::Font::bold));
    const auto titleBounds = getLocalBounds().removeFromTop(50);
    g.drawText(" Distortion", titleBounds, juce::Justification::centred, true);

    // Draw subtle border
    g.setColour(juce::Colours::grey.withAlpha(0.3f));
    g.drawRect(getLocalBounds(), 1);
}

void PluginEditor::resized()
{
    const int margin = 20;
    const int titleHeight = 50;
    const int compressionBarHeight = 90;  // NEW: Height for compression section
    const int sliderWidth = 100;
    const int sliderHeight = 100;
    const int labelHeight = 20;
    const int spacing = 40;
    const int minScopeHeight = 100;

    // Calculate available space
    auto bounds = getLocalBounds().reduced(margin);
    auto contentArea = bounds.withTrimmedTop(titleHeight);

    // ========== COMPRESSION BAR AT TOP (NEW SECTION) ==========
    auto compressionArea = contentArea.removeFromTop(compressionBarHeight);

    // Compression section title
    auto compTitleArea = compressionArea.removeFromTop(20);
    compSectionLabel.setBounds(compTitleArea);

    // ========== COMPRESSION CONTROLS LAYOUT (4 knobs + dropdown + toggle) ==========
    const int compKnobSize = 60;
    const int compSpacing = 15;
    const int compControlsWidth = (4 * compKnobSize) + (3 * compSpacing) + 60 + 30;  // 4 knobs + dropdown + toggle
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

    // Enable toggle
    const int toggleX = dropdownX + dropdownWidth + 15;
    const int toggleSize = 24;
    compEnableToggle.setBounds(toggleX, compKnobY + 15, toggleSize, toggleSize);

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

    // ========== GAIN REDUCTION METER (RIGHT SIDE, AFTER COMPRESSION CONTROLS) ==========
    const int meterWidth = 40;
    const int meterHeight = 65;
    const int meterX = toggleX + toggleSize + 20;  // Position after the enable toggle
    const int meterY = compressionArea.getY() + 5;
    gainReductionMeter.setBounds(meterX, meterY, meterWidth, meterHeight);

    // ========== RANDOMIZE BUTTON (LEFT SIDE OF COMPRESSION SECTION) ==========
    const int randomizeButtonWidth = 90;
    const int randomizeButtonHeight = 30;
    const int randomizeButtonX = compressionArea.getX() + 10;
    const int randomizeButtonY = compressionArea.getY() + 30;
    randomizeButton.setBounds(randomizeButtonX, randomizeButtonY, randomizeButtonWidth, randomizeButtonHeight);
    // ===========================================================

    // Calculate space needed for distortion controls
    const int controlsHeight = sliderHeight + labelHeight + 20;

    // Allocate space: give oscilloscope what's left after compression bar and controls
    int scopeHeight = contentArea.getHeight() - controlsHeight;
    scopeHeight = juce::jmax(scopeHeight, minScopeHeight);

    auto scopeArea = contentArea.removeFromTop(scopeHeight);
    oscilloscope.setBounds(scopeArea.reduced(5));

    // Position distortion sliders in the remaining bottom area
    auto controlArea = contentArea;

    const int totalSliderWidth = 6 * sliderWidth + 5 * spacing;  // 6 sliders (distMix moved to top)
    const int startX = controlArea.getX() + (controlArea.getWidth() - totalSliderWidth) / 2;
    const int sliderY = controlArea.getY() + (controlArea.getHeight() - sliderHeight - labelHeight) / 2;

    // Position sliders and labels
    auto positionSliderAndLabel = [&](juce::Slider& slider, juce::Label& label, int index)
        {
            const int x = startX + index * (sliderWidth + spacing);
            slider.setBounds(x, sliderY, sliderWidth, sliderHeight);
            label.setBounds(x, sliderY + sliderHeight + 5, sliderWidth, labelHeight);
        };

    positionSliderAndLabel(inputGainSlider, inputGainLabel, 0);
    positionSliderAndLabel(highPassFreqSlider, highPassFreqLabel, 1);
    positionSliderAndLabel(distortionAmountSlider, distortionAmountLabel, 2);
    positionSliderAndLabel(outputGainSlider, outputGainLabel, 3);
    positionSliderAndLabel(lfoRateSlider, lfoRateLabel, 4);
    positionSliderAndLabel(lfoDepthSlider, lfoDepthLabel, 5);

    // Position lock icons for main sliders (top-right corner of each knob)
    const int lockSize = 16;
    const int lockOffset = 5;
    inputGainLock.setBounds(inputGainSlider.getX() + sliderWidth - lockSize - lockOffset,
                           inputGainSlider.getY() + lockOffset, lockSize, lockSize);
    highPassFreqLock.setBounds(highPassFreqSlider.getX() + sliderWidth - lockSize - lockOffset,
                              highPassFreqSlider.getY() + lockOffset, lockSize, lockSize);
    distortionAmountLock.setBounds(distortionAmountSlider.getX() + sliderWidth - lockSize - lockOffset,
                                  distortionAmountSlider.getY() + lockOffset, lockSize, lockSize);
    outputGainLock.setBounds(outputGainSlider.getX() + sliderWidth - lockSize - lockOffset,
                            outputGainSlider.getY() + lockOffset, lockSize, lockSize);
    lfoRateLock.setBounds(lfoRateSlider.getX() + sliderWidth - lockSize - lockOffset,
                         lfoRateSlider.getY() + lockOffset, lockSize, lockSize);
    lfoDepthLock.setBounds(lfoDepthSlider.getX() + sliderWidth - lockSize - lockOffset,
                          lfoDepthSlider.getY() + lockOffset, lockSize, lockSize);

    // ========== CLIP TYPE COMBOBOX POSITIONING ==========
    const int comboBoxWidth = 54;
    const int comboBoxHeight = 15;

    const int hiPassCenter = startX + 1 * (sliderWidth + spacing) + sliderWidth / 2;
    const int distortionCenter = startX + 2 * (sliderWidth + spacing) + sliderWidth / 2;
    const int comboCenterX = (hiPassCenter + distortionCenter) / 2;

    const int comboY = sliderY + 30;
    clipTypeComboBox.setBounds(
        comboCenterX - comboBoxWidth / 2,
        comboY,
        comboBoxWidth,
        comboBoxHeight
    );

    clipTypeLabel.setBounds(
        comboCenterX - comboBoxWidth / 2,
        comboY + comboBoxHeight + 2,
        comboBoxWidth,
        14
    );

    // ========== DIST MIX KNOB POSITIONING (between Distortion and Output) ==========
    const int distortionKnobCenter = startX + 2 * (sliderWidth + spacing) + sliderWidth / 2;
    const int outputKnobCenter = startX + 3 * (sliderWidth + spacing) + sliderWidth / 2;
    const int distMixCenterX = (distortionKnobCenter + outputKnobCenter) / 2;

    const int distMixKnobSize = 50;  // Smaller knob size to fit between main knobs
    distMixSlider.setBounds(
        distMixCenterX - distMixKnobSize / 2,
        comboY - 5,  // Same Y level as clip type dropdown
        distMixKnobSize,
        distMixKnobSize
    );

    const int distMixLabelHeight = 14;
    distMixLabel.setBounds(
        distMixCenterX - distMixKnobSize / 2,
        comboY - 5 + distMixKnobSize + 2,
        distMixKnobSize,
        distMixLabelHeight
    );

    // ========== 808-SAFE TOGGLE BUTTON POSITIONING ==========
    const int safeToggleSize = 24;  // Changed from toggleSize

    const int knob1Center = startX + sliderWidth / 2;
    const int knob2Center = startX + sliderWidth + spacing + sliderWidth / 2;
    const int safeCenterX = (knob1Center + knob2Center) / 2;  // Changed from centerX

    const int safeToggleX = safeCenterX - safeToggleSize / 2;  // Changed from toggleX
    const int safeToggleY = comboY + (comboBoxHeight / 2) - (safeToggleSize / 2);  // Changed from toggleY
    bandSplitToggle.setBounds(safeToggleX, safeToggleY, safeToggleSize, safeToggleSize);

    const int labelWidth = 80;
    bandSplitLabel.setBounds(
        safeCenterX - labelWidth / 2,  // Changed from centerX
        safeToggleY + safeToggleSize + 5,  // Changed variables
        labelWidth,
        16
    );

    // Position remaining lock icons
    const int smallLockSize = 12;
    distMixLock.setBounds(distMixSlider.getX() + distMixKnobSize - smallLockSize - 3,
                         distMixSlider.getY() + 3, smallLockSize, smallLockSize);
    clipTypeLock.setBounds(clipTypeComboBox.getX() + comboBoxWidth - smallLockSize - 2,
                          clipTypeComboBox.getY(), smallLockSize, smallLockSize);
    bandSplitLock.setBounds(bandSplitToggle.getX() + safeToggleSize - smallLockSize,
                           bandSplitToggle.getY(), smallLockSize, smallLockSize);
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
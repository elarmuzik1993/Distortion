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
    clipTypeComboBox.addItem("Soft Clip", 1);
    clipTypeComboBox.addItem("Hard Clip", 2);
    clipTypeComboBox.addItem("Tube Warmth", 3);
    clipTypeComboBox.addItem("Fuzz", 4);
    clipTypeComboBox.addItem("Asymmetric", 5);
    
    clipTypeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.parameters, "clipType", clipTypeComboBox);
    
    addAndMakeVisible(clipTypeLabel);
    clipTypeLabel.setText("Clip Type", juce::dontSendNotification);
    clipTypeLabel.setJustificationType(juce::Justification::centred);
    clipTypeLabel.setColour(juce::Label::textColourId, juce::Colours::black);
    clipTypeLabel.setFont(juce::Font(12.0f, juce::Font::bold));
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

    // ========== COMPRESSION CONTROLS LAYOUT (5 knobs + dropdown + toggle) ==========
    const int compKnobSize = 60;
    const int compSpacing = 15;
    const int compControlsWidth = (5 * compKnobSize) + (4 * compSpacing) + 60 + 30;  // 5 knobs + dropdown + toggle
    const int compStartX = compressionArea.getRight() - compControlsWidth - 20;
    const int compKnobY = compressionArea.getY() + 5;

    // Peak Reduction knob
    compPeakReductionSlider.setBounds(compStartX, compKnobY, compKnobSize, compKnobSize);
    compPeakReductionLabel.setBounds(compStartX, compKnobY + compKnobSize, compKnobSize, 15);

    // Makeup Gain knob
    const int makeupX = compStartX + compKnobSize + compSpacing;
    compMakeupGainSlider.setBounds(makeupX, compKnobY, compKnobSize, compKnobSize);
    compMakeupGainLabel.setBounds(makeupX, compKnobY + compKnobSize, compKnobSize, 15);

    // Wet/Dry knob (NEW)
    const int wetDryX = makeupX + compKnobSize + compSpacing;
    compWetDrySlider.setBounds(wetDryX, compKnobY, compKnobSize, compKnobSize);
    compWetDryLabel.setBounds(wetDryX, compKnobY + compKnobSize, compKnobSize, 15);

    // Crossover knob (NEW)
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

    // ========== GAIN REDUCTION METER (RIGHT SIDE, AFTER COMPRESSION CONTROLS) ==========
    const int meterWidth = 40;
    const int meterHeight = 65;
    const int meterX = toggleX + toggleSize + 20;  // Position after the enable toggle
    const int meterY = compressionArea.getY() + 5;
    gainReductionMeter.setBounds(meterX, meterY, meterWidth, meterHeight);
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

    const int totalSliderWidth = 6 * sliderWidth + 5 * spacing;
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
}
/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginEditor.h"
#include "PluginProcessor.h"

//Setup Slider in Constructor Here

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), oscilloscope(p)
{
    addAndMakeVisible(oscilloscope);
    setSize(600, 400);
    setResizable(true, true);
    setResizeLimits(350, 350, 800, 600);
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
    const int sliderWidth = 100;
    const int sliderHeight = 100;
    const int labelHeight = 20;
    const int spacing = 40;
    const int minScopeHeight = 100;  // Minimum height for oscilloscope

    // Calculate available space
    auto bounds = getLocalBounds().reduced(margin);
    auto contentArea = bounds.withTrimmedTop(titleHeight);

    // Calculate space needed for controls
    const int controlsHeight = sliderHeight + labelHeight + 20;  // 20px padding

    // Allocate space: give oscilloscope what's left after controls, but with a minimum
    int scopeHeight = contentArea.getHeight() - controlsHeight;
    scopeHeight = juce::jmax(scopeHeight, minScopeHeight);  // Ensure minimum height

    auto scopeArea = contentArea.removeFromTop(scopeHeight);
    oscilloscope.setBounds(scopeArea.reduced(5));

    // Position sliders in the remaining bottom area
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

    // Position above knobs with some spacing
    const int comboY = sliderY + 30;  // slightly above knobs
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
    const int toggleSize = 24;  // Square checkbox size

    // Calculate exact center between Input Gain and Hi-Pass knobs
    const int knob1Center = startX + sliderWidth / 2;
    const int knob2Center = startX + sliderWidth + spacing + sliderWidth / 2;
    const int centerX = (knob1Center + knob2Center) / 2;

    // Match Y position with ComboBox
    const int toggleX = centerX - toggleSize / 2;
    const int toggleY = comboY + (comboBoxHeight / 2) - (toggleSize / 2);  // same vertical level as combo box
    bandSplitToggle.setBounds(toggleX, toggleY, toggleSize, toggleSize);

    // Position "808-Safe" label below the checkbox
    const int labelWidth = 80;
    bandSplitLabel.setBounds(
        centerX - labelWidth / 2,
        toggleY + toggleSize + 5,  // 5px below checkbox
        labelWidth,
        16
    );
}
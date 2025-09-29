/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginEditor.h"
#include "PluginProcessor.h"

//==============================================================================
PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), oscilloscope(p)
{
    addAndMakeVisible(oscilloscope);
    // Set minimum and maximum size constraints
    setSize(600, 400);
    setResizable(true, true);
    setResizeLimits(350, 350, 800, 600);

    // Setup all sliders
    setupSlider(inputGainSlider, inputGainLabel, "Input Gain",
        inputGainAttachment, "inputGain");
    setupSlider(distortionAmountSlider, distortionAmountLabel, "Distortion Amount",
        distortionAmountAttachment, "distortionAmount");
    setupSlider(outputGainSlider, outputGainLabel, "Output Gain",
        outputGainAttachment, "outputGain");
}

//==============================================================================
void PluginEditor::setupSlider(CustomKnob& slider,  // CHANGE THIS LINE
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
    // Create gradient background
    juce::ColourGradient gradient(
        juce::Colour::fromRGB(0x63, 0xFF, 0x2F), 0, 0,                     // top colour
        juce::Colour::fromRGB(0x80, 0xFF, 0x00), 0, (float)getHeight(),    // bottom colour
        false);

    g.setGradientFill(gradient);
    g.fillAll();

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
    const int minScopeHeight = 80;  // Minimum height for oscilloscope

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

    const int totalSliderWidth = 3 * sliderWidth + 2 * spacing;
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
    positionSliderAndLabel(distortionAmountSlider, distortionAmountLabel, 1);
    positionSliderAndLabel(outputGainSlider, outputGainLabel, 2);
}
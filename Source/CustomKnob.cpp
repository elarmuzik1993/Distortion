/*
  ==============================================================================
    CustomKnob.cpp
    Created: 28 Sep 2025 5:09:33pm
    Author:  boris
  ==============================================================================
*/
#include "CustomKnob.h"
// Display for digits 
CustomKnob::CustomKnob()
{
    setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 25);

    
    auto startAngle = 7.0f * juce::MathConstants<float>::pi / 6.0f;  // 7 o'clock (210°)
    auto endAngle = startAngle + (5.0f * juce::MathConstants<float>::pi / 3.0f);  // +300° rotation

    setRotaryParameters(startAngle, endAngle, true);

    // Custom text display - shows clean integer values
    setTextValueSuffix("");
    // Override how the value is displayed as text
    textFromValueFunction = [](double value)
        {
            return juce::String(static_cast<int>(value));  // Show as whole number
        };
    // Style the text box like a digital display
    setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xff00ff00));  // Bright green text
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff1a1a1a));  // Dark background
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff333333));  // Dark border
    setColour(juce::Slider::textBoxHighlightColourId, juce::Colour(0xff004400));  // Dark green highlight
    // Make text box read-only so it looks more like a display
    setTextBoxIsEditable(false);
}
void CustomKnob::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto knobArea = bounds.removeFromTop(bounds.getHeight() - 25.0f);
    auto centre = knobArea.getCentre();
    auto radius = juce::jmin(knobArea.getWidth(), knobArea.getHeight()) / 2.0f - 5.0f;
    auto knobBounds = juce::Rectangle<float>(radius * 2.0f, radius * 2.0f).withCentre(centre);

    // Dark circular background
    g.setColour(juce::Colour(0xff2a2a2a));
    g.fillEllipse(knobBounds);

    // Arc parameters
    auto startAngle = 7.0f * juce::MathConstants<float>::pi / 6.0f;
    auto endAngle = startAngle + (5.0f * juce::MathConstants<float>::pi / 3.0f);
    auto sliderPos = (getValue() - getMinimum()) / (getMaximum() - getMinimum());
    auto currentAngle = startAngle + sliderPos * (endAngle - startAngle);
    auto arcRadius = radius - 3.0f;

    // Draw red arc
    juce::Path arcPath;
    arcPath.addCentredArc(centre.x, centre.y,
        arcRadius, arcRadius,
        0.0f,
        startAngle, currentAngle,
        true);

    g.setColour(juce::Colour(0xffff4444));
    g.strokePath(arcPath, juce::PathStrokeType(4.0f));
}

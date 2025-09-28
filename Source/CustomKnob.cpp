/*
  ==============================================================================
    CustomKnob.cpp
    Created: 28 Sep 2025 5:09:33pm
    Author:  boris
  ==============================================================================
*/

#include "CustomKnob.h"

CustomKnob::CustomKnob()
{
    setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);

    // Add this line to format the text better
    setNumDecimalPlacesToDisplay(1.0);  // Show only 1 decimal place
}

void CustomKnob::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Reserve space for the text box at the bottom
    auto knobArea = bounds.removeFromTop(bounds.getHeight() - 25.0f); // Leave 25px for text
    auto centre = knobArea.getCentre();

    // Make knob fit within the reserved area
    auto radius = juce::jmin(knobArea.getWidth(), knobArea.getHeight()) / 2.0f - 5.0f;
    auto knobBounds = juce::Rectangle<float>(radius * 2.0f, radius * 2.0f).withCentre(centre);

    // Calculate rotation based on slider value
    auto toAngle = [](double value)
        {
            return juce::MathConstants<float>::pi * 1.5f + value * juce::MathConstants<float>::pi * 1.5f;
        };

    auto sliderPos = getValue();
    auto rotationAngle = toAngle((sliderPos - getMinimum()) / (getMaximum() - getMinimum()));

    // Draw the knob background (circle)
    g.setColour(juce::Colours::darkgrey);
    g.fillEllipse(knobBounds);

    // Draw the knob border
    g.setColour(juce::Colours::white);
    g.drawEllipse(knobBounds, 2.0f);

    // Draw the pointer
    juce::Path pointer;
    auto pointerLength = radius * 0.7f;
    auto pointerThickness = 3.0f;

    pointer.addRectangle(-pointerThickness * 0.5f, -pointerLength, pointerThickness, pointerLength);

    // Transform and draw the pointer
    g.setColour(juce::Colours::orange);
    auto transform = juce::AffineTransform::rotation(rotationAngle).translated(centre.x, centre.y);
    g.fillPath(pointer, transform);
}
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
    // Reserve space for the text box at the bottom
    auto knobArea = bounds.removeFromTop(bounds.getHeight() - 25.0f);
    auto centre = knobArea.getCentre();
    // Make knob fit within the reserved area
    auto radius = juce::jmin(knobArea.getWidth(), knobArea.getHeight()) / 2.0f - 5.0f;
    auto knobBounds = juce::Rectangle<float>(radius * 2.0f, radius * 2.0f).withCentre(centre);
    // =====================================================
    // SVG-style knob background (radial gradient + stroke)
    // =====================================================
    {
        juce::ColourGradient gradient(juce::Colour::fromRGB(0x66, 0x66, 0x66), centre.x, centre.y,
            juce::Colours::black, centre.x, centre.y + radius, true);
        gradient.addColour(0.0001, juce::Colour::fromRGB(0x4F, 0x4F, 0x4F));
        gradient.addColour(1.0, juce::Colours::black);
        g.setGradientFill(gradient);
        g.fillEllipse(knobBounds);
        g.setColour(juce::Colours::white.withAlpha(0.3f)); // subtle stroke
        g.drawEllipse(knobBounds, 1.0f);
    }
    // =====================================================
    // Small indicator circle (pointer)
    // =====================================================
    {
        // Define start and end angles: 7 o'clock to 5 o'clock (300° clockwise rotation)
        auto startAngle = 4.0f * juce::MathConstants<float>::pi / 6.0f;  // 7 o'clock (210°)
        auto endAngle = startAngle + (5.0f * juce::MathConstants<float>::pi / 3.0f);  // +300° rotation

        // Calculate normalized position (0.0 to 1.0)
        auto sliderPos = (getValue() - getMinimum()) / (getMaximum() - getMinimum());

        // Map slider position to angle range
        auto rotationAngle = startAngle + sliderPos * (endAngle - startAngle);

        auto indicatorRadius = 5.5f;
        auto indicatorX = centre.x + std::cos(rotationAngle) * (radius - 15.0f);
        auto indicatorY = centre.y + std::sin(rotationAngle) * (radius - 15.0f);
        juce::Rectangle<float> indicator(indicatorRadius * 2, indicatorRadius * 2);
        indicator.setCentre(indicatorX, indicatorY);
        g.setColour(juce::Colours::black);
        g.fillEllipse(indicator);
        g.setColour(juce::Colour::fromRGB(0x5D, 0x5D, 0x5D));
        g.drawEllipse(indicator, 1.0f);
    }
}
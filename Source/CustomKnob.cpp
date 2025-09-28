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
    setNumDecimalPlacesToDisplay(1);  // Show only 1 decimal place
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
    // Small indicator circle (instead of pointer)
    // =====================================================
    {
        // compute angle from slider value
        auto toAngle = [](double value)
            {
                return juce::MathConstants<float>::pi * 1.5f + value * juce::MathConstants<float>::pi * 1.5f;
            };

        auto sliderPos = getValue();
        auto rotationAngle = toAngle((sliderPos - getMinimum()) / (getMaximum() - getMinimum()));

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

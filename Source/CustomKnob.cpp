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
    setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);

    auto startAngle = 7.0f * juce::MathConstants<float>::pi / 6.0f;  // 7 o'clock (210°)
    auto endAngle = startAngle + (5.0f * juce::MathConstants<float>::pi / 3.0f);  // +300° rotation

    setRotaryParameters(startAngle, endAngle, true);
}

void CustomKnob::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto knobArea = bounds;
    auto centre = knobArea.getCentre();
    auto radius = juce::jmin(knobArea.getWidth(), knobArea.getHeight()) / 2.0f - 5.0f;
    auto knobBounds = juce::Rectangle<float>(radius * 2.0f, radius * 2.0f).withCentre(centre);

    // Dark circular background
    g.setColour(juce::Colour(0xff2a2a2a));
    g.fillEllipse(knobBounds);

    // Draw modulation indicator ring (pulsing glow when LFO is targeting this knob)
    if (modulationActive && modulationIntensity > 0.0f)
    {
        const float glowAlpha = 0.3f + 0.4f * modulationIntensity;
        g.setColour(juce::Colours::cyan.withAlpha(glowAlpha));
        g.drawEllipse(knobBounds.reduced(2.0f), 2.0f);

        // Inner glow
        g.setColour(juce::Colours::cyan.withAlpha(glowAlpha * 0.5f));
        g.drawEllipse(knobBounds.reduced(4.0f), 1.5f);
    }

    // Arc parameters
    auto startAngle = 7.0f * juce::MathConstants<float>::pi / 6.0f;
    auto endAngle = startAngle + (5.0f * juce::MathConstants<float>::pi / 3.0f);
    auto sliderPos = (getValue() - getMinimum()) / (getMaximum() - getMinimum());
    auto currentAngle = startAngle + sliderPos * (endAngle - startAngle);
    auto arcRadius = radius - 0.2f;

    // Alpha increases with slider value (0.15 → 1.0)
    float alpha = static_cast<float>(juce::jmap(sliderPos, 0.0, 1.0, 0.15, 1.0));
    juce::Colour dynamicRed = juce::Colours::red.withAlpha(alpha);

    // Draw red arc (only once)
    juce::Path arcPath;
    arcPath.addCentredArc(centre.x, centre.y,
        arcRadius, arcRadius,
        0.0f, startAngle, currentAngle, true);

    g.setColour(dynamicRed);
    g.strokePath(arcPath, juce::PathStrokeType(4.0f));

    // === White pointer (indicator line) ===
    const float pointerLength = radius + 0.1f;  // extend slightly beyond knob edge
    const float pointerThickness = 3.0f;

    juce::Path pointer;
    pointer.addRoundedRectangle(-pointerThickness * 0.5f, -pointerLength,
        pointerThickness, pointerLength * 0.5f, 1.0f);

    g.setColour(juce::Colours::white);
    g.fillPath(pointer, juce::AffineTransform::rotation(currentAngle).translated(centre.x, centre.y));
}

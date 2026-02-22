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

    // Arc parameters
    auto startAngle = 7.0f * juce::MathConstants<float>::pi / 6.0f;
    auto endAngle = startAngle + (5.0f * juce::MathConstants<float>::pi / 3.0f);
    auto sliderPos = (getValue() - getMinimum()) / (getMaximum() - getMinimum());
    auto currentAngle = startAngle + sliderPos * (endAngle - startAngle);
    auto arcRadius = radius - 0.2f;

    // Alpha increases with slider value (0.15 → 1.0)
    float alpha = static_cast<float>(juce::jmap(sliderPos, 0.0, 1.0, 0.15, 1.0));
    juce::Colour dynamicRed = juce::Colours::red.withAlpha(alpha);

    // Draw red arc
    juce::Path arcPath;
    arcPath.addCentredArc(centre.x, centre.y,
        arcRadius, arcRadius,
        0.0f, startAngle, currentAngle, true);

    g.setColour(dynamicRed);
    g.strokePath(arcPath, juce::PathStrokeType(4.0f));

    // LFO modulation arc
    // Dim arc shows full depth range, bright fill sweeps at LFO rate
    if (lfoArcActive && lfoArcDepth > 0.01f)
    {
        const juce::Colour lfoColour(0xFF44FF88);

        // Max modulated position: at full depth, covers full knob rotation
        float maxModPos = juce::jlimit(0.0f, 1.0f, (float)sliderPos + lfoArcDepth);
        float maxModAngle = startAngle + maxModPos * (endAngle - startAngle);

        // Background arc: always shows full depth range (dim)
        if (maxModAngle - (float)currentAngle > 0.005f)
        {
            juce::Path rangePath;
            rangePath.addCentredArc(centre.x, centre.y,
                arcRadius, arcRadius,
                0.0f, (float)currentAngle, maxModAngle, true);

            g.setColour(lfoColour.withAlpha(0.15f));
            g.strokePath(rangePath, juce::PathStrokeType(4.0f));
        }

        // Animated fill: sweeps from knob position toward max at LFO rate
        float lfoSine = std::sin(lfoArcPhase * juce::MathConstants<float>::twoPi);
        float fillAmount = (lfoSine + 1.0f) * 0.5f;  // 0..1 unipolar
        float fillPos = juce::jlimit(0.0f, 1.0f,
            (float)sliderPos + fillAmount * lfoArcDepth);
        float fillAngle = startAngle + fillPos * (endAngle - startAngle);

        if (fillAngle - (float)currentAngle > 0.005f)
        {
            juce::Path fillPath;
            fillPath.addCentredArc(centre.x, centre.y,
                arcRadius, arcRadius,
                0.0f, (float)currentAngle, fillAngle, true);

            g.setColour(lfoColour.withAlpha(0.6f));
            g.strokePath(fillPath, juce::PathStrokeType(4.0f));
        }
    }

    // === White pointer (indicator line) ===
    const float pointerLength = radius + 0.1f;
    const float pointerThickness = 3.0f;

    juce::Path pointer;
    pointer.addRoundedRectangle(-pointerThickness * 0.5f, -pointerLength,
        pointerThickness, pointerLength * 0.5f, 1.0f);

    g.setColour(juce::Colours::white);
    g.fillPath(pointer, juce::AffineTransform::rotation(currentAngle).translated(centre.x, centre.y));
}

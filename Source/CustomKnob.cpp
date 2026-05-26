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
    setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);

    auto startAngle = 7.0f * juce::MathConstants<float>::pi / 6.0f;  // 7 o'clock (210°)
    auto endAngle = startAngle + (5.0f * juce::MathConstants<float>::pi / 3.0f);  // +300° rotation

    setRotaryParameters(startAngle, endAngle, true);
}

void CustomKnob::paint(juce::Graphics& g)
{
    // ── Coordinate system (spec math frame: 0 = 3 o'clock, y-down, clockwise +) ──
    const float cx = getWidth()  * 0.5f;
    const float cy = getHeight() * 0.5f;
    const float R  = juce::jmin(cx, cy) - 1.0f;

    const float arcR   = R - 4.0f;          // outer value arc
    const float lfoR   = arcR - 5.0f;       // inner LFO arc (5px gap)
    const float bodyR  = lfoR - 4.0f;       // knob cap
    const float tickR1 = bodyR * 0.35f;     // tick inner
    const float tickR2 = bodyR - 1.0f;      // tick outer

    const float START = juce::MathConstants<float>::pi * (2.0f / 3.0f); // 120° = 7 o'clock
    const float SWEEP = juce::MathConstants<float>::pi * (5.0f / 3.0f); // 300°
    const float END   = START + SWEEP;

    const float norm = static_cast<float>(
        (getValue() - getMinimum()) / (getMaximum() - getMinimum()));
    const float valueAngle = START + norm * SWEEP;

    // cos/sin point math uses the spec math frame directly.
    auto ptOnCircle = [](float ox, float oy, float r, float a)
    {
        return juce::Point<float>(ox + r * std::cos(a), oy + r * std::sin(a));
    };

    // JUCE arc functions use 0 = top-centre, so convert math-frame angles by +π/2.
    auto makeArc = [](float ox, float oy, float r, float a0, float a1)
    {
        const float h = juce::MathConstants<float>::halfPi;
        juce::Path p;
        p.addCentredArc(ox, oy, r, r, 0.0f, a0 + h, a1 + h, true);
        return p;
    };

    const juce::PathStrokeType stroke(2.5f, juce::PathStrokeType::curved,
                                            juce::PathStrokeType::rounded);

    // 1. Rim shadow
    g.setColour(juce::Colour(0xff111111));
    g.fillEllipse(cx - R, cy - R, R * 2.0f, R * 2.0f);

    // 2. Knob body (approximated radial gradient)
    juce::ColourGradient body(
        juce::Colour(0xff424242), cx - bodyR * 0.2f, cy - bodyR * 0.3f,
        juce::Colour(0xff1e1e1e), cx + bodyR * 0.4f, cy + bodyR * 0.4f, true);
    g.setGradientFill(body);
    g.fillEllipse(cx - bodyR, cy - bodyR, bodyR * 2.0f, bodyR * 2.0f);

    // 3. Outer groove (full 270° track)
    g.setColour(juce::Colour(0xff282828));
    g.strokePath(makeArc(cx, cy, arcR, START, END), stroke);

    // 4. Value arc — pale red at low values, saturating to deep red as the knob turns up
    if (norm > 0.005f)
    {
        const float sat = juce::jmap(norm, 0.0f, 1.0f, 0.30f, 0.95f);
        const float bri = juce::jmap(norm, 0.0f, 1.0f, 0.95f, 0.78f);
        const juce::Colour arcColour = juce::Colour::fromHSV(0.0f, sat, bri, 1.0f);

        g.setColour(arcColour.withAlpha(juce::jmap(norm, 0.0f, 1.0f, 0.20f, 0.35f)));
        g.strokePath(makeArc(cx, cy, arcR, START, valueAngle),
                     juce::PathStrokeType(5.0f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
        g.setColour(arcColour);
        g.strokePath(makeArc(cx, cy, arcR, START, valueAngle), stroke);
    }

    // 5-8. Inner LFO ring (rendered only when an LFO targets this knob)
    if (lfoArcActive)
    {
        const juce::PathStrokeType lfoStroke(2.0f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded);
        const juce::Colour lfoColour(0xff3ecf72);

        // 5. Inner LFO groove
        g.setColour(juce::Colour(0xff222222));
        g.strokePath(makeArc(cx, cy, lfoR, START, END), lfoStroke);

        if (lfoArcDepth > 0.01f)
        {
            // Dim arc: full modulation range (value → value + depth)
            const float maxModPos   = juce::jlimit(0.0f, 1.0f, norm + lfoArcDepth);
            const float maxModAngle = START + maxModPos * SWEEP;
            if (maxModAngle - valueAngle > 0.005f)
            {
                g.setColour(lfoColour.withAlpha(0.18f));
                g.strokePath(makeArc(cx, cy, lfoR, valueAngle, maxModAngle), lfoStroke);
            }

            // 6/7. Animated fill sweeping at the live LFO rate (two-pass glow)
            const float lfoSine    = std::sin(lfoArcPhase * juce::MathConstants<float>::twoPi);
            const float fillAmount = (lfoSine + 1.0f) * 0.5f;
            const float fillPos    = juce::jlimit(0.0f, 1.0f, norm + fillAmount * lfoArcDepth);
            const float fillAngle  = START + fillPos * SWEEP;
            if (fillAngle - valueAngle > 0.005f)
            {
                g.setColour(lfoColour.withAlpha(0.22f));
                g.strokePath(makeArc(cx, cy, lfoR, valueAngle, fillAngle),
                             juce::PathStrokeType(5.0f, juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::rounded));
                g.setColour(lfoColour.withAlpha(0.9f));
                g.strokePath(makeArc(cx, cy, lfoR, valueAngle, fillAngle), lfoStroke);
            }

            // 8. Center notch dot marks the base value within the mod range
            const auto dot = ptOnCircle(cx, cy, lfoR, valueAngle);
            const float dotR = 1.8f;
            g.setColour(lfoColour);
            g.fillEllipse(dot.x - dotR, dot.y - dotR, dotR * 2.0f, dotR * 2.0f);
        }
    }

    // 9. Tick line
    const auto t1 = ptOnCircle(cx, cy, tickR1, valueAngle);
    const auto t2 = ptOnCircle(cx, cy, tickR2, valueAngle);
    g.setColour(juce::Colour(0xffe0e0e0));
    g.drawLine(t1.x, t1.y, t2.x, t2.y, 2.0f);
}

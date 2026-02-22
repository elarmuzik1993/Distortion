#pragma once
#include <JuceHeader.h>

class CustomKnob : public juce::Slider
{
public:
    CustomKnob();

    void paint(juce::Graphics& g) override;

    // Set LFO arc: active = targeted by LFO, lfoPhase = 0-1 cycle position, depth = 0-1 intensity
    void setLFOArc(bool active, float lfoPhase, float depth)
    {
        lfoArcActive = active;
        lfoArcPhase = lfoPhase;
        lfoArcDepth = depth;
        repaint();
    }

private:
    bool lfoArcActive = false;
    float lfoArcPhase = 0.0f;   // 0-1, current LFO cycle position
    float lfoArcDepth = 0.0f;   // 0-1, controls arc intensity/size

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CustomKnob)
};
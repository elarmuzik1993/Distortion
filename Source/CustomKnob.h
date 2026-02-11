#pragma once
#include <JuceHeader.h>

class CustomKnob : public juce::Slider
{
public:
    CustomKnob();

    void paint(juce::Graphics& g) override;

    void setModulationIndicator(bool active, float intensity = 1.0f)
    {
        modulationActive = active;
        modulationIntensity = intensity;
        repaint();
    }

private:
    bool modulationActive = false;
    float modulationIntensity = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CustomKnob)
};
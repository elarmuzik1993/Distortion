#pragma once
#include <JuceHeader.h>

class CustomKnob : public juce::Slider
{
public:
    CustomKnob(); 

    void paint(juce::Graphics& g) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CustomKnob)
};
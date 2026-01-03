#include <iostream>
#include <JuceHeader.h>

int main() {
    juce::AudioParameterFloat param(
        juce::ParameterID{"test", 1},
        "Test",
        juce::NormalisableRange<float>(0.0f, 100.0f),
        50.0f
    );
    
    std::cout << "Default value: " << param.get() << std::endl;
    
    return 0;
}

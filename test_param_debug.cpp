#include <iostream>
#include <JuceHeader.h>
#include "../Source/PluginProcessor.h"
#include "../Source/Tests/TestUtilities.h"

int main() {
    juce::ScopedJuceInitialiser_GUI juceInit;

    std::cout << "Creating processor...\n";
    PluginProcessor processor;
    processor.prepareToPlay(44100.0, 512);

    std::cout << "Initial distortion amount: " << processor.distortionAmountParam->load() << "\n";
    std::cout << "Initial output gain: " << processor.outputGainParam->load() << "\n";

    // Set distortion to 50.0
    std::cout << "\nSetting distortion to 50.0...\n";
    TestUtilities::setParameter(processor.parameters, "distortionAmount", 50.0f);

    std::cout << "After setting - distortion amount: " << processor.distortionAmountParam->load() << "\n";

    // Set output gain to 100.0
    std::cout << "\nSetting output gain to 100.0...\n";
    TestUtilities::setParameter(processor.parameters, "outputGain", 100.0f);

    std::cout << "After setting - output gain: " << processor.outputGainParam->load() << "\n";

    return 0;
}

#include <iostream>
#include <JuceHeader.h>

// Include test utilities
#define JUCE_DEBUG 1
#include "Source/PluginProcessor.h"
#include "Source/Tests/TestUtilities.h"

using namespace TestUtilities;

int main() {
    juce::ScopedJuceInitialiser_GUI juceInit;

    std::cout << "Creating processor...\n";
    PluginProcessor processor;

    std::cout << "Calling prepareToPlay...\n";
    processor.prepareToPlay(44100.0, 512);

    std::cout << "Setting distortion to 50...\n";
    setParameter(processor.parameters, "distortionAmount", 50.0f);

    std::cout << "Distortion param value: " << processor.distortionAmountParam->load() << "\n";

    std::cout << "Creating buffer...\n";
    auto buffer = generateSineWave(1000.0, 44100.0, 512, 0.5f);
    juce::MidiBuffer midi;

    std::cout << "Input peak: " << calculatePeak(buffer) << "\n";

    std::cout << "Processing block...\n";
    processor.processBlock(buffer, midi);

    float outputPeak = calculatePeak(buffer);
    bool hasNaN = containsInvalidSamples(buffer);

    std::cout << "Output peak: " << outputPeak << "\n";
    std::cout << "Has NaN: " << (hasNaN ? "YES" : "NO") << "\n";

    return 0;
}

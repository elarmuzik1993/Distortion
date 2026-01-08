/*
  ==============================================================================

    Distortion Plugin Test Runner

    A standalone console application that runs all unit tests for the
    Distortion plugin. This is separate from the plugin to avoid issues
    with nested PluginProcessor creation during testing.

  ==============================================================================
*/

#include <JuceHeader.h>

// Force JUCE_DEBUG to be defined for test builds
#ifndef JUCE_DEBUG
#define JUCE_DEBUG 1
#endif

#include "../PluginProcessor.h"
#include "DistortionTests.h"

//==============================================================================
int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    std::cout << "Starting test runner...\n";
    std::cout.flush();

    // Initialize JUCE for GUI components (needed for audio processors)
    juce::ScopedJuceInitialiser_GUI juceInit;

    std::cout << "JUCE initialized.\n";
    std::cout.flush();

    std::cout << "\n";
    std::cout << "================================================================\n";
    std::cout << "       DISTORTION PLUGIN TEST RUNNER\n";
    std::cout << "================================================================\n";
    std::cout << "\n";
    std::cout.flush();

    // Register all tests
    std::cout << "Registering tests...\n";
    std::cout.flush();
    registerAllTests();
    std::cout << "Tests registered.\n";
    std::cout.flush();

    juce::UnitTestRunner runner;
    runner.setAssertOnFailure(false);
    runner.setPassesAreLogged(true);

    // Run all tests
    std::cout << "Running tests...\n";
    std::cout.flush();
    runner.runAllTests();
    std::cout << "Tests completed.\n";
    std::cout.flush();

    // Calculate and display results
    int totalPasses = 0;
    int totalFailures = 0;

    std::cout << "\n";
    std::cout << "================================================================\n";
    std::cout << "                      TEST RESULTS\n";
    std::cout << "================================================================\n";

    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        auto* result = runner.getResult(i);
        totalPasses += result->passes;
        totalFailures += result->failures;

        if (result->failures > 0)
        {
            std::cout << "FAILED: " << result->unitTestName << " ("
                      << result->passes << " passed, "
                      << result->failures << " failed)\n";

            for (const auto& message : result->messages)
                std::cout << "  - " << message << "\n";
        }
        else
        {
            std::cout << "PASSED: " << result->unitTestName << " ("
                      << result->passes << " assertions)\n";
        }
    }

    std::cout << "\n";
    std::cout << "================================================================\n";

    if (totalFailures == 0)
    {
        std::cout << "  ALL TESTS PASSED: " << totalPasses << " assertions\n";
    }
    else
    {
        std::cout << "  TESTS FAILED: " << totalPasses << " passed, "
                  << totalFailures << " FAILED\n";
    }

    std::cout << "================================================================\n";
    std::cout << "\n";

    return totalFailures > 0 ? 1 : 0;
}

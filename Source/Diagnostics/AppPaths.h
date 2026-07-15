#pragma once
#include <JuceHeader.h>

namespace diag
{
    // Mirrors the parent of PluginEditor::getPresetDirectory() (same app-data product folder).
    inline juce::File productDir()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Monolit Beatz")
                   .getChildFile ("Distortion");
    }

    inline juce::File reportsDir()
    {
        auto d = productDir().getChildFile ("reports");
        if (! d.exists())
        {
            auto result = d.createDirectory();   // JUCE createDirectory() also creates parent dirs
            jassert (result.wasOk());             // surface failure (e.g. permissions) in debug builds
            juce::ignoreUnused (result);          // result unused in release (jassert compiled out)
        }
        return d;
    }

    inline juce::File settingsFile()  { return productDir().getChildFile ("settings.xml"); }
    inline juce::File installIdFile() { return productDir().getChildFile ("installId"); }
}

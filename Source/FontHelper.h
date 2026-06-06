#pragma once
#include <JuceHeader.h>

namespace Fonts {
    inline juce::Font getOrbitron(float size, bool bold = false)
    {
        static auto typefaceBold = juce::Typeface::createSystemTypefaceFor(
            BinaryData::OrbitronBold_ttf, BinaryData::OrbitronBold_ttfSize);
        static auto typefaceRegular = juce::Typeface::createSystemTypefaceFor(
            BinaryData::OrbitronRegular_ttf, BinaryData::OrbitronRegular_ttfSize);
        return juce::Font(bold ? typefaceBold : typefaceRegular).withHeight(size);
    }
}

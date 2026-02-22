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

namespace UIColors {
    const juce::Colour accent       (0xFFFF2244);
    const juce::Colour accentLight  (0xFFFF4466);
    const juce::Colour bg           (0xFF0D0D0D);
    const juce::Colour scopeBg      (0xFF080808);
    const juce::Colour knobBody     (0xFF3A3A3A);
    const juce::Colour knobMid      (0xFF1E1E1E);
    const juce::Colour knobInner    (0xFF151515);
    const juce::Colour knobBorder   (0xFF333333);
    const juce::Colour lfoGreen     (0xFF44FF88);
    const juce::Colour cyan         (0xFF44CCFF);
    const juce::Colour textDim      (0xFF2A2A2A);
    const juce::Colour textMid      (0xFF888888);
    const juce::Colour panelBg      (0xFF111111);
    const juce::Colour divider      (0xFF282828);
    const juce::Colour ledOff       (0xFF1A1A1A);
    const juce::Colour ledBorder    (0xFF282828);
}

#pragma once

#include "PluginProcessor.h"

// The v2.3.0 multimode input filter (filterMode, highPassFreq) runs in place of
// Shape while the legacyFilter switch is on, so old sessions sound identical. In
// Shape mode it sits at its transparent default whatever those parameters hold.
namespace LegacyInputFilter
{
    // Whether it is doing anything: a high pass near the subsonic floor and a low
    // pass near Nyquist are transparent; a band pass always cuts.
    inline bool isActive(int mode, float hz) noexcept
    {
        switch (mode)
        {
            case 1:  return hz < DSPConstants::LOWPASS_TRANSPARENT_MIN_FREQ;  // Low Pass
            case 2:  return true;                                             // Band Pass
            default: return hz > DSPConstants::HIPASS_TRANSPARENT_MAX_FREQ;   // High Pass
        }
    }

    // LFO destination 2 in Legacy mode, as v2.3 had it: the cutoff swings up to
    // 1.5 octaves either side of the knob (lfo in -1..1), clamped to 20 Hz-20 kHz.
    inline float modulatedCutoff(float hz, float lfo) noexcept
    {
        const float centreLog = std::log2(juce::jmax(20.0f, hz));
        return std::pow(2.0f, juce::jlimit(4.32f, 14.29f, centreLog + lfo * 1.5f));
    }
}

#pragma once

#include "PluginProcessor.h"

// The v2.3.0 multimode input filter lives on as hidden parameters (filterMode,
// highPassFreq) so old sessions sound identical. This says whether it is doing
// anything: a high pass near the subsonic floor and a low pass near Nyquist are
// transparent; a band pass always cuts.
namespace LegacyInputFilter
{
    inline bool isActive(int mode, float hz) noexcept
    {
        switch (mode)
        {
            case 1:  return hz < DSPConstants::LOWPASS_TRANSPARENT_MIN_FREQ;  // Low Pass
            case 2:  return true;                                             // Band Pass
            default: return hz > DSPConstants::HIPASS_TRANSPARENT_MAX_FREQ;   // High Pass
        }
    }
}

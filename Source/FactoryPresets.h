/*
  ==============================================================================

    FactoryPresets.h

    Single source of truth for the built-in factory preset bank. Defined once
    and consumed by the editor's preset browser (refreshPresetList /
    loadFactoryPreset) and by the unit tests. Kept free of GUI/processor
    coupling so there is exactly one definition to maintain.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <vector>

namespace FactoryPresets
{
    struct ParamValue
    {
        const char* id;
        float       value;
    };

    struct Preset
    {
        const char*             name;
        std::vector<ParamValue> overrides;  // applied on top of baseline()
    };

    // Neutral starting point applied before any preset's overrides so a factory
    // preset loads deterministically regardless of the prior parameter state.
    // (linearPhaseDry is intentionally excluded — it is a global oversampling
    // quality toggle, not a sound-design parameter, and resetting it on every
    // preset load would force an oversampler rebuild and override the user's
    // chosen character.)
    inline const std::vector<ParamValue>& baseline()
    {
        static const std::vector<ParamValue> b = {
            { "inputGain",          50.0f },
            { "outputGain",         50.0f },
            { "distortionAmount",   20.0f },
            { "highPassFreq",       20.0f },
            { "filterMode",          0.0f },
            { "legacyFilter",        0.0f },   // presets are Shape sounds
            { "shape",               0.0f },
            { "subGuardFreq",       60.0f },
            { "clipType",            0.0f },
            { "distMix",           100.0f },
            { "tone",            20000.0f },
            { "waveshaperClean",     0.0f },
            { "waveshaperMix",       0.0f },
            { "lfoRate",             0.0f },
            { "lfoDepth",            0.0f },
            { "lfoWaveform",         0.0f },
            { "lfoEnabled",          0.0f },
            { "lfoDestination",      0.0f },
            { "lfoBpmSync",          0.0f },
            { "lfoBpmDivision",      2.0f },
            { "lfoInvert",           0.0f },
            { "compPeakReduction",   0.0f },
            { "compMakeupGain",     50.0f },
            { "compRatio",           0.0f },
            { "compEnabled",         0.0f },
            { "autoGainEnabled",     1.0f },
            { "extremeEnabled",      0.0f },
            { "cleanBoost",          0.0f },
            { "globalMix",         100.0f },
        };
        return b;
    }

    // The factory bank. Only "Default" (the baseline, a one-click reset) while the
    // sound is still in development; the voiced presets come back once it settles
    // (owner, 2026-10-25). A new preset is one entry here: { name, overrides }.
    // Sessions are unaffected: they store parameter values, not preset names.
    inline const std::vector<Preset>& all()
    {
        static const std::vector<Preset> presets = {
            { "Default", {} },
        };
        return presets;
    }

    // Whether name matches a built-in factory preset.
    inline bool isFactory(const juce::String& name)
    {
        for (const auto& p : all())
            if (name == p.name)
                return true;
        return false;
    }

    // Applies baseline() then the named preset's overrides to apvts. Returns
    // false (and changes nothing) if name is not a factory preset.
    inline bool apply(juce::AudioProcessorValueTreeState& apvts, const juce::String& name)
    {
        const Preset* found = nullptr;
        for (const auto& p : all())
            if (name == p.name) { found = &p; break; }

        if (found == nullptr)
            return false;

        const auto set = [&apvts](const ParamValue& pv)
        {
            if (auto* param = apvts.getParameter(pv.id))
                param->setValueNotifyingHost(param->convertTo0to1(pv.value));
        };

        for (const auto& pv : baseline())        set(pv);
        for (const auto& pv : found->overrides)  set(pv);
        return true;
    }
}

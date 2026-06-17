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
            { "distortionAmount",    0.0f },
            { "highPassFreq",       20.0f },
            { "filterMode",          0.0f },
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

    // The factory bank. The first eight entries preserve the original presets
    // verbatim (existing sessions/preset files load identically); the remainder
    // round the bank out to cover every clip type and common use case.
    inline const std::vector<Preset>& all()
    {
        static const std::vector<Preset> presets = {
            { "Default", {} },
            { "Warm Tube", {
                { "inputGain", 60.0f }, { "outputGain", 45.0f },
                { "distortionAmount", 30.0f }, { "highPassFreq", 80.0f },
                { "clipType", 1.0f }, { "distMix", 70.0f } } },
            { "Hard Clip", {
                { "inputGain", 70.0f }, { "outputGain", 40.0f },
                { "distortionAmount", 70.0f }, { "highPassFreq", 100.0f },
                { "subGuardFreq", 150.0f }, { "clipType", 0.0f },
                { "compPeakReduction", 30.0f }, { "compMakeupGain", 60.0f },
                { "compEnabled", 1.0f } } },
            { "Soft Saturation", {
                { "inputGain", 55.0f }, { "outputGain", 48.0f },
                { "distortionAmount", 20.0f }, { "highPassFreq", 40.0f },
                { "clipType", 3.0f }, { "distMix", 50.0f } } },
            { "808 Safe", {
                { "inputGain", 65.0f }, { "outputGain", 45.0f },
                { "distortionAmount", 60.0f }, { "highPassFreq", 150.0f },
                { "subGuardFreq", 150.0f }, { "clipType", 3.0f } } },
            { "Parallel Grit", {
                { "inputGain", 58.0f }, { "distortionAmount", 85.0f },
                { "highPassFreq", 60.0f }, { "clipType", 5.0f },
                { "globalMix", 35.0f }, { "compPeakReduction", 20.0f },
                { "compEnabled", 1.0f } } },
            { "Vocal Warmth", {
                { "distortionAmount", 25.0f }, { "highPassFreq", 100.0f },
                { "clipType", 1.0f }, { "tone", 6000.0f }, { "distMix", 60.0f },
                { "lfoEnabled", 1.0f }, { "lfoDestination", 1.0f },
                { "lfoRate", 0.3f }, { "lfoDepth", 30.0f } } },
            { "EXTREME", {
                { "inputGain", 75.0f }, { "outputGain", 35.0f },
                { "distortionAmount", 90.0f }, { "highPassFreq", 120.0f },
                { "subGuardFreq", 100.0f }, { "clipType", 6.0f },
                { "extremeEnabled", 1.0f }, { "compPeakReduction", 40.0f },
                { "compRatio", 1.0f }, { "compEnabled", 1.0f } } },
            // ---- v2.2 additions ----------------------------------------------
            { "Bass Driver", {
                { "inputGain", 62.0f }, { "outputGain", 46.0f },
                { "distortionAmount", 45.0f }, { "highPassFreq", 50.0f },
                { "subGuardFreq", 120.0f }, { "clipType", 4.0f },
                { "tone", 9000.0f }, { "compPeakReduction", 25.0f },
                { "compMakeupGain", 55.0f }, { "compEnabled", 1.0f } } },
            { "Lo-Fi Crush", {
                { "inputGain", 60.0f }, { "outputGain", 44.0f },
                { "distortionAmount", 55.0f }, { "highPassFreq", 100.0f },
                { "clipType", 2.0f }, { "tone", 5000.0f }, { "distMix", 80.0f } } },
            { "Transformer Glue", {
                { "inputGain", 54.0f }, { "outputGain", 50.0f },
                { "distortionAmount", 35.0f }, { "clipType", 4.0f },
                { "tone", 12000.0f }, { "distMix", 65.0f },
                { "compPeakReduction", 15.0f }, { "compEnabled", 1.0f } } },
            { "Drum Smash", {
                { "inputGain", 64.0f }, { "outputGain", 42.0f },
                { "distortionAmount", 75.0f }, { "highPassFreq", 60.0f },
                { "clipType", 0.0f }, { "globalMix", 50.0f },
                { "compPeakReduction", 35.0f }, { "compRatio", 1.0f },
                { "compEnabled", 1.0f } } },
            { "Vintage Tape", {
                { "inputGain", 56.0f }, { "outputGain", 48.0f },
                { "distortionAmount", 30.0f }, { "highPassFreq", 30.0f },
                { "clipType", 3.0f }, { "tone", 8000.0f },
                { "distMix", 75.0f }, { "waveshaperMix", 20.0f } } },
            { "Diode Bite", {
                { "inputGain", 60.0f }, { "outputGain", 45.0f },
                { "distortionAmount", 55.0f }, { "highPassFreq", 120.0f },
                { "clipType", 5.0f }, { "tone", 7000.0f }, { "distMix", 90.0f } } },
            { "Tremolo Drive", {
                { "inputGain", 56.0f }, { "outputGain", 48.0f },
                { "distortionAmount", 40.0f }, { "clipType", 1.0f },
                { "distMix", 80.0f }, { "lfoEnabled", 1.0f },
                { "lfoDestination", 4.0f }, { "lfoRate", 4.0f },
                { "lfoDepth", 50.0f }, { "lfoWaveform", 0.0f } } },
            { "Screamer", {
                { "inputGain", 58.0f }, { "outputGain", 46.0f },
                { "distortionAmount", 25.0f }, { "highPassFreq", 40.0f },
                { "clipType", 1.0f }, { "tone", 14000.0f },
                { "distMix", 80.0f }, { "cleanBoost", 1.0f } } },
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

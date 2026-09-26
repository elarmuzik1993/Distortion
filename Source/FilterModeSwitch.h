#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

/**
    Moves the input filter's cutoff when the user switches its mode, so a filter
    left at one mode's default starts at the next mode's default rather than
    somewhere that silences the input (High Pass at 20 Hz becomes Low Pass at
    20 Hz otherwise). A cutoff the user has set is left alone.

    Only a user gesture triggers it (the combo box's attachment wraps each pick in
    one). Presets, saved sessions, randomize and host automation set the mode
    without a gesture, so what they restore is never changed.
*/
namespace FilterModeSwitch
{
    enum Mode { highPass = 0, lowPass = 1, bandPass = 2 };

    inline float defaultCutoff (int mode)
    {
        switch (mode)
        {
            case lowPass:  return DSPConstants::DEFAULT_LOWPASS_FREQ;
            case bandPass: return DSPConstants::DEFAULT_BANDPASS_FREQ;
            default:       return DSPConstants::DEFAULT_HIPASS_FREQ;
        }
    }

    // High and low pass count as at their default anywhere they pass everything.
    inline bool isAtDefault (int mode, float hz)
    {
        switch (mode)
        {
            case lowPass:  return hz >= DSPConstants::LOWPASS_TRANSPARENT_MIN_FREQ;
            case bandPass: return std::abs (hz - DSPConstants::DEFAULT_BANDPASS_FREQ) < 0.5f;
            default:       return hz <= DSPConstants::HIPASS_TRANSPARENT_MAX_FREQ;
        }
    }

    inline float cutoffAfterSwitch (int fromMode, int toMode, float hz)
    {
        if (fromMode == toMode || ! isAtDefault (fromMode, hz))
            return hz;
        return defaultCutoff (toMode);
    }

    class Listener : private juce::AudioProcessorParameter::Listener
    {
    public:
        Listener (juce::RangedAudioParameter& mode, juce::RangedAudioParameter& cutoff)
            : modeParam (mode), cutoffParam (cutoff), lastMode (modeFrom (mode.getValue()))
        {
            modeParam.addListener (this);
        }

        ~Listener() override { modeParam.removeListener (this); }

    private:
        int modeFrom (float normalised) const
        {
            return juce::roundToInt (modeParam.convertFrom0to1 (normalised));
        }

        // Both callbacks can arrive on any thread (automation runs on the audio
        // thread), so state is atomic and only the message thread acts.
        void parameterGestureChanged (int, bool gestureIsStarting) override
        {
            inGesture = gestureIsStarting;
        }

        void parameterValueChanged (int, float newValue) override
        {
            const int toMode = modeFrom (newValue);
            const int fromMode = lastMode.exchange (toMode);

            if (! inGesture || fromMode == toMode
                || ! juce::MessageManager::existsAndIsCurrentThread())
                return;

            const float hz = cutoffParam.convertFrom0to1 (cutoffParam.getValue());
            const float target = cutoffAfterSwitch (fromMode, toMode, hz);
            if (target == hz)
                return;

            cutoffParam.beginChangeGesture();
            cutoffParam.setValueNotifyingHost (cutoffParam.convertTo0to1 (target));
            cutoffParam.endChangeGesture();
        }

        juce::RangedAudioParameter& modeParam;
        juce::RangedAudioParameter& cutoffParam;
        std::atomic<int> lastMode;
        std::atomic<bool> inGesture { false };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Listener)
    };
}

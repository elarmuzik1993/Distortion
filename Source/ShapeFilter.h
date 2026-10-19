#pragma once

#include <JuceHeader.h>
#include "FilterCoeffs.h"
#include <cmath>

// Shape voicing (Orange Micro Dark style, tuned for 808s and bass). Values are at
// amount 1.0; each ShapeFilter instance scales the dB figures by its amount.
namespace DSPConstants
{
    constexpr float  SHAPE_MIN           = -100.0f;  // full Bark
    constexpr float  SHAPE_MAX           =  100.0f;  // full Scoop
    constexpr double SHAPE_MID_FREQ      = 600.0;    // 808/bass harmonics that carry on small speakers
    constexpr double SHAPE_MID_Q         = 0.7;      // broad: voicing, not a notch
    constexpr float  SHAPE_MID_BOOST_DB  = 12.0f;    // at full Bark
    constexpr float  SHAPE_MID_CUT_DB    = 15.0f;    // at full Scoop
    constexpr double SHAPE_SHELF_FREQ    = 2500.0;   // fizz / edge region of distorted bass
    constexpr double SHAPE_SHELF_Q       = 0.707;
    constexpr float  SHAPE_SHELF_DB      = 6.0f;     // cut at Bark, boost at Scoop
    constexpr float  SHAPE_TAPER         = 1.0f;     // linear: half a turn gives half the effect
    constexpr float  SHAPE_PRE_AMOUNT    = 1.0f;     // pre-drive instance
    constexpr float  SHAPE_POST_AMOUNT   = 0.75f;    // post-drive instance, at full drive
    constexpr float  SHAPE_POST_FULL_DRIVE = 10.0f;  // distortion % where the post-drive half reaches full amount
    constexpr float  SHAPE_LFO_RANGE     = 50.0f;    // knob units at full LFO depth
    constexpr float  SHAPE_FLAT_EPS      = 0.01f;    // |shape| below this counts as flat
    constexpr double SHAPE_SMOOTH_TIME_S = 0.02;
    constexpr double SHAPE_FLAT_HOLD_S   = 0.01;     // keep filtering this long after reaching Flat
    constexpr int    SHAPE_SUBBLOCK      = 32;       // samples per coefficient update
}

/**
    One Shape stage: a broad mid peak/dip plus an opposing high shelf, driven by a
    single bipolar value (-100 Bark .. 0 Flat .. +100 Scoop). The processor runs one
    instance before the drive and one after it. Coefficients are rewritten in place
    every SHAPE_SUBBLOCK samples from a smoothed value, so nothing allocates on the
    audio thread. At 0 (target and current) process() does nothing at all.
*/
class ShapeFilter
{
public:
    static float taper(float shape) noexcept
    {
        const float s = juce::jlimit(-1.0f, 1.0f, shape / DSPConstants::SHAPE_MAX);
        return std::copysign(std::pow(std::abs(s), DSPConstants::SHAPE_TAPER), s);
    }

    static float midGainDb(float shape, float amount) noexcept
    {
        const float t = taper(shape);
        return amount * (t < 0.0f ? -t * DSPConstants::SHAPE_MID_BOOST_DB
                                  : -t * DSPConstants::SHAPE_MID_CUT_DB);
    }

    static float shelfGainDb(float shape, float amount) noexcept
    {
        return amount * taper(shape) * DSPConstants::SHAPE_SHELF_DB;
    }

    // Post-drive amount for a distortion amount in %. It fades in with the drive, so
    // at the true-bypass threshold (where only the pre-drive half runs) both paths
    // shape alike, and it only reaches full strength once there are harmonics to shape.
    static float postDriveAmount(float drivePercent) noexcept
    {
        return DSPConstants::SHAPE_POST_AMOUNT
             * juce::jlimit(0.0f, 1.0f, drivePercent / DSPConstants::SHAPE_POST_FULL_DRIVE);
    }

    // Knob value plus block-level LFO modulation (-1..1), clamped to the range.
    static float modulated(float knob, float lfoModulation) noexcept
    {
        return juce::jlimit(DSPConstants::SHAPE_MIN, DSPConstants::SHAPE_MAX,
                            knob + lfoModulation * DSPConstants::SHAPE_LFO_RANGE);
    }

    static juce::String toText(float value)
    {
        const int n = juce::roundToInt(std::abs(value));
        if (n == 0)
            return "Flat";
        return juce::String(value < 0.0f ? "Bark " : "Scoop ") + juce::String(n);
    }

    static float fromText(const juce::String& text)
    {
        const auto t = text.trim();
        if (t.equalsIgnoreCase("Flat"))
            return 0.0f;
        const float n = t.retainCharacters("0123456789.").getFloatValue();
        if (t.startsWithIgnoreCase("Bark"))  return -n;
        if (t.startsWithIgnoreCase("Scoop")) return n;
        return t.getFloatValue();
    }

    void prepare(const juce::dsp::ProcessSpec& spec, float initialShape, float amountIn)
    {
        sampleRate = spec.sampleRate;
        flatHoldSamples = static_cast<int>(spec.sampleRate * DSPConstants::SHAPE_FLAT_HOLD_S);
        mid.prepare(spec);
        shelf.prepare(spec);
        // Five-coefficient biquads (allocates: prepare runs off the audio thread).
        *mid.state   = juce::dsp::IIR::Coefficients<float>(1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
        *shelf.state = juce::dsp::IIR::Coefficients<float>(1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
        smoothed.reset(spec.sampleRate, DSPConstants::SHAPE_SMOOTH_TIME_S);
        smoothed.setCurrentAndTargetValue(juce::jlimit(DSPConstants::SHAPE_MIN,
                                                       DSPConstants::SHAPE_MAX, initialShape));
        smoothedAmount.reset(spec.sampleRate, DSPConstants::SHAPE_SMOOTH_TIME_S);
        smoothedAmount.setCurrentAndTargetValue(juce::jmax(0.0f, amountIn));
        reset();
    }

    void setTarget(float shape) noexcept
    {
        smoothed.setTargetValue(juce::jlimit(DSPConstants::SHAPE_MIN, DSPConstants::SHAPE_MAX, shape));
    }

    // Scales both dB figures (smoothed, so it can follow the drive amount).
    void setAmount(float amountIn) noexcept
    {
        smoothedAmount.setTargetValue(juce::jmax(0.0f, amountIn));
    }

    // True while process() will touch the signal: while shaping, and for
    // SHAPE_FLAT_HOLD_S after reaching Flat so the filter tail can decay.
    bool isActive() const noexcept
    {
        return isShaping() || flatSamplesLeft > 0;
    }

    void process(juce::dsp::AudioBlock<float>& block) noexcept
    {
        const int total = static_cast<int>(block.getNumSamples());
        if (isShaping())
        {
            flatSamplesLeft = flatHoldSamples;
        }
        else if (flatSamplesLeft > 0)
        {
            // Just reached Flat: the coefficients are identity now, but the filter
            // memory still carries a decaying error. Keep running until it has died
            // away, so clearing it below cannot click.
            flatSamplesLeft -= total;
        }
        else
        {
            // Settled flat: skip entirely (bit-transparent), and clear the filter
            // memory once so a later re-engage starts clean.
            if (! stateClear)
            {
                mid.reset();
                shelf.reset();
                stateClear = true;
            }
            return;
        }

        stateClear = false;
        for (int start = 0; start < total; start += DSPConstants::SHAPE_SUBBLOCK)
        {
            const int len = juce::jmin(DSPConstants::SHAPE_SUBBLOCK, total - start);
            const float v = smoothed.skip(len);
            const float a = smoothedAmount.skip(len);
            if (std::abs(v - lastWritten) > 1.0e-3f || std::abs(a - lastAmount) > 1.0e-4f)
            {
                writeCoefficients(v, a);
                lastWritten = v;
                lastAmount = a;
            }
            auto sub = block.getSubBlock(static_cast<size_t>(start), static_cast<size_t>(len));
            juce::dsp::ProcessContextReplacing<float> ctx(sub);
            mid.process(ctx);
            shelf.process(ctx);
        }
    }

    // Clears filter memory and snaps the smoother to its target (state load,
    // transport reset). Forces a coefficient rewrite on the next process().
    void reset() noexcept
    {
        smoothed.setCurrentAndTargetValue(smoothed.getTargetValue());
        smoothedAmount.setCurrentAndTargetValue(smoothedAmount.getTargetValue());
        mid.reset();
        shelf.reset();
        stateClear = true;
        flatSamplesLeft = 0;
        lastWritten = 1.0e9f;
        lastAmount = -1.0f;
    }

private:
    // Shaping at all: shape and amount both away from zero (target or current).
    bool isShaping() const noexcept
    {
        return std::abs(smoothed.getTargetValue()) * smoothedAmount.getTargetValue() > DSPConstants::SHAPE_FLAT_EPS
            || std::abs(smoothed.getCurrentValue()) * smoothedAmount.getCurrentValue() > DSPConstants::SHAPE_FLAT_EPS;
    }

    void writeCoefficients(float v, float a) noexcept
    {
        writePeakFilterCoeffs(*mid.state, sampleRate, DSPConstants::SHAPE_MID_FREQ,
                              DSPConstants::SHAPE_MID_Q,
                              juce::Decibels::decibelsToGain(static_cast<double>(midGainDb(v, a))));
        writeHighShelfCoeffs(*shelf.state, sampleRate, DSPConstants::SHAPE_SHELF_FREQ,
                             DSPConstants::SHAPE_SHELF_Q, static_cast<double>(shelfGainDb(v, a)));
    }

    using Biquad = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                                  juce::dsp::IIR::Coefficients<float>>;
    Biquad mid, shelf;
    juce::SmoothedValue<float> smoothed;
    juce::SmoothedValue<float> smoothedAmount;
    double sampleRate = 44100.0;
    float lastWritten = 1.0e9f;
    float lastAmount = -1.0f;
    int flatHoldSamples = 0;
    int flatSamplesLeft = 0;  // > 0 while holding after reaching Flat
    bool stateClear = true;
};

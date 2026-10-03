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
    constexpr float  SHAPE_MID_BOOST_DB  = 9.0f;     // at full Bark
    constexpr float  SHAPE_MID_CUT_DB    = 12.0f;    // at full Scoop
    constexpr double SHAPE_SHELF_FREQ    = 2500.0;   // fizz / edge region of distorted bass
    constexpr double SHAPE_SHELF_Q       = 0.707;
    constexpr float  SHAPE_SHELF_DB      = 4.0f;     // cut at Bark, boost at Scoop
    constexpr float  SHAPE_TAPER         = 1.5f;     // gentle near noon
    constexpr float  SHAPE_PRE_AMOUNT    = 1.0f;     // pre-drive instance
    constexpr float  SHAPE_POST_AMOUNT   = 0.5f;     // post-drive instance
    constexpr float  SHAPE_LFO_RANGE     = 50.0f;    // knob units at full LFO depth
    constexpr float  SHAPE_FLAT_EPS      = 0.01f;    // |shape| below this counts as flat
    constexpr double SHAPE_SMOOTH_TIME_S = 0.02;
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
        amount = amountIn;
        mid.prepare(spec);
        shelf.prepare(spec);
        // Five-coefficient biquads (allocates: prepare runs off the audio thread).
        *mid.state   = juce::dsp::IIR::Coefficients<float>(1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
        *shelf.state = juce::dsp::IIR::Coefficients<float>(1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
        smoothed.reset(spec.sampleRate, DSPConstants::SHAPE_SMOOTH_TIME_S);
        smoothed.setCurrentAndTargetValue(juce::jlimit(DSPConstants::SHAPE_MIN,
                                                       DSPConstants::SHAPE_MAX, initialShape));
        reset();
    }

    void setTarget(float shape) noexcept
    {
        smoothed.setTargetValue(juce::jlimit(DSPConstants::SHAPE_MIN, DSPConstants::SHAPE_MAX, shape));
    }

    bool isActive() const noexcept
    {
        return std::abs(smoothed.getTargetValue()) > DSPConstants::SHAPE_FLAT_EPS
            || std::abs(smoothed.getCurrentValue()) > DSPConstants::SHAPE_FLAT_EPS;
    }

    void process(juce::dsp::AudioBlock<float>& block) noexcept
    {
        if (! isActive())
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
        const int total = static_cast<int>(block.getNumSamples());
        for (int start = 0; start < total; start += DSPConstants::SHAPE_SUBBLOCK)
        {
            const int len = juce::jmin(DSPConstants::SHAPE_SUBBLOCK, total - start);
            const float v = smoothed.skip(len);
            if (std::abs(v - lastWritten) > 1.0e-3f)
            {
                writeCoefficients(v);
                lastWritten = v;
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
        mid.reset();
        shelf.reset();
        stateClear = true;
        lastWritten = 1.0e9f;
    }

private:
    void writeCoefficients(float v) noexcept
    {
        writePeakFilterCoeffs(*mid.state, sampleRate, DSPConstants::SHAPE_MID_FREQ,
                              DSPConstants::SHAPE_MID_Q,
                              juce::Decibels::decibelsToGain(static_cast<double>(midGainDb(v, amount))));
        writeHighShelfCoeffs(*shelf.state, sampleRate, DSPConstants::SHAPE_SHELF_FREQ,
                             DSPConstants::SHAPE_SHELF_Q, static_cast<double>(shelfGainDb(v, amount)));
    }

    using Biquad = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                                  juce::dsp::IIR::Coefficients<float>>;
    Biquad mid, shelf;
    juce::SmoothedValue<float> smoothed;
    double sampleRate = 44100.0;
    float amount = 1.0f;
    float lastWritten = 1.0e9f;
    bool stateClear = true;
};

#include "SubGuardSplitter.h"
#include "FilterCoeffs.h"

SubGuardSplitter::SubGuardSplitter()
{
    // Valid until prepare/startTracking set the real rates (see PluginProcessor's constructor).
    smoothedFreq.reset(44100.0, DSPConstants::SUBGUARD_FREQ_SMOOTH_TIME_S);
    smoothedFreq.setCurrentAndTargetValue(DSPConstants::SUBGUARD_FREQ_DEFAULT);
}

void SubGuardSplitter::prepare(const juce::dsp::ProcessSpec& spec, float freqParam, int bandBufferSamples)
{
    // Use safe frequency for filter initialization when OFF (prevents divide-by-zero)
    const float filterInitFreq = (freqParam <= 1.0f) ? 60.0f : freqParam;

    // LR24 filters (4th order = 2 cascaded 2nd-order stages) - PRESERVE mode
    lr24Low1.prepare(spec);
    *lr24Low1.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(spec.sampleRate, filterInitFreq);
    lr24Low1.reset();

    lr24Low2.prepare(spec);
    *lr24Low2.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(spec.sampleRate, filterInitFreq);
    lr24Low2.reset();

    lr24High1.prepare(spec);
    *lr24High1.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(spec.sampleRate, filterInitFreq);
    lr24High1.reset();

    lr24High2.prepare(spec);
    *lr24High2.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(spec.sampleRate, filterInitFreq);
    lr24High2.reset();

    // LR12 filters (2nd order = single stage, Q=0.5 for true Linkwitz-Riley 2) - AGGRESSIVE mode
    constexpr float lr2Q = 0.5f;
    lr12Low.prepare(spec);
    *lr12Low.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(spec.sampleRate, filterInitFreq, lr2Q);
    lr12Low.reset();

    lr12High.prepare(spec);
    *lr12High.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(spec.sampleRate, filterInitFreq, lr2Q);
    lr12High.reset();

    // LR18 filters (1st + 2nd order = true 3rd-order Butterworth) - CONTROL mode.
    // The 2nd-order section MUST use Butterworth Q=1.0 (not 0.5): a 3rd-order
    // Butterworth LP+HP sums to allpass (flat) with the same polarity. Q=0.5 gives
    // three coincident real poles, whose LP+HP scoops ~6dB at the crossover.
    constexpr float lr18Q = 1.0f;
    lr18Low1.prepare(spec);
    *lr18Low1.state = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(spec.sampleRate, filterInitFreq);
    lr18Low1.reset();

    lr18Low2.prepare(spec);
    *lr18Low2.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(spec.sampleRate, filterInitFreq, lr18Q);
    lr18Low2.reset();

    lr18High1.prepare(spec);
    *lr18High1.state = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass(spec.sampleRate, filterInitFreq);
    lr18High1.reset();

    lr18High2.prepare(spec);
    *lr18High2.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(spec.sampleRate, filterInitFreq, lr18Q);
    lr18High2.reset();

    const int numChannels = static_cast<int>(spec.numChannels);
    lowBand.setSize(numChannels, bandBufferSamples, false, false, true);
    highBand.setSize(numChannels, bandBufferSamples, false, false, true);
    lowBandB.setSize(numChannels, bandBufferSamples, false, false, true);
    highBandB.setSize(numChannels, bandBufferSamples, false, false, true);
}

void SubGuardSplitter::startTracking(float freqParam, double blockRate)
{
    // Use the actual parameter value, not the filter's init frequency. This smoother is
    // consumed once PER BLOCK (see process), so it must be reset at the block rate, not
    // the (oversampled) sample rate. Resetting at the sample rate made the 50ms ramp take
    // ~8820 blocks (~100s) to settle, so the crossover barely tracked the knob.
    smoothedFreq.reset(blockRate, DSPConstants::SUBGUARD_FREQ_SMOOTH_TIME_S);
    smoothedFreq.setCurrentAndTargetValue(freqParam);
    lastFreq = freqParam;

    // Seed the order state so the first active block doesn't spawn a spurious crossfade.
    currentOrder = orderForFrequency(freqParam);
    crossfadeActive = false;
    crossfadePos = 0;
    wasActive = (freqParam > 1.0f);
}

void SubGuardSplitter::reset() noexcept
{
    // Every order, not just the active one: the next active block may pick any of them,
    // and a bank left holding its last signal pops on a preset or session load.
    resetOrderFilters(Order::LR24);
    resetOrderFilters(Order::LR18);
    resetOrderFilters(Order::LR12);

    // Abort any in-flight order crossfade; the next active block re-seeds the order
    // from the current frequency (OFF->ON snap path).
    crossfadeActive = false;
    crossfadePos = 0;
    wasActive = false;

    // Force a coefficient update on the next active block.
    lastFreq = -1.0f;
}

void SubGuardSplitter::setInactive() noexcept
{
    wasActive = false;        // next ON snaps to the target order (no spurious crossfade)
    crossfadeActive = false;
}

void SubGuardSplitter::setTargetFrequency(float hz) noexcept
{
    smoothedFreq.setTargetValue(hz);
}

bool SubGuardSplitter::process(const juce::dsp::AudioBlock<float>& input,
                               size_t numChannels, size_t numSamples, double sampleRate) noexcept
{
    const float currentFreq = smoothedFreq.getNextValue();

    if (std::abs(currentFreq - lastFreq) > 0.5f)
    {
        updateCoefficients(currentFreq, sampleRate);
        lastFreq = currentFreq;
    }

    const Order targetOrder = orderForFrequency(currentFreq);

    // OFF -> ON: snap to the target order; do not crossfade from a stale bank.
    activatedThisBlock = !wasActive;
    if (!wasActive)
    {
        currentOrder = targetOrder;
        crossfadeActive = false;
    }
    wasActive = true;

    // Start (or re-aim) an order crossfade when the slope changes. The outgoing bank
    // is already settled; the incoming bank is reset so it fades in from a clean state
    // while its blend weight ramps 0->1, which hides both the stale-state pop and the
    // instantaneous magnitude/phase jump of a hard switch.
    const int crossfadeLen = juce::jmax(1,
        static_cast<int>(DSPConstants::SUBGUARD_CROSSFADE_TIME_S * sampleRate));

    if (!crossfadeActive)
    {
        if (targetOrder != currentOrder)
        {
            fromOrder = currentOrder;
            toOrder = targetOrder;
            crossfadePos = 0;
            crossfadeActive = true;
            resetOrderFilters(toOrder);
        }
    }
    else if (targetOrder != toOrder)
    {
        fromOrder = currentOrder;
        toOrder = targetOrder;
        crossfadePos = 0;
        resetOrderFilters(toOrder);
    }

    // SAFETY CHECK: Ensure we have enough buffer space
    const int requiredBufferSize = static_cast<int>(numSamples);
    if (requiredBufferSize > lowBand.getNumSamples() || requiredBufferSize > highBand.getNumSamples()
        || (crossfadeActive && (requiredBufferSize > lowBandB.getNumSamples()
                                || requiredBufferSize > highBandB.getNumSamples())))
        return false;

    // Copy input into the primary band buffers
    for (size_t channel = 0; channel < numChannels; ++channel)
    {
        const int channelIdx = static_cast<int>(channel);
        const int sampleCount = static_cast<int>(numSamples);

        lowBand.copyFrom(channelIdx, 0, input.getChannelPointer(channel), sampleCount);
        highBand.copyFrom(channelIdx, 0, input.getChannelPointer(channel), sampleCount);
    }

    if (!crossfadeActive)
    {
        // Single bank: filter the primary buffers in place.
        filterBands(currentOrder, lowBand, highBand, numChannels, numSamples);
        return true;
    }

    // Dual bank: copy input into the secondary buffers, filter each order, then
    // linearly blend the low/high splits per sample from the outgoing to the
    // incoming order across the crossfade window. (Linear, not equal-power: the
    // two orders' bands are filtered versions of the same input, i.e. correlated.)
    for (size_t channel = 0; channel < numChannels; ++channel)
    {
        const int channelIdx = static_cast<int>(channel);
        const int sampleCount = static_cast<int>(numSamples);

        lowBandB.copyFrom(channelIdx, 0, input.getChannelPointer(channel), sampleCount);
        highBandB.copyFrom(channelIdx, 0, input.getChannelPointer(channel), sampleCount);
    }

    filterBands(fromOrder, lowBand,  highBand,  numChannels, numSamples);   // outgoing
    filterBands(toOrder,   lowBandB, highBandB, numChannels, numSamples);   // incoming

    int xfPos = crossfadePos;
    for (size_t sample = 0; sample < numSamples; ++sample)
    {
        const float t = juce::jmin(1.0f, static_cast<float>(xfPos) / static_cast<float>(crossfadeLen));
        const float wFrom = 1.0f - t;
        const float wTo = t;

        for (size_t channel = 0; channel < numChannels; ++channel)
        {
            const int ch = static_cast<int>(channel);
            auto* lowA  = lowBand.getWritePointer(ch);
            auto* highA = highBand.getWritePointer(ch);
            const auto* lowB  = lowBandB.getReadPointer(ch);
            const auto* highB = highBandB.getReadPointer(ch);

            lowA[sample]  = lowA[sample]  * wFrom + lowB[sample]  * wTo;
            highA[sample] = highA[sample] * wFrom + highB[sample] * wTo;
        }

        if (xfPos < crossfadeLen)
            ++xfPos;
    }
    crossfadePos = xfPos;

    if (crossfadePos >= crossfadeLen)
    {
        crossfadeActive = false;
        currentOrder = toOrder;
    }
    return true;
}

SubGuardSplitter::Order SubGuardSplitter::orderForFrequency(float freq) noexcept
{
    // Determine filter order based on frequency zones with hysteresis
    // PRESERVE zone (60 Hz): LR24 - steepest slope for maximum sub protection
    if (freq <= 80.0f)
        return Order::LR24;

    // CONTROL zone (100 Hz): LR18 - balanced response
    if (freq >= 92.0f && freq <= 125.0f)
        return Order::LR18;

    // AGGRESSIVE zone (150 Hz): LR12 - gentle slope, more frequency overlap
    if (freq >= 142.0f)
        return Order::LR12;

    // Transition zones - use nearest neighbor
    if (freq < 92.0f)
        return Order::LR24;  // Between PRESERVE and CONTROL
    else
        return Order::LR18;  // Between CONTROL and AGGRESSIVE
}

void SubGuardSplitter::updateCoefficients(float freq, double sampleRate) noexcept
{
    // RT-safe in-place coefficient update. Every per-channel IIR::Filter was
    // bound to the Coefficients object pointed to by .state at prepare() time,
    // so writing through .state here updates what each channel's process()
    // reads on the next block. No allocation; no standby swap.
    constexpr double butterworthQ = 0.7071067811865476;

    // LR24 filters (2 cascaded 2nd-order stages)
    if (lr24Low1.state != nullptr)
        writeSecondOrderLowPassCoeffs(*lr24Low1.state, sampleRate, freq, butterworthQ);
    if (lr24Low2.state != nullptr)
        writeSecondOrderLowPassCoeffs(*lr24Low2.state, sampleRate, freq, butterworthQ);
    if (lr24High1.state != nullptr)
        writeSecondOrderHighPassCoeffs(*lr24High1.state, sampleRate, freq, butterworthQ);
    if (lr24High2.state != nullptr)
        writeSecondOrderHighPassCoeffs(*lr24High2.state, sampleRate, freq, butterworthQ);

    // LR12 filters (single 2nd-order stage with Q=0.5 for true Linkwitz-Riley 2)
    // Default Butterworth Q=0.707 causes +3dB boost at crossover; LR2 Q=0.5 sums flat
    constexpr float lr2Q = 0.5f;
    if (lr12Low.state != nullptr)
        writeSecondOrderLowPassCoeffs(*lr12Low.state, sampleRate, freq, lr2Q);
    if (lr12High.state != nullptr)
        writeSecondOrderHighPassCoeffs(*lr12High.state, sampleRate, freq, lr2Q);

    // LR18 filters (1st + 2nd order cascaded = true 3rd-order Butterworth).
    // The 2nd-order section uses Butterworth Q=1.0 so LP+HP sums flat (allpass);
    // Q=0.5 (coincident poles) would scoop ~6dB at the crossover.
    constexpr float lr18Q = 1.0f;
    if (lr18Low1.state != nullptr)
        writeFirstOrderLowPassCoeffs(*lr18Low1.state, sampleRate, freq);
    if (lr18Low2.state != nullptr)
        writeSecondOrderLowPassCoeffs(*lr18Low2.state, sampleRate, freq, lr18Q);
    if (lr18High1.state != nullptr)
        writeFirstOrderHighPassCoeffs(*lr18High1.state, sampleRate, freq);
    if (lr18High2.state != nullptr)
        writeSecondOrderHighPassCoeffs(*lr18High2.state, sampleRate, freq, lr18Q);
}

void SubGuardSplitter::filterBands(Order order,
                                   juce::AudioBuffer<float>& lowBuf,
                                   juce::AudioBuffer<float>& highBuf,
                                   size_t numChannels, size_t numSamples) noexcept
{
    auto lowBlock  = juce::dsp::AudioBlock<float>(lowBuf ).getSubBlock(0, numSamples);
    auto highBlock = juce::dsp::AudioBlock<float>(highBuf).getSubBlock(0, numSamples);

    switch (order)
    {
        case Order::LR12:
            lr12Low.process(juce::dsp::ProcessContextReplacing<float>(lowBlock));
            lr12High.process(juce::dsp::ProcessContextReplacing<float>(highBlock));
            break;
        case Order::LR18:
            lr18Low1.process(juce::dsp::ProcessContextReplacing<float>(lowBlock));
            lr18Low2.process(juce::dsp::ProcessContextReplacing<float>(lowBlock));
            lr18High1.process(juce::dsp::ProcessContextReplacing<float>(highBlock));
            lr18High2.process(juce::dsp::ProcessContextReplacing<float>(highBlock));
            break;
        case Order::LR24:
        default:
            lr24Low1.process(juce::dsp::ProcessContextReplacing<float>(lowBlock));
            lr24Low2.process(juce::dsp::ProcessContextReplacing<float>(lowBlock));
            lr24High1.process(juce::dsp::ProcessContextReplacing<float>(highBlock));
            lr24High2.process(juce::dsp::ProcessContextReplacing<float>(highBlock));
            break;
    }

    // Bake the 2nd-order Linkwitz-Riley flat-sum polarity into the high band: LR12's LP
    // and HP are antiphase at the crossover, so inverting the high band makes the later
    // "low + high" recombine a flat allpass sum. Doing it pre-distortion keeps a single
    // distortion pass and lets the order crossfade blend the high bands cleanly (no
    // through-zero dip). LR18/LR24 sum flat with the same polarity, so they are untouched.
    if (order == Order::LR12)
    {
        for (size_t ch = 0; ch < numChannels; ++ch)
        {
            auto* h = highBuf.getWritePointer(static_cast<int>(ch));
            for (size_t s = 0; s < numSamples; ++s)
                h[s] = -h[s];
        }
    }
}

void SubGuardSplitter::resetOrderFilters(Order order) noexcept
{
    switch (order)
    {
        case Order::LR12:
            lr12Low.reset();
            lr12High.reset();
            break;
        case Order::LR18:
            lr18Low1.reset();
            lr18Low2.reset();
            lr18High1.reset();
            lr18High2.reset();
            break;
        case Order::LR24:
        default:
            lr24Low1.reset();
            lr24Low2.reset();
            lr24High1.reset();
            lr24High2.reset();
            break;
    }
}

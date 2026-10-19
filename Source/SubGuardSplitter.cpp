#include "SubGuardSplitter.h"

#include <cmath>

namespace
{
    // The two section Qs of a 4th-order Butterworth.
    constexpr double kButterworth4Q[2] = { 0.54119610014619698, 1.3065629648763766 };
}

SubGuardSplitter::SubGuardSplitter()
{
    // Valid until prepare/startTracking set the real rates (see PluginProcessor's constructor).
    smoothedFreq.reset(44100.0, DSPConstants::SUBGUARD_FREQ_SMOOTH_TIME_S);
    smoothedFreq.setCurrentAndTargetValue(DSPConstants::SUBGUARD_FREQ_DEFAULT);
}

void SubGuardSplitter::prepare(const juce::dsp::ProcessSpec& spec, float freqParam, int bandBufferSamples)
{
    sampleRate = spec.sampleRate;

    // Use safe frequency for filter initialization when OFF (prevents divide-by-zero)
    const float filterInitFreq = (freqParam <= 1.0f) ? DSPConstants::SUBGUARD_FREQ_DEFAULT : freqParam;
    updateCoefficients(filterInitFreq);

    const auto numChannels = static_cast<size_t>(spec.numChannels);
    for (auto* states : { &lowStateA, &lowStateB, &highPreState, &highPostState })
        states->assign(numChannels, Half{});

    lowBand.setSize(static_cast<int>(numChannels), bandBufferSamples, false, false, true);
    highBand.setSize(static_cast<int>(numChannels), bandBufferSamples, false, false, true);
}

void SubGuardSplitter::startTracking(float freqParam, double blockRate)
{
    // Use the actual parameter value, not the filter's init frequency. This smoother is
    // consumed once PER BLOCK (see process), so it must be reset at the block rate, not
    // the (oversampled) sample rate. Resetting at the sample rate made the 50ms ramp take
    // ~8820 blocks (~100s) to settle, so the crossover barely tracked the knob.
    smoothedFreq.reset(blockRate, DSPConstants::SUBGUARD_FREQ_SMOOTH_TIME_S);
    smoothedFreq.setCurrentAndTargetValue(freqParam);
    wasActive = (freqParam > 1.0f);
}

void SubGuardSplitter::reset() noexcept
{
    for (auto* states : { &lowStateA, &lowStateB, &highPreState, &highPostState })
        for (auto& half : *states)
            half = Half{};

    wasActive = false;

    // Force a coefficient update on the next active block.
    lastFreq = -1.0f;
}

void SubGuardSplitter::setInactive() noexcept
{
    wasActive = false;
}

void SubGuardSplitter::setTargetFrequency(float hz) noexcept
{
    smoothedFreq.setTargetValue(hz);
}

bool SubGuardSplitter::process(const juce::dsp::AudioBlock<float>& input,
                               size_t numChannels, size_t numSamples) noexcept
{
    const float currentFreq = smoothedFreq.getNextValue();

    if (std::abs(currentFreq - lastFreq) > 0.5f)
        updateCoefficients(currentFreq);

    activatedThisBlock = !wasActive;
    wasActive = true;

    // SAFETY CHECK: Ensure we have enough buffer space
    const int requiredBufferSize = static_cast<int>(numSamples);
    if (requiredBufferSize > lowBand.getNumSamples() || requiredBufferSize > highBand.getNumSamples()
        || numChannels > lowStateA.size() || numChannels > static_cast<size_t>(lowBand.getNumChannels()))
        return false;

    for (size_t channel = 0; channel < numChannels; ++channel)
    {
        const int ch = static_cast<int>(channel);
        lowBand.copyFrom(ch, 0, input.getChannelPointer(channel), requiredBufferSize);
        highBand.copyFrom(ch, 0, input.getChannelPointer(channel), requiredBufferSize);

        auto* low = lowBand.getWritePointer(ch);
        runHalf(lowCoeffs, lowStateA[channel], low, numSamples);
        runHalf(lowCoeffs, lowStateB[channel], low, numSamples);
        runHalf(highCoeffs, highPreState[channel], highBand.getWritePointer(ch), numSamples);
    }
    return true;
}

void SubGuardSplitter::processPostDrive(juce::dsp::AudioBlock<float> block) noexcept
{
    const size_t numChannels = juce::jmin(block.getNumChannels(), highPostState.size());
    for (size_t channel = 0; channel < numChannels; ++channel)
        runHalf(highCoeffs, highPostState[channel], block.getChannelPointer(channel), block.getNumSamples());
}

void SubGuardSplitter::writeSection(Coeffs& dest, double rate, double cutoffHz, double q, bool highPass) noexcept
{
    // Bilinear transform with the cutoff prewarped, as JUCE's makeLowPass / makeHighPass
    // and FilterCoeffs.h's writers do, in double throughout.
    const double k = std::tan(juce::MathConstants<double>::pi * cutoffHz / rate);
    const double kSquared = k * k;
    const double norm = 1.0 / (1.0 + k / q + kSquared);

    dest.b0 = (highPass ? 1.0 : kSquared) * norm;
    dest.b1 = (highPass ? -2.0 : 2.0 * kSquared) * norm;
    dest.b2 = dest.b0;
    dest.a1 = 2.0 * (kSquared - 1.0) * norm;
    dest.a2 = (1.0 - k / q + kSquared) * norm;
}

void SubGuardSplitter::updateCoefficients(float freq) noexcept
{
    // Never at or above Nyquist, and never at 0 (the off value).
    const double fc = juce::jlimit(1.0, 0.45 * sampleRate, static_cast<double>(freq));
    for (int s = 0; s < kSectionsPerHalf; ++s)
    {
        writeSection(lowCoeffs[static_cast<size_t>(s)],  sampleRate, fc, kButterworth4Q[s], false);
        writeSection(highCoeffs[static_cast<size_t>(s)], sampleRate, fc, kButterworth4Q[s], true);
    }
    lastFreq = freq;
}

void SubGuardSplitter::runHalf(const std::array<Coeffs, kSectionsPerHalf>& c, Half& state,
                               float* data, size_t numSamples) noexcept
{
    for (size_t s = 0; s < kSectionsPerHalf; ++s)
    {
        const auto& k = c[s];
        double s1 = state[s].s1, s2 = state[s].s2;
        for (size_t i = 0; i < numSamples; ++i)
        {
            const double x = data[i];
            const double y = k.b0 * x + s1;
            s1 = k.b1 * x - k.a1 * y + s2;
            s2 = k.b2 * x - k.a2 * y;
            data[i] = static_cast<float>(y);
        }
        state[s] = { s1, s2 };
    }
}

#pragma once

#include <JuceHeader.h>
#include <cmath>

// In-place RBJ biquad coefficient writers (a0-normalised, 5 raw coefficients) so
// nothing allocates on the audio thread. Shared by PluginProcessor and ShapeFilter.

// RBJ peaking-EQ biquad, written in place (a0-normalized to 5 raw coeffs) so we
// never allocate on the audio thread. Mirrors juce::dsp::IIR::Coefficients::
// makePeakFilter(sampleRate, frequency, Q, gainFactor) exactly, so the live
// coefficient updates match a prepare-time makePeakFilter bit-for-bit.
// gainFactor is linear (juce::Decibels::decibelsToGain(dB)).
inline void writePeakFilterCoeffs(juce::dsp::IIR::Coefficients<float>& dest,
                           const double sampleRate,
                           const double frequency,
                           const double q,
                           const double gainFactor) noexcept
{
    const auto A = std::sqrt(gainFactor);
    const auto omega = (2.0 * juce::MathConstants<double>::pi * frequency) / sampleRate;
    const auto alpha = std::sin(omega) / (2.0 * q);
    const auto c2 = -2.0 * std::cos(omega);
    const auto alphaTimesA = alpha * A;
    const auto alphaOverA = alpha / A;
    const auto a0inv = 1.0 / (1.0 + alphaOverA);
    auto* coeffs = dest.getRawCoefficients();

    coeffs[0] = static_cast<float>((1.0 + alphaTimesA) * a0inv);  // b0
    coeffs[1] = static_cast<float>(c2 * a0inv);                   // b1
    coeffs[2] = static_cast<float>((1.0 - alphaTimesA) * a0inv);  // b2
    coeffs[3] = static_cast<float>(c2 * a0inv);                   // a1
    coeffs[4] = static_cast<float>((1.0 - alphaOverA) * a0inv);   // a2
}

// RBJ high-shelf, written in place (a0-normalized) so we never allocate on the
// audio thread. gainDb > 0 boosts highs above cutoffHz; < 0 cuts them.
inline void writeHighShelfCoeffs(juce::dsp::IIR::Coefficients<float>& dest,
                          const double sampleRate,
                          const double cutoffHz,
                          const double q,
                          const double gainDb) noexcept
{
    const auto A = std::pow(10.0, gainDb / 40.0);
    const auto w0 = juce::MathConstants<double>::twoPi * cutoffHz / sampleRate;
    const auto cosw0 = std::cos(w0);
    const auto alpha = std::sin(w0) / (2.0 * q);
    const auto twoSqrtAalpha = 2.0 * std::sqrt(A) * alpha;
    const auto Aplus = A + 1.0;
    const auto Aminus = A - 1.0;

    const auto b0 =      A * (Aplus + Aminus * cosw0 + twoSqrtAalpha);
    const auto b1 = -2.0 * A * (Aminus + Aplus * cosw0);
    const auto b2 =      A * (Aplus + Aminus * cosw0 - twoSqrtAalpha);
    const auto a0 =          (Aplus - Aminus * cosw0 + twoSqrtAalpha);
    const auto a1 =  2.0 *   (Aminus - Aplus * cosw0);
    const auto a2 =          (Aplus - Aminus * cosw0 - twoSqrtAalpha);

    const auto invA0 = 1.0 / a0;
    auto* coeffs = dest.getRawCoefficients();
    coeffs[0] = static_cast<float>(b0 * invA0);
    coeffs[1] = static_cast<float>(b1 * invA0);
    coeffs[2] = static_cast<float>(b2 * invA0);
    coeffs[3] = static_cast<float>(a1 * invA0);
    coeffs[4] = static_cast<float>(a2 * invA0);
}

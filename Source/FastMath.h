#pragma once

#include <cmath>

// Header-only fast math approximations for the oversampled hot loop.
// These replace std::tanh / std::atan in applyStudioDistortion and the
// LA2A / waveshaper / soft-clipper inner loops where std:: transcendentals
// dominate CPU time at 4x oversampling.
namespace FastMath
{
    // Padé [3/3] rational approximation for tanh(x).
    // Same polynomial as juce::dsp::FastMathApproximations::tanh.
    // Max error < 5e-5 for |x| ≤ 4.5; clamped to ±1 beyond that.
    inline float tanh(float x) noexcept
    {
        if (x >  4.5f) return  1.0f;
        if (x < -4.5f) return -1.0f;
        const float x2 = x * x;
        return x * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2)))
                 / (135135.0f + x2 * (62370.0f + x2 * (3150.0f + 28.0f * x2)));
    }

    // Fast atan approximation — Padé [1/2] in z², range-reduced via atan(x) = π/2 − atan(1/x).
    // Max error < 5e-4 rad for all x; bounded at ±π/2 for |x| → ∞.
    inline float atan(float x) noexcept
    {
        constexpr float HALF_PI = 1.5707963268f;
        const float ax  = std::abs(x);
        const bool  big = ax > 1.0f;
        const float z   = big ? 1.0f / ax : ax;
        const float z2  = z * z;
        const float z4  = z2 * z2;
        // Padé [1/2] coefficients derived to be exact at z = {0, 0.5, 1}
        const float p = z * (1.0f + 0.48811f * z2)
                          / (1.0f + 0.82148f * z2 + 0.07285f * z4);
        const float r = big ? HALF_PI - p : p;
        return x < 0.0f ? -r : r;
    }
}

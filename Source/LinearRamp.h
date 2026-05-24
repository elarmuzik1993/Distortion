#pragma once

#include <cstddef>

/** Block-paced linear interpolator: a single ramp from the previous block's
    final value to this block's target, advanced one sample at a time.

    Replaces the `lastFoo` / `fooDelta` / `currentFoo` triads used in the
    oversampled-domain hot loops for zipper-free parameter smoothing.

    Lifecycle:
      - `reset(v)` at construction / `prepareToPlay` / state restore (seed).
      - `rampTo(target, N)` at the top of each block (sets delta).
      - `advance()` once per sample in the inner loop (returns current, then steps).
      - After the block, `peek()` is the final value — no manual writeback needed.

    RT-safe: no allocation, no syscalls, all `noexcept`. */
template <typename Float = float>
struct LinearRamp
{
    Float current = Float{};
    Float delta   = Float{};

    /** Seed the ramp directly. Use at init, state restore, or to force a jump. */
    void reset(Float value) noexcept
    {
        current = value;
        delta   = Float{};
    }

    /** Configure this block's ramp: linearly interpolate from the current value
        to `target` across `numSamples` steps. Safe for `numSamples == 0`. */
    void rampTo(Float target, std::size_t numSamples) noexcept
    {
        delta = (numSamples > 0)
            ? (target - current) / static_cast<Float>(numSamples)
            : Float{};
    }

    /** Return the current value, then step toward target. Call once per sample. */
    Float advance() noexcept
    {
        const Float v = current;
        current += delta;
        return v;
    }

    /** Read the current value without stepping. */
    Float peek() const noexcept { return current; }
};

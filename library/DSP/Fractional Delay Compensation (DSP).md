---
date: 2026-04-23
status: knowledge
tags: [dsp, audio, latency, phase, oversampling, juce, notes]
---

# Fractional Delay Compensation (DSP)

## Definition
**Fractional delay compensation** is delaying a signal by a **non-integer number of samples** (e.g. \(12.37\) samples) so two signal paths line up in time/phase before mixing.

Conceptually:

\[
y[n] = x[n - N - \alpha], \quad N \in \mathbb{Z},\; 0 \le \alpha < 1
\]

- \(N\): integer sample delay
- \(\alpha\): fractional-sample delay

## Why it matters (typical plugin cases)
- **Parallel dry/wet**: if wet processing adds latency (filters, oversampling, lookahead, etc.), mixing against an un-delayed dry path causes **comb filtering** and phase smear.
- **Multiband / split–process–sum**: crossovers and different per-band processing can introduce mismatched group delay between bands.
- **Oversampling chains**: up/downsampling filters add group delay; some of it may not align neatly to “whole samples” at the base rate.

## Host latency compensation vs fractional alignment
- Hosts compensate plugin latency in **integer samples** (e.g. via JUCE `setLatencySamples()`).
- **Fractional** alignment is something the plugin must do **internally** when it mixes parallel paths (dry/wet or bands) and needs tighter phase alignment than integer-only delay provides.

## How it’s implemented
To approximate a “partial sample”, you use interpolation / special filters:
- **FIR/Lagrange fractional delay**: good quality, higher CPU; common for short fractional delays.
- **Thiran all-pass fractional delay**: good phase-delay behavior with flat magnitude (IIR-style).
- **Linear interpolation**: cheapest, lowest quality (audible/visible at high frequencies).

In practice, this often looks like:
- Keep a **small delay line** for the dry path (or whichever path is “early”).
- Apply integer delay + a fractional stage.
- Then mix with the late path.

## In this codebase (anchor)
The Architecture Contract documents:
- “Global mix with **fractional dry-delay alignment** via `dryDelayState`.”

This is specifically to keep **global mix (dry/wet)** phase-coherent when the wet path has latency (e.g., oversampling) that shouldn’t be left uncorrected when recombining.

## Related threading-model reminder (so you can reuse it)
- **Audio thread (`processBlock`)**: must stay real-time safe (no blocking, no allocation).
- **Message thread**: safe place for heavy rebuilds (e.g., oversampling reconfiguration) triggered via `AsyncUpdater`.
- **Comms**: atomics for params/meters; `juce::AbstractFifo` for scope samples.
- **Caution**: holding `getCallbackLock()` during a rebuild is safe but can still cause **dropouts** if the lock is held long—keep the locked section minimal or swap in prebuilt objects.


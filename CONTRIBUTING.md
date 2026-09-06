# Contributing to Sledge Distortion

Thanks for looking. Sledge is a JUCE audio plugin built by one person, and it is
open source because a distortion plugin gets better when the people using it can
open the hood.

Contributions of every size are welcome — a typo in this file counts. If you are
not sure whether an idea fits, open an issue and ask before writing code; it is
much cheaper than finding out in review.

## Ways to help

- **Report a bug.** DAW, OS, plugin format, sample rate and buffer size, plus what
  you did and what you heard. Audio bugs are often sample-rate- or
  buffer-size-specific, so those two fields matter more than they look.
- **Test in a DAW nobody has tried.** `docs/DAW_QA_matrix.md` tracks host
  coverage. Filling in a row is a real contribution.
- **Fix something.** Starter-sized work is labelled `good first issue`. If that
  list is empty, open an issue saying what you'd like to take on and it can be
  scoped with you.
- **Improve the DSP.** This is the deep end — read
  `docs/Architecture Contract.md` first; the signal chain has a defined order and
  changing it has audible consequences.

## Getting set up

You need CMake 3.22+ and a C++17 compiler. **JUCE 7.0.12 is fetched
automatically** by CMake at a pinned tag — you do not install it yourself.

Platform extras: ALSA development libraries on Linux
(`libasound2-dev`, plus `libcurl4-openssl-dev` for `juce_core`), Xcode
command-line tools on macOS.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target Distortion_VST3
```

The built plugin lands in `build/Distortion_artefacts/Release/VST3/`.

## Before you open a pull request

**Run the tests.** 3100+ assertions, a JUCE `UnitTest` runner covering DSP,
compression, LFO, `processBlock`, thread safety, sample rates from 44.1k to 192k,
the graphic EQ, state I/O, factory presets and settings persistence.

```bash
cmake --build build --target DistortionTests
./build/DistortionTests_artefacts/Debug/DistortionTests
```

**Add a test for what you changed.** A bugfix without a failing-then-passing test
is hard to keep fixed.

**Validate in a host harness.** CI gates on `pluginval --strictness-level 10`, so
a plugin that fails it will not merge.

**If you touched the UI**, verify with the snapshot tool rather than by
screenshotting the standalone app — the standalone's menu bar and "audio input is
muted" banner shift and clip the editor, which makes a correct layout look broken:

```bash
cmake --build build --target DistortionUiSnapshot
./DistortionUiSnapshot --out shots              # all four window scales
./DistortionUiSnapshot --collapsed --out shots  # scope folded away
```

**If you touched anything memory-related**, run the soak harness — *on Linux*.
Its leak half reads `/proc/self/statm`, so on other platforms it reports
`SOAK INCOMPLETE` rather than passing.

## Rules that will bite you

These are not style preferences. Each one is load-bearing, and the full set lives
in [`AGENTS.md`](AGENTS.md).

- **No dynamic allocation in `processBlock`.** Ever. Allocate in
  `prepareToPlay`.
- **No locks in the oscilloscope path.** It is a lock-free `juce::AbstractFifo`
  SPSC queue. A `SpinLock` used to live there and was deliberately removed to keep
  the audio thread contention-free — please do not reintroduce one.
- **Consume `SmoothedValue` in a sample-first loop**, or you will exhaust the
  ramp partway through a buffer.
- **DSP constants belong in the `DSPConstants` namespace** in
  `PluginProcessor.h`, not scattered as literals.
- **Adding a dependency means adding its notice.** Append it to
  `THIRD-PARTY-NOTICES.md` and confirm the licence is GPL v3-compatible. This is
  a distribution obligation, not bookkeeping.
- **Do not weaken the licence assertions** in `.github/workflows/build.yml` to
  make a build pass. Each packaging step fails the build if `LICENSE` or
  `THIRD-PARTY-NOTICES.md` is missing from the artifact; if one fires, the
  packaging is wrong, not the check.

## What CI runs

Every pull request builds and tests on **Windows and Linux**, and runs pluginval
at strictness 10 on both. The **macOS** job is gated to version tags and manual
dispatch, so a green PR has not been checked on macOS — if your change touches
macOS-specific code or packaging, say so in the PR and it can be dispatched.

## Licence

Sledge Distortion is **GPL v3**, and contributions are accepted under the same
licence. This is not a free choice: JUCE 7 is offered under either paid terms or
the GPL v3, and this project takes the GPL route, so a permissive licence is not
available while the build links the non-ISC JUCE modules.

**You keep the copyright on what you write.** There is a short
[Contributor Licence Agreement](CLA.md) — one page, no signature, no bot. It is a
*licence*, not an assignment: you keep your rights and can reuse your own work
anywhere. What it grants is permission broad enough to relicense the project if
that is ever forced — JUCE is paid-or-GPL, and one unreachable contributor could
otherwise freeze the licence permanently.

Opening a pull request means you agree to it. If you would rather not, say so in
the PR — for a typo or a docs fix I would rather have the fix than the paperwork.

## Where to read next

- [`AGENTS.md`](AGENTS.md) — the project's source of truth: architecture,
  engineering standards, build and release process.
- [`docs/Architecture Contract.md`](docs/Architecture%20Contract.md) — the signal
  chain, in order, and what may not move.
- [`CHANGELOG.md`](CHANGELOG.md) — what changed and when.

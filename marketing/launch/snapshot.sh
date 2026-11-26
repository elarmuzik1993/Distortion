#!/usr/bin/env bash
# Renders the plugin image for the launch post, marketing/launch/assets/ui.png:
# the state of the README screenshot (docs/images/interface-eq-lfo-compression.png:
# its EQ drawing, knob positions and a NAM profile playing) with EXTREME on,
# painted at 3x pixel density (2880x1692) without the JUCE badge or version label.
#
# It builds DistortionUiSnapshot in BUILD_DIR (default build, configured by
# scripts/verify.sh or by hand) first, so the tool is never stale; NAM Core's
# example models, fetched by that configure, supply the profile. Afterwards run
# node marketing/launch/render.js to rebuild the post images.
set -euo pipefail
cd "$(dirname "$0")/../.."

build=${BUILD_DIR:-build}
cmake --build "$build" --target DistortionUiSnapshot -j > /dev/null
# Debug or Release, whichever the build folder last produced.
tool=$(ls -t "$build"/DistortionUiSnapshot_artefacts/*/DistortionUiSnapshot | head -n 1)
# A full-size example capture: it bends the scope trace the way a driven bass
# does, where the small test models stay a clean sine.
model=$build/_deps/nam_core-src/example_models/A2.nam

# The 31 EQ dots, read off the README screenshot (dB). Dot 0 at -6.2 dB is its
# "HP 41" high-pass; dot 30 at 0 leaves the low-pass off.
eq=(-6.2 -1.6 0.5 2.0 2.4 2.7 2.0 1.0 -0.2 -1.6 -3.5 -4.9 -6.0 -7.4 -8.5 -9.3
    -9.7 -9.6 -9.7 -9.6 -9.6 -9.3 -8.8 -8.0 -6.8 -4.9 -3.0 -0.9 0.9 1.8 0)
eq_args=()
for i in "${!eq[@]}"; do eq_args+=(--param "eqThird$i=${eq[$i]}"); done

# Knobs measured from the same screenshot's pointer angles (the knobs turn 300
# degrees and draw their whole range linearly); the rest, Sub Guard's 60 Hz
# included, are defaults.
knob_args=(--param distMix=54 --param distortionAmount=75 --param tone=12100)

out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT
run=()
command -v xvfb-run > /dev/null && run=(xvfb-run -a)
"${run[@]}" "$tool" --scale 100 --eq --extreme --signal --signal-hz 220 --signal-amp 0.3 \
    --profile "$model" --pixel-scale 3 --marketing "${knob_args[@]}" "${eq_args[@]}" --out "$out"
cp "$out"/scale-100-*.png marketing/launch/assets/ui.png
echo "marketing/launch/assets/ui.png"

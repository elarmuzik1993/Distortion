#!/usr/bin/env bash
# =============================================================================
# build_pkg.sh — build a macOS distribution .pkg for Sledge Distortion.
#
# Assembles the built VST3 + AU + Standalone artefacts into a single
# double-clickable installer that drops each format into its system location:
#   VST3       -> /Library/Audio/Plug-Ins/VST3
#   AU         -> /Library/Audio/Plug-Ins/Components
#   Standalone -> /Applications
#
# Component .pkgs are built with pkgbuild and stitched together with
# productbuild + installer/macos/distribution.xml.
#
# The installer .pkg is signed with a "Developer ID Installer" identity ONLY
# when APPLE_INSTALLER_IDENTITY is set (mirrors the inert-until-secrets Windows
# signing). Without it an unsigned .pkg is produced — Gatekeeper will warn, but
# the build still succeeds so CI stays green before an Apple account exists.
#
# Usage:
#   installer/macos/build_pkg.sh <version> <vst3> <au> <app> [out_dir]
# Env:
#   APPLE_INSTALLER_IDENTITY  "Developer ID Installer: … (TEAMID)"  (optional)
# =============================================================================
set -euo pipefail

VERSION="${1:?usage: build_pkg.sh <version> <vst3> <au> <app> [out_dir]}"
VST3_PATH="${2:?missing VST3 bundle path}"
AU_PATH="${3:?missing AU (.component) bundle path}"
APP_PATH="${4:?missing Standalone (.app) bundle path}"
OUT_DIR="${5:-dist}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PKG_ID_BASE="com.monolitbeatz.MonolitDistortion"

for p in "$VST3_PATH" "$AU_PATH" "$APP_PATH"; do
    if [[ ! -e "$p" ]]; then
        echo "error: artefact not found: $p" >&2
        exit 1
    fi
done

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
mkdir -p "$OUT_DIR" "$WORK/pkgs"

# --- Stage each format under its own payload root -------------------------
stage_component() {
    # $1 bundle  $2 staging-subdir  $3 identifier-suffix  $4 install-location
    local bundle="$1" subdir="$2" idsuffix="$3" location="$4"
    local root="$WORK/root-$subdir"
    mkdir -p "$root"
    cp -R "$bundle" "$root/"
    pkgbuild \
        --root "$root" \
        --identifier "$PKG_ID_BASE.$idsuffix" \
        --version "$VERSION" \
        --install-location "$location" \
        "$WORK/pkgs/$subdir.pkg"
}

stage_component "$VST3_PATH" vst3 vst3 "/Library/Audio/Plug-Ins/VST3"
stage_component "$AU_PATH"   au   au   "/Library/Audio/Plug-Ins/Components"
stage_component "$APP_PATH"  app  app  "/Applications"

# --- Combine into a distribution product ----------------------------------
# Stage a resources dir so distribution.xml's <license file="LICENSE"/> resolves
# (the repo LICENSE lives at the root, not next to this script).
RES="$WORK/resources"
mkdir -p "$RES"
if [[ -f "$SCRIPT_DIR/../../LICENSE" ]]; then
    cp "$SCRIPT_DIR/../../LICENSE" "$RES/LICENSE"
fi

OUT_PKG="$OUT_DIR/SledgeDistortion-$VERSION-macOS.pkg"
PRODUCT_ARGS=(
    --distribution "$SCRIPT_DIR/distribution.xml"
    --package-path "$WORK/pkgs"
    --resources "$RES"
    --version "$VERSION"
)

if [[ -n "${APPLE_INSTALLER_IDENTITY:-}" ]]; then
    echo "Signing installer with: $APPLE_INSTALLER_IDENTITY"
    productbuild "${PRODUCT_ARGS[@]}" --sign "$APPLE_INSTALLER_IDENTITY" "$OUT_PKG"
else
    echo "APPLE_INSTALLER_IDENTITY unset — building UNSIGNED installer."
    productbuild "${PRODUCT_ARGS[@]}" "$OUT_PKG"
fi

echo "Built: $OUT_PKG"

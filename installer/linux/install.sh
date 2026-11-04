#!/usr/bin/env bash
# =============================================================================
# install.sh - install the Sledge Distortion VST3 on Linux.
#
# Ships inside the release tarball next to "Sledge Distortion.vst3".
#
#   ./install.sh             per-user:   ~/.vst3
#   ./install.sh --system    all users:  /usr/lib/vst3   (needs root, e.g. sudo)
#   ./install.sh --dir PATH  anywhere else (a folder your host scans)
#
# An existing copy is replaced, so upgrading is just running this again. Only
# "Sledge Distortion.vst3" is touched; presets, settings and the NAM profile
# library (~/.config/Monolit Beatz/Sledge Distortion) are never changed.
# =============================================================================
set -euo pipefail

BUNDLE="Sledge Distortion.vst3"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEST="${HOME}/.vst3"

usage() { sed -n '3,14p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; }

while [[ $# -gt 0 ]]; do
    case "$1" in
        --system) DEST="/usr/lib/vst3" ;;
        --dir)    [[ $# -ge 2 ]] || { echo "error: --dir needs a path" >&2; exit 2; }
                  DEST="$2"; shift ;;
        -h|--help) usage; exit 0 ;;
        *) echo "error: unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
    shift
done

SRC="$HERE/$BUNDLE"
if [[ ! -f "$SRC/Contents/x86_64-linux/Sledge Distortion.so" ]]; then
    echo "error: '$BUNDLE' not found next to this script - run it from the extracted tarball." >&2
    exit 1
fi

if [[ "$(uname -m)" != "x86_64" ]]; then
    echo "error: this build is x86_64 only (this machine is $(uname -m))." >&2
    exit 1
fi

if ! mkdir -p "$DEST" 2>/dev/null || [[ ! -w "$DEST" ]]; then
    echo "error: cannot write to $DEST. For --system, run with sudo." >&2
    exit 1
fi

# Replace, never merge: stale files from an older bundle must not survive.
# Copy to a temp name first so a failed copy never leaves a half-installed
# plugin where a good one used to be.
TMP="$DEST/.${BUNDLE}.new.$$"
trap 'rm -rf "$TMP"' EXIT
cp -R "$SRC" "$TMP"
rm -rf "${DEST:?}/$BUNDLE"
mv "$TMP" "$DEST/$BUNDLE"
trap - EXIT

echo "Installed: $DEST/$BUNDLE"
echo "Rescan plug-ins in your host (Reaper, Bitwig, Ardour...) to pick it up."

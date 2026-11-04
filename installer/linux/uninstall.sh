#!/usr/bin/env bash
# =============================================================================
# uninstall.sh - remove the Sledge Distortion VST3 installed by install.sh.
#
#   ./uninstall.sh             per-user:   ~/.vst3
#   ./uninstall.sh --system    all users:  /usr/lib/vst3   (needs root, e.g. sudo)
#   ./uninstall.sh --dir PATH  the folder you passed to install.sh --dir
#   ./uninstall.sh --purge     also delete presets, settings and the NAM profile
#                              library (~/.config/Monolit Beatz/Sledge Distortion)
#
# Without --purge your presets and imported NAM profiles are kept.
# =============================================================================
set -euo pipefail

BUNDLE="Sledge Distortion.vst3"
DEST="${HOME}/.vst3"
PURGE=0

usage() { sed -n '3,12p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; }

while [[ $# -gt 0 ]]; do
    case "$1" in
        --system) DEST="/usr/lib/vst3" ;;
        --dir)    [[ $# -ge 2 ]] || { echo "error: --dir needs a path" >&2; exit 2; }
                  DEST="$2"; shift ;;
        --purge)  PURGE=1 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "error: unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
    shift
done

if [[ -e "$DEST/$BUNDLE" ]]; then
    rm -rf "${DEST:?}/$BUNDLE"
    echo "Removed: $DEST/$BUNDLE"
else
    echo "Nothing to remove: $DEST/$BUNDLE does not exist."
fi

if [[ "$PURGE" -eq 1 ]]; then
    DATA="${XDG_CONFIG_HOME:-$HOME/.config}/Monolit Beatz/Sledge Distortion"
    if [[ -d "$DATA" ]]; then
        rm -rf "$DATA"
        echo "Removed user data: $DATA"
    fi
    rmdir "$(dirname "$DATA")" 2>/dev/null || true   # vendor folder, only if now empty
else
    echo "Kept your presets, settings and NAM profiles (use --purge to delete them)."
fi

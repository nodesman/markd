#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEFAULT_BRANDING_DIR="$(cd "$SCRIPT_DIR/../../resources/branding" && pwd)"

SRC_PNG="${1:-$DEFAULT_BRANDING_DIR/app_icon.png}"
OUT_DIR="${2:-$DEFAULT_BRANDING_DIR}"

# 1. Dependency Pre-Check: Verify ffmpeg and ffprobe exist on PATH
if ! command -v ffmpeg >/dev/null 2>&1; then
    echo "Error: 'ffmpeg' is not installed or not on PATH. Install via 'brew install ffmpeg'." >&2
    exit 2
fi

if ! command -v ffprobe >/dev/null 2>&1; then
    echo "Error: 'ffprobe' is not installed or not on PATH. Install via 'brew install ffmpeg'." >&2
    exit 2
fi

# 2. Source File & Format Pre-Check
if [[ ! -f "$SRC_PNG" ]] || [[ ! -s "$SRC_PNG" ]]; then
    echo "Error: Source icon file not found or empty at: $SRC_PNG" >&2
    exit 3
fi

PROBE_OUT="$(ffprobe -v error -select_streams v:0 -show_entries stream=codec_name,width,height,pix_fmt -of default=noprint_wrappers=1 "$SRC_PNG" 2>/dev/null || true)"
CODEC="$(echo "$PROBE_OUT" | awk -F= '$1=="codec_name"{print $2}')"
WIDTH="$(echo "$PROBE_OUT" | awk -F= '$1=="width"{print $2}')"
HEIGHT="$(echo "$PROBE_OUT" | awk -F= '$1=="height"{print $2}')"

if [[ "$CODEC" != "png" ]] || [[ -z "$WIDTH" ]] || [[ -z "$HEIGHT" ]]; then
    echo "Error: Source icon ($SRC_PNG) is not a valid PNG image (detected codec: '${CODEC:-unknown}')." >&2
    exit 3
fi

# 3. Dimension & Aspect Ratio Pre-Check (must be 1:1 square and >= 512x512)
if [[ "$WIDTH" -ne "$HEIGHT" ]]; then
    echo "Error: Source icon ($SRC_PNG) must have a 1:1 square aspect ratio (got ${WIDTH}x${HEIGHT})." >&2
    exit 4
fi

if [[ "$WIDTH" -lt 512 ]]; then
    echo "Error: Source icon ($SRC_PNG) must be at least 512x512 pixels for high-DPI acuity (got ${WIDTH}x${HEIGHT})." >&2
    exit 4
fi

mkdir -p "$OUT_DIR"

# 4. Generate Windows .ico via ffmpeg with Lanczos resampling
ICO_OUT="$OUT_DIR/markd.ico"
ffmpeg -y -v error -i "$SRC_PNG" -vf "scale=256:256:flags=lanczos" "$ICO_OUT"

# 5. Generate macOS .icns pyramid via ffmpeg + iconutil (when on macOS)
if command -v iconutil >/dev/null 2>&1; then
    ICONSET_TMP="$(mktemp -d "${TMPDIR:-/tmp}/markd_iconset.XXXXXX")/markd.iconset"
    mkdir -p "$ICONSET_TMP"
    for SIZE in 16 32 128 256 512; do
        DOUBLE=$((SIZE * 2))
        ffmpeg -y -v error -i "$SRC_PNG" -vf "scale=${SIZE}:${SIZE}:flags=lanczos" "$ICONSET_TMP/icon_${SIZE}x${SIZE}.png"
        ffmpeg -y -v error -i "$SRC_PNG" -vf "scale=${DOUBLE}:${DOUBLE}:flags=lanczos" "$ICONSET_TMP/icon_${SIZE}x${SIZE}@2x.png"
    done
    iconutil -c icns "$ICONSET_TMP" -o "$OUT_DIR/markd.icns"
    rm -rf "$(dirname "$ICONSET_TMP")"
fi

echo "Verified ${WIDTH}x${HEIGHT} PNG and generated icons in $OUT_DIR"

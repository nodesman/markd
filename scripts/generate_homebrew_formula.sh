#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
QT_PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
DIST_DIR="${DIST_DIR:-$QT_PROJECT_DIR/build/dist}"

VERSION=""
URL=""
SHA256=""
OUTPUT=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --version)
            VERSION="$2"
            shift 2
            ;;
        --url)
            URL="$2"
            shift 2
            ;;
        --sha256)
            SHA256="$2"
            shift 2
            ;;
        --output)
            OUTPUT="$2"
            shift 2
            ;;
        *)
            shift
            ;;
    esac
done

if [[ -z "$VERSION" ]]; then
    VERSION="$(grep -E 'MARKD_VERSION_STR' "$QT_PROJECT_DIR/src/core/Version.h" | sed -E 's/.*"([^"]+)".*/\1/')"
fi

TAR_NAME="Markd-${VERSION}-darwin-arm64.tar.gz"

if [[ -z "$SHA256" ]] && [[ -f "$DIST_DIR/$TAR_NAME" ]]; then
    SHA256="$(shasum -a 256 "$DIST_DIR/$TAR_NAME" | awk '{print $1}')"
fi

if [[ -z "$SHA256" ]] || ! printf '%s' "$SHA256" | grep -Eq '^[0-9a-fA-F]{64}$'; then
    echo "Error: Valid 64-char SHA-256 digest required (pass --sha256 or build $DIST_DIR/$TAR_NAME first)" >&2
    exit 1
fi

if [[ -z "$URL" ]]; then
    URL="https://github.com/nodesman/markd/releases/download/v${VERSION}/${TAR_NAME}"
fi

FORMULA_CONTENT="$(cat <<EOF
class Markd < Formula
  desc "Native C++20 / Qt 6 Markdown instrument for the command line"
  homepage "https://markd.aramudan.dev"
  url "${URL}"
  version "${VERSION}"
  sha256 "${SHA256}"
  license "MIT"

  def install
    libexec.install "Markd.app"
    bin.write_exec_script libexec/"Markd.app/Contents/MacOS/markd"
  end

  test do
    assert_match version.to_s, shell_output("#{bin}/markd --version")
  end
end
EOF
)"

if [[ -n "$OUTPUT" ]]; then
    mkdir -p "$(dirname "$OUTPUT")"
    printf '%s\n' "$FORMULA_CONTENT" > "$OUTPUT"
    echo "Wrote Homebrew formula to $OUTPUT"
else
    printf '%s\n' "$FORMULA_CONTENT"
fi

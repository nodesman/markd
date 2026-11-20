#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
QT_PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
DIST_DIR="${DIST_DIR:-$QT_PROJECT_DIR/build/dist}"

VERSION=""
URL=""
SHA256=""
OUTPUT=""
CASK_OUTPUT=""

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
        --cask-output)
            CASK_OUTPUT="$2"
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
    URL="https://github.com/nodesman/markd-releases/releases/download/v${VERSION}/${TAR_NAME}"
fi

FORMULA_CONTENT="$(cat <<EOF
class Markd < Formula
  desc "Native C++20 / Qt 6 Markdown instrument for the command line"
  homepage "https://markd.aramudan.dev"
  url "${URL}", using: :nounzip
  version "${VERSION}"
  sha256 "${SHA256}"
  license "GPL-3.0-only"

  def install
    # Keep the signed, self-contained Markd.app out of Homebrew Mach-O relocation, which
    # rewrites the bundled Qt linkage and breaks the app. Store the archive as a plain file
    # and unpack it in post_install, which runs after relocation.
    libexec.install Dir["*.tar.gz"].first => "Markd.tar.gz"
    bin.write_exec_script libexec/"Markd.app/Contents/MacOS/markd"
  end

  def post_install
    rm_r libexec/"Markd.app" if (libexec/"Markd.app").exist?
    system "tar", "-xzf", libexec/"Markd.tar.gz", "-C", libexec
    register_with_launch_services
  end

  def register_with_launch_services
    lsregister = "/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister"
    system lsregister, "-f", "#{opt_libexec}/Markd.app" if File.executable?(lsregister)
    system "#{bin}/markd", "--register-default"
  rescue StandardError
    nil
  end

  def caveats
    <<~EOS
      To ensure Finder opens .md files with Markd by default, run:
        markd --register-default
    EOS
  end

  test do
    assert_match version.to_s, shell_output("#{bin}/markd --version")
  end
end
EOF
)"

CASK_CONTENT="$(cat <<EOF
cask "markd" do
  version "${VERSION}"
  sha256 "${SHA256}"

  url "${URL}"
  name "Markd"
  desc "Fast native C++20 / Qt 6 Markdown reader and editor"
  homepage "https://markd.aramudan.dev"

  app "Markd.app"
  binary "#{appdir}/Markd.app/Contents/MacOS/markd"

  postflight do
    system_command "/usr/bin/xattr",
                   args: ["-cr", "#{appdir}/Markd.app"],
                   sudo: false
    system_command "#{appdir}/Markd.app/Contents/MacOS/markd",
                   args: ["--register-default"],
                   sudo: false
  end

  zap trash: [
    "~/Library/Application Support/Markd",
    "~/Library/Preferences/dev.aramudan.markd.Markd.plist",
    "~/Library/Preferences/org.markd.app.plist",
  ]
end
EOF
)"

if [[ -n "$CASK_OUTPUT" ]]; then
    mkdir -p "$(dirname "$CASK_OUTPUT")"
    printf '%s\n' "$CASK_CONTENT" > "$CASK_OUTPUT"
    echo "Wrote Homebrew cask to $CASK_OUTPUT"
fi

if [[ -n "$OUTPUT" ]]; then
    mkdir -p "$(dirname "$OUTPUT")"
    printf '%s\n' "$FORMULA_CONTENT" > "$OUTPUT"
    echo "Wrote Homebrew formula to $OUTPUT"
elif [[ -z "$CASK_OUTPUT" ]]; then
    printf '%s\n' "$FORMULA_CONTENT"
fi

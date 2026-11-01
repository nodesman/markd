#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
QT_PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
REPO_ROOT="$(cd "$QT_PROJECT_DIR/.." && pwd)"

QT_DIR="${QT_DIR:-/Users/rajsekharan/Qt/6.5.3/macos}"
BUILD_DIR="${BUILD_DIR:-$QT_PROJECT_DIR/build}"
DIST_DIR="${DIST_DIR:-$QT_PROJECT_DIR/build/dist}"
VERSION=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --version)
            VERSION="$2"
            shift 2
            ;;
        --build-dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        --dist-dir)
            DIST_DIR="$2"
            shift 2
            ;;
        --qt-dir)
            QT_DIR="$2"
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

if [[ ! -x "$BUILD_DIR/markd" ]]; then
    cmake -B "$BUILD_DIR" -S "$QT_PROJECT_DIR" -DCMAKE_PREFIX_PATH="$QT_DIR/lib/cmake"
    cmake --build "$BUILD_DIR" -j8 --target markd
fi

rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR/Markd.app/Contents/MacOS"
mkdir -p "$DIST_DIR/Markd.app/Contents/Resources"

cp "$BUILD_DIR/markd" "$DIST_DIR/Markd.app/Contents/MacOS/markd"
chmod 0755 "$DIST_DIR/Markd.app/Contents/MacOS/markd"

if [[ -f "$REPO_ROOT/resources/branding/markd.icns" ]]; then
    cp "$REPO_ROOT/resources/branding/markd.icns" "$DIST_DIR/Markd.app/Contents/Resources/markd.icns"
fi

cat <<EOF > "$DIST_DIR/Markd.app/Contents/Info.plist"
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleDevelopmentRegion</key>
    <string>en</string>
    <key>CFBundleDisplayName</key>
    <string>Markd</string>
    <key>CFBundleExecutable</key>
    <string>markd</string>
    <key>CFBundleIconFile</key>
    <string>markd.icns</string>
    <key>CFBundleIdentifier</key>
    <string>org.markd.app</string>
    <key>CFBundleInfoDictionaryVersion</key>
    <string>6.0</string>
    <key>CFBundleName</key>
    <string>Markd</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>${VERSION}</string>
    <key>CFBundleVersion</key>
    <string>${VERSION}</string>
    <key>LSMinimumSystemVersion</key>
    <string>13.0</string>
    <key>NSHighResolutionCapable</key>
    <true/>
    <key>CFBundleDocumentTypes</key>
    <array>
        <dict>
            <key>CFBundleTypeExtensions</key>
            <array>
                <string>md</string>
                <string>markdown</string>
                <string>mdown</string>
                <string>mkd</string>
            </array>
            <key>LSItemContentTypes</key>
            <array>
                <string>net.daringfireball.markdown</string>
                <string>public.markdown</string>
                <string>public.plain-text</string>
            </array>
            <key>CFBundleTypeName</key>
            <string>Markdown Document</string>
            <key>CFBundleTypeRole</key>
            <string>Editor</string>
            <key>LSHandlerRank</key>
            <string>Owner</string>
        </dict>
    </array>
</dict>
</plist>
EOF

if [[ -x "$QT_DIR/bin/macdeployqt" ]]; then
    "$QT_DIR/bin/macdeployqt" "$DIST_DIR/Markd.app" -always-overwrite
fi

if command -v install_name_tool >/dev/null 2>&1; then
    for rpath_candidate in \
        "@executable_path/../libexec/Markd.app/Contents/Frameworks" \
        "/opt/homebrew/opt/markd/libexec/Markd.app/Contents/Frameworks" \
        "/usr/local/opt/markd/libexec/Markd.app/Contents/Frameworks" \
        "/Applications/Markd.app/Contents/Frameworks" \
        "$QT_DIR/lib"; do
        install_name_tool -add_rpath "$rpath_candidate" "$DIST_DIR/Markd.app/Contents/MacOS/markd" 2>/dev/null || true
    done
fi

codesign --force --deep --sign - "$DIST_DIR/Markd.app"

DMG_NAME="Markd-${VERSION}-macOS.dmg"
TAR_NAME="Markd-${VERSION}-darwin-arm64.tar.gz"
BIN_NAME="markd-darwin-arm64"

hdiutil create -volname "Markd ${VERSION}" -srcfolder "$DIST_DIR/Markd.app" -ov -format UDZO "$DIST_DIR/$DMG_NAME" >/dev/null
tar -czf "$DIST_DIR/$TAR_NAME" -C "$DIST_DIR" Markd.app
cp "$DIST_DIR/Markd.app/Contents/MacOS/markd" "$DIST_DIR/$BIN_NAME"
chmod 0755 "$DIST_DIR/$BIN_NAME"

(
    cd "$DIST_DIR"
    shasum -a 256 "$DMG_NAME" "$TAR_NAME" "$BIN_NAME" > SHA256SUMS.txt
)

echo "Packaged macOS release v${VERSION} in $DIST_DIR:"
cat "$DIST_DIR/SHA256SUMS.txt"

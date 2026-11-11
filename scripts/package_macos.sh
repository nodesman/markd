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

cmake -B "$BUILD_DIR" -S "$QT_PROJECT_DIR" \
    -DCMAKE_PREFIX_PATH="$QT_DIR/lib/cmake" \
    -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_COVERAGE=OFF
cmake --build "$BUILD_DIR" -j8 --target markd markd_webengine

APP_BUNDLE="$DIST_DIR/Markd.app"
rm -rf "$DIST_DIR"
mkdir -p "$APP_BUNDLE/Contents/MacOS"
mkdir -p "$APP_BUNDLE/Contents/Frameworks"
mkdir -p "$APP_BUNDLE/Contents/Resources"

cp "$BUILD_DIR/markd" "$APP_BUNDLE/Contents/MacOS/markd"
chmod 0755 "$APP_BUNDLE/Contents/MacOS/markd"
cp "$BUILD_DIR/libmarkd_webengine.dylib" "$APP_BUNDLE/Contents/Frameworks/libmarkd_webengine.dylib"
chmod 0755 "$APP_BUNDLE/Contents/Frameworks/libmarkd_webengine.dylib"

if [[ -f "$REPO_ROOT/resources/branding/markd.icns" ]]; then
    cp "$REPO_ROOT/resources/branding/markd.icns" "$APP_BUNDLE/Contents/Resources/markd.icns"
fi

cat <<EOF > "$APP_BUNDLE/Contents/Info.plist"
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
    <key>CFBundleURLTypes</key>
    <array>
        <dict>
            <key>CFBundleURLName</key>
            <string>org.markd.url</string>
            <key>CFBundleURLSchemes</key>
            <array>
                <string>markd</string>
            </array>
        </dict>
    </array>
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
    "$QT_DIR/bin/macdeployqt" "$APP_BUNDLE" \
        -executable="$APP_BUNDLE/Contents/Frameworks/libmarkd_webengine.dylib" \
        -always-overwrite
fi

# Prune dead plugins, unused Chromium DevTools/locale .pak files, and unreferenced frameworks BEFORE rpath & codesign
rm -rf "$APP_BUNDLE/Contents/PlugIns/platforminputcontexts"
rm -rf "$APP_BUNDLE/Contents/PlugIns/position"
rm -f  "$APP_BUNDLE/Contents/PlugIns/imageformats/libqpdf.dylib"
rm -rf "$APP_BUNDLE/Contents/Frameworks/QtVirtualKeyboard.framework"
rm -rf "$APP_BUNDLE/Contents/Frameworks/QtSerialPort.framework"
rm -rf "$APP_BUNDLE/Contents/Frameworks/QtPdf.framework"

WEBENGINE_RES_DIR="$APP_BUNDLE/Contents/Frameworks/QtWebEngineCore.framework/Versions/A/Resources"
if [[ -d "$WEBENGINE_RES_DIR" ]]; then
    rm -f "$WEBENGINE_RES_DIR/qtwebengine_devtools_resources.pak"
    if [[ -d "$WEBENGINE_RES_DIR/qtwebengine_locales" ]]; then
        find "$WEBENGINE_RES_DIR/qtwebengine_locales" -type f -name '*.pak' ! -name 'en*.pak' -delete
    fi
fi

APP_BIN="$APP_BUNDLE/Contents/MacOS/markd"
WEBENGINE_PLUGIN="$APP_BUNDLE/Contents/Frameworks/libmarkd_webengine.dylib"

# Thin universal (x86_64 + arm64) Qt frameworks to match APP_BIN when single-architecture
if command -v lipo >/dev/null 2>&1; then
    TARGET_ARCH="$(lipo -archs "$APP_BIN" 2>/dev/null | awk '{print $1}')"
    ARCH_COUNT="$(lipo -archs "$APP_BIN" 2>/dev/null | wc -w | tr -d ' ')"
    if [[ "$ARCH_COUNT" == "1" && -n "$TARGET_ARCH" ]]; then
        python3 - "$APP_BUNDLE" "$TARGET_ARCH" <<'PY'
import os, subprocess, sys
bundle, target_arch = sys.argv[1], sys.argv[2]
for root, _, files in os.walk(os.path.join(bundle, "Contents")):
    for name in files:
        full = os.path.join(root, name)
        if os.path.islink(full) or not os.path.isfile(full):
            continue
        if name.endswith((".plist", ".icns", ".pak", ".dat", ".bin", ".json", ".qmltypes", ".png")):
            continue
        info = subprocess.check_output(["file", "-b", full], text=True)
        if "Mach-O universal binary" not in info:
            continue
        archs = subprocess.check_output(["lipo", "-archs", full], text=True).split()
        if target_arch in archs and len(archs) > 1:
            thin_tmp = full + ".thin"
            subprocess.check_call(["lipo", "-thin", target_arch, full, "-output", thin_tmp])
            os.chmod(thin_tmp, os.stat(full).st_mode)
            os.replace(thin_tmp, full)
PY
    fi
fi

if command -v otool >/dev/null 2>&1 && command -v install_name_tool >/dev/null 2>&1; then
    for target_macho in "$APP_BIN" "$WEBENGINE_PLUGIN"; do
        while IFS= read -r existing_rpath; do
            case "$existing_rpath" in
                /Users/*|/home/*|/tmp/*|/var/*|/opt/homebrew/Cellar/*|"$QT_DIR"*|"$BUILD_DIR"*)
                    install_name_tool -delete_rpath "$existing_rpath" "$target_macho" 2>/dev/null || true
                    ;;
            esac
        done < <(otool -l "$target_macho" | awk '/cmd LC_RPATH/{getline; getline; if ($1 == "path") print $2}')
    done

    install_name_tool -id "@rpath/libmarkd_webengine.dylib" "$WEBENGINE_PLUGIN" 2>/dev/null || true
    install_name_tool -add_rpath "@loader_path" "$WEBENGINE_PLUGIN" 2>/dev/null || true

    for rpath_candidate in \
        "@executable_path/../Frameworks" \
        "@loader_path/../Frameworks" \
        "@executable_path/../libexec/Markd.app/Contents/Frameworks" \
        "/opt/homebrew/opt/markd/libexec/Markd.app/Contents/Frameworks" \
        "/usr/local/opt/markd/libexec/Markd.app/Contents/Frameworks" \
        "/Applications/Markd.app/Contents/Frameworks"; do
        install_name_tool -add_rpath "$rpath_candidate" "$APP_BIN" 2>/dev/null || true
    done

    if otool -L "$APP_BIN" | grep -q "QtWebEngineCore"; then
        echo "Error: $APP_BIN directly links QtWebEngineCore (must load lazily via libmarkd_webengine.dylib)" >&2
        exit 1
    fi
fi

for required_bundle_item in \
    "$APP_BUNDLE/Contents/Frameworks/QtWidgets.framework" \
    "$APP_BUNDLE/Contents/Frameworks/QtGui.framework" \
    "$APP_BUNDLE/Contents/Frameworks/QtCore.framework" \
    "$APP_BUNDLE/Contents/PlugIns/platforms/libqcocoa.dylib" \
    "$APP_BUNDLE/Contents/PlugIns/styles/libqmacstyle.dylib" \
    "$APP_BUNDLE/Contents/PlugIns/tls"; do
    if [[ ! -e "$required_bundle_item" ]]; then
        echo "Error: Missing required bundled Qt component: $required_bundle_item" >&2
        exit 1
    fi
done

# Transitive Mach-O linkage audit across every binary, framework, helper, and plugin in Markd.app
python3 - "$APP_BUNDLE" <<'PY'
import os
import subprocess
import sys

bundle = sys.argv[1]
contents = os.path.join(bundle, "Contents")
frameworks_dir = os.path.join(contents, "Frameworks")

macho_targets = []
for root, _, files in os.walk(contents):
    for name in files:
        full = os.path.join(root, name)
        if os.path.islink(full) or not os.path.isfile(full):
            continue
        if name.endswith((".plist", ".icns", ".pak", ".dat", ".bin", ".json", ".qmltypes", ".png")):
            continue
        file_out = subprocess.check_output(["file", "-b", full], text=True)
        if "Mach-O" in file_out:
            macho_targets.append(full)

if not macho_targets:
    sys.exit("Error: Zero Mach-O binaries discovered in bundle")

errors = []
bad_prefixes = ("/Users/", "/home/", "/opt/homebrew/", "/usr/local/Cellar/")

for binary in macho_targets:
    otool_out = subprocess.check_output(["otool", "-L", binary], text=True).splitlines()
    for raw_line in otool_out:
        if not raw_line.startswith("\t"):
            continue
        dep = raw_line.strip().split(" (")[0]
        if dep.startswith(bad_prefixes):
            errors.append(f"Leaked host path in {binary}: {dep}")
        elif dep.startswith("@rpath/"):
            rel = dep[len("@rpath/"):]
            candidate = os.path.join(frameworks_dir, rel)
            if not os.path.exists(candidate):
                errors.append(f"Missing @rpath target for {binary}: {dep} -> {candidate}")

rpath_out = subprocess.check_output(["otool", "-l", os.path.join(contents, "MacOS", "markd")], text=True).splitlines()
for idx, line in enumerate(rpath_out):
    if "cmd LC_RPATH" in line and idx + 2 < len(rpath_out):
        parts = rpath_out[idx + 2].strip().split()
        if len(parts) >= 2 and parts[0] == "path" and parts[1].startswith(("/Users/", "/home/", "/tmp/", "/opt/homebrew/Cellar/")):
            errors.append(f"Leaked LC_RPATH in markd executable: {parts[1]}")

if errors:
    for err in errors:
        print(err, file=sys.stderr)
    sys.exit(1)

print(f"Verified transitive Mach-O linkage across {len(macho_targets)} bundle binaries.")
PY

codesign --force --deep --sign - "$APP_BUNDLE"
codesign --verify --deep --strict --verbose=2 "$APP_BUNDLE"

SMOKE_VERSION="$(QT_QPA_PLATFORM=offscreen "$APP_BIN" --version)"
echo "Bundle smoke check passed: $SMOKE_VERSION"

DMG_NAME="Markd-${VERSION}-macOS.dmg"
TAR_NAME="Markd-${VERSION}-darwin-arm64.tar.gz"
BIN_NAME="markd-darwin-arm64"

hdiutil create -volname "Markd ${VERSION}" -srcfolder "$APP_BUNDLE" -ov -format UDZO "$DIST_DIR/$DMG_NAME" >/dev/null
tar -czf "$DIST_DIR/$TAR_NAME" -C "$DIST_DIR" Markd.app
cp "$APP_BIN" "$DIST_DIR/$BIN_NAME"
chmod 0755 "$DIST_DIR/$BIN_NAME"

(
    cd "$DIST_DIR"
    shasum -a 256 "$DMG_NAME" "$TAR_NAME" "$BIN_NAME" > SHA256SUMS.txt
)

echo "Packaged macOS release v${VERSION} in $DIST_DIR:"
cat "$DIST_DIR/SHA256SUMS.txt"

#!/usr/bin/env bash
# Runs inside the Ubuntu build container: compiles Markd and writes dist/markd_<version>_<arch>.deb.
# Layout: /usr/lib/markd/{markd,libmarkd_webengine.so}, /usr/bin/markd -> ../lib/markd/markd
set -euo pipefail
SRC=${SRC:-/src}
OUT=${OUT:-/out}
BUILD=/tmp/markd-build
VERSION=$(sed -n 's/.*MARKD_VERSION_STR = "\(.*\)";/\1/p' "$SRC/src/core/Version.h")
ARCH=$(dpkg --print-architecture)
STAGE=/tmp/markd-stage/markd_${VERSION}_${ARCH}

cmake -S "$SRC" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr >/dev/null
cmake --build "$BUILD" --target markd markd_webengine

rm -rf "$STAGE"
install -Dm755 "$BUILD/markd" "$STAGE/usr/lib/markd/markd"
install -Dm644 "$BUILD/libmarkd_webengine.so" "$STAGE/usr/lib/markd/libmarkd_webengine.so"
strip --strip-unneeded "$STAGE/usr/lib/markd/markd" "$STAGE/usr/lib/markd/libmarkd_webengine.so"
mkdir -p "$STAGE/usr/bin"
ln -s ../lib/markd/markd "$STAGE/usr/bin/markd"
install -Dm644 "$SRC/packaging/linux/markd.desktop" "$STAGE/usr/share/applications/markd.desktop"
install -Dm644 "$SRC/resources/branding/app_icon.png" "$STAGE/usr/share/icons/hicolor/1024x1024/apps/markd.png"
install -Dm644 "$SRC/resources/branding/app_icon_64.png" "$STAGE/usr/share/icons/hicolor/64x64/apps/markd.png"
install -Dm644 "$SRC/LICENSE" "$STAGE/usr/share/doc/markd/copyright"

# Library dependencies come from the binaries; the web engine helper process and the
# platform plugins are loaded at runtime, so they are listed by hand.
mkdir -p /tmp/shlibs/debian && cd /tmp/shlibs
printf 'Source: markd\n\nPackage: markd\nArchitecture: any\n' > debian/control
SHLIBS=$(dpkg-shlibdeps -O -e"$STAGE/usr/lib/markd/markd" -e"$STAGE/usr/lib/markd/libmarkd_webengine.so" 2>/dev/null | sed -n 's/^shlibs:Depends=//p')
RUNTIME="qt6-qpa-plugins, libqt6svg6"

mkdir -p "$STAGE/DEBIAN"
cat > "$STAGE/DEBIAN/control" <<CONTROL
Package: markd
Version: ${VERSION}
Section: text
Priority: optional
Architecture: ${ARCH}
Depends: ${SHLIBS}, ${RUNTIME}
Maintainer: Raj <raj@aramudan.dev>
Homepage: https://markd.aramudan.dev
Installed-Size: $(du -sk "$STAGE/usr" | cut -f1)
Description: Fast, native Markdown reader and editor
 Markd opens Markdown files as clean, readable pages, with equations,
 diagrams, highlighted code and a table of contents, and lets you
 switch to a plain editor with one key.
CONTROL

mkdir -p "$OUT"
dpkg-deb --build --root-owner-group "$STAGE" "$OUT/markd_${VERSION}_${ARCH}.deb"
echo "Built $OUT/markd_${VERSION}_${ARCH}.deb"

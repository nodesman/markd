#!/usr/bin/env bash
# Publishes the Linux packages in dist/linux:
#   1. uploads the .deb files to the GitHub release v<version> on nodesman/markd-releases
#   2. writes the signed APT index into the website's apt/ folder (lab/markd-website by default)
#   3. makes sure the website redirects apt/pool/... to those GitHub release files
# The signing key is read from 1Password (op) at signing time and deleted afterwards.
# Then deploy the website and run: scripts/test_published_apt.sh
#   WEBSITE=/path/to/markd-website scripts/publish_apt_repo.sh
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
WEBSITE=${WEBSITE:-$ROOT/../../../lab/markd-website}
RELEASES_REPO=${RELEASES_REPO:-nodesman/markd-releases}
KEY_REF=${KEY_REF:-op://Private/Markd APT Signing Key}
DIST="$ROOT/dist/linux"
VERSION=$(sed -n 's/.*MARKD_VERSION_STR = "\(.*\)";/\1/p' "$ROOT/src/core/Version.h")

ls "$DIST"/markd_"$VERSION"_*.deb >/dev/null || { echo "No dist/linux/markd_${VERSION}_*.deb; run scripts/package_linux.sh first" >&2; exit 1; }
[ -d "$WEBSITE" ] || { echo "Website folder not found: $WEBSITE" >&2; exit 1; }

# 1. GitHub release assets (replaces same-named ones)
gh release upload "v$VERSION" "$DIST"/markd_"$VERSION"_*.deb --repo "$RELEASES_REPO" --clobber

# 2. Signed index, built from only the debs for this version
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
mkdir -p "$WORK/dist" "$WORK/key" "$WORK/repo"
cp "$DIST"/markd_"$VERSION"_*.deb "$WORK/dist/"
chmod 700 "$WORK/key"
op read "$KEY_REF/private_key" > "$WORK/key/private.asc"
KEY_ID=$(op read "$KEY_REF/fingerprint")
docker run --rm -e KEY_ID="$KEY_ID" -v "$WORK/dist":/dist:ro -v "$WORK/key":/key:ro -v "$WORK/repo":/repo \
    -v "$ROOT/packaging/linux":/pkg:ro markd-linux-build:24.04 bash /pkg/build_apt_repo.sh
rm -f "$WORK/key/private.asc"

rm -rf "$WEBSITE/apt/dists"
mkdir -p "$WEBSITE/apt"
cp -R "$WORK/repo/dists" "$WEBSITE/apt/"
cp "$WORK/repo/markd-archive-keyring.gpg" "$WEBSITE/apt/"

# 3. pool/<version>/<file> -> GitHub release asset (no binaries in the website)
RULE="/apt/pool/main/m/markd/:version/:file https://github.com/$RELEASES_REPO/releases/download/v:version/:file 302"
grep -qF "$RULE" "$WEBSITE/_redirects" || echo "$RULE" >> "$WEBSITE/_redirects"
echo "APT index for $VERSION written to $WEBSITE/apt. Deploy the website, then run scripts/test_published_apt.sh"

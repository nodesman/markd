#!/usr/bin/env bash
# Builds the Linux .deb in Docker and tests installing it with apt in a clean Ubuntu container.
#   scripts/package_linux.sh                 # host architecture (arm64 on Apple Silicon)
#   PLATFORM=linux/amd64 scripts/package_linux.sh   # x86_64 (emulated on Apple Silicon: slow)
#   SKIP_TEST=1 scripts/package_linux.sh     # build only
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
PLATFORM=${PLATFORM:-}
UBUNTU=${UBUNTU:-24.04}
PLATFORM_ARG=${PLATFORM:+--platform=$PLATFORM}
TAG=markd-linux-build:${UBUNTU}${PLATFORM:+-${PLATFORM##*/}}
DIST="$ROOT/dist/linux"
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

mkdir -p "$DIST"
git -C "$ROOT" archive HEAD | tar -x -C "$WORK"
# Pick up uncommitted packaging edits too, so the script can be tried before committing.
cp -R "$ROOT/packaging" "$WORK/"

docker build -q $PLATFORM_ARG --build-arg UBUNTU="$UBUNTU" -t "$TAG" -f "$ROOT/packaging/linux/Dockerfile.build" "$ROOT/packaging/linux" >/dev/null
docker run --rm $PLATFORM_ARG -v "$WORK":/src:ro -v "$DIST":/out "$TAG" bash /src/packaging/linux/build_deb.sh

[ -n "${SKIP_TEST:-}" ] && exit 0
docker run --rm $PLATFORM_ARG -v "$DIST":/dist:ro -v "$ROOT/packaging/linux":/pkg:ro "ubuntu:$UBUNTU" bash /pkg/test_apt_install.sh

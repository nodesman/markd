#!/usr/bin/env bash
# Installs Markd from the live APT repository in clean Ubuntu containers, the way users do.
#   scripts/test_published_apt.sh                   # arm64 and amd64
#   APT_URL=https://markd.aramudan.dev/apt PLATFORMS=linux/amd64 scripts/test_published_apt.sh
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
APT_URL=${APT_URL:-https://markd.aramudan.dev/apt}
for platform in ${PLATFORMS:-linux/arm64 linux/amd64}; do
    echo "== $platform"
    docker run --rm --platform="$platform" -e APT_URL="$APT_URL" -v "$ROOT/packaging/linux":/pkg:ro ubuntu:24.04 bash /pkg/test_apt_repo.sh 2>&1 | grep -v debconf
done

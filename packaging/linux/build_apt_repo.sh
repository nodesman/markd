#!/usr/bin/env bash
# Runs inside an Ubuntu container: turns the .deb files in /dist into a signed APT repository in /repo.
#   /repo/pool/main/m/markd/<version>/*.deb  (served on the website as redirects to GitHub Releases)
#   /repo/dists/stable/{Release,InRelease,Release.gpg}
#   /repo/dists/stable/main/binary-<arch>/Packages{,.gz}
#   /repo/markd-archive-keyring.gpg   (public key users install)
# Signing key: /key/private.asc if present (imported into a throwaway keyring), else the keyring in
# GNUPGHOME. KEY_ID picks the key.
set -euo pipefail
REPO=${REPO:-/repo}
export DEBIAN_FRONTEND=noninteractive
command -v apt-ftparchive >/dev/null || { apt-get update -qq && apt-get install -y -qq apt-utils gnupg >/dev/null 2>&1; }
if [ -f /key/private.asc ]; then
    export GNUPGHOME=$(mktemp -d)
    gpg --batch --quiet --import /key/private.asc 2>/dev/null
fi
export GNUPGHOME=${GNUPGHOME:-/gnupg}
KEY_ID=${KEY_ID:?set KEY_ID to the signing key}

for deb in /dist/*.deb; do
    version=$(dpkg-deb -f "$deb" Version)
    mkdir -p "$REPO/pool/main/m/markd/$version"
    cp "$deb" "$REPO/pool/main/m/markd/$version/"
done
cd "$REPO"
ARCHES=$(for d in pool/main/m/markd/*/*.deb; do dpkg-deb -f "$d" Architecture; done | sort -u | tr '\n' ' ')
for arch in $ARCHES; do
    mkdir -p "dists/stable/main/binary-$arch"
    apt-ftparchive --arch "$arch" packages pool > "dists/stable/main/binary-$arch/Packages"
    gzip -9kf "dists/stable/main/binary-$arch/Packages"
done
apt-ftparchive \
    -o APT::FTPArchive::Release::Origin=Markd \
    -o APT::FTPArchive::Release::Label=Markd \
    -o APT::FTPArchive::Release::Suite=stable \
    -o APT::FTPArchive::Release::Codename=stable \
    -o APT::FTPArchive::Release::Components=main \
    -o "APT::FTPArchive::Release::Architectures=${ARCHES% }" \
    release dists/stable > dists/stable/Release
rm -f dists/stable/InRelease dists/stable/Release.gpg
gpg --batch --yes --local-user "$KEY_ID" --clearsign -o dists/stable/InRelease dists/stable/Release
gpg --batch --yes --local-user "$KEY_ID" -abs -o dists/stable/Release.gpg dists/stable/Release
gpg --batch --yes --export "$KEY_ID" > markd-archive-keyring.gpg
echo "APT repository for: $ARCHES"

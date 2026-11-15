#!/usr/bin/env bash
# Runs inside a fresh ubuntu container: adds the Markd APT repository exactly as the website tells
# users to, then installs with apt-get. APT_URL points at the repository (local server in tests).
set -euo pipefail
APT_URL=${APT_URL:?}
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq && apt-get install -y -qq curl ca-certificates >/dev/null
# --- the user-facing install steps ---
curl -fsSL "$APT_URL/markd-archive-keyring.gpg" -o /usr/share/keyrings/markd-archive-keyring.gpg
echo "deb [signed-by=/usr/share/keyrings/markd-archive-keyring.gpg] $APT_URL stable main" > /etc/apt/sources.list.d/markd.list
apt-get update -qq
apt-get install -y -qq markd >/dev/null
# ---
markd --version
apt-cache policy markd | sed -n 1,4p
echo "PASS: apt-get install markd from the signed repository"

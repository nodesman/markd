#!/usr/bin/env bash
# Runs inside a fresh ubuntu container that has never seen Qt: installs the .deb with apt the way a
# user would, then proves the installed markd starts and renders a real page with the web engine.
set -euo pipefail
DEB=$(ls /dist/markd_*.deb | head -1)
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y -qq "$DEB" >/dev/null
echo "installed: $(dpkg-query -W -f='${Package} ${Version} ${Architecture}' markd)"

test "$(readlink -f "$(command -v markd)")" = /usr/lib/markd/markd
markd --version
markd --help >/dev/null
test -f /usr/share/applications/markd.desktop

printf '# Hello from apt\n\nInline math $e^{i\\pi}+1=0$.\n\n```python\nprint("hi")\n```\n' > /tmp/hello.md
# offscreen platform: no display in the container. --no-sandbox: Chromium's sandbox needs
# privileges a default container doesn't grant; on a desktop it runs sandboxed as normal.
OUTPUT=$(HOME=/tmp/home QT_QPA_PLATFORM=offscreen QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox \
    MARKD_STARTUP_BENCHMARK=2 timeout 60 markd /tmp/hello.md 2>&1 || true)
echo "$OUTPUT" | grep -E 'MARKD_BENCH_(WINDOW_FRAME|HYDRATED)_MS' || { echo "$OUTPUT"; echo "FAIL: page never rendered"; exit 1; }
echo "PASS: apt install, launch and web engine render"

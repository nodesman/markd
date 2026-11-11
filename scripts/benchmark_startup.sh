#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
QT_PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$QT_PROJECT_DIR/build}"
MARKD_BIN=""
ITERATIONS=20

while [[ $# -gt 0 ]]; do
    case "$1" in
        --bin)
            MARKD_BIN="$2"
            shift 2
            ;;
        --iterations)
            ITERATIONS="$2"
            shift 2
            ;;
        *)
            shift
            ;;
    esac
done

if [[ -z "$MARKD_BIN" ]]; then
    DIST_BIN="$BUILD_DIR/dist/Markd.app/Contents/MacOS/markd"
    RAW_BIN="$BUILD_DIR/markd"
    if [[ -x "$DIST_BIN" && ( ! -x "$RAW_BIN" || ! "$RAW_BIN" -nt "$DIST_BIN" ) ]]; then
        MARKD_BIN="$DIST_BIN"
    elif [[ -x "$RAW_BIN" ]]; then
        MARKD_BIN="$RAW_BIN"
    else
        echo "Error: Could not locate markd binary in $BUILD_DIR" >&2
        exit 1
    fi
fi

python3 - "$MARKD_BIN" "$ITERATIONS" <<'PY'
import hashlib
import math
import os
import re
import signal
import subprocess
import sys
import tempfile
import time

markd_bin = os.path.abspath(sys.argv[1])
iterations = int(sys.argv[2])
qpa_platform = os.environ.get("MARKD_BENCH_QPA", "offscreen")

print(f"=== Markd Startup Latency Gate ({iterations} iterations, QPA={qpa_platform}) ===")
print(f"Binary: {markd_bin} ({os.path.getsize(markd_bin) / (1024 * 1024):.2f} MiB)")

def p95(samples):
    ordered = sorted(samples)
    idx = max(0, min(len(ordered) - 1, math.ceil(0.95 * len(ordered)) - 1))
    return ordered[idx]

with tempfile.TemporaryDirectory(prefix="mb_", dir="/tmp") as raw_tmpdir:
    tmpdir = os.path.realpath(raw_tmpdir)
    sample_md = os.path.join(tmpdir, "sample.md")
    with open(sample_md, "w", encoding="utf-8") as fh:
        fh.write("# Instant Startup Benchmark\n\n")
        for i in range(40):
            fh.write(f"## Section {i + 1}\n\nParagraph prose with **bold**, `inline_code()`, and $E = mc^2$.\n\n")
            fh.write("```cpp\nint compute(int x) { return x * 2; }\n```\n\n")

    # Warm filesystem page cache once before timed trials
    subprocess.run([markd_bin, "--version"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=5.0, check=True)

    # 1. Cold CLI --version (< 25 ms p95)
    cli_samples = []
    for _ in range(iterations):
        t0 = time.perf_counter()
        res = subprocess.run([markd_bin, "--version"], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=5.0, check=True)
        elapsed_ms = (time.perf_counter() - t0) * 1000.0
        if "Markd version" not in res.stdout:
            sys.exit(f"Unexpected --version output: {res.stdout}")
        cli_samples.append(elapsed_ms)

    # 2. Secondary single-instance QLocalSocket forward (< 60 ms p95)
    isolated_home = os.path.join(tmpdir, "home")
    isolated_tmp = os.path.join(tmpdir, "tmp")
    os.makedirs(isolated_home, exist_ok=True)
    os.makedirs(isolated_tmp, exist_ok=True)
    digest = hashlib.sha256(isolated_home.encode("utf-8")).hexdigest()[:12]
    expected_socket = os.path.join(isolated_tmp, f"markd-ipc-{digest}")

    ipc_env = os.environ.copy()
    ipc_env["HOME"] = isolated_home
    ipc_env["TMPDIR"] = isolated_tmp
    ipc_env["QT_QPA_PLATFORM"] = qpa_platform
    ipc_env.pop("MARKD_STARTUP_BENCHMARK", None)

    primary = subprocess.Popen(
        [markd_bin, sample_md],
        env=ipc_env,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    ipc_samples = []
    try:
        deadline = time.perf_counter() + 5.0
        while not os.path.exists(expected_socket) and time.perf_counter() < deadline:
            time.sleep(0.01)
        if not os.path.exists(expected_socket):
            sys.exit(f"Primary instance did not bind IPC socket at {expected_socket}")

        second_md = os.path.join(tmpdir, "second.md")
        with open(second_md, "w", encoding="utf-8") as fh:
            fh.write("# Secondary Forwarded Document\n")

        # Ensure primary instance has completed initial post-show hydration and its event loop is idle
        time.sleep(2.0)
        subprocess.run([markd_bin, second_md], env=ipc_env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=5.0, check=True)

        for _ in range(iterations):
            t0 = time.perf_counter()
            subprocess.run([markd_bin, second_md], env=ipc_env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=5.0, check=True)
            ipc_samples.append((time.perf_counter() - t0) * 1000.0)
    finally:
        primary.send_signal(signal.SIGTERM)
        try:
            primary.wait(timeout=3.0)
        except subprocess.TimeoutExpired:
            primary.kill()

    # 3. Cold GUI first native frame + InstantPreviewSurface paint (< 120 ms p95, WEBENGINE_INIT=0)
    bench_env = os.environ.copy()
    bench_env["HOME"] = os.path.join(tmpdir, "bench_home")
    bench_env["TMPDIR"] = os.path.join(tmpdir, "bench_tmp")
    os.makedirs(bench_env["HOME"], exist_ok=True)
    os.makedirs(bench_env["TMPDIR"], exist_ok=True)
    bench_env["QT_QPA_PLATFORM"] = qpa_platform

    env_stage1 = bench_env.copy()
    env_stage1["MARKD_STARTUP_BENCHMARK"] = "1"
    # Warm dynamic loader cache once
    subprocess.run([markd_bin, sample_md], env=env_stage1, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, timeout=5.0, check=True)

    instant_wall_samples = []
    instant_internal_samples = []
    for _ in range(iterations):
        t0 = time.perf_counter()
        proc = subprocess.Popen([markd_bin, sample_md], env=env_stage1, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        first_line = proc.stdout.readline()
        wall_ms = (time.perf_counter() - t0) * 1000.0
        rest_out, err_out = proc.communicate(timeout=5.0)
        full_out = first_line + rest_out
        if proc.returncode != 0:
            sys.exit(f"Stage 1 exited {proc.returncode}: {err_out}")
        m = re.search(r"MARKD_BENCH_INSTANT_PAINT_MS=(\d+)\s+WEBENGINE_INIT=(\d+)", full_out)
        if not m:
            sys.exit(f"Missing MARKD_BENCH_INSTANT_PAINT_MS in stdout: {full_out}\nstderr: {err_out}")
        if int(m.group(2)) != 0:
            sys.exit(f"Pre-paint WebEngine initialization detected (WEBENGINE_INIT={m.group(2)})")
        instant_internal_samples.append(float(m.group(1)))
        instant_wall_samples.append(wall_ms)

    # 4. Full WebBridge::documentRendered prose hydration (< 450 ms p95)
    env_stage2 = bench_env.copy()
    env_stage2["MARKD_STARTUP_BENCHMARK"] = "2"
    subprocess.run([markd_bin, sample_md], env=env_stage2, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, timeout=10.0, check=True)

    hydrated_samples = []
    for _ in range(iterations):
        res = subprocess.run([markd_bin, sample_md], env=env_stage2, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=10.0, check=True)
        m = re.search(r"MARKD_BENCH_HYDRATED_MS=(\d+)", res.stdout)
        if not m:
            sys.exit(f"Missing MARKD_BENCH_HYDRATED_MS in stdout: {res.stdout}\nstderr: {res.stderr}")
        hydrated_samples.append(float(m.group(1)))

cli_p95 = p95(cli_samples)
ipc_p95 = p95(ipc_samples)
instant_p95 = p95(instant_wall_samples)
instant_inproc_p95 = p95(instant_internal_samples)
hydrated_p95 = p95(hydrated_samples)

print(f"  1. CLI --version wall-clock p95:           {cli_p95:6.2f} ms  (budget <  25.00 ms)")
print(f"  2. Secondary IPC forward wall-clock p95:   {ipc_p95:6.2f} ms  (budget <  60.00 ms)")
print(f"  3. Cold InstantPreviewSurface paint p95:   {instant_inproc_p95:6.2f} ms  (wall {instant_p95:5.1f} ms, budget < 120.00 ms, WEBENGINE_INIT=0)")
print(f"  4. Full WebBridge prose hydration p95:     {hydrated_p95:6.2f} ms  (budget < 450.00 ms)")

failures = []
if cli_p95 >= 25.0:
    failures.append(f"CLI --version p95 {cli_p95:.2f} ms >= 25.0 ms")
if ipc_p95 >= 60.0:
    failures.append(f"Secondary IPC forward p95 {ipc_p95:.2f} ms >= 60.0 ms")
if instant_inproc_p95 >= 120.0:
    failures.append(f"Cold InstantPreviewSurface paint p95 {instant_inproc_p95:.2f} ms >= 120.0 ms")
if hydrated_p95 >= 450.0:
    failures.append(f"Full WebBridge prose hydration p95 {hydrated_p95:.2f} ms >= 450.0 ms")

if failures:
    for f in failures:
        print(f"FAILED: {f}", file=sys.stderr)
    sys.exit(1)

print("PASS: All 4 startup latency budgets verified.")
PY

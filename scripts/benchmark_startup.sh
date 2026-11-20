#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
QT_PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$QT_PROJECT_DIR/build}"
BASELINE_FILE="${MARKD_STARTUP_BASELINE:-$SCRIPT_DIR/startup_baseline.json}"
MARKD_BIN=""
ITERATIONS=20
MODE="run"

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
        --self-test)
            MODE="self-test"
            shift
            ;;
        --record-baseline)
            MODE="record-baseline"
            shift
            ;;
        *)
            shift
            ;;
    esac
done

if [[ "$MODE" != "self-test" && -z "$MARKD_BIN" ]]; then
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

python3 - "$MODE" "${MARKD_BIN:-}" "$ITERATIONS" "$BASELINE_FILE" <<'PY'
import hashlib
import json
import math
import os
import re
import signal
import subprocess
import sys
import tempfile
import threading
import time

mode, markd_bin, iterations, baseline_file = sys.argv[1], sys.argv[2], int(sys.argv[3]), sys.argv[4]
qpa_platform = os.environ.get("MARKD_BENCH_QPA", "offscreen")

# ---- Locked phase contract (Task 8.12.7) -------------------------------------------------------
PHASES = ["main_entry", "window_shown", "plugin_loaded", "profile_created",
          "template_loaded", "bridge_ready", "content_revealed"]
STEP_LABELS = {
    "main_entry": "launch -> main()",
    "window_shown": "main() -> window shown",
    "plugin_loaded": "web engine plugin loaded",
    "profile_created": "Chromium profile created",
    "template_loaded": "template + scripts loaded",
    "bridge_ready": "QWebChannel handshake",
    "content_revealed": "render + reveal",
}
MARK_RE = re.compile(r"^MARKD_PHASE (\w+) (\d{13})$")
DRIFT_RATIO = 1.25
TOTAL_BUDGETS = {"cli": 25.0, "ipc": 60.0, "frame": 120.0, "reveal": 450.0}

def p95(samples):
    ordered = sorted(samples)
    idx = max(0, min(len(ordered) - 1, math.ceil(0.95 * len(ordered)) - 1))
    return ordered[idx]

def median(samples):
    ordered = sorted(samples)
    mid = len(ordered) // 2
    return ordered[mid] if len(ordered) % 2 else (ordered[mid - 1] + ordered[mid]) / 2.0

def budget_from(p95_ms):
    """Execution contract rule: 1.5 x measured p95, rounded up to the next 5 ms."""
    return float(math.ceil(1.5 * p95_ms / 5.0) * 5)

def parse_steps(stderr_text, spawn_epoch_ms):
    """First occurrence of each locked mark -> per-step durations. Raises on a missing mark."""
    first = {}
    for line in stderr_text.splitlines():
        m = MARK_RE.match(line.strip())
        if m and m.group(1) in PHASES and m.group(1) not in first:
            first[m.group(1)] = int(m.group(2))
    missing = [p for p in PHASES if p not in first]
    if missing:
        raise ValueError("missing phase marks: " + ", ".join(missing))
    steps, previous = {}, spawn_epoch_ms
    for name in PHASES:
        steps[name] = first[name] - previous
        previous = first[name]
    return steps

def evaluate(step_samples, gate_samples, baseline):
    """Returns (failures, warnings) for per-step limits, gate budgets and median drift."""
    failures, warnings = [], []
    phases = (baseline or {}).get("phases", {})
    for name in PHASES:
        samples = step_samples.get(name, [])
        if not samples or name not in phases:
            continue
        step_p95, step_med = p95(samples), median(samples)
        if step_p95 > phases[name]["limit_ms"]:
            failures.append(f"phase '{name}' ({STEP_LABELS[name]}) p95 {step_p95:.1f} ms > limit {phases[name]['limit_ms']:.0f} ms")
        if step_med > phases[name]["median_ms"] * DRIFT_RATIO:
            warnings.append(f"phase '{name}' median {step_med:.1f} ms drifted > 25% above baseline {phases[name]['median_ms']:.1f} ms")
    gates = (baseline or {}).get("gates", {})
    for name, samples in gate_samples.items():
        if samples and name in gates and p95(samples) >= gates[name]["budget_ms"]:
            failures.append(f"gate '{name}' p95 {p95(samples):.1f} ms >= budget {gates[name]['budget_ms']:.0f} ms")
    return failures, warnings

# ---- Self-test (synthetic data; no timing, no binary) ------------------------------------------
def self_test():
    baseline = {"phases": {p: {"median_ms": 10.0, "limit_ms": 20.0} for p in PHASES},
                "gates": {"warm_open": {"p95_ms": 50.0, "budget_ms": 75.0}}}
    ok = {p: [10.0] * 10 for p in PHASES}
    results = []
    def it(sentence, condition):
        results.append(condition)
        print(f"  [it] {sentence}: {'ok' if condition else 'FAILED'}")
    print("--- Self-test: startup benchmark guard ---")
    slow = dict(ok, profile_created=[30.0] * 10)
    f, _ = evaluate(slow, {}, baseline)
    it("names the phase that exceeds its limit when the benchmark fails", len(f) == 1 and "'profile_created'" in f[0])
    drift = dict(ok, template_loaded=[14.0] * 10)
    f, w = evaluate(drift, {}, baseline)
    it("warns on baseline drift without failing the gate", not f and len(w) == 1 and "'template_loaded'" in w[0])
    f, w = evaluate(ok, {"warm_open": [40.0] * 10}, baseline)
    it("passes a run within limits and baseline", not f and not w)
    lines = "\n".join(f"MARKD_PHASE {p} {1700000000000 + i}" for i, p in enumerate(PHASES) if p != "bridge_ready")
    try:
        parse_steps(lines, 1700000000000)
        missing_detected = False
    except ValueError as err:
        missing_detected = "bridge_ready" in str(err)
    it("fails when a phase mark is missing from the output", missing_detected)
    f, _ = evaluate(ok, {"warm_open": [80.0] * 10}, baseline)
    it("fails a gate that reaches its budget", len(f) == 1 and "'warm_open'" in f[0])
    if not all(results):
        sys.exit("Self-test FAILED")
    print("PASS: Self-test: startup benchmark guard")

self_test()
if mode == "self-test":
    sys.exit(0)

# ---- Measurement --------------------------------------------------------------------------------
markd_bin = os.path.abspath(markd_bin)
print(f"=== Markd Startup Latency Gate ({iterations} iterations, QPA={qpa_platform}) ===")
print(f"Binary: {markd_bin} ({os.path.getsize(markd_bin) / (1024 * 1024):.2f} MiB)")

def isolated_env(root, name):
    env = os.environ.copy()
    env["HOME"] = os.path.join(root, f"{name}_home")
    env["TMPDIR"] = os.path.join(root, f"{name}_tmp")
    os.makedirs(env["HOME"], exist_ok=True)
    os.makedirs(env["TMPDIR"], exist_ok=True)
    env["QT_QPA_PLATFORM"] = qpa_platform
    env.pop("MARKD_STARTUP_BENCHMARK", None)
    env.pop("MARKD_STARTUP_PHASES", None)
    return env

def expected_socket_for(env):
    digest = hashlib.sha256(env["HOME"].encode("utf-8")).hexdigest()[:12]
    return os.path.join(env["TMPDIR"], f"markd-ipc-{digest}")

def start_primary(env, document):
    primary = subprocess.Popen([markd_bin, document], env=env, stdout=subprocess.DEVNULL,
                               stderr=subprocess.PIPE, text=True)
    socket_path = expected_socket_for(env)
    deadline = time.perf_counter() + 5.0
    while not os.path.exists(socket_path) and time.perf_counter() < deadline:
        time.sleep(0.01)
    if not os.path.exists(socket_path):
        primary.kill()
        sys.exit(f"Primary instance did not bind IPC socket at {socket_path}")
    return primary

def stop(proc):
    proc.send_signal(signal.SIGTERM)
    try:
        proc.wait(timeout=3.0)
    except subprocess.TimeoutExpired:
        proc.kill()

with tempfile.TemporaryDirectory(prefix="mb_", dir="/tmp") as raw_tmpdir:
    tmpdir = os.path.realpath(raw_tmpdir)
    sample_md = os.path.join(tmpdir, "sample.md")
    with open(sample_md, "w", encoding="utf-8") as fh:
        fh.write("# Startup Benchmark\n\n")
        for i in range(40):
            fh.write(f"## Section {i + 1}\n\nParagraph prose with **bold**, `inline_code()`, and $E = mc^2$.\n\n")
            fh.write("```cpp\nint compute(int x) { return x * 2; }\n```\n\n")

    subprocess.run([markd_bin, "--version"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=5.0, check=True)

    # 1. Cold CLI --version
    cli_samples = []
    for _ in range(iterations):
        t0 = time.perf_counter()
        res = subprocess.run([markd_bin, "--version"], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=5.0, check=True)
        cli_samples.append((time.perf_counter() - t0) * 1000.0)
        if "Markd version" not in res.stdout:
            sys.exit(f"Unexpected --version output: {res.stdout}")

    # 2. Secondary single-instance forward (isolated HOME/TMPDIR; never the user's real socket)
    ipc_env = isolated_env(tmpdir, "ipc")
    primary = start_primary(ipc_env, sample_md)
    ipc_samples = []
    try:
        second_md = os.path.join(tmpdir, "second.md")
        with open(second_md, "w", encoding="utf-8") as fh:
            fh.write("# Secondary Forwarded Document\n")
        time.sleep(2.0)
        subprocess.run([markd_bin, second_md], env=ipc_env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=5.0, check=True)
        for _ in range(iterations):
            t0 = time.perf_counter()
            subprocess.run([markd_bin, second_md], env=ipc_env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=5.0, check=True)
            ipc_samples.append((time.perf_counter() - t0) * 1000.0)
    finally:
        stop(primary)

    bench_env = isolated_env(tmpdir, "bench")

    # 3. Cold window frame visible (web engine not yet started)
    env_stage1 = dict(bench_env, MARKD_STARTUP_BENCHMARK="1")
    subprocess.run([markd_bin, sample_md], env=env_stage1, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, timeout=5.0, check=True)
    frame_samples = []
    for _ in range(iterations):
        res = subprocess.run([markd_bin, sample_md], env=env_stage1, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=5.0)
        if res.returncode != 0:
            sys.exit(f"Stage 1 exited {res.returncode}: {res.stderr}")
        m = re.search(r"MARKD_BENCH_WINDOW_FRAME_MS=(\d+)\s+WEBENGINE_INIT=(\d+)", res.stdout)
        if not m:
            sys.exit(f"Missing MARKD_BENCH_WINDOW_FRAME_MS in stdout: {res.stdout}\nstderr: {res.stderr}")
        if int(m.group(2)) != 0:
            sys.exit(f"Web engine started before the first frame (WEBENGINE_INIT={m.group(2)})")
        frame_samples.append(float(m.group(1)))

    # 4. Cold content revealed, with per-phase marks
    env_stage2 = dict(bench_env, MARKD_STARTUP_BENCHMARK="2", MARKD_STARTUP_PHASES="1")
    subprocess.run([markd_bin, sample_md], env=env_stage2, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, timeout=10.0, check=True)
    reveal_samples, step_samples = [], {p: [] for p in PHASES}
    for _ in range(iterations):
        spawn_ms = time.time() * 1000.0
        res = subprocess.run([markd_bin, sample_md], env=env_stage2, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=10.0, check=True)
        m = re.search(r"MARKD_BENCH_HYDRATED_MS=(\d+)", res.stdout)
        if not m:
            sys.exit(f"Missing MARKD_BENCH_HYDRATED_MS in stdout: {res.stdout}\nstderr: {res.stderr}")
        reveal_samples.append(float(m.group(1)))
        try:
            steps = parse_steps(res.stderr, spawn_ms)
        except ValueError as err:
            sys.exit(f"Phase trace incomplete: {err}")
        for name, value in steps.items():
            step_samples[name].append(value)

    # 5. Warm open: second file via IPC into a running instance, through to its content_revealed mark
    warm_env = dict(isolated_env(tmpdir, "warm"), MARKD_STARTUP_PHASES="1")
    primary = start_primary(warm_env, sample_md)
    reveals, reveals_lock = [], threading.Lock()
    def read_marks(stream):
        for line in stream:
            mm = MARK_RE.match(line.strip())
            if mm and mm.group(1) == "content_revealed":
                with reveals_lock:
                    reveals.append(int(mm.group(2)))
    reader = threading.Thread(target=read_marks, args=(primary.stderr,), daemon=True)
    reader.start()
    warm_samples = []
    try:
        time.sleep(2.0)
        for i in range(iterations + 1):
            warm_md = os.path.join(tmpdir, f"warm_{i}.md")
            with open(warm_md, "w", encoding="utf-8") as fh:
                fh.write(f"# Warm Document {i}\n\nForwarded into the running instance.\n")
            t0 = time.time() * 1000.0
            subprocess.run([markd_bin, warm_md], env=warm_env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=5.0, check=True)
            deadline = time.perf_counter() + 10.0
            mark = None
            while mark is None and time.perf_counter() < deadline:
                with reveals_lock:
                    later = [r for r in reveals if r >= t0]
                mark = min(later) if later else None
                time.sleep(0.005)
            if mark is None:
                sys.exit(f"Warm open {i}: no content_revealed mark within 10 s")
            with reveals_lock:
                reveals.remove(mark)
            if i > 0:  # first sample warms the path
                warm_samples.append(mark - t0)
    finally:
        stop(primary)

    # 6. First mode switch after an edit in a booted Edit window
    env_stage3 = dict(bench_env, MARKD_STARTUP_BENCHMARK="3")
    switch_samples = []
    for i in range(iterations + 1):
        res = subprocess.run([markd_bin, sample_md], env=env_stage3, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=15.0, check=True)
        m = re.search(r"MARKD_BENCH_MODE_SWITCH_MS=(\d+)", res.stdout)
        if not m:
            sys.exit(f"Missing MARKD_BENCH_MODE_SWITCH_MS in stdout: {res.stdout}\nstderr: {res.stderr}")
        if i > 0:
            switch_samples.append(float(m.group(1)))

# ---- Report -------------------------------------------------------------------------------------
cli_p95, ipc_p95, frame_p95, reveal_p95 = p95(cli_samples), p95(ipc_samples), p95(frame_samples), p95(reveal_samples)
gate_samples = {"warm_open": warm_samples, "mode_switch": switch_samples}

print(f"  1. CLI --version wall-clock p95:           {cli_p95:6.2f} ms  (budget <  25.00 ms)")
print(f"  2. Secondary IPC forward wall-clock p95:   {ipc_p95:6.2f} ms  (budget <  60.00 ms)")
print(f"  3. Cold window frame visible p95:          {frame_p95:6.2f} ms  (budget < 120.00 ms, WEBENGINE_INIT=0)")
print(f"  4. Cold content revealed p95:              {reveal_p95:6.2f} ms  (budget < 450.00 ms)")
print(f"  5. Warm open (IPC -> reveal) p95:          {p95(warm_samples):6.2f} ms")
print(f"  6. First mode switch after edit p95:       {p95(switch_samples):6.2f} ms")
print("  Startup phases (step durations, ms):")
print(f"    {'phase':<18} {'step':<28} {'median':>8} {'p95':>8}")
for name in PHASES:
    print(f"    {name:<18} {STEP_LABELS[name]:<28} {median(step_samples[name]):8.1f} {p95(step_samples[name]):8.1f}")

if mode == "record-baseline":
    record = {
        "rule": "limit_ms / budget_ms = 1.5 x measured p95, rounded up to the next 5 ms; drift warning at > 25% above median_ms",
        "recorded_with": {"iterations": iterations, "qpa": qpa_platform},
        "phases": {name: {"median_ms": round(median(step_samples[name]), 1),
                          "p95_ms": round(p95(step_samples[name]), 1),
                          "limit_ms": budget_from(p95(step_samples[name]))} for name in PHASES},
        "gates": {name: {"p95_ms": round(p95(s), 1), "budget_ms": budget_from(p95(s))} for name, s in gate_samples.items()},
    }
    with open(baseline_file, "w", encoding="utf-8") as fh:
        json.dump(record, fh, indent=2)
        fh.write("\n")
    print(f"Recorded baseline: {baseline_file}")

baseline = None
if os.path.exists(baseline_file):
    with open(baseline_file, encoding="utf-8") as fh:
        baseline = json.load(fh)
else:
    print(f"WARNING: no baseline at {baseline_file}; per-phase limits and gate 5-6 budgets not enforced")

failures, warnings = evaluate(step_samples, gate_samples, baseline)
if cli_p95 >= TOTAL_BUDGETS["cli"]:
    failures.append(f"CLI --version p95 {cli_p95:.2f} ms >= 25.0 ms")
if ipc_p95 >= TOTAL_BUDGETS["ipc"]:
    failures.append(f"Secondary IPC forward p95 {ipc_p95:.2f} ms >= 60.0 ms")
if frame_p95 >= TOTAL_BUDGETS["frame"]:
    failures.append(f"Cold window frame visible p95 {frame_p95:.2f} ms >= 120.0 ms")
if reveal_p95 >= TOTAL_BUDGETS["reveal"]:
    failures.append(f"Cold content revealed p95 {reveal_p95:.2f} ms >= 450.0 ms")

for w in warnings:
    print(f"WARNING: {w}")
if failures:
    for f in failures:
        print(f"FAILED: {f}", file=sys.stderr)
    sys.exit(1)
print("PASS: All startup latency budgets verified.")
PY

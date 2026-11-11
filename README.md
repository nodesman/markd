# Markd

**Markd** is a fast, native cross-platform Markdown reader and editor built in **C++20** and **Qt 6** for macOS, Windows, and Linux.

Designed for engineers who live in the terminal and read technical specifications, RFCs, and architecture documents all day, Markd opens documents in under **180 ms** without Electron bloat, cloud accounts, or background telemetry.

* **Website & Downloads:** [https://markd.aramudan.dev](https://markd.aramudan.dev)
* **License:** [GPL-3.0](LICENSE) (matching Qt 6 Open Source GPLv3 / LGPLv3)

---

## Highlights & Systems Architecture

* **Two-Stage Sub-180 ms Cold Start:**
  * **Stage 1 (`InstantPreviewSurface`):** Native `QTextBrowser` paints styled GitHub-flavored Markdown, editorial margins, and heading anchors in `< 120 ms` p95 before Chromium initializes.
  * **Stage 2 (Deferred `QLibrary` Hydration):** `libmarkd_webengine` (`QtWebEngineWidgets` + `QtWebChannel`) loads dynamically after the first native frame paints and seamlessly transfers scroll ratio, zoom factor, Find-in-Page state, and keyboard focus.
* **Single-Instance Multi-Window IPC (`QLocalServer`):**
  * CLI invocations (`markd README.md`) bypass `QApplication` initialization when an instance is already running, forwarding file paths over a per-user local socket in `< 60 ms` p95 and raising existing windows without duplicate tabs or processes.
* **Offline-First Technical Rendering:**
  * Bundled **Markdown-It**, **KaTeX** math typesetting, **Highlight.js** syntax cards with one-click copy, and lazy-loaded **Mermaid** diagrams sanitized through **DOMPurify** (`foreignObject` HTML label support with strict CSS/script scrubbing).
* **Live Disk Hot-Reload & Native Export:**
  * `QFileSystemWatcher` reloads edited `.md` files automatically while retaining scroll position; supports standalone HTML export, PDF print/export, and rich HTML clipboard copy.
* **Disciplined C++20 Codebase:**
  * Decomposed UI/domain coordinators (`<= 200` lines on strict modules), strict Object Calisthenics enforcement, headless `CTest` suites compiled with `-UNDEBUG`, and `100%` `c8` branch/line coverage on the JavaScript rendering layer.

---

## Installation

### macOS (Homebrew)
```sh
brew install nodesman/markd/markd
```

### POSIX One-Liner (macOS & Linux)
```sh
curl -fsSL https://markd.aramudan.dev/install.sh | sh
```

### Windows (PowerShell)
```powershell
irm https://markd.aramudan.dev/install.ps1 | iex
```

---

## Building from Source

### Prerequisites
* **C++20 compiler:** Clang 15+ (macOS/Linux), GCC 12+ (Linux), or MSVC 2022 (Windows)
* **CMake:** `>= 3.16`
* **Qt 6 (`>= 6.5`):** `Core`, `Gui`, `Widgets`, `Network`, `WebChannel`, `WebEngineWidgets`, `Test`
* **Node.js (`>= 20`):** Optional — used only for running the headless JSDOM/coverage web test suite

### Configure, Build & Test (CMake)
```sh
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="/path/to/Qt/6.5.3/macos/lib/cmake"
cmake --build build -j8
ctest --test-dir build --output-on-failure
```

### Run Web Rendering Tests (100% Branch & Line Coverage)
```sh
cd web
npm ci
npm run test:coverage
```

### Run Using `task` ([Taskfile](https://taskfile.dev))
```sh
task build          # Configure & compile Release binary + assertion-enabled CTest runner
task test           # Run decoupled C++ (CTest) and Web (Node/JSDOM) suites
task benchmark      # Verify p95 cold-start, CLI, and IPC latency budgets
task package-macos  # Build self-contained arm64 Markd.app, .dmg, and .tar.gz
```

---

## Repository Layout

```text
├── CMakeLists.txt       # C++20 build graph (markd binary + libmarkd_webengine plugin + test_cli)
├── Taskfile.yml         # Developer build, test, benchmark, and packaging automation
├── src/
│   ├── app/             # CLI fast-path parser, QApplication lifecycle, QLocalServer IPC
│   ├── bridge/          # Bidirectional QWebChannel C++ <-> DOM contract
│   ├── core/            # DocumentSession, live reload, clipboard, silent updater, provenance
│   ├── parser/          # Markdown heading & Table-of-Contents slug parser
│   └── ui/              # InstantPreviewSurface, PreviewView, EditorView, TOC, platform chrome
├── web/                 # Offline Markdown-It + KaTeX + Mermaid renderer & 100%-covered test suite
├── resources/           # Obsidian QSS theme, icons, showcase document, Qt resource manifests
├── scripts/             # Startup latency benchmarks, icon pipeline, and macOS release bundler
└── tests/               # 20 headless C++20 BDD test suites
```

## License

Released under the [GNU General Public License v3.0 (GPL-3.0)](LICENSE). Copyright © 2026 Raj Aramudan.

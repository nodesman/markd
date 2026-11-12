<p align="center">
  <img src="resources/branding/app_icon.png" alt="Markd Icon" width="112" height="112" />
</p>

<h1 align="center">Markd</h1>

<p align="center">
  <strong>Fast, native C++20 &amp; Qt 6 Markdown reader and editor for macOS, Windows, and Linux.</strong><br/>
  Opens technical specs, RFCs, equations, and architecture diagrams in under <strong>180 ms</strong> — zero Electron, zero cloud accounts, zero telemetry.
</p>

<p align="center">
  <a href="https://markd.aramudan.dev"><strong>Website</strong></a> ·
  <a href="#installation"><strong>Install</strong></a> ·
  <a href="#features"><strong>Features</strong></a> ·
  <a href="LICENSE"><strong>GPL-3.0 License</strong></a>
</p>

---

<p align="center">
  <img src="docs/assets/markd-preview.png" alt="Markd Preview Mode on macOS" width="880" />
</p>

---

## Features

* **Instant Cold Start (`< 180 ms`):** Opens `.md` documents immediately from Finder, Explorer, or your terminal (`markd README.md`) with a native two-stage rendering pipeline — no multi-second Chromium splash wait.
* **Dual `Preview` & `Edit` Modes (`Cmd+E` / `Ctrl+E`):** Toggle effortlessly between distraction-free Obsidian-dark editorial typography and a clean, line-numbered monospace Markdown editor.
* **100% Offline Math & Diagrams:** Built-in **KaTeX** typesetting (`$...$` and `$$...$$` equations) and **Mermaid** flowcharts, sequence diagrams, and state machines with interactive pan and `0.2x–5.0x` zoom controls — zero network or CDN calls.
* **Code Presentation Cards:** **Highlight.js** syntax highlighting wrapped in structured code cards with language badges and one-click Copy-to-Clipboard.
* **Collapsible Table of Contents (`Cmd+\`) & Live Telemetry:** Synchronized document outline sidebar alongside a subtle bottom status bar displaying word count, reading time, active heading (`§`), character encoding, and zoom scale.
* **Live Disk Hot-Reload:** `QFileSystemWatcher` automatically re-renders open files the instant your external IDE, CLI script, or coding agent writes to disk while preserving your scroll position.
* **Single-Instance Multi-Window Workflow (`Cmd+\``):** Opening files from the shell or file manager reuses the running process over a `< 60 ms` local IPC socket, raising already-open documents instead of spawning duplicate processes or browser tabs.
* **In-Page Find & Rich Export:** Floating `Cmd+F` search bar across both Preview and Edit modes, **Export to Standalone HTML** (`Cmd+Shift+E`), **Print / Save to PDF** (`Cmd+P`), and **Copy as Rich HTML** (`Cmd+Shift+C`).
* **Automatic `.md` File Association:** Native OS file-type registration (`markd --register-default` or `File -> Make Default for Markdown`) binds `.md`, `.markdown`, `.mdown`, `.mkd`, and `markd://open?path=...` links across macOS LaunchServices, Windows Registry, and Linux `xdg-mime`.

---

## Installation

### macOS (Apple Silicon) with Homebrew
```sh
brew install nodesman/markd/markd
```
Upgrade with `brew update && brew upgrade markd`. Downloads (`.dmg`, `.tar.gz`) are on the [releases page](https://markd.aramudan.dev/releases).

Windows, Linux and npm packages are in development.

---

## License

Released under the [GNU General Public License v3.0 (GPL-3.0)](LICENSE). Copyright © 2026 Raj Aramudan.

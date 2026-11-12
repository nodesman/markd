<p align="center">
  <img src="resources/branding/app_icon.png" alt="Markd Icon" width="112" height="112" />
</p>

<h1 align="center">Markd</h1>

<p align="center">
  <strong>A fast, native Markdown reader and editor for the Mac.</strong><br/>
  Type <code>markd README.md</code> and read. No Electron, no account.
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

* **Opens fast.** Run `markd README.md` in your terminal, or double-click a file in Finder. The document opens in its own window.
* **Read or edit.** Switch between the formatted page and the plain text with ⌃E.
* **Math and diagrams.** Equations (KaTeX) and diagrams (Mermaid) appear as you'd expect, even offline. You can zoom and pan around large diagrams.
* **Clear code.** Code blocks are colored by language and have a Copy button.
* **Find your way.** An outline of your headings sits in a sidebar (⌃\\). The status bar shows the word count, reading time and the section you're in.
* **Stays current.** When another program changes the file, Markd reloads it and keeps your place.
* **One app, many windows.** Each file gets its own window. Open a file that's already open and Markd brings its window to the front.
* **Find, share, print.** Search the page (⌘F), copy it as rich text for email or docs (⌃⌥C), save it as a single HTML file (⇧⌃E), or print it (⌘P).
* **Make it the default.** Run `markd --register-default` to open `.md` files with Markd.

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

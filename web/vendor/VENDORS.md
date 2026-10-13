# Third-Party Vendor Governance Ledger

This directory (`web/vendor/`) contains minified third-party JavaScript libraries, CSS stylesheets, and binary WOFF2 font files utilized by Markd's WebEngine frontend presentation engine. All libraries are vendored using the **Package-by-Library** organization.

| Package / Component | Version | License | Upstream Origin / Repository | Target Directory | File Count & Payloads |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **KaTeX Core** | 0.16.8 | MIT | https://github.com/KaTeX/KaTeX | `web/vendor/katex/` | 2 files (`katex.min.js`, `katex.min.css`) |
| **KaTeX Fonts** | 0.16.8 | SIL OFL 1.1 | https://github.com/KaTeX/KaTeX | `web/vendor/katex/fonts/` | 20 files (`KaTeX_*.woff2`) |
| **Mermaid** | 10.9.1 | MIT | https://github.com/mermaid-js/mermaid | `web/vendor/mermaid/` | 1 file (`mermaid.min.js`) |
| **Highlight.js** | 11.9.0 | BSD-3-Clause | https://github.com/highlightjs/highlight.js | `web/vendor/highlight/` | 2 files (`highlight.min.js`, `github-dark.min.css`) |
| **markdown-it** | 13.0.2 | MIT | https://github.com/markdown-it/markdown-it | `web/vendor/markdown-it/` | 1 file (`markdown-it.min.js`) |
| **markdown-it-anchor** | 8.6.7 | MIT | https://github.com/valeriangalliat/markdown-it-anchor | `web/vendor/markdown-it/` | 1 file (`markdownItAnchor.umd.js`) |
| **markdown-it-task-lists** | 2.1.1 | ISC | https://github.com/revin/markdown-it-task-lists | `web/vendor/markdown-it/` | 1 file (`markdown-it-task-lists.min.js`) |
| **markdown-it-texmath** | 1.0.0 | MIT | https://github.com/goessner/markdown-it-texmath | `web/vendor/markdown-it/` | 1 file (`texmath.min.js`) |
| **DOMPurify** | 3.0.8 | Apache-2.0 / MPL-2.0 | https://github.com/cure53/DOMPurify | `web/vendor/purify/` | 1 file (`purify.min.js`) |
| **Qt WebChannel** | Qt 6.5.3 | LGPLv3 / BSD | https://code.qt.io/cgit/qt/qtwebchannel.git | `web/vendor/qt/` | 1 file (`qwebchannel.js`) |

*Total Vendored Payload Files*: Exactly 31 files (22 KaTeX + 1 Mermaid + 2 Highlight + 4 Markdown-It + 1 Purify + 1 Qt).

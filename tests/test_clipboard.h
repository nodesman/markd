#pragma once

#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QAction>
#include <QTextCursor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <cassert>
#include "Zinsser.h"
#include "core/Converter.h"
#include "core/HtmlClipboardProvider.h"
#include "ui/MainWindow.h"
#include "ui/StatusBarTelemetry.h"
#include "ui/PreviewView.h"

inline void testClipboardProvider(QApplication& app) {
    using Zinsser::describe;
    using Zinsser::it;

    describe("Headless HtmlClipboardProvider, dual MIME formatting, and race-free action routing", [&]() {
        it("extracts clean text plain and styled text html", [&]() {
            QMimeData* mime = HtmlClipboardProvider::createFromMarkdown("# Hello\n\nWorld");
            assert(mime != nullptr);
            assert(mime->hasText());
            assert(mime->hasHtml());
            assert(mime->text() == "# Hello\n\nWorld");
            assert(!mime->text().contains("<"));
            assert(!mime->text().contains(">"));
            assert(mime->html().contains("Hello"));
            assert(mime->html().contains("World"));
            delete mime;
        });

        it("strips interactive copy buttons and headers from html", [&]() {
            QString dirtyHtml = 
                "<div class=\"code-block-card\">"
                "<div class=\"code-block-header\"><span class=\"code-lang\">rust</span><button class=\"copy-btn\">Copy</button></div>"
                "<pre><code>fn main() {}</code></pre>"
                "</div>"
                "<div class=\"expandable-toolbar\"><button class=\"table-expand-btn\">Expand</button></div>"
                "<script>alert('xss');</script>";
            QString clean = HtmlClipboardProvider::sanitizeHtml(dirtyHtml);
            assert(!clean.contains("code-block-header"));
            assert(!clean.contains("copy-btn"));
            assert(!clean.contains("table-expand-btn"));
            assert(!clean.contains("expandable-toolbar"));
            assert(!clean.contains("<script"));
            assert(clean.contains("<pre>"));
            assert(clean.contains("<code>fn main() {}</code>"));
        });

        it("inlines code card styles with dark palette tokens", [&]() {
            QString input = "<pre><code>test</code></pre><p><code>inline</code></p>";
            QString styled = HtmlClipboardProvider::inlineStyles(input);
            assert(styled.contains("background: #0d1117"));
            assert(styled.contains("border: 1px solid #1e242c"));
            assert(styled.contains("color: #f0f6fc"));
            assert(styled.contains("background: #161b22"));
            assert(styled.contains("border: 1px solid #30363d"));
        });

        it("inlines table border and background styles", [&]() {
            QString input = "<table><tr><th>Header</th></tr><tr><td>Data</td></tr></table>";
            QString styled = HtmlClipboardProvider::inlineStyles(input);
            assert(styled.contains("border-collapse: collapse"));
            assert(styled.contains("background: #161b22"));
            assert(styled.contains("border: 1px solid #30363d"));
            assert(styled.contains("color: #c9d1d9"));
        });

        it("merges aligned table styles strips katex mathml and inlines syntax tokens in clipboard html", [&]() {
            const QRegularExpression duplicateStyleRegex("<[^>]+\\bstyle=[\"'][^\"']*[\"'][^>]+\\bstyle=");

            QMimeData* mdMime = HtmlClipboardProvider::createFromMarkdown(
                "| Centered | Right |\n|:---:|---:|\n| `pill` | val |\n\n```rust\nfn main() {}\n```");
            assert(mdMime != nullptr);
            const QString mdHtml = mdMime->html();
            assert(!duplicateStyleRegex.match(mdHtml).hasMatch());
            assert(mdHtml.contains("border: 1px solid #30363d"));
            assert(mdHtml.contains("background: #0d1117"));
            delete mdMime;

            const QString previewDom =
                "<article id=\"content\" class=\"markdown-rendered\">"
                "<table><thead><tr><th style=\"text-align:center\">Centered</th><th style=\"text-align:right\">Right</th></tr></thead>"
                "<tbody><tr><td style=\"text-align:center\"><code>inline_pill</code></td><td/></tr></tbody></table>"
                "<div class=\"code-block-card\">"
                "<div class=\"code-block-header\"><span class=\"code-lang\">rust</span><button class=\"copy-btn\">Copy</button></div>"
                "<pre class=\"hljs\"><code style=\"white-space: pre-wrap;\"><span class=\"hljs-keyword\">fn</span> <span class=\"hljs-title\">main</span>() {}</code></pre>"
                "</div>"
                "<div class=\"expandable-toolbar\"/>"
                "<button class=\"custom-copy-btn\">KeepButton</button>"
                "<p><span class=\"katex\"><span class=\"katex-mathml\"><math>E=mc^2</math></span><span class=\"katex-html\">E=mc^2</span></span></p>"
                "<div class=\"fullview-overlay\"><div class=\"fullview-header\"><div class=\"fullview-toolbar\"><button class=\"close-btn\">✕</button></div></div><div class=\"fullview-body\">Modal</div></div>"
                "</article>";

            QMimeData* previewMime = HtmlClipboardProvider::createFromPreviewHtml(previewDom, "fallback");
            assert(previewMime != nullptr);
            const QString clipHtml = previewMime->html();
            assert(!duplicateStyleRegex.match(clipHtml).hasMatch());
            assert(clipHtml.contains("text-align:center"));
            assert(clipHtml.contains("text-align:right"));
            assert(clipHtml.contains("background: #161b22"));
            assert(clipHtml.contains("white-space: pre-wrap;"));
            assert(clipHtml.contains("KeepButton"));
            assert(!clipHtml.contains("<td/ "));
            assert(!clipHtml.contains("katex-mathml"));
            assert(clipHtml.contains("katex-html"));
            assert(!clipHtml.contains("fullview-overlay"));
            assert(!clipHtml.contains("fullview-toolbar"));
            assert(!clipHtml.contains("close-btn"));
            assert(!clipHtml.contains("code-block-header"));
            assert(clipHtml.contains("color: #ff7b72"));
            assert(clipHtml.contains("color: #d2a8ff"));

            const QRegularExpression preInnerCodePillRegex("<pre\\b[^>]*>\\s*<code\\b[^>]*1px solid #30363d");
            assert(!preInnerCodePillRegex.match(clipHtml).hasMatch());
            assert(clipHtml.contains("inline_pill"));
            delete previewMime;
        });

        it("normalizes unicode paragraph separators to linebreaks", [&]() {
            QString raw = QString("Heading") + QChar(0x2029) + QString("Body") + QChar(0x2028) + QString("Sub");
            QString normalized = HtmlClipboardProvider::normalizeSelection(raw);
            assert(normalized == "Heading\nBody\nSub");
        });

        it("converts selected markdown snippet in edit mode", [&]() {
            MainWindow win;
            win.resize(900, 650);
            win.show();
            app.processEvents();

            QString fullText = "# Full Doc\n\n## Subclause\nSelected text only\n\nMore text";
            win.editor()->setPlainText(fullText);
            QString target = "## Subclause\nSelected text only";
            int start = fullText.indexOf(target);
            QTextCursor cur = win.editor()->textCursor();
            cur.setPosition(start);
            cur.setPosition(start + target.length(), QTextCursor::KeepAnchor);
            win.editor()->setTextCursor(cur);
            assert(win.editor()->textCursor().hasSelection());

            win.copyEditorHtmlToClipboard();
            app.processEvents();

            QClipboard* cb = QApplication::clipboard();
            assert(cb != nullptr);
            const QMimeData* mime = cb->mimeData();
            assert(mime != nullptr);
            assert(mime->hasText());
            assert(mime->text() == "## Subclause\nSelected text only");
            assert(mime->hasHtml());
            assert(!mime->text().contains("# Full Doc"));
        });

        it("falls back to full document when selection is empty", [&]() {
            MainWindow win;
            win.resize(900, 650);
            win.show();
            app.processEvents();

            win.editor()->setPlainText("# Complete Buffer\nParagraph line");
            QTextCursor cur = win.editor()->textCursor();
            cur.clearSelection();
            win.editor()->setTextCursor(cur);
            assert(!win.editor()->textCursor().hasSelection());

            win.copyEditorHtmlToClipboard();
            app.processEvents();

            QClipboard* cb = QApplication::clipboard();
            assert(cb != nullptr);
            const QMimeData* mime = cb->mimeData();
            assert(mime != nullptr);
            assert(mime->hasText());
            assert(mime->text() == "# Complete Buffer\nParagraph line");
            assert(mime->hasHtml());
        });

        it("flashes copied as html transient status notification", [&]() {
            MainWindow win;
            win.resize(900, 650);
            win.show();
            app.processEvents();

            win.editor()->setPlainText("Quick telemetry check");
            win.copyHtmlToClipboard();
            app.processEvents();

            assert(win.statusBar() != nullptr);
            assert(win.statusBar()->notificationLabel() != nullptr);
            assert(win.statusBar()->notificationLabel()->text() == "Copied as HTML");
            assert(win.statusBar()->notificationLabel()->isVisible());
        });

        it("discards pending preview clipboard payload when switching to edit mode", [&]() {
            MainWindow win;
            win.resize(900, 650);
            win.show();
            app.processEvents();

            win.setMode(MainWindow::Mode::Preview);
            app.processEvents();

            win.copyPreviewHtmlToClipboard();
            const uint64_t previewGen = win.clipboardOpGeneration();

            win.setMode(MainWindow::Mode::Edit);
            app.processEvents();

            assert(win.clipboardOpGeneration() > previewGen);
            assert(!win.isCopyingHtml());

            win.editor()->setPlainText("# Protected Edit Content");
            win.copyEditorHtmlToClipboard();
            app.processEvents();

            QClipboard* cb = QApplication::clipboard();
            assert(cb != nullptr);
            assert(cb->text() == "# Protected Edit Content");
            app.processEvents();
            assert(cb->text() == "# Protected Edit Content");
        });

        it("routes copy and select all actions to web engine in preview mode", [&]() {
            MainWindow win;
            win.resize(900, 650);
            win.show();
            app.processEvents();

            win.editor()->setPlainText("Editor Specific Text");
            QTextCursor cur = win.editor()->textCursor();
            cur.select(QTextCursor::Document);
            win.editor()->setTextCursor(cur);

            QAction* copyAct = win.findChild<QAction*>("action_copy");
            assert(copyAct != nullptr);

            win.setMode(MainWindow::Mode::Edit);
            app.processEvents();
            copyAct->trigger();
            app.processEvents();
            assert(QApplication::clipboard()->text() == "Editor Specific Text");

            win.setMode(MainWindow::Mode::Preview);
            app.processEvents();
            QApplication::clipboard()->clear();

            win.editor()->setPlainText("Mutated Text");
            cur = win.editor()->textCursor();
            cur.select(QTextCursor::Document);
            win.editor()->setTextCursor(cur);

            copyAct->trigger();
            app.processEvents();
            assert(QApplication::clipboard()->text().isEmpty());
        });

        it("ignores redundant consecutive copy requests while clipboard operation is in flight", [&]() {
            MainWindow win;
            win.resize(900, 650);
            win.show();
            app.processEvents();

            win.setIsCopyingHtml(true);
            const uint64_t genBefore = win.clipboardOpGeneration();
            bool callbackInvoked = false;
            bool callbackResult = true;

            win.copyHtmlToClipboard([&](bool success) {
                callbackInvoked = true;
                callbackResult = success;
            });
            app.processEvents();

            assert(callbackInvoked);
            assert(!callbackResult);
            assert(win.clipboardOpGeneration() == genBefore);
            assert(win.isCopyingHtml());
            win.setIsCopyingHtml(false);
        });

        it("handles window teardown safely without dereferencing null clipboard", [&]() {
            QMimeData* data = HtmlClipboardProvider::createFromMarkdown("# Teardown Test");
            assert(data != nullptr);
            delete data;

            QMimeData* previewData = HtmlClipboardProvider::createFromPreviewHtml("<p>Hello</p>", "Hello");
            assert(previewData != nullptr);
            delete previewData;

            PreviewView detachedView;
            detachedView.toHtml(nullptr);

            {
                MainWindow win;
                win.copyEditorHtmlToClipboard();
                win.copyPreviewHtmlToClipboard();
            }
            app.processEvents();
        });
    });
}


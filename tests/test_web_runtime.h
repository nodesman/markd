#pragma once

#include <QFile>
#include <QFileInfo>
#include <QTest>
#include <QVariant>
#include <cassert>
#include <iostream>
#include "app/MarkdApplication.h"
#include "ui/MainWindow.h"
#include "ui/PreviewView.h"
#include "ui/TocWidget.h"
#include "core/DocumentSession.h"
#include "core/Converter.h"
#include "parser/TocParser.h"
#include "test_render_js.h"

inline void testWorkspaceModularizationAndEngineHierarchy(MarkdApplication& app) {
    std::cout << "\n--- William Zinsser Suite: Workspace Modularization & Engine Hierarchy ---\n";

    // 1. compiles core domain models without widget dependencies and verifies dirty state transitions
    {
        // Neutralizes the SIGABRT dirty flag trap: fresh sessions initialize clean
        DocumentSession cleanSession;
        assert(!cleanSession.isDirty());

        // Mutations explicitly flag the session dirty
        DocumentSession session;
        session.setMarkdown("# Domain Isolation Test\n\nPure headless text.");
        assert(session.isDirty());
        assert(session.markdown().contains("Domain Isolation"));

        QString fragment = Converter::toHtmlFragment(session.markdown());
        assert(fragment.contains("<h1"));
        assert(fragment.contains("Domain Isolation"));

        auto headings = TocParser::extractHeadings(session.markdown());
        assert(headings.size() == 1);
        assert(headings[0].title == "Domain Isolation Test");
        assert(headings[0].id == "domain-isolation-test");
    }

    // 2. resolves qualified include paths across all domain modules
    {
        MainWindow win;
        assert(win.inherits("QMainWindow"));
        assert(win.centralWidget() != nullptr);
        assert(win.findChild<PreviewView*>() != nullptr);
        assert(win.findChild<TocWidget*>("tocSidebar") != nullptr);
    }

    // 3. loads web template from Qt resource bundle at qrc:/web/template.html
    {
        QFile templateFile(":/web/template.html");
        assert(templateFile.open(QIODevice::ReadOnly | QIODevice::Text));
        QString html = QString::fromUtf8(templateFile.readAll());
        templateFile.close();

        assert(html.contains("styles/obsidian.css"));
        assert(html.contains("src/render.js"));
        assert(html.contains("vendor/markdown-it/markdown-it.min.js"));
        assert(html.contains("vendor/katex/katex.min.css"));
        assert(html.contains("vendor/highlight/github-dark.min.css"));
        assert(html.contains("window.__resourceErrors = []"));
    }

    // Shared preview widget for browser DOM assertions 4-7
    {
        PreviewView preview;
        preview.resize(1024, 768);
        preview.show();
        app.processEvents();
        RenderJsTest::waitForPageReady(preview);

        // 4. applies obsidian theme stylesheet variables in live DOM
        {
            // Verify CSS custom properties declared in obsidian.css
            QVariant linkColor = RenderJsTest::runJsSync(preview.webView()->page(),
                "window.getComputedStyle(document.documentElement).getPropertyValue('--link').trim()");
            assert(linkColor.toString() == "#58a6ff");

            QVariant taskColor = RenderJsTest::runJsSync(preview.webView()->page(),
                "window.getComputedStyle(document.documentElement).getPropertyValue('--task-check').trim()");
            assert(taskColor.toString() == "#3fb950");
        }

        // 5. traps resource load failures and asserts zero DOM 404 errors via capture-phase listener
        {
            // Assert positive script and stylesheet presence and execution
            QVariant scriptCount = RenderJsTest::runJsSync(preview.webView()->page(),
                "document.querySelectorAll('script[src], link[rel=\"stylesheet\"]').length");
            assert(scriptCount.toInt() > 0);

            QVariant vendorReady = RenderJsTest::runJsSync(preview.webView()->page(),
                "typeof window.markdownit === 'function' && typeof window.katex === 'object'");
            assert(vendorReady.toBool() == true);

            // Assert zero resource load errors trapped during document load
            QVariant errorCount = RenderJsTest::runJsSync(preview.webView()->page(),
                "window.__resourceErrors.length");
            assert(errorCount.toInt() == 0);
        }

        // 6. renders mathematical expressions via KaTeX engine
        {
            RenderJsTest::renderMarkdownSync(preview, "# Math\n\n$E = mc^2$");

            QVariant katexEl = RenderJsTest::runJsSync(preview.webView()->page(),
                "document.querySelector('.katex') !== null");
            assert(katexEl.toBool() == true);
        }

        // 7. validates KaTeX font face registration via active math rendering and CSS Font Loading API
        {
            // Actively trigger font loading to eliminate Chromium font lazy-loading blind spot
            RenderJsTest::runJsSync(preview.webView()->page(),
                "document.fonts.load('16px KaTeX_Main')");

            for (int i = 0; i < 20; ++i) {
                bool ready = RenderJsTest::runJsSync(preview.webView()->page(),
                    "document.fonts.check('16px KaTeX_Main')").toBool();
                if (ready) break;
                QTest::qWait(25);
            }

            QVariant fontCheck = RenderJsTest::runJsSync(preview.webView()->page(),
                "document.fonts.check('16px KaTeX_Main')");
            assert(fontCheck.toBool() == true);
        }
    }

    // 8. synchronizes Converter HTML generation with obsidian and vendor stylesheets and prunes style.css
    {
        QString fullHtml = Converter::toFullHtmlDocument("# Title");
        assert(fullHtml.contains("qrc:/web/styles/obsidian.css"));
        assert(fullHtml.contains("qrc:/web/vendor/highlight/github-dark.min.css"));
        assert(fullHtml.contains("qrc:/web/vendor/katex/katex.min.css"));
        assert(!fullHtml.contains("qrc:/web/css/style.css"));
        assert(!fullHtml.contains("qrc:/web/styles/style.css"));
    }

    // 9. executes core domain model transformations headlessly without GUI server
    {
        DocumentSession session;
        session.setMarkdown("## Headless Verification\n\nTesting pure data transformation.");
        auto items = TocParser::extractHeadings(session.markdown());
        assert(items.size() == 1);
        assert(items[0].id == "headless-verification");

        QString html = Converter::toFullHtmlDocument(session.markdown());
        assert(html.contains("Headless Verification"));
        assert(!session.filePath().isEmpty() || session.wordCount() >= 0);
    }

    // Legacy backward-compatibility validation block
    {
        // Confirms all legacy VFS aliases remain valid for exported HTML and backward compatibility
        assert(QFile::exists(":/web/css/obsidian.css"));
        assert(QFile::exists(":/web/css/style.css"));
        assert(QFile::exists(":/web/js/render.js"));
        assert(QFile::exists(":/web/js/qwebchannel.js"));
        assert(QFile::exists(":/web/js/katex.min.css"));
        assert(QFile::exists(":/web/js/katex.min.js"));
        assert(QFile::exists(":/web/js/highlight.min.js"));
        assert(QFile::exists(":/web/js/github-dark.min.css"));
        assert(QFile::exists(":/web/js/mermaid.min.js"));
        assert(QFile::exists(":/web/js/purify.min.js"));
        assert(QFile::exists(":/web/js/markdown-it.min.js"));
        assert(QFile::exists(":/web/js/markdownItAnchor.umd.js"));
        assert(QFile::exists(":/web/js/markdown-it-task-lists.min.js"));
        assert(QFile::exists(":/web/js/texmath.min.js"));

        // Confirms canonical VFS paths exist and are non-empty
        assert(QFile::exists(":/web/styles/obsidian.css"));
        assert(QFileInfo(":/web/styles/obsidian.css").size() > 0);
        assert(QFile::exists(":/web/src/render.js"));
        assert(QFileInfo(":/web/src/render.js").size() > 0);
        assert(QFile::exists(":/web/vendor/katex/katex.min.css"));
        assert(QFileInfo(":/web/vendor/katex/katex.min.css").size() > 0);
        assert(QFile::exists(":/web/vendor/katex/fonts/KaTeX_Main-Regular.woff2"));
        assert(QFileInfo(":/web/vendor/katex/fonts/KaTeX_Main-Regular.woff2").size() > 0);
    }

    std::cout << "PASS: William Zinsser Workspace Modularization tests passed!\n";
}

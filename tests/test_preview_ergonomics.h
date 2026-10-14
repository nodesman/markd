#pragma once

#include <QApplication>
#include <QFile>
#include <QTemporaryFile>
#include <QDir>
#include <cassert>
#include <iostream>
#include "Zinsser.h"
#include "core/Converter.h"
#include "ui/MainWindow.h"
#include "ui/ModeSwitcher.h"
#include "ui/PreviewView.h"

inline void testPreviewEditorialErgonomics(QApplication& app) {
    using Zinsser::describe;
    using Zinsser::it;

    QFile cssFile(":/web/css/obsidian.css");
    assert(cssFile.open(QIODevice::ReadOnly | QIODevice::Text));
    QString css = QString::fromUtf8(cssFile.readAll());
    cssFile.close();
    assert(!css.isEmpty());

    QFile jsFile(":/web/js/render.js");
    assert(jsFile.open(QIODevice::ReadOnly | QIODevice::Text));
    QString js = QString::fromUtf8(jsFile.readAll());
    jsFile.close();
    assert(!js.isEmpty());

    describe("Mermaid diagram viewport alignment and zoom geometry", [&]() {
        it("enforces text-center alignment and prohibits flexbox clipping on viewport container", [&]() {
            assert(css.contains(".mermaid-viewport"));
            assert(css.contains("text-align: center;"));
            assert(!css.contains(".mermaid-viewport {\n    justify-content: center"));
            assert(!css.contains(".mermaid-viewport {\n    display: flex;\n    justify-content: center"));
        });

        it("anchors transform origin to top-center on inner container", [&]() {
            assert(css.contains(".mermaid-viewport .mermaid-inner"));
            assert(css.contains("transform-origin: top center;"));
            assert(css.contains("text-align: left;"));
        });

        it("centers svg element via automatic margins without maximum width constraints", [&]() {
            assert(css.contains(".mermaid svg"));
            assert(css.contains("margin: 0 auto;"));
            assert(css.contains("display: block;"));
            assert(css.contains("max-width: none;"));
        });

        it("enables block display overrides on fullview diagram modal", [&]() {
            assert(css.contains(".fullview-body.is-diagram"));
            assert(css.contains("display: block;"));
            assert(css.contains(".fullview-body.is-diagram .fullview-inner"));
        });
    });

    describe("Archival paper matte styling and contrast guards", [&]() {
        it("scopes paper matte styling to paragraph images while excluding badges and status indicators", [&]() {
            assert(!css.contains("#content img[src$=\".svg\"]"));
            assert(css.contains("#content p > img[src*=\".svg\" i]"));
            assert(css.contains("#content p > a > img[src*=\".svg\" i]"));
            assert(css.contains("#content .diagram-img"));
            assert(css.contains(".fullview-body img[src*=\".svg\" i]"));

            assert(css.contains("background-color: #f0f6fc;"));
            assert(css.contains("border: 1px solid #30363d;"));
            assert(css.contains("border-radius: 8px;"));
            assert(css.contains("padding: 16px;"));
            assert(css.contains("margin: 16px auto;"));
            assert(css.contains("max-width: 90%;"));
            assert(css.contains("box-sizing: border-box;"));

            assert(css.contains(":not([src*=\"badge\"])"));
            assert(css.contains(":not([src*=\"shields.io\"])"));
            assert(css.contains(":not([src*=\"travis-ci\"])"));
            assert(css.contains(":not([src*=\"codecov\"])"));
            assert(css.contains(":not([class*=\"badge\"])"));
        });

        it("applies universal contrast drop-shadow filter to transparent png diagrams", [&]() {
            assert(css.contains("#content img.diagram-png"));
            assert(css.contains("#content img[data-diagram=\"true\"]"));
            assert(css.contains("#content img[alt*=\"diagram\" i][src$=\".png\" i]"));
            assert(css.contains("#content img[alt*=\"architecture\" i][src$=\".png\" i]"));
            assert(css.contains("#content img[src*=\"diagram\" i][src$=\".png\" i]"));
            assert(css.contains("filter: drop-shadow(0 0 1px rgba(255, 255, 255, 0.45));"));
        });

        it("protects standard photo images from paper matte border and background styles", [&]() {
            int imgIdx = css.indexOf("img {");
            assert(imgIdx != -1);
            int imgClose = css.indexOf("}", imgIdx);
            assert(imgClose != -1);
            QString defaultImgRule = css.mid(imgIdx, imgClose - imgIdx + 1);
            assert(!defaultImgRule.contains("#f0f6fc"));
            assert(!defaultImgRule.contains("padding: 16px"));
            assert(!defaultImgRule.contains("#30363d"));
        });

        it("enforces transparent backgrounds and avoids page breaks in print media queries", [&]() {
            assert(css.contains("@media print"));
            int printIdx = css.indexOf("@media print");
            assert(printIdx != -1);
            QString printBlock = css.mid(printIdx);
            assert(printBlock.contains("background-color: transparent !important;"));
            assert(printBlock.contains("border: 1px solid #d0d7de !important;"));
            assert(printBlock.contains(":not([src*=\"badge\"])"));
            assert(printBlock.contains(":not([src*=\"shields.io\"])"));
            assert(printBlock.contains("transform: none !important;"));
            assert(printBlock.contains("overflow: visible !important;"));
            assert(printBlock.contains("break-inside: avoid;"));
            assert(printBlock.contains("page-break-inside: avoid;"));
        });
    });

    describe("Render script transforms and preview document lifecycle", [&]() {
        it("provides diagram zoom-pan transform math and cleans up modal event listeners", [&]() {
            assert(js.contains("setupDiagramZoomPan"));
            assert(js.contains("openFullView"));
            assert(js.contains("applyTransform"));
            assert(js.contains("fvApply"));
            assert(!js.contains("style.transformOrigin"));
            assert(js.contains("contentNode.style.transform = '';"));
            assert(js.contains("document.removeEventListener('mousemove', fvMouseMoveHandler);"));
            assert(js.contains("document.removeEventListener('mouseup', fvMouseUpHandler);"));
            assert(js.contains("fvApply(false);"));
        });

        it("publishes window Markd engine API and purges legacy DOM search bar", [&]() {
            assert(js.contains("window.Markd ="));
            assert(js.contains("activeFullViewClose"));
            assert(js.contains("closeFullView"));
            assert(!js.contains("markd-find-bar"));
            assert(!js.contains("extractAndSendTOC"));
        });

        it("extracts mermaid code blocks and compiles mixed media documents", [&]() {
            assert(js.contains("language-mermaid"));
            assert(js.contains("wrapMermaidDiagrams"));
            assert(js.contains("mermaid-viewport"));
            assert(js.contains("mermaid-inner"));

            QString mdContent =
                "# Architecture Monograph\n\n"
                "```mermaid\ngraph TD\n    A[Client] --> B[Edge API]\n```\n\n"
                "![System Schematic](assets/architecture.svg)\n\n"
                "[![CI Status](https://img.shields.io/badge/build-passing.svg)](https://ci.example.com)\n\n"
                "![Dark Mode Architecture Diagram](assets/system_diagram.png)\n\n"
                "![Standard Photo](assets/photo.jpg)\n";

            QString htmlFragment = Converter::toHtmlFragment(mdContent);
            assert(htmlFragment.contains("Architecture Monograph"));
            assert(htmlFragment.contains("architecture.svg"));
            assert(htmlFragment.contains("build-passing.svg"));
            assert(htmlFragment.contains("system_diagram.png"));
            assert(htmlFragment.contains("photo.jpg"));

            QString fullHtml = Converter::toFullHtmlDocument(mdContent);
            assert(fullHtml.contains("qrc:/web/styles/obsidian.css"));
            assert(fullHtml.contains("id=\"content\""));
        });

        it("renders mixed media document in preview and preserves mode transitions", [&]() {
            QString mdContent =
                "# Architecture Monograph\n\n"
                "```mermaid\ngraph TD\n    A[Client] --> B[Edge API]\n```\n\n"
                "![System Schematic](assets/architecture.svg)\n\n"
                "[![CI Status](https://img.shields.io/badge/build-passing.svg)](https://ci.example.com)\n\n"
                "![Dark Mode Architecture Diagram](assets/system_diagram.png)\n\n"
                "![Standard Photo](assets/photo.jpg)\n";

            MainWindow win;
            win.resize(1000, 650);
            win.show();
            win.activateWindow();
            app.processEvents();

            QTemporaryFile tempFile(QDir::tempPath() + "/markd_editorial_test_XXXXXX.md");
            tempFile.setAutoRemove(true);
            assert(tempFile.open());
            tempFile.write(mdContent.toUtf8());
            tempFile.close();

            win.loadFile(tempFile.fileName());
            app.processEvents();

            assert(win.previewView() != nullptr);
            assert(win.editor() != nullptr);

            win.modeSwitcher()->setMode(ModeSwitcher::DocumentMode::Preview);
            app.processEvents();
            assert(win.modeSwitcher()->currentMode() == ModeSwitcher::DocumentMode::Preview);

            win.modeSwitcher()->setMode(ModeSwitcher::DocumentMode::Edit);
            app.processEvents();
            assert(win.modeSwitcher()->currentMode() == ModeSwitcher::DocumentMode::Edit);
        });
    });
}

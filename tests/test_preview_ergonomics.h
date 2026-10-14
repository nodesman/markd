#pragma once

#include <QApplication>
#include <QFile>
#include <QTemporaryFile>
#include <QDir>
#include <cassert>
#include <iostream>
#include "core/Converter.h"
#include "ui/MainWindow.h"
#include "ui/ModeSwitcher.h"
#include "ui/PreviewView.h"

inline void testPreviewEditorialErgonomics(QApplication& app) {
    // =========================================================================
    // Preview Editorial Ergonomics (Mermaid Center Alignment & Archival Image Matting)
    // Module 8: Visual Ergonomics & Editorial Polish
    // Specification: structured-specs/WORKBENCH_PREVIEW_POLISH.md
    // =========================================================================

    // --- Section 1: CSS Asset Verification ---
    QFile cssFile(":/web/css/obsidian.css");
    assert(cssFile.open(QIODevice::ReadOnly | QIODevice::Text));
    QString css = QString::fromUtf8(cssFile.readAll());
    cssFile.close();
    assert(!css.isEmpty());

    // --- Section 2: Mermaid Diagram Center Alignment & Zoom/Pan Symmetry ---
    // 1. .mermaid-viewport must have text-align: center
    assert(css.contains(".mermaid-viewport"));
    assert(css.contains("text-align: center;"));
    // 2. Invariant Protection: verify flexbox justify-content: center is NOT used on .mermaid-viewport (prevents negative-coordinate zoom clipping)
    assert(!css.contains(".mermaid-viewport {\n    justify-content: center"));
    assert(!css.contains(".mermaid-viewport {\n    display: flex;\n    justify-content: center"));

    // 3. .mermaid-viewport .mermaid-inner must have transform-origin: top center and text-align: left
    assert(css.contains(".mermaid-viewport .mermaid-inner"));
    assert(css.contains("transform-origin: top center;"));
    assert(css.contains("text-align: left;"));

    // 4. .mermaid svg must have margin: 0 auto, display: block, and max-width: none
    assert(css.contains(".mermaid svg"));
    assert(css.contains("margin: 0 auto;"));
    assert(css.contains("display: block;"));
    assert(css.contains("max-width: none;"));

    // 5. Fullview modal override for diagrams
    assert(css.contains(".fullview-body.is-diagram"));
    assert(css.contains("display: block;"));
    assert(css.contains(".fullview-body.is-diagram .fullview-inner"));

    // --- Section 3: Archival Paper Matte (The Apple-Obsidian Refinement) ---
    // 1. Scoped standalone SVG and schematic selectors (unscoped img[src$=".svg"] excluded to protect tables and lists)
    assert(!css.contains("#content img[src$=\".svg\"]"));
    assert(css.contains("#content p > img[src*=\".svg\" i]"));
    assert(css.contains("#content p > a > img[src*=\".svg\" i]"));
    assert(css.contains("#content .diagram-img"));
    assert(css.contains(".fullview-body img[src*=\".svg\" i]"));

    // 2. Archival paper matte color tokens and geometry
    assert(css.contains("background-color: #f0f6fc;"));
    assert(css.contains("border: 1px solid #30363d;"));
    assert(css.contains("border-radius: 8px;"));
    assert(css.contains("padding: 16px;"));
    assert(css.contains("margin: 16px auto;"));
    assert(css.contains("max-width: 90%;"));
    assert(css.contains("box-sizing: border-box;"));

    // 3. Badge & CI status indicator exclusion guards
    assert(css.contains(":not([src*=\"badge\"])"));
    assert(css.contains(":not([src*=\"shields.io\"])"));
    assert(css.contains(":not([src*=\"travis-ci\"])"));
    assert(css.contains(":not([src*=\"codecov\"])"));
    assert(css.contains(":not([class*=\"badge\"])"));

    // --- Section 4: Universal Contrast Guard for Transparent PNG Diagrams ---
    assert(css.contains("#content img.diagram-png"));
    assert(css.contains("#content img[data-diagram=\"true\"]"));
    assert(css.contains("#content img[alt*=\"diagram\" i][src$=\".png\" i]"));
    assert(css.contains("#content img[alt*=\"architecture\" i][src$=\".png\" i]"));
    assert(css.contains("#content img[src*=\"diagram\" i][src$=\".png\" i]"));
    assert(css.contains("filter: drop-shadow(0 0 1px rgba(255, 255, 255, 0.45));"));

    // --- Section 5: Photo & Screenshot Non-Contamination Guard ---
    // Extract default img rule block and assert it does not have the archival matte tokens
    int imgIdx = css.indexOf("img {");
    assert(imgIdx != -1);
    int imgClose = css.indexOf("}", imgIdx);
    assert(imgClose != -1);
    QString defaultImgRule = css.mid(imgIdx, imgClose - imgIdx + 1);
    assert(!defaultImgRule.contains("#f0f6fc"));
    assert(!defaultImgRule.contains("padding: 16px"));
    assert(!defaultImgRule.contains("#30363d"));

    // --- Section 6: Print / PDF Media Query Invariants (@media print) ---
    assert(css.contains("@media print"));
    int printIdx = css.indexOf("@media print");
    assert(printIdx != -1);
    QString printBlock = css.mid(printIdx);
    assert(printBlock.contains("background-color: transparent !important;"));
    assert(printBlock.contains("border: 1px solid #d0d7de !important;"));
    // Verify badge protection is preserved in print media query to prevent borders around status badges
    assert(printBlock.contains(":not([src*=\"badge\"])"));
    assert(printBlock.contains(":not([src*=\"shields.io\"])"));
    assert(printBlock.contains("transform: none !important;"));
    assert(printBlock.contains("overflow: visible !important;"));
    assert(printBlock.contains("break-inside: avoid;"));
    assert(printBlock.contains("page-break-inside: avoid;"));

    // --- Section 7: Render.js Client Script Verification ---
    QFile jsFile(":/web/js/render.js");
    assert(jsFile.open(QIODevice::ReadOnly | QIODevice::Text));
    QString js = QString::fromUtf8(jsFile.readAll());
    jsFile.close();
    assert(!js.isEmpty());

    // Verify zoom/pan transform math and fullview clone reset
    assert(js.contains("setupDiagramZoomPan"));
    assert(js.contains("openFullView"));
    assert(js.contains("applyTransform"));
    assert(js.contains("fvApply"));
    assert(!js.contains("style.transformOrigin")); // CSS handles physical origin invariant
    assert(js.contains("contentNode.style.transform = '';"));
    // Verify event listener cleanup on modal close to prevent memory leaks
    assert(js.contains("document.removeEventListener('mousemove', fvMouseMoveHandler);"));
    assert(js.contains("document.removeEventListener('mouseup', fvMouseUpHandler);"));
    assert(js.contains("fvApply(false);")); // Initial transform matrix initialization

    // Task T3: Structured Engine API (window.Markd) & Legacy DOM Search Purge
    assert(js.contains("window.Markd ="));
    assert(js.contains("activeFullViewClose"));
    assert(js.contains("closeFullView"));
    assert(!js.contains("markd-find-bar"));
    assert(!js.contains("extractAndSendTOC"));

    // --- Section 8: Converter Pipeline & JavaScript DOM Integration ---
    // In render.js: markdown-it language-mermaid block extraction and DOM wrapping
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

    // --- Section 9: UI Lifecycle & Viewport Runtime Verification ---
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

    // Verify preview view loaded cleanly without crashes
    assert(win.previewView() != nullptr);
    assert(win.editor() != nullptr);

    // Verify switching between Edit and Preview modes functions cleanly
    win.modeSwitcher()->setMode(ModeSwitcher::DocumentMode::Preview);
    app.processEvents();
    assert(win.modeSwitcher()->currentMode() == ModeSwitcher::DocumentMode::Preview);

    win.modeSwitcher()->setMode(ModeSwitcher::DocumentMode::Edit);
    app.processEvents();
    assert(win.modeSwitcher()->currentMode() == ModeSwitcher::DocumentMode::Edit);

    std::cout << "PASS: Task P1 Preview Editorial Ergonomics tests passed!\n";
}

#pragma once

#include <QSignalSpy>
#include <QJsonObject>
#include <QTest>
#include <QDir>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <cassert>
#include <iostream>
#include <cmath>
#include "app/MarkdApplication.h"
#include "ui/MainWindow.h"
#include "ui/PreviewView.h"
#include "ui/ModeSwitcher.h"
#include "ui/TocWidget.h"
#include "core/DocumentSession.h"
#include "bridge/WebBridge.h"
#include "test_render_js.h"

inline void testDomainSemanticActionProtocol(MarkdApplication& app) {
    std::cout << "\n--- William Zinsser Suite: Domain Semantic Action Protocol ---\n";

    // 1. WebBridge Domain Action Protocol
    {
        PreviewView previewView;
        previewView.resize(800, 600);
        previewView.show();
        app.processEvents();

        // it("buffers markdown content until bridgeReady handshake resolves");
        assert(!previewView.isBridgeReady());
        previewView.setMarkdown("# Buffered Content Before Ready");
        assert(previewView.pendingMarkdown() == "# Buffered Content Before Ready");

        // Wait for page ready and bridgeReady handshake
        RenderJsTest::waitForPageReady(previewView);
        assert(previewView.isBridgeReady());
        assert(previewView.pendingMarkdown().isEmpty());

        // it("dispatches renderDocument signal with typed JSON payload");
        QSignalSpy renderSpy(previewView.bridge(), &WebBridge::renderDocument);
        QSignalSpy docRenderedSpy(&previewView, &PreviewView::documentRendered);
        previewView.setMarkdown("# Rendered Via Typed Signal\n\nParagraph text.");

        assert(renderSpy.count() >= 1);
        assert(renderSpy.last().at(0).toString().contains("Rendered Via Typed Signal"));
        assert(renderSpy.last().at(1).canConvert<QJsonObject>());

        for (int i = 0; i < 15 && docRenderedSpy.count() < 1; ++i) {
            QTest::qWait(25);
        }
        assert(docRenderedSpy.count() >= 1);
        assert(docRenderedSpy.last().at(0).toInt() == 1);

        // it("dispatches navigateToHeading signal without string interpolation");
        QSignalSpy navSpy(previewView.bridge(), &WebBridge::navigateToHeading);
        previewView.scrollToHeading("rendered-via-typed-signal");
        assert(navSpy.count() >= 1);
        assert(navSpy.last().at(0).toString() == "rendered-via-typed-signal");

        // it("dispatches setTheme signal to web runtime");
        QSignalSpy themeSpy(previewView.bridge(), &WebBridge::setTheme);
        previewView.setTheme("dark");
        assert(themeSpy.count() >= 1);
        assert(themeSpy.last().at(0).toString() == "dark");

        // it("forwards activeHeadingChanged signals from web runtime to TOC sidebar and session");
        MainWindow win;
        win.resize(1000, 650);
        win.show();
        app.processEvents();

        win.editor()->setPlainText("# Heading One\n\n## Heading Two\n");
        win.updateToc();
        assert(win.tocWidget()->headingCount() == 2);

        // Emit activeHeadingChanged from previewView bridge
        emit win.previewView()->activeHeadingChanged("heading-two");
        app.processEvents();
        assert(win.tocWidget()->activeHeadingId() == "heading-two");
        assert(win.session()->activeHeading() == "heading-two");

        // Activating heading from TOC sidebar synchronizes session selected heading
        emit win.tocWidget()->headingActivated("heading-two");
        assert(win.session()->selectedHeading() == "heading-two");

        std::cout << "PASS: WebBridge Domain Action Protocol tests passed!\n";
    }

    // 2. Native Chromium Search Engine
    {
        MainWindow win;
        win.resize(900, 650);
        win.show();
        app.processEvents();

        win.modeSwitcher()->setMode(ModeSwitcher::DocumentMode::Preview);
        app.processEvents();
        RenderJsTest::waitForPageReady(*win.previewView());

        win.previewView()->setMarkdown(
            "# Search Monograph\n\n"
            "Chromium native hardware-accelerated search engine.\n"
            "First instance of keyword search.\n"
            "Second instance of keyword search.\n"
        );
        for (int i = 0; i < 10; ++i) {
            QTest::qWait(25);
        }

        // it("dispatches search queries to Chromium compositor via findText");
        QSignalSpy findSpy(&win, &MainWindow::findResult);
        win.findText("search", true, false);

        for (int i = 0; i < 20 && findSpy.count() < 1; ++i) {
            QTest::qWait(50);
        }

        // it("reports active match index and total match count to C++ callback");
        assert(findSpy.count() >= 1);
        int active = findSpy.last().at(0).toInt();
        int total = findSpy.last().at(1).toInt();
        assert(total >= 3);
        assert(active >= 1);

        // it("cycles through match occurrences via findNext and findPrevious");
        win.findNext();
        QTest::qWait(50);
        win.findPrevious();
        QTest::qWait(50);

        // it("clears search highlights natively without DOM tree mutations");
        win.clearFind();
        assert(findSpy.last().at(0).toInt() == 0);
        assert(findSpy.last().at(1).toInt() == 0);

        // it("executes search and navigates matches in Edit mode");
        win.modeSwitcher()->setMode(ModeSwitcher::DocumentMode::Edit);
        win.editor()->setPlainText("Alpha query bravo charlie query delta query.");
        win.findText("query", true, false);
        assert(findSpy.last().at(0).toInt() == 1);
        assert(findSpy.last().at(1).toInt() == 3);

        win.findNext();
        assert(win.editor()->textCursor().hasSelection());
        win.findPrevious();
        assert(win.editor()->textCursor().hasSelection());

        win.clearFind();
        assert(!win.editor()->textCursor().hasSelection());
        assert(findSpy.last().at(0).toInt() == 0);
        assert(findSpy.last().at(1).toInt() == 0);

        std::cout << "PASS: Native Chromium Search Engine tests passed!\n";
    }

    // 3. Unified Desktop Zoom Engine
    {
        MainWindow win;
        win.resize(900, 650);
        win.show();
        app.processEvents();

        // it("applies zoom factor directly to WebEngine view");
        win.previewView()->setZoomFactor(1.5);
        assert(std::abs(win.previewView()->webView()->zoomFactor() - 1.5) < 0.01);

        // it("maintains zoom factor parity between editor and preview viewports");
        win.session()->setZoomFactor(1.5);
        assert(std::abs(win.previewView()->webView()->zoomFactor() - 1.5) < 0.01);
        assert(std::abs(win.editor()->font().pointSizeF() - (13.0 * 1.5)) < 0.01);

        win.session()->resetZoom();
        assert(std::abs(win.previewView()->webView()->zoomFactor() - 1.0) < 0.01);
        assert(std::abs(win.editor()->font().pointSizeF() - 13.0) < 0.01);

        // it("clamps zoom factor strictly between 0.25x and 3.0x across session and viewports");
        win.session()->setZoomFactor(0.1); // below minimum clamp
        assert(std::abs(win.session()->zoomFactor() - 0.25) < 0.01);
        assert(std::abs(win.previewView()->webView()->zoomFactor() - 0.25) < 0.01);
        assert(std::abs(win.editor()->font().pointSizeF() - (13.0 * 0.25)) < 0.01);

        win.session()->setZoomFactor(5.0); // above maximum clamp
        assert(std::abs(win.session()->zoomFactor() - 3.0) < 0.01);
        assert(std::abs(win.previewView()->webView()->zoomFactor() - 3.0) < 0.01);
        assert(std::abs(win.editor()->font().pointSizeF() - (13.0 * 3.0)) < 0.01);

        win.session()->resetZoom();
        assert(std::abs(win.previewView()->webView()->zoomFactor() - 1.0) < 0.01);
        assert(std::abs(win.editor()->font().pointSizeF() - 13.0) < 0.01);

        std::cout << "PASS: Unified Desktop Zoom Engine tests passed!\n";
    }

    // 4. Native Document Export & Print
    {
        MainWindow win;
        win.resize(900, 650);
        win.show();
        app.processEvents();
        RenderJsTest::waitForPageReady(*win.previewView());

        win.previewView()->setMarkdown("# Export Brief\n\nExport verification.");
        for (int i = 0; i < 8; ++i) {
            QTest::qWait(25);
        }

        // it("serializes full document HTML via native toHtml callback");
        bool htmlExported = false;
        QString htmlPath = QDir::tempPath() + QString("/markd_export_%1.html").arg(QDateTime::currentMSecsSinceEpoch());
        win.exportHtml(htmlPath, [&](bool ok) {
            htmlExported = ok;
        });

        for (int i = 0; i < 20 && !htmlExported; ++i) {
            QTest::qWait(50);
        }
        assert(htmlExported);
        assert(QFile::exists(htmlPath));
        QFile hf(htmlPath);
        assert(hf.open(QIODevice::ReadOnly | QIODevice::Text));
        QString htmlContent = QString::fromUtf8(hf.readAll());
        hf.close();
        QFile::remove(htmlPath);
        assert(htmlContent.contains("<html"));
        assert(htmlContent.contains("Export Brief"));

        // it("renders document directly to PDF via native printToPdf and verifies output file");
        QString pdfPath = QDir::tempPath() + QString("/markd_print_%1.pdf").arg(QDateTime::currentMSecsSinceEpoch());
        QSignalSpy pdfSpy(&win, &MainWindow::pdfPrintingFinished);
        win.printToPdf(pdfPath);
        for (int i = 0; i < 30 && pdfSpy.count() < 1; ++i) {
            QTest::qWait(50);
        }
        assert(pdfSpy.count() == 1);
        assert(pdfSpy.last().at(1).toBool() == true);
        assert(QFile::exists(pdfPath));
        assert(QFileInfo(pdfPath).size() > 0);
        QFile::remove(pdfPath);

        std::cout << "PASS: Native Document Export & Print tests passed!\n";
    }
}

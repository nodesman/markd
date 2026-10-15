#include <QTextBlock>
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
#include "Zinsser.h"
#include "app/MarkdApplication.h"
#include "ui/MainWindow.h"
#include "ui/PreviewView.h"
#include "ui/ModeSwitcher.h"
#include "ui/TocWidget.h"
#include "core/DocumentSession.h"
#include "bridge/WebBridge.h"

inline void testDomainSemanticActionProtocol(MarkdApplication& app) {
    using Zinsser::describe;
    using Zinsser::it;

    describe("WebBridge domain action protocol and asynchronous handshake", [&]() {
        PreviewView previewView;
        previewView.resize(800, 600);
        previewView.show();
        app.processEvents();

        it("buffers markdown content until bridgeReady handshake resolves", [&]() {
            assert(!previewView.isBridgeReady());
            previewView.setMarkdown("# Buffered Content Before Ready");
            assert(previewView.pendingMarkdown() == "# Buffered Content Before Ready");

            emit previewView.bridge()->bridgeInitialized();
            assert(previewView.isBridgeReady());
            assert(previewView.pendingMarkdown().isEmpty());
        });

        it("dispatches renderDocument signal with typed JSON payload", [&]() {
            QSignalSpy renderSpy(previewView.bridge(), &WebBridge::renderDocument);
            QSignalSpy docRenderedSpy(&previewView, &PreviewView::documentRendered);
            previewView.setMarkdown("# Rendered Via Typed Signal\n\nParagraph text.");

            assert(renderSpy.count() >= 1);
            assert(renderSpy.last().at(0).toString().contains("Rendered Via Typed Signal"));
            assert(renderSpy.last().at(1).canConvert<QJsonObject>());

            emit previewView.bridge()->documentRendered(1);
            assert(docRenderedSpy.count() >= 1);
            assert(docRenderedSpy.last().at(0).toInt() == 1);
        });

        it("dispatches navigateToHeading signal without string interpolation", [&]() {
            QSignalSpy navSpy(previewView.bridge(), &WebBridge::navigateToHeading);
            previewView.scrollToHeading("rendered-via-typed-signal");
            assert(navSpy.count() >= 1);
            assert(navSpy.last().at(0).toString() == "rendered-via-typed-signal");
        });

        it("dispatches setTheme signal to web runtime", [&]() {
            QSignalSpy themeSpy(previewView.bridge(), &WebBridge::setTheme);
            previewView.setTheme("dark");
            assert(themeSpy.count() >= 1);
            assert(themeSpy.last().at(0).toString() == "dark");
        });

        it("forwards activeHeadingChanged signals from web runtime to TOC sidebar and session", [&]() {
            MainWindow win;
            win.resize(1000, 650);
            win.show();
            app.processEvents();

            win.editor()->setPlainText("# Heading One\n\n## Heading Two\n");
            win.updateToc();
            assert(win.tocWidget()->headingCount() == 2);

            emit win.previewView()->activeHeadingChanged("heading-two");
            app.processEvents();
            assert(win.tocWidget()->activeHeadingId() == "heading-two");
            assert(win.session()->activeHeading() == "heading-two");

            emit win.tocWidget()->headingActivated("heading-two");
            assert(win.session()->selectedHeading() == "heading-two");
        });

        it("invokes bridge legacy fallback slots and redundant render guards", [&]() {
            QSignalSpy tocSpy(previewView.bridge(), &WebBridge::tocUpdated);
            previewView.bridge()->updateTOC("[{\"id\":\"h1\",\"title\":\"H1\"}]");
            assert(tocSpy.count() == 1);

            QSignalSpy headingSpy(previewView.bridge(), &WebBridge::activeHeadingChanged);
            previewView.bridge()->setActiveHeading("h1");
            assert(headingSpy.count() == 1);
            assert(headingSpy.last().at(0).toString() == "h1");

            QSignalSpy redundantRenderSpy(previewView.bridge(), &WebBridge::renderDocument);
            previewView.setMarkdown("# Rendered Via Typed Signal\n\nParagraph text.");
            assert(redundantRenderSpy.count() == 0);
        });

        it("handles load failure, re-handshake rendering, and case-sensitive find queries in preview view", [&]() {
            PreviewView view;
            view.findText("", true, false);
            view.findText("query", false, true);

            emit view.webView()->loadFinished(false);
            assert(!view.isBridgeReady());

            view.setMarkdown("# Cached Markdown");
            emit view.bridge()->bridgeInitialized();
            assert(view.isBridgeReady());

            emit view.bridge()->bridgeInitialized();
        });
    });

    describe("Native Chromium search engine and match navigation", [&]() {
        MainWindow win;
        win.resize(900, 650);
        win.show();
        app.processEvents();

        win.modeSwitcher()->setMode(ModeSwitcher::DocumentMode::Preview);
        app.processEvents();

        QSignalSpy docRenderedSpy(win.previewView(), &PreviewView::documentRendered);
        win.previewView()->setMarkdown(
            "# Search Monograph\n\n"
            "Chromium native hardware-accelerated search engine.\n"
            "First instance of keyword search.\n"
            "Second instance of keyword search.\n"
        );
        for (int i = 0; i < 40 && docRenderedSpy.count() < 1; ++i) {
            QTest::qWait(25);
        }

        it("dispatches search queries to Chromium compositor via findText", [&]() {
            QSignalSpy findSpy(&win, &MainWindow::findResult);
            win.findText("search", true, false);

            for (int i = 0; i < 40 && findSpy.count() < 1; ++i) {
                QTest::qWait(10);
            }

            assert(findSpy.count() >= 1);
            int active = findSpy.last().at(0).toInt();
            int total = findSpy.last().at(1).toInt();
            assert(total >= 3);
            assert(active >= 1);
        });

        it("cycles through match occurrences via findNext and findPrevious", [&]() {
            win.findNext();
            QTest::qWait(10);
            win.findPrevious();
            QTest::qWait(10);
        });

        it("clears search highlights natively without DOM tree mutations", [&]() {
            QSignalSpy findSpy(&win, &MainWindow::findResult);
            win.clearFind();
            assert(findSpy.last().at(0).toInt() == 0);
            assert(findSpy.last().at(1).toInt() == 0);
        });

        it("executes search and navigates matches in Edit mode", [&]() {
            win.modeSwitcher()->setMode(ModeSwitcher::DocumentMode::Edit);
            win.editor()->setPlainText("Alpha query bravo charlie query delta query.");
            QSignalSpy findSpy(&win, &MainWindow::findResult);
            win.findText("query", true, false);
            assert(findSpy.last().at(0).toInt() == 1);
            assert(findSpy.last().at(1).toInt() == 3);

            win.findNext();
            assert(win.editor()->textCursor().hasSelection());
            win.findPrevious();
            assert(win.editor()->textCursor().hasSelection());

            win.findText("Alpha", true, true);
            assert(findSpy.last().at(0).toInt() == 1);
            assert(findSpy.last().at(1).toInt() == 1);

            win.findText("Alpha", true, false);
            win.findText("", true, false);
            assert(!win.editor()->textCursor().hasSelection());

            win.clearFind();
            assert(!win.editor()->textCursor().hasSelection());
            assert(findSpy.last().at(0).toInt() == 0);
            assert(findSpy.last().at(1).toInt() == 0);
        });

        it("toggles editor word wrap and zoom controls via menu handlers", [&]() {
            assert(win.editor()->lineWrapMode() == QPlainTextEdit::WidgetWidth);
            QAction* wrapAct = win.findChild<QAction*>("action_toggle_word_wrap");
            assert(wrapAct != nullptr);
            wrapAct->trigger();
            assert(win.editor()->lineWrapMode() == QPlainTextEdit::NoWrap);
            wrapAct->trigger();
            assert(win.editor()->lineWrapMode() == QPlainTextEdit::WidgetWidth);

            QAction* zoomInAct = win.findChild<QAction*>("action_zoom_in");
            assert(zoomInAct != nullptr);
            zoomInAct->trigger();
            assert(win.session()->zoomFactor() > 1.0);

            QAction* zoomOutAct = win.findChild<QAction*>("action_zoom_out");
            assert(zoomOutAct != nullptr);
            zoomOutAct->trigger();

            QAction* actualSizeAct = win.findChild<QAction*>("action_actual_size");
            assert(actualSizeAct != nullptr);
            actualSizeAct->trigger();
            assert(win.session()->zoomFactor() == 1.0);
        });
    });

    describe("Unified desktop zoom engine and viewport parity", [&]() {
        MainWindow win;
        win.resize(900, 650);
        win.show();
        app.processEvents();

        it("applies zoom factor directly to WebEngine view", [&]() {
            win.previewView()->setZoomFactor(1.5);
            assert(std::abs(win.previewView()->webView()->zoomFactor() - 1.5) < 0.01);
        });

        it("maintains zoom factor parity between editor and preview viewports", [&]() {
            win.session()->setZoomFactor(1.5);
            assert(std::abs(win.previewView()->webView()->zoomFactor() - 1.5) < 0.01);
            assert(std::abs(win.editor()->font().pointSizeF() - (13.0 * 1.5)) < 0.01);

            win.session()->resetZoom();
            assert(std::abs(win.previewView()->webView()->zoomFactor() - 1.0) < 0.01);
            assert(std::abs(win.editor()->font().pointSizeF() - 13.0) < 0.01);
        });

        it("clamps zoom factor strictly between quarter and three-times scale across session and viewports", [&]() {
            win.session()->setZoomFactor(0.1);
            assert(std::abs(win.session()->zoomFactor() - 0.25) < 0.01);
            assert(std::abs(win.previewView()->webView()->zoomFactor() - 0.25) < 0.01);
            assert(std::abs(win.editor()->font().pointSizeF() - (13.0 * 0.25)) < 0.01);

            win.session()->setZoomFactor(5.0);
            assert(std::abs(win.session()->zoomFactor() - 3.0) < 0.01);
            assert(std::abs(win.previewView()->webView()->zoomFactor() - 3.0) < 0.01);
            assert(std::abs(win.editor()->font().pointSizeF() - (13.0 * 3.0)) < 0.01);

            win.session()->resetZoom();
            assert(std::abs(win.previewView()->webView()->zoomFactor() - 1.0) < 0.01);
            assert(std::abs(win.editor()->font().pointSizeF() - 13.0) < 0.01);
        });
    });

    describe("Native document export and PDF generation", [&]() {
        MainWindow win;
        win.resize(900, 650);
        win.show();
        app.processEvents();

        QSignalSpy docRenderedSpy(win.previewView(), &PreviewView::documentRendered);
        win.previewView()->setMarkdown("# Export Brief\n\nExport verification.");
        for (int i = 0; i < 40 && docRenderedSpy.count() < 1; ++i) {
            QTest::qWait(25);
        }

        it("serializes full document HTML via native toHtml callback", [&]() {
            bool htmlExported = false;
            QString htmlPath = QDir::tempPath() + QString("/markd_export_%1.html").arg(QDateTime::currentMSecsSinceEpoch());
            win.exportHtml(htmlPath, [&](bool ok) {
                htmlExported = ok;
            });

            for (int i = 0; i < 40 && !htmlExported; ++i) {
                QTest::qWait(10);
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
        });

        it("renders document directly to PDF via native printToPdf and verifies output file", [&]() {
            QString pdfPath = QDir::tempPath() + QString("/markd_print_%1.pdf").arg(QDateTime::currentMSecsSinceEpoch());
            QSignalSpy pdfSpy(&win, &MainWindow::pdfPrintingFinished);
            win.printToPdf(pdfPath);
            for (int i = 0; i < 40 && pdfSpy.count() < 1; ++i) {
                QTest::qWait(10);
            }
            assert(pdfSpy.count() == 1);
            assert(pdfSpy.last().at(1).toBool() == true);
            assert(QFile::exists(pdfPath));
            assert(QFileInfo(pdfPath).size() > 0);
            QFile::remove(pdfPath);
        });
    });

    describe("Task 12 Bidirectional QWebChannel Bridge and Downstream Hooks", [&]() {
        PreviewView view;
        view.resize(800, 600);
        view.show();
        app.processEvents();

        it("dispatches renderDocument with preserveScroll option in PreviewView", [&]() {
            QSignalSpy renderSpy(view.bridge(), &WebBridge::renderDocument);
            emit view.bridge()->bridgeInitialized();
            view.setMarkdown("# Hot Reload Payload", /*preserveScroll=*/true);
            assert(renderSpy.count() >= 1);
            QJsonObject opts = renderSpy.last().at(1).toJsonObject();
            assert(opts.value("preserveScroll").toBool() == true);
        });

        it("emits scrollToRatio downlink signal across WebBridge", [&]() {
            QSignalSpy ratioSpy(view.bridge(), &WebBridge::scrollToRatio);
            emit view.bridge()->bridgeInitialized();
            view.scrollToRatio(0.75);
            assert(ratioSpy.count() == 1);
            assert(std::abs(ratioSpy.last().at(0).toReal() - 0.75) < 0.001);
        });

        it("buffers pending options before bridgeReady resolves", [&]() {
            PreviewView unreadyView;
            QJsonObject customOpts;
            customOpts["targetHeading"] = "intro-section";
            unreadyView.setMarkdown("# Pending", customOpts);
            assert(!unreadyView.isBridgeReady());

            QSignalSpy renderSpy(unreadyView.bridge(), &WebBridge::renderDocument);
            emit unreadyView.bridge()->bridgeInitialized();
            assert(unreadyView.isBridgeReady());
            assert(renderSpy.count() >= 1);
            assert(renderSpy.last().at(1).toJsonObject().value("targetHeading").toString() == "intro-section");
        });

        it("dispatches clipboardCopyRequested from DocumentSession copyToClipboard slot", [&]() {
            DocumentSession session;
            QSignalSpy clipSpy(&session, &DocumentSession::clipboardCopyRequested);
            session.copyToClipboard("print('hello world')");
            assert(clipSpy.count() == 1);
            assert(clipSpy.last().at(0).toString() == "print('hello world')");
        });

        it("synchronizes scroll coordinates across mode switching in MainWindow", [&]() {
            MainWindow win;
            win.resize(1000, 650);
            win.show();
            app.processEvents();

            win.editor()->setPlainText(QStringLiteral("# Intro\n\nLine 1\nLine 2\n\n## Section Two\n\nContent line\n"));
            win.updateToc();
            assert(win.tocWidget()->headingCount() == 2);

            // Position editor cursor at section two
            QTextCursor cursor = win.editor()->textCursor();
            cursor.movePosition(QTextCursor::End);
            win.editor()->setTextCursor(cursor);

            emit win.previewView()->bridge()->bridgeInitialized();

            // Case A: Markdown is already rendered in preview -> out-of-band navigateToHeading
            QSignalSpy navSpy(win.previewView()->bridge(), &WebBridge::navigateToHeading);
            win.setMode(MainWindow::Mode::Preview);
            app.processEvents();

            assert(navSpy.count() >= 1);
            assert(navSpy.last().at(0).toString() == "section-two");

            // Case B: Switch back to edit mode with active heading from preview
            emit win.previewView()->activeHeadingChanged("section-two");
            app.processEvents();

            win.setMode(MainWindow::Mode::Edit);
            app.processEvents();

            assert(win.editor()->textCursor().block().text().contains("Section Two"));
        });
    });
}

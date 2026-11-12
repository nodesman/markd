#pragma once

#include <QAccessible>
#include <QSignalSpy>
#include <QTest>
#include <QDir>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <cassert>
#include <iostream>
#include "Zinsser.h"
#include "app/MarkdApplication.h"
#include "ui/PreviewView.h"
#include "ui/MainWindow.h"
#include "core/Converter.h"
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryFile>
#include <QWebEnginePage>
#include <QWebEngineView>

// Runs JavaScript in a real Chromium page and returns its JSON result, polling the event loop.
inline QJsonObject evaluateJsonInPage(QWebEnginePage* page, const QString& expression)
{
    QString json;
    bool done = false;
    page->runJavaScript(QStringLiteral("JSON.stringify(%1)").arg(expression), [&](const QVariant& value) {
        json = value.toString();
        done = true;
    });
    QElapsedTimer timer;
    timer.start();
    while (!done && timer.elapsed() < 10000) QTest::qWait(10);
    assert(done);
    return QJsonDocument::fromJson(json.toUtf8()).object();
}

inline const char* kRevealProbe =
    "{ opacity: getComputedStyle(document.getElementById('content')).opacity,"
    "  hydrating: document.body.classList.contains('is-hydrating'),"
    "  scrollHeight: document.documentElement.scrollHeight,"
    "  innerHeight: window.innerHeight,"
    "  scrollY: window.scrollY,"
    "  text: document.getElementById('content').textContent.slice(0, 200) }";

inline void testNativeChromiumModernization(MarkdApplication& app) {
    using Zinsser::describe;
    using Zinsser::it;

    PreviewView previewView;
    previewView.resize(800, 600);
    previewView.show();
    app.processEvents();

    describe("Native Chromium preview engine and hardware-accelerated operations", [&]() {
        it("exposes active session web channel and web view handles", [&]() {
            assert(previewView.session() != nullptr);
            assert(previewView.channel() != nullptr);
            assert(previewView.webView() != nullptr);
        });

        it("traverses accessibility parent hierarchy on preview web view without dereferencing dangling renderer pointers", [&]() {
            QAccessibleInterface* iface = QAccessible::queryAccessibleInterface(previewView.webView());
            assert(iface != nullptr);
            assert(iface->isValid());
            for (int depth = 0; depth < 8 && iface != nullptr; ++depth) {
                iface = iface->parent();
            }
        });

        it("dispatches hardware-accelerated find text operations and clears search highlights", [&]() {
            QSignalSpy findSpy(&previewView, &PreviewView::findResult);
            previewView.findText("Markdown", true, false);
            QTest::qWait(100);
            previewView.clearFind();
            assert(findSpy.count() >= 1);
            assert(findSpy.last().at(0).toInt() == 0);
            assert(findSpy.last().at(1).toInt() == 0);
        });

        it("exports page content to standalone html string via async callback", [&]() {
            bool htmlReceived = false;
            QString exportedHtml;
            previewView.toHtml([&](const QString& html) {
                htmlReceived = true;
                exportedHtml = html;
            });

            for (int i = 0; i < 20 && !htmlReceived; ++i) {
                QTest::qWait(50);
            }
            assert(htmlReceived);
            assert(!exportedHtml.isEmpty());
        });

        it("serializes rendered page to pdf file via native print engine", [&]() {
            QString pdfPath = QDir::tempPath() + QString("/markd_test_%1.pdf").arg(QDateTime::currentMSecsSinceEpoch());
            QSignalSpy pdfSpy(&previewView, &PreviewView::pdfPrintingFinished);
            previewView.printToPdf(pdfPath);
            for (int i = 0; i < 30 && pdfSpy.count() < 1; ++i) {
                QTest::qWait(50);
            }
            assert(pdfSpy.count() == 1);
            assert(pdfSpy.last().at(1).toBool() == true);
            assert(QFile::exists(pdfPath));
            assert(QFileInfo(pdfPath).size() > 0);
            QFile::remove(pdfPath);
        });
    });

    describe("Real Chromium single reveal", [&]() {
        it("opens a long file at the top with its content visible", [&]() {
            QTemporaryFile tempFile(QDir::tempPath() + "/markd_e2e_top_XXXXXX.md");
            assert(tempFile.open());
            QString markdown = "# E2E Top Marker\n\n";
            for (int i = 0; i < 200; ++i) {
                markdown += QString("Paragraph %1 makes the page much taller than the window.\n\n").arg(i);
            }
            tempFile.write(markdown.toUtf8());
            tempFile.close();

            MainWindow win(tempFile.fileName(), MainWindow::Mode::Preview, true);
            win.resize(900, 600);
            QSignalSpy renderedSpy(win.previewView(), &PreviewView::documentRendered);
            win.show();
            assert(renderedSpy.count() > 0 || renderedSpy.wait(15000));
            QTest::qWait(400); // longer than any smooth-scroll animation would take

            const QJsonObject state = evaluateJsonInPage(win.previewView()->webView()->page(), kRevealProbe);
            assert(state.value("opacity").toString() == QStringLiteral("1"));
            assert(!state.value("hydrating").toBool());
            assert(state.value("scrollHeight").toInt() > state.value("innerHeight").toInt());
            assert(state.value("scrollY").toDouble() == 0.0);
            assert(state.value("text").toString().contains("E2E Top Marker"));
        });

        it("exports standalone html with visible content", [&]() {
            const QString markdown = QStringLiteral("# Export Marker\n\nVisible exported prose.\n");
            const QStringList exports = {
                Converter::toFullHtmlDocument(markdown),
                Converter::bundleStandaloneHtml(Converter::toHtmlFragment(markdown), QStringLiteral("Export Test")),
            };
            for (const QString& html : exports) {
                QWebEnginePage page(PreviewView::sharedPreviewProfile());
                QSignalSpy loadSpy(&page, &QWebEnginePage::loadFinished);
                page.setHtml(html, QUrl(QStringLiteral("qrc:/web/")));
                assert(loadSpy.wait(15000));
                const QJsonObject state = evaluateJsonInPage(&page,
                    QStringLiteral("{ opacity: getComputedStyle(document.getElementById('content')).opacity,"
                                   "  text: document.getElementById('content').textContent }"));
                assert(state.value("opacity").toString() == QStringLiteral("1"));
                assert(state.value("text").toString().contains("Visible exported prose"));
            }
        });
    });
}

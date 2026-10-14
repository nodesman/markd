#pragma once

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
}

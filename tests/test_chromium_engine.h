#pragma once

#include <QSignalSpy>
#include <QTest>
#include <QDir>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <cassert>
#include <iostream>
#include "app/MarkdApplication.h"
#include "ui/PreviewView.h"

inline void testNativeChromiumModernization(MarkdApplication& app) {
    PreviewView previewView;
    previewView.resize(800, 600);
    previewView.show();
    app.processEvents();

    // 1. exposes active DocumentSession and QWebChannel
    assert(previewView.session() != nullptr);
    assert(previewView.channel() != nullptr);
    assert(previewView.webView() != nullptr);

    // 2. dispatches hardware-accelerated find text operations
    QSignalSpy findSpy(&previewView, &PreviewView::findResult);
    previewView.findText("Markdown", true, false);
    QTest::qWait(100);

    // 3. clears search highlighting and resets result count
    previewView.clearFind();
    assert(findSpy.count() >= 1);
    assert(findSpy.last().at(0).toInt() == 0);
    assert(findSpy.last().at(1).toInt() == 0);

    // 4. exports page content to HTML via asynchronous callback
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

    // 5. initiates PDF export with target file path and validates output file
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

    std::cout << "PASS: William Zinsser Native Chromium modernization tests passed!\n";
}

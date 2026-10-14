#pragma once

#include <QApplication>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QEventLoop>
#include <QTimer>
#include <QElapsedTimer>
#include <QThread>
#include <cassert>
#include <iostream>
#include <string>

#include "ui/PreviewView.h"

namespace RenderJsTest {

inline void waitForPageReady(PreviewView& previewView, int timeoutMs = 8000) {
    QWebEngineView* webView = previewView.webView();
    assert(webView != nullptr);

    QEventLoop loop;
    bool loadedOk = false;

    QMetaObject::Connection conn = QObject::connect(webView, &QWebEngineView::loadFinished, [&](bool ok) {
        loadedOk = ok;
        loop.quit();
    });

    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, [&]() {
        loop.quit();
    });
    timer.start(timeoutMs);

    loop.exec();
    QObject::disconnect(conn);

    QWebEnginePage* page = webView->page();
    QElapsedTimer checkTimer;
    checkTimer.start();
    while (checkTimer.elapsed() < 3000) {
        bool ready = false;
        QEventLoop checkLoop;
        page->runJavaScript(
            "document.readyState === 'complete' && window.Markd && window.Markd.isReady === true && window.session !== null && window.markdBridge !== null",
            [&](const QVariant& val) {
                ready = val.toBool();
                checkLoop.quit();
            }
        );
        checkLoop.exec();
        if (ready && previewView.isBridgeReady()) return;
        QThread::msleep(20);
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
    std::cerr << "FATAL: Timeout waiting for QWebEnginePage to reach readyState 'complete'\n";
    assert(false && "QWebEnginePage readyState timeout");
}

inline QVariant runJsSync(QWebEnginePage* page, const QString& js, int timeoutMs = 5000) {
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);

    QVariant result;
    bool finished = false;
    page->runJavaScript(js, [&](const QVariant& res) {
        result = res;
        finished = true;
        loop.quit();
    });
    loop.exec();
    if (!finished) {
        std::cerr << "FATAL: Synchronous JavaScript timed out: " << js.toStdString() << "\n";
        assert(false && "runJsSync timed out");
    }
    return result;
}

inline void renderMarkdownSync(PreviewView& previewView, const QString& markdown) {
    previewView.setMarkdown(markdown);
    for (int i = 0; i < 8; ++i) {
        QThread::msleep(25);
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
    }
}

} // namespace RenderJsTest

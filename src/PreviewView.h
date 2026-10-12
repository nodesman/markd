#pragma once

#include <QWidget>
#include <QWebEngineView>
#include <QWebChannel>
#include "WebBridge.h"

class PreviewView : public QWidget {
    Q_OBJECT

public:
    explicit PreviewView(QWidget* parent = nullptr);
    ~PreviewView() override = default;

    QWebEngineView* webView() const { return m_webView; }
    WebBridge* bridge() const { return m_bridge; }
    QWebChannel* channel() const { return m_channel; }

    void setMarkdown(const QString& markdown);
    void loadTemplate();
    void scrollToHeading(const QString& id);

signals:
    void tocUpdated(const QString& tocJson);
    void activeHeadingChanged(const QString& headingId);

private:
    QWebEngineView* m_webView = nullptr;
    WebBridge* m_bridge = nullptr;
    QWebChannel* m_channel = nullptr;
    bool m_pageLoaded = false;
    QString m_pendingMarkdown;
};

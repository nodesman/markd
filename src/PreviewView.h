#pragma once

#include <QWidget>
#include <QWebEngineView>
#include <QWebChannel>
#include "WebBridge.h"

class DocumentSession;

class PreviewView : public QWidget {
    Q_OBJECT

public:
    explicit PreviewView(QWidget* parent = nullptr);
    ~PreviewView() override = default;

    QWebEngineView* webView() const { return m_webView; }
    WebBridge* bridge() const { return m_bridge; }
    QWebChannel* channel() const { return m_channel; }
    DocumentSession* session() const { return m_session; }
    void setSession(DocumentSession* session);

    void setMarkdown(const QString& markdown);
    void loadTemplate();
    void scrollToHeading(const QString& id);
    void setZoomFactor(qreal factor);

signals:
    void tocUpdated(const QString& tocJson);
    void activeHeadingChanged(const QString& headingId);

private:
    QWebEngineView* m_webView = nullptr;
    WebBridge* m_bridge = nullptr;
    QWebChannel* m_channel = nullptr;
    DocumentSession* m_session = nullptr;
    bool m_pageLoaded = false;
    QString m_pendingMarkdown;
};

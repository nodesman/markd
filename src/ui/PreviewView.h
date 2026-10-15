#pragma once

#include <QWidget>
#include <QWebEngineView>
#include <QWebChannel>
#include "bridge/WebBridge.h"

#include <QPageLayout>
#include <functional>

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

    bool isBridgeReady() const { return m_bridgeReady; }
    QString pendingMarkdown() const { return m_pendingMarkdown; }
    QString currentMarkdown() const { return m_currentMarkdown; }

    void setMarkdown(const QString& markdown);
    void setMarkdown(const QString& markdown, bool preserveScroll);
    void setMarkdown(const QString& markdown, const QJsonObject& options);
    void scrollToRatio(qreal ratio);
    void loadTemplate();
    void scrollToHeading(const QString& id);
    void setTheme(const QString& themeName);
    void setZoomFactor(qreal factor);

    void findText(const QString& text, bool forward = true, bool caseSensitive = false);
    void findNext();
    void findPrevious();
    void clearFind();
    void printToPdf(const QString& filePath, const QPageLayout& layout = QPageLayout(QPageSize(QPageSize::A4), QPageLayout::Portrait, QMarginsF(15, 15, 15, 15), QPageLayout::Millimeter));
    void toHtml(const std::function<void(const QString&)>& callback);

signals:
    void bridgeInitialized();
    void tocUpdated(const QString& tocJson);
    void activeHeadingChanged(const QString& headingId);
    void findResult(int activeMatch, int totalMatches);
    void documentRendered(int headingCount);
    void pdfPrintingFinished(const QString& filePath, bool success);

private:
    QWebEngineView* m_webView = nullptr;
    WebBridge* m_bridge = nullptr;
    QWebChannel* m_channel = nullptr;
    DocumentSession* m_session = nullptr;
    bool m_pageLoaded = false;
    bool m_bridgeReady = false;
    QString m_pendingMarkdown;
    QJsonObject m_pendingOptions;
    QString m_currentMarkdown;
    QString m_currentActiveHeading;
    QString m_lastFindText;
    bool m_lastFindCaseSensitive = false;
};

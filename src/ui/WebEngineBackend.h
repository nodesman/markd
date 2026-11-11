#pragma once

#include <QObject>
#include <QPageLayout>
#include <QString>
#include <QUrl>
#include <functional>

class QWebChannel;
class QWebEngineProfile;
class QWebEngineView;
class QWidget;

class WebEngineBackend : public QObject {
    Q_OBJECT

public:
    explicit WebEngineBackend(QObject* parent = nullptr) : QObject(parent) {}
    ~WebEngineBackend() override = default;

    virtual QWidget* viewWidget() const = 0;
    virtual QWebEngineView* rawWebView() const = 0;
    virtual void openUrl(const QUrl& url) = 0;
    virtual void applyZoom(qreal factor) = 0;
    virtual void searchInPage(const QString& query, bool forward, bool caseSensitive) = 0;
    virtual void clearSearchHighlights() = 0;
    virtual void copySelectedContent() = 0;
    virtual void selectAllPageContent() = 0;
    virtual bool holdsViewportFocus() const = 0;
    virtual void exportHtmlContent(const std::function<void(const QString&)>& callback) = 0;
    virtual void renderPdfDocument(const QString& filePath, const QPageLayout& layout) = 0;

signals:
    void pageLoadFinished(bool ok);
    void searchMatchReady(int activeMatch, int totalMatches);
    void pdfExportCompleted(const QString& filePath, bool success);
};

using CreateWebEngineBackendFn = WebEngineBackend* (*)(QWebChannel* channel, QWidget* parent);
using SharedWebEngineProfileFn = QWebEngineProfile* (*)();

extern "C" {
Q_DECL_EXPORT WebEngineBackend* markdCreateWebEngineBackend(QWebChannel* channel, QWidget* parent);
Q_DECL_EXPORT QWebEngineProfile* markdSharedWebEngineProfile();
}

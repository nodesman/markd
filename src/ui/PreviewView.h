#pragma once

#include <QWidget>
#if defined(MARKD_STATIC_WEBENGINE_BACKEND)
#include <QWebEngineView>
#include <QWebEngineProfile>
#endif
#include <QWebChannel>
#include <QPageLayout>
#include <QPointer>
#include <functional>
#include "bridge/WebBridge.h"
#include "ui/WebEngineBackend.h"

class DocumentSession;
class QLabel;
class QShowEvent;
class QStackedLayout;
class QWebEngineProfile;
class QWebEngineView;

class PreviewView : public QWidget {
    Q_OBJECT

public:
    explicit PreviewView(DocumentSession* session, QWidget* parent = nullptr);
    explicit PreviewView(QWidget* parent = nullptr);
    ~PreviewView() override;

    static QWebEngineProfile* sharedPreviewProfile();
    // Test seam: replaces the plugin factory (nullptr simulates a missing plugin) until cleared.
    static void overrideBackendFactory(CreateWebEngineBackendFn factory, const QString& failureReason = QString());
    static void clearBackendFactoryOverride();

    QWebEngineView* webView() const;
    bool isWebEngineInitialized() const { return m_backend != nullptr; }
    bool isRevealed() const { return m_revealed; }
    QLabel* loadFailureLabel() const { return m_loadFailureLabel; }

    void schedulePostShowHydration();
    void ensureWebEngineInitialized();
    void focusPreviewSurface();
    bool hasPreviewFocus() const;
    void copySelection();
    void selectAllContent();

    WebBridge* bridge() const { return m_bridge; }
    QWebChannel* channel() const { return m_channel; }
    DocumentSession* session() const { return m_session; }
    void setSession(DocumentSession* session);

    bool isBridgeReady() const { return m_bridgeReady; }
    QString pendingMarkdown() const { return m_pendingMarkdown; }
    QString currentMarkdown() const { return m_currentMarkdown; }
    QString renderedMarkdown() const { return m_renderedMarkdown; }

    void setMarkdown(const QString& markdown);
    void setMarkdown(const QString& markdown, bool preserveScroll);
    void setMarkdown(const QString& markdown, const QJsonObject& options);
    void scrollToRatio(qreal ratio);
    void loadTemplate();
    void scrollToHeading(const QString& id);
    void setTheme(const QString& themeName);
    void setZoomFactor(qreal factor);
    qreal zoomFactor() const { return m_zoomFactor; }

    void findText(const QString& text, bool forward = true, bool caseSensitive = false);
    void findNext();
    void findPrevious();
    void clearFind();
    void printToPdf(const QString& filePath, const QPageLayout& layout = QPageLayout(QPageSize(QPageSize::A4), QPageLayout::Portrait, QMarginsF(15, 15, 15, 15), QPageLayout::Millimeter));
    void toHtml(const std::function<void(const QString&)>& callback);

    bool eventFilter(QObject* watched, QEvent* event) override;

signals:
    void bridgeInitialized();
    void tocUpdated(const QString& tocJson);
    void activeHeadingChanged(const QString& headingId);
    void findResult(int activeMatch, int totalMatches);
    void documentRendered(int headingCount);
    void pdfPrintingFinished(const QString& filePath, bool success);
    void zoomDeltaRequested(qreal delta);

protected:
    void showEvent(QShowEvent* event) override;

private:
    void wireBridgeSignals();
    void bindSessionSignals();
    void applyDeferredRequestsOnFirstReveal();
    void showLoadFailure(const QString& reason);
    void pollCocoaExposeThenHydrate(int remainingTicks);
    void installWebViewEventFilters();

    QStackedLayout* m_stackLayout = nullptr;
    QLabel* m_loadFailureLabel = nullptr;
    WebEngineBackend* m_backend = nullptr;
    WebBridge* m_bridge = nullptr;
    QWebChannel* m_channel = nullptr;
    DocumentSession* m_session = nullptr;
    bool m_revealed = false;
    bool m_hydrationScheduled = false;
    bool m_pageLoaded = false;
    bool m_bridgeReady = false;
    QString m_pendingMarkdown;
    QJsonObject m_pendingOptions;
    QString m_pendingHeadingId;
    QString m_currentMarkdown;
    QString m_renderedMarkdown;
    QString m_currentActiveHeading;
    QString m_lastFindText;
    bool m_lastFindCaseSensitive = false;
    qreal m_zoomFactor = 1.0;
};

#include "ui/PreviewView.h"
#include "core/DocumentSession.h"
#include <QVBoxLayout>
#include <QWebEnginePage>
#include <QWebEngineFindTextResult>
#include <QJsonDocument>
#include <QJsonArray>
#include <QUrl>
#include <QNativeGestureEvent>
#include <QWheelEvent>
#include <QChildEvent>
#include <cmath>

PreviewView::PreviewView(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("previewContainer");
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(0, 0);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_webView = new QWebEngineView(this);
    m_webView->setObjectName("previewView");
    m_webView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_webView->setMinimumSize(0, 0);

    // Invariant: White-Flash Compositor Pre-Clear (#050505)
    m_webView->page()->setBackgroundColor(QColor("#050505"));

    // IPC Architecture Bridge Pre-wiring
    m_bridge = new WebBridge(this);
    m_session = new DocumentSession(this);

    m_channel = new QWebChannel(this);
    m_channel->registerObject(QStringLiteral("markdBridge"), m_bridge);
    m_channel->registerObject(QStringLiteral("session"), m_session);
    m_webView->page()->setWebChannel(m_channel);

    connect(m_bridge, &WebBridge::bridgeInitialized, this, [this]() {
        m_bridgeReady = true;
        emit bridgeInitialized();
        if (m_session && !m_session->theme().isEmpty()) {
            emit m_bridge->setTheme(m_session->theme());
        }
        if (!m_pendingMarkdown.isEmpty()) {
            QString md = m_pendingMarkdown;
            QJsonObject opts = m_pendingOptions;
            m_pendingMarkdown.clear();
            m_pendingOptions = QJsonObject();
            setMarkdown(md, opts);
            return;
        }
        if (!m_currentMarkdown.isEmpty()) {
            QJsonObject opts;
            opts[QStringLiteral("force")] = true;
            emit m_bridge->renderDocument(m_currentMarkdown, opts);
            return;
        }
        if (m_session && !m_session->markdown().isEmpty()) {
            setMarkdown(m_session->markdown());
        }
    });

    connect(m_bridge, &WebBridge::tocUpdated, this, &PreviewView::tocUpdated);
    connect(m_bridge, &WebBridge::activeHeadingChanged, this, [this](const QString& id) {
        if (m_currentActiveHeading == id) return;
        m_currentActiveHeading = id;
        if (m_session && m_session->activeHeading() != id) {
            m_session->setActiveHeading(id);
        }
        emit activeHeadingChanged(id);
    });
    connect(m_bridge, &WebBridge::documentRendered, this, [this](int headingCount) {
        m_renderedMarkdown = m_currentMarkdown;
        emit documentRendered(headingCount);
    });

    connect(m_session, &DocumentSession::tocUpdated, this, &PreviewView::tocUpdated);
    connect(m_session, &DocumentSession::activeHeadingChanged, this, [this](const QString& id) {
        if (m_currentActiveHeading == id) return;
        m_currentActiveHeading = id;
        emit activeHeadingChanged(id);
    });

    // Native hardware-accelerated Chromium search result notification
    connect(m_webView->page(), &QWebEnginePage::findTextFinished, this, [this](const QWebEngineFindTextResult& result) {
        emit findResult(result.activeMatch(), result.numberOfMatches());
    });

    // Native asynchronous PDF print completion notification
    connect(m_webView->page(), &QWebEnginePage::pdfPrintingFinished, this, &PreviewView::pdfPrintingFinished);

    connect(m_webView, &QWebEngineView::loadFinished, this, [this](bool ok) {
        m_pageLoaded = ok;
        if (!ok) {
            m_bridgeReady = false;
        }
        installWebViewEventFilters();
    });

    layout->addWidget(m_webView);
    installWebViewEventFilters();

    loadTemplate();
}

void PreviewView::installWebViewEventFilters()
{
    installEventFilter(this);
    if (!m_webView) return;
    m_webView->installEventFilter(this);
    if (QWidget* proxy = m_webView->focusProxy()) {
        proxy->installEventFilter(this);
    }
    const QList<QObject*> children = m_webView->findChildren<QObject*>();
    for (QObject* child : children) {
        child->installEventFilter(this);
    }
}

bool PreviewView::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::ChildAdded) {
        auto* childEvent = static_cast<QChildEvent*>(event);
        if (childEvent->child()) {
            childEvent->child()->installEventFilter(this);
        }
        return QWidget::eventFilter(watched, event);
    }
    if (event->type() == QEvent::NativeGesture) {
        auto* gesture = static_cast<QNativeGestureEvent*>(event);
        if (gesture->gestureType() == Qt::ZoomNativeGesture) {
            event->accept();
            if (std::abs(gesture->value()) >= 1e-6) {
                emit zoomDeltaRequested(gesture->value());
            }
            return true;
        }
    }
    if (event->type() == QEvent::Wheel) {
        auto* wheel = static_cast<QWheelEvent*>(event);
        if (wheel->modifiers() & Qt::ControlModifier) {
            event->accept();
            const int dy = wheel->angleDelta().y();
            if (dy != 0) {
                emit zoomDeltaRequested(dy > 0 ? 0.10 : -0.10);
            }
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void PreviewView::loadTemplate()
{
    m_bridgeReady = false;
    m_pageLoaded = false;
    m_currentMarkdown.clear();
    m_renderedMarkdown.clear();
    m_currentActiveHeading.clear();
    m_webView->setUrl(QUrl("qrc:/web/template.html"));
}

void PreviewView::setSession(DocumentSession* session)
{
    if (m_session == session) return;
    if (m_session) {
        disconnect(m_session, nullptr, this, nullptr);
        if (m_channel) {
            m_channel->deregisterObject(m_session);
        }
        if (m_session->parent() == this) {
            delete m_session;
        }
    }
    m_session = session;
    if (m_channel && m_session) {
        m_channel->registerObject(QStringLiteral("session"), m_session);
        connect(m_session, &DocumentSession::tocUpdated, this, &PreviewView::tocUpdated);
        connect(m_session, &DocumentSession::activeHeadingChanged, this, [this](const QString& id) {
            if (m_currentActiveHeading == id) return;
            m_currentActiveHeading = id;
            emit activeHeadingChanged(id);
        });
        connect(m_session, &DocumentSession::themeChanged, this, &PreviewView::setTheme);
        if (!m_session->markdown().isEmpty()) {
            setMarkdown(m_session->markdown());
        }
    }
}

void PreviewView::setMarkdown(const QString& markdown)
{
    setMarkdown(markdown, QJsonObject());
}

void PreviewView::setMarkdown(const QString& markdown, bool preserveScroll)
{
    QJsonObject opts;
    if (preserveScroll) {
        opts[QStringLiteral("preserveScroll")] = true;
    }
    setMarkdown(markdown, opts);
}

void PreviewView::setMarkdown(const QString& markdown, const QJsonObject& options)
{
    if (m_bridgeReady && m_currentMarkdown == markdown && options.isEmpty()) {
        return;
    }

    if (m_session && m_session->markdown() != markdown) {
        m_session->setMarkdown(markdown);
    }

    if (m_renderedMarkdown != markdown) {
        m_renderedMarkdown.clear();
    }

    if (!m_bridgeReady) {
        m_pendingMarkdown = markdown;
        m_pendingOptions = options;
        return;
    }

    m_currentMarkdown = markdown;

    // Pure typed domain semantic action dispatch (zero DOM leakage, zero runJavaScript)
    emit m_bridge->renderDocument(markdown, options);
}

void PreviewView::scrollToRatio(qreal ratio)
{
    if (m_bridgeReady) {
        emit m_bridge->scrollToRatio(ratio);
    }
}

void PreviewView::scrollToHeading(const QString& id)
{
    if (m_bridgeReady) {
        emit m_bridge->navigateToHeading(id);
    }
}

void PreviewView::setTheme(const QString& themeName)
{
    if (m_bridgeReady) {
        emit m_bridge->setTheme(themeName);
    }
}

void PreviewView::setZoomFactor(qreal factor)
{
    m_zoomFactor = std::clamp(factor, 0.25, 3.0);
    if (m_webView) {
        m_webView->setZoomFactor(m_zoomFactor);
    }
}

void PreviewView::findText(const QString& text, bool forward, bool caseSensitive)
{
    m_lastFindText = text;
    m_lastFindCaseSensitive = caseSensitive;
    if (!m_webView) return;

    if (text.isEmpty()) {
        clearFind();
        return;
    }

    QWebEnginePage::FindFlags flags;
    if (!forward) {
        flags |= QWebEnginePage::FindBackward;
    }
    if (caseSensitive) {
        flags |= QWebEnginePage::FindCaseSensitively;
    }

    m_webView->findText(text, flags);
}

void PreviewView::findNext()
{
    if (!m_lastFindText.isEmpty()) {
        findText(m_lastFindText, true, m_lastFindCaseSensitive);
    }
}

void PreviewView::findPrevious()
{
    if (!m_lastFindText.isEmpty()) {
        findText(m_lastFindText, false, m_lastFindCaseSensitive);
    }
}

void PreviewView::clearFind()
{
    m_lastFindText.clear();
    if (m_webView) {
        m_webView->findText(QString());
    }
    emit findResult(0, 0);
}

void PreviewView::printToPdf(const QString& filePath, const QPageLayout& layout)
{
    if (m_webView && m_webView->page()) {
        m_webView->page()->printToPdf(filePath, layout);
    }
}

void PreviewView::toHtml(const std::function<void(const QString&)>& callback)
{
    if (!callback) return;
    if (m_webView && m_webView->page()) {
        m_webView->page()->toHtml(callback);
        return;
    }
    callback(QString());
}

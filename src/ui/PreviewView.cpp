#include "ui/PreviewView.h"
#include "ui/WebEngineBackend.h"
#include "core/DocumentSession.h"
#include <QApplication>
#include <QChildEvent>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLibrary>
#include <QNativeGestureEvent>
#include <QPalette>
#include <QShowEvent>
#include <QStackedLayout>
#include <QTimer>
#include <QUrl>
#include <QWheelEvent>
#include <QWindow>
#include <algorithm>
#include <cmath>

namespace {

struct BackendFactoryOverride {
    bool active = false;
    CreateWebEngineBackendFn factory = nullptr;
    QString failureReason;
};

BackendFactoryOverride& backendFactoryOverride()
{
    static BackendFactoryOverride s_override;
    return s_override;
}

QString& lastPluginLoadError()
{
    static QString s_error;
    return s_error;
}

#if !defined(MARKD_STATIC_WEBENGINE_BACKEND)
QLibrary* loadWebEnginePlugin()
{
    static QLibrary* s_lib = nullptr;
    if (s_lib && s_lib->isLoaded()) return s_lib;

    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir(appDir).filePath(QStringLiteral("libmarkd_webengine.dylib")),
        QDir(appDir).filePath(QStringLiteral("../Frameworks/libmarkd_webengine.dylib")),
        QDir(appDir).filePath(QStringLiteral("../libexec/Markd.app/Contents/Frameworks/libmarkd_webengine.dylib")),
        QDir(appDir).filePath(QStringLiteral("markd_webengine.dll")),
        QDir(appDir).filePath(QStringLiteral("libmarkd_webengine.so")),
        QDir(appDir).filePath(QStringLiteral("../lib/libmarkd_webengine.so")),
    };

    for (const QString& path : candidates) {
        if (!QFileInfo::exists(path)) continue;
        auto* candidateLib = new QLibrary(path, QCoreApplication::instance());
        if (candidateLib->load()) {
            s_lib = candidateLib;
            return s_lib;
        }
        lastPluginLoadError() = candidateLib->errorString();
        delete candidateLib;
    }

    auto* fallbackLib = new QLibrary(QStringLiteral("markd_webengine"), QCoreApplication::instance());
    if (fallbackLib->load()) {
        s_lib = fallbackLib;
        return s_lib;
    }
    if (lastPluginLoadError().isEmpty()) lastPluginLoadError() = fallbackLib->errorString();
    delete fallbackLib;
    return nullptr;
}
#endif

CreateWebEngineBackendFn resolveBackendFactory()
{
    if (backendFactoryOverride().active) return backendFactoryOverride().factory;
#if defined(MARKD_STATIC_WEBENGINE_BACKEND)
    return &markdCreateWebEngineBackend;
#else
    QLibrary* lib = loadWebEnginePlugin();
    return lib ? reinterpret_cast<CreateWebEngineBackendFn>(lib->resolve("markdCreateWebEngineBackend")) : nullptr;
#endif
}

SharedWebEngineProfileFn resolveProfileFactory()
{
#if defined(MARKD_STATIC_WEBENGINE_BACKEND)
    return &markdSharedWebEngineProfile;
#else
    QLibrary* lib = loadWebEnginePlugin();
    return lib ? reinterpret_cast<SharedWebEngineProfileFn>(lib->resolve("markdSharedWebEngineProfile")) : nullptr;
#endif
}

QString backendFailureReason()
{
    if (!backendFactoryOverride().failureReason.isEmpty()) return backendFactoryOverride().failureReason;
    return lastPluginLoadError().isEmpty() ? QObject::tr("libmarkd_webengine could not be found") : lastPluginLoadError();
}

// Keeps a scroll ratio requested before the bridge was ready, unless the new options bring their own.
QJsonObject withBufferedScrollRatio(const QJsonObject& options, const QJsonObject& previous)
{
    const QString key = QStringLiteral("scrollRatio");
    if (options.contains(key) || !previous.contains(key)) return options;
    QJsonObject merged = options;
    merged[key] = previous.value(key);
    return merged;
}

} // namespace

PreviewView::PreviewView(QWidget* parent)
    : PreviewView(nullptr, parent)
{
}

PreviewView::PreviewView(DocumentSession* session, QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("previewContainer"));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(0, 0);

    // Obsidian canvas until the web view's own #050505 page paints over it.
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor("#050505"));
    setPalette(pal);
    setAutoFillBackground(true);

    m_stackLayout = new QStackedLayout(this);
    m_stackLayout->setContentsMargins(0, 0, 0, 0);
    m_stackLayout->setStackingMode(QStackedLayout::StackAll);

    m_bridge = new WebBridge(this);
    m_channel = new QWebChannel(this);
    m_channel->registerObject(QStringLiteral("markdBridge"), m_bridge);

    m_session = session ? session : new DocumentSession(this);
    m_channel->registerObject(QStringLiteral("session"), m_session);
    bindSessionSignals();
    wireBridgeSignals();

    installEventFilter(this);

    if (!m_session->markdown().isEmpty()) {
        setMarkdown(m_session->markdown());
    }
}

PreviewView::~PreviewView()
{
    delete m_backend;
    m_backend = nullptr;
}

QWebEngineProfile* PreviewView::sharedPreviewProfile()
{
    SharedWebEngineProfileFn fn = resolveProfileFactory();
    return fn ? fn() : nullptr;
}

void PreviewView::overrideBackendFactory(CreateWebEngineBackendFn factory, const QString& failureReason)
{
    backendFactoryOverride() = BackendFactoryOverride{true, factory, failureReason};
}

void PreviewView::clearBackendFactoryOverride()
{
    backendFactoryOverride() = BackendFactoryOverride{};
}

QWebEngineView* PreviewView::webView() const
{
    const_cast<PreviewView*>(this)->ensureWebEngineInitialized();
    return m_backend ? m_backend->rawWebView() : nullptr;
}

void PreviewView::wireBridgeSignals()
{
    connect(m_bridge, &WebBridge::bridgeInitialized, this, [this]() {
        m_bridgeReady = true;
        emit bridgeInitialized();
        if (m_session && !m_session->theme().isEmpty()) {
            emit m_bridge->setTheme(m_session->theme());
        }
        if (!m_pendingMarkdown.isEmpty()) {
            const QString md = m_pendingMarkdown;
            const QJsonObject opts = m_pendingOptions;
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
        applyDeferredRequestsOnFirstReveal();
        emit documentRendered(headingCount);
    });
}

void PreviewView::bindSessionSignals()
{
    if (!m_session) return;
    connect(m_session, &DocumentSession::tocUpdated, this, &PreviewView::tocUpdated);
    connect(m_session, &DocumentSession::activeHeadingChanged, this, [this](const QString& id) {
        if (m_currentActiveHeading == id) return;
        m_currentActiveHeading = id;
        emit activeHeadingChanged(id);
    });
    connect(m_session, &DocumentSession::themeChanged, this, &PreviewView::setTheme);
}

void PreviewView::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    schedulePostShowHydration();
}

void PreviewView::schedulePostShowHydration()
{
    if (m_backend || m_hydrationScheduled) return;
    if (qEnvironmentVariableIntValue("MARKD_STARTUP_BENCHMARK") == 1) return;
    m_hydrationScheduled = true;

    if (QGuiApplication::platformName() == QLatin1String("cocoa")) {
        pollCocoaExposeThenHydrate(16);
        return;
    }
    QTimer::singleShot(0, this, &PreviewView::ensureWebEngineInitialized);
}

void PreviewView::pollCocoaExposeThenHydrate(int remainingTicks)
{
    if (m_backend) return;
    QWidget* top = window();
    QWindow* handle = top ? top->windowHandle() : nullptr;
    if ((handle && handle->isExposed()) || remainingTicks <= 0) {
        QTimer::singleShot(0, this, &PreviewView::ensureWebEngineInitialized);
        return;
    }
    QTimer::singleShot(12, this, [this, remainingTicks]() {
        pollCocoaExposeThenHydrate(remainingTicks - 1);
    });
}

void PreviewView::ensureWebEngineInitialized()
{
    if (m_backend || m_loadFailureLabel) return;
    CreateWebEngineBackendFn factory = resolveBackendFactory();
    if (!factory) {
        showLoadFailure(backendFailureReason());
        return;
    }

    m_backend = factory(m_channel, this);
    if (!m_backend) {
        showLoadFailure(backendFailureReason());
        return;
    }

    m_backend->applyZoom(m_zoomFactor);
    connect(m_backend, &WebEngineBackend::searchMatchReady, this, &PreviewView::findResult);
    connect(m_backend, &WebEngineBackend::pdfExportCompleted, this, &PreviewView::pdfPrintingFinished);
    connect(m_backend, &WebEngineBackend::pageLoadFinished, this, [this](bool ok) {
        m_pageLoaded = ok;
        if (!ok) m_bridgeReady = false;
        installWebViewEventFilters();
    });

    // QStackedLayout hands focus to the newly current view if the preview held it.
    if (QWidget* view = m_backend->viewWidget()) {
        m_stackLayout->addWidget(view);
    }
    installWebViewEventFilters();
    m_backend->openUrl(QUrl(QStringLiteral("qrc:/web/template.html")));
}

void PreviewView::showLoadFailure(const QString& reason)
{
    m_loadFailureLabel = new QLabel(tr("Preview engine failed to load: %1").arg(reason), this);
    m_loadFailureLabel->setObjectName(QStringLiteral("previewLoadFailure"));
    m_loadFailureLabel->setAlignment(Qt::AlignCenter);
    m_loadFailureLabel->setWordWrap(true);
    m_loadFailureLabel->setStyleSheet(QStringLiteral("QLabel { color: #8b949e; background-color: #050505; padding: 24px; }"));
    m_stackLayout->addWidget(m_loadFailureLabel);
    m_loadFailureLabel->raise();
}

void PreviewView::applyDeferredRequestsOnFirstReveal()
{
    if (m_revealed) return;
    m_revealed = true;
    if (!m_pendingHeadingId.isEmpty() && m_bridgeReady) {
        const QString target = m_pendingHeadingId;
        m_pendingHeadingId.clear();
        emit m_bridge->navigateToHeading(target);
    }
    if (!m_lastFindText.isEmpty() && m_backend) {
        m_backend->searchInPage(m_lastFindText, true, m_lastFindCaseSensitive);
    }
}

void PreviewView::focusPreviewSurface()
{
    if (m_backend && m_backend->viewWidget()) {
        m_backend->viewWidget()->setFocus(Qt::OtherFocusReason);
        return;
    }
    setFocus(Qt::OtherFocusReason);
}

bool PreviewView::hasPreviewFocus() const
{
    if (hasFocus()) return true;
    if (m_backend && m_backend->holdsViewportFocus()) return true;
    QWidget* focused = QApplication::focusWidget();
    return focused && isAncestorOf(focused);
}

void PreviewView::copySelection()
{
    if (m_backend) {
        m_backend->copySelectedContent();
    }
}

void PreviewView::selectAllContent()
{
    if (m_backend) {
        m_backend->selectAllPageContent();
    }
}

void PreviewView::installWebViewEventFilters()
{
    installEventFilter(this);
    QWidget* view = m_backend ? m_backend->viewWidget() : nullptr;
    if (!view) return;
    view->installEventFilter(this);
    if (QWidget* proxy = view->focusProxy()) {
        proxy->installEventFilter(this);
    }
    const QList<QObject*> children = view->findChildren<QObject*>();
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
    ensureWebEngineInitialized();
    m_bridgeReady = false;
    m_pageLoaded = false;
    m_currentMarkdown.clear();
    m_renderedMarkdown.clear();
    m_currentActiveHeading.clear();
    if (m_backend) {
        m_backend->openUrl(QUrl(QStringLiteral("qrc:/web/template.html")));
    }
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
        bindSessionSignals();
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
        m_pendingOptions = withBufferedScrollRatio(options, m_pendingOptions);
        m_pendingMarkdown = markdown;
        return;
    }
    m_currentMarkdown = markdown;
    emit m_bridge->renderDocument(markdown, options);
}

void PreviewView::scrollToRatio(qreal ratio)
{
    if (m_bridgeReady) {
        emit m_bridge->scrollToRatio(ratio);
        return;
    }
    m_pendingOptions[QStringLiteral("scrollRatio")] = ratio;
}

void PreviewView::scrollToHeading(const QString& id)
{
    if (m_bridgeReady) {
        emit m_bridge->navigateToHeading(id);
        return;
    }
    m_pendingHeadingId = id;
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
    if (m_backend) {
        m_backend->applyZoom(m_zoomFactor);
    }
}

void PreviewView::findText(const QString& text, bool forward, bool caseSensitive)
{
    m_lastFindText = text;
    m_lastFindCaseSensitive = caseSensitive;

    if (text.isEmpty()) {
        clearFind();
        return;
    }
    if (m_backend) {
        m_backend->searchInPage(text, forward, caseSensitive);
    }
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
    if (m_backend) {
        m_backend->clearSearchHighlights();
    }
    emit findResult(0, 0);
}

void PreviewView::printToPdf(const QString& filePath, const QPageLayout& layout)
{
    ensureWebEngineInitialized();
    if (m_backend) {
        m_backend->renderPdfDocument(filePath, layout);
    }
}

void PreviewView::toHtml(const std::function<void(const QString&)>& callback)
{
    if (!callback) return;
    ensureWebEngineInitialized();
    if (m_backend) {
        m_backend->exportHtmlContent(callback);
        return;
    }
    callback(QString());
}

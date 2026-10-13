#include "ui/PreviewView.h"
#include "core/DocumentSession.h"
#include <QVBoxLayout>
#include <QWebEnginePage>
#include <QWebEngineFindTextResult>
#include <QJsonDocument>
#include <QJsonArray>
#include <QUrl>

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

    connect(m_bridge, &WebBridge::tocUpdated, this, &PreviewView::tocUpdated);
    connect(m_bridge, &WebBridge::activeHeadingChanged, this, &PreviewView::activeHeadingChanged);

    connect(m_session, &DocumentSession::tocUpdated, this, &PreviewView::tocUpdated);
    connect(m_session, &DocumentSession::activeHeadingChanged, this, &PreviewView::activeHeadingChanged);

    // Native hardware-accelerated Chromium search result notification
    connect(m_webView->page(), &QWebEnginePage::findTextFinished, this, [this](const QWebEngineFindTextResult& result) {
        emit findResult(result.activeMatch(), result.numberOfMatches());
    });

    connect(m_webView, &QWebEngineView::loadFinished, this, [this](bool ok) {
        m_pageLoaded = ok;
        if (ok && !m_pendingMarkdown.isEmpty()) {
            setMarkdown(m_pendingMarkdown);
            m_pendingMarkdown.clear();
        }
    });

    layout->addWidget(m_webView);

    loadTemplate();
}

void PreviewView::loadTemplate()
{
    m_webView->setUrl(QUrl("qrc:/web/template.html"));
}

void PreviewView::setSession(DocumentSession* session)
{
    if (m_session == session) return;
    if (m_session && m_session->parent() == this) {
        m_session->deleteLater();
    }
    m_session = session;
    if (m_channel && m_session) {
        m_channel->registerObject(QStringLiteral("session"), m_session);
        connect(m_session, &DocumentSession::tocUpdated, this, &PreviewView::tocUpdated, Qt::UniqueConnection);
        connect(m_session, &DocumentSession::activeHeadingChanged, this, &PreviewView::activeHeadingChanged, Qt::UniqueConnection);
    }
}

void PreviewView::setMarkdown(const QString& markdown)
{
    if (m_session && m_session->markdown() != markdown) {
        m_session->setMarkdown(markdown);
    }

    if (!m_pageLoaded) {
        m_pendingMarkdown = markdown;
        return;
    }

    // Call JavaScript renderMarkdown function in template.html
    // Safe JSON serialization of string
    QString jsonString = QString::fromUtf8(QJsonDocument(QJsonArray{markdown}).toJson(QJsonDocument::Compact));
    QString js = QString("if (typeof renderMarkdown === 'function') { renderMarkdown(%1[0]); }").arg(jsonString);
    m_webView->page()->runJavaScript(js);
}

void PreviewView::scrollToHeading(const QString& id)
{
    QString js = QString("if (typeof scrollToHeading === 'function') { scrollToHeading('%1'); }").arg(id);
    m_webView->page()->runJavaScript(js);
}

void PreviewView::setZoomFactor(qreal factor)
{
    if (m_webView) {
        m_webView->setZoomFactor(factor);
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
    if (m_webView && m_webView->page()) {
        m_webView->page()->toHtml(callback);
    }
}

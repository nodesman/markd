#include "ui/WebEngineBackend.h"
#include <QColor>
#include <QCoreApplication>
#include <QPointer>
#include <QSizePolicy>
#include <QWebChannel>
#include <QWebEngineFindTextResult>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineView>
#include <QWidget>

namespace {

class ChromiumWebEngineBackend final : public WebEngineBackend {
public:
    ChromiumWebEngineBackend(QWebChannel* channel, QWidget* parentWidget)
        : WebEngineBackend(parentWidget)
        , m_view(new QWebEngineView(parentWidget))
    {
        m_view->setObjectName(QStringLiteral("previewView"));
        m_view->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        m_view->setMinimumSize(0, 0);

        auto* page = new QWebEnginePage(markdSharedWebEngineProfile(), m_view);
        page->setBackgroundColor(QColor("#050505"));
        page->setWebChannel(channel);
        m_view->setPage(page);

        connect(page, &QWebEnginePage::findTextFinished, this, &ChromiumWebEngineBackend::forwardFindResult);
        connect(page, &QWebEnginePage::pdfPrintingFinished, this, &WebEngineBackend::pdfExportCompleted);
        connect(m_view, &QWebEngineView::loadFinished, this, &WebEngineBackend::pageLoadFinished);
    }

    ~ChromiumWebEngineBackend() override
    {
        delete m_view.data();
    }

    QWidget* viewWidget() const override
    {
        return m_view.data();
    }

    QWebEngineView* rawWebView() const override
    {
        return m_view.data();
    }

    void openUrl(const QUrl& url) override
    {
        if (m_view) m_view->setUrl(url);
    }

    void applyZoom(qreal factor) override
    {
        if (m_view) m_view->setZoomFactor(factor);
    }

    void searchInPage(const QString& query, bool forward, bool caseSensitive) override
    {
        if (!m_view) return;
        QWebEnginePage::FindFlags flags;
        if (!forward) flags |= QWebEnginePage::FindBackward;
        if (caseSensitive) flags |= QWebEnginePage::FindCaseSensitively;
        m_view->findText(query, flags);
    }

    void clearSearchHighlights() override
    {
        if (m_view) m_view->findText(QString());
    }

    void copySelectedContent() override
    {
        if (m_view) m_view->triggerPageAction(QWebEnginePage::Copy);
    }

    void selectAllPageContent() override
    {
        if (m_view) m_view->triggerPageAction(QWebEnginePage::SelectAll);
    }

    bool holdsViewportFocus() const override
    {
        if (!m_view) return false;
        if (m_view->hasFocus()) return true;
        QWidget* proxy = m_view->focusProxy();
        return proxy && proxy->hasFocus();
    }

    void exportHtmlContent(const std::function<void(const QString&)>& callback) override
    {
        if (!callback) return;
        if (!m_view || !m_view->page()) return callback(QString());
        m_view->page()->toHtml(callback);
    }

    void renderPdfDocument(const QString& filePath, const QPageLayout& layout) override
    {
        if (m_view && m_view->page()) m_view->page()->printToPdf(filePath, layout);
    }

private:
    void forwardFindResult(const QWebEngineFindTextResult& res)
    {
        emit searchMatchReady(res.activeMatch(), res.numberOfMatches());
    }

    QPointer<QWebEngineView> m_view;
};

} // namespace

extern "C" {

QWebEngineProfile* markdSharedWebEngineProfile()
{
    static QPointer<QWebEngineProfile> s_profile;
    if (!s_profile) {
        s_profile = new QWebEngineProfile(QCoreApplication::instance());
        s_profile->setHttpCacheType(QWebEngineProfile::NoCache);
        s_profile->setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies);
        s_profile->setSpellCheckEnabled(false);
    }
    return s_profile.data();
}

WebEngineBackend* markdCreateWebEngineBackend(QWebChannel* channel, QWidget* parent)
{
    return new ChromiumWebEngineBackend(channel, parent);
}

}

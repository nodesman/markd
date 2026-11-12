#pragma once

#include <QStringList>
#include <QUrl>
#include <QWidget>
#include "ui/WebEngineBackend.h"

// Records what PreviewView asks of the web engine, so hydration ordering is testable without Chromium.
class FakeWebEngineBackend final : public WebEngineBackend {
public:
    explicit FakeWebEngineBackend(QWidget* parent)
        : WebEngineBackend(parent)
        , m_widget(new QWidget(parent))
    {
        m_widget->setFocusPolicy(Qt::StrongFocus);
    }

    QWidget* viewWidget() const override { return m_widget; }
    QWebEngineView* rawWebView() const override { return nullptr; }
    void openUrl(const QUrl& url) override { openedUrl = url; }
    void applyZoom(qreal factor) override { zoom = factor; }
    void searchInPage(const QString& query, bool, bool) override { searches.append(query); }
    void clearSearchHighlights() override {}
    void copySelectedContent() override { ++copies; }
    void selectAllPageContent() override { ++selectAlls; }
    bool holdsViewportFocus() const override { return m_widget->hasFocus(); }
    void exportHtmlContent(const std::function<void(const QString&)>& callback) override { callback(QString()); }
    void renderPdfDocument(const QString&, const QPageLayout&) override {}

    QUrl openedUrl;
    qreal zoom = 1.0;
    QStringList searches;
    int copies = 0;
    int selectAlls = 0;

private:
    QWidget* m_widget = nullptr;
};

inline FakeWebEngineBackend*& lastFakeWebEngineBackendSlot()
{
    static FakeWebEngineBackend* s_last = nullptr;
    return s_last;
}

inline FakeWebEngineBackend* lastFakeWebEngineBackend() { return lastFakeWebEngineBackendSlot(); }

inline WebEngineBackend* createFakeWebEngineBackend(QWebChannel*, QWidget* parent)
{
    auto* fake = new FakeWebEngineBackend(parent);
    lastFakeWebEngineBackendSlot() = fake;
    return fake;
}

inline WebEngineBackend* createNullWebEngineBackend(QWebChannel*, QWidget*) { return nullptr; }

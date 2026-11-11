#pragma once

#include <QHash>
#include <QTextBrowser>
#include <QUrl>
#include <functional>

class InstantPreviewSurface : public QTextBrowser {
    Q_OBJECT

public:
    explicit InstantPreviewSurface(QWidget* parent = nullptr);
    ~InstantPreviewSurface() override = default;

    void renderMarkdown(const QString& markdown, const QString& baseFilePath = QString());
    double verticalScrollRatio() const;
    void setVerticalScrollRatio(double ratio);
    void scrollToAnchorName(const QString& anchor);
    void applyZoomFactor(qreal factor);
    bool findString(const QString& subString, bool backward = false, bool caseSensitive = false);
    int countOccurrences(const QString& subString, bool caseSensitive = false) const;
    void copySelection();
    void selectAllContent();
    void setExternalUrlHandler(std::function<void(const QUrl&)> handler);

signals:
    void externalLinkActivated(const QUrl& url);
    void internalAnchorActivated(const QString& anchor);

private:
    void configureObsidianPalette();
    void handleAnchorActivated(const QUrl& url);

    std::function<void(const QUrl&)> m_externalUrlHandler;
    QHash<QString, int> m_headingPositions;
};

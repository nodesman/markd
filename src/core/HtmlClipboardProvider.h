#pragma once

#include <QString>
#include <QMimeData>

class HtmlClipboardProvider {
public:
    static QMimeData* createFromMarkdown(const QString& markdown);
    static QMimeData* createFromPreviewHtml(const QString& rawHtml, const QString& markdownFallback);
    static QString normalizeSelection(const QString& rawText);
    static QString sanitizeHtml(const QString& rawHtml);
    static QString inlineStyles(const QString& htmlFragment);

private:
    static QString inlineCodeStyles(const QString& html);
    static QString inlineTableStyles(const QString& html);
    static QString inlineBlockquoteStyles(const QString& html);
    static QString extractInnerHtml(const QString& rawHtml);
};

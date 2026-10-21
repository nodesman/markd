#pragma once

#include <QString>
#include <QMimeData>

class HtmlClipboardProvider {
public:
    static QMimeData* createFromMarkdown(const QString& markdown);
    static QMimeData* createFromPreviewHtml(const QString& rawHtml, const QString& markdownFallback);
    static QString normalizeSelection(const QString& rawText);
    static QString stripElementsByClass(const QString& html, const QString& tagName, const QString& className);
    static QString sanitizeExportHtml(const QString& rawHtml);
    static QString sanitizeHtml(const QString& rawHtml);
    static QString mergeTagStyle(const QString& html, const QString& tagName, const QString& cssRules);
    static QString inlineStyles(const QString& htmlFragment);
    static QString extractInnerHtml(const QString& rawHtml);

private:
    static QString inlineCodeStyles(const QString& html);
    static QString inlineSyntaxTokens(const QString& html);
    static QString inlineTableStyles(const QString& html);
    static QString inlineBlockquoteStyles(const QString& html);
};


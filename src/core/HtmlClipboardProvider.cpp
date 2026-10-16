#include "core/HtmlClipboardProvider.h"
#include "core/Converter.h"
#include <QRegularExpression>

QString HtmlClipboardProvider::normalizeSelection(const QString& rawText)
{
    QString normalized = rawText;
    normalized.replace(QChar(0x2029), QLatin1Char('\n'));
    normalized.replace(QChar(0x2028), QLatin1Char('\n'));
    return normalized;
}

QString HtmlClipboardProvider::sanitizeHtml(const QString& rawHtml)
{
    QString sanitized = rawHtml;
    static const QRegularExpression scriptRegex("<script[^>]*>.*?</script>", QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption);
    sanitized.remove(scriptRegex);
    static const QRegularExpression headerRegex("<div[^>]*class=[\"'][^\"']*code-block-header[^\"']*[\"'][^>]*>.*?</div>", QRegularExpression::DotMatchesEverythingOption);
    sanitized.remove(headerRegex);
    static const QRegularExpression copyBtnRegex("<button[^>]*class=[\"'][^\"']*copy-btn[^\"']*[\"'][^>]*>.*?</button>", QRegularExpression::DotMatchesEverythingOption);
    sanitized.remove(copyBtnRegex);
    static const QRegularExpression tableBtnRegex("<button[^>]*class=[\"'][^\"']*table-expand-btn[^\"']*[\"'][^>]*>.*?</button>", QRegularExpression::DotMatchesEverythingOption);
    sanitized.remove(tableBtnRegex);
    static const QRegularExpression toolbarRegex("<div[^>]*class=[\"'][^\"']*expandable-toolbar[^\"']*[\"'][^>]*>.*?</div>", QRegularExpression::DotMatchesEverythingOption);
    sanitized.remove(toolbarRegex);
    return sanitized.trimmed();
}

QString HtmlClipboardProvider::inlineCodeStyles(const QString& html)
{
    QString res = html;
    static const QRegularExpression preRegex("<pre(\\b[^>]*)?>", QRegularExpression::CaseInsensitiveOption);
    res.replace(preRegex, QStringLiteral("<pre style=\"background: #0d1117; border: 1px solid #1e242c; border-radius: 6px; padding: 12px; font-family: ui-monospace, SFMono-Regular, Menlo, monospace; font-size: 13px; color: #f0f6fc; overflow: auto;\"\\1>"));
    static const QRegularExpression codeRegex("<code(\\b[^>]*)?>", QRegularExpression::CaseInsensitiveOption);
    res.replace(codeRegex, QStringLiteral("<code style=\"background: #161b22; border: 1px solid #30363d; border-radius: 4px; padding: 2px 5px; font-family: ui-monospace, SFMono-Regular, Menlo, monospace; font-size: 12px; color: #f0f6fc;\"\\1>"));
    return res;
}

QString HtmlClipboardProvider::inlineTableStyles(const QString& html)
{
    QString res = html;
    static const QRegularExpression tableRegex("<table(\\b[^>]*)?>", QRegularExpression::CaseInsensitiveOption);
    res.replace(tableRegex, QStringLiteral("<table style=\"border-collapse: collapse; border: 1px solid #30363d; margin: 16px 0; width: 100%;\"\\1>"));
    static const QRegularExpression thRegex("<th(\\b[^>]*)?>", QRegularExpression::CaseInsensitiveOption);
    res.replace(thRegex, QStringLiteral("<th style=\"border: 1px solid #30363d; background: #161b22; color: #f0f6fc; padding: 8px 12px; font-weight: 600; text-align: left;\"\\1>"));
    static const QRegularExpression tdRegex("<td(\\b[^>]*)?>", QRegularExpression::CaseInsensitiveOption);
    res.replace(tdRegex, QStringLiteral("<td style=\"border: 1px solid #30363d; padding: 8px 12px; color: #c9d1d9;\"\\1>"));
    return res;
}

QString HtmlClipboardProvider::inlineBlockquoteStyles(const QString& html)
{
    QString res = html;
    static const QRegularExpression bqRegex("<blockquote(\\b[^>]*)?>", QRegularExpression::CaseInsensitiveOption);
    res.replace(bqRegex, QStringLiteral("<blockquote style=\"border-left: 3px solid #30363d; padding-left: 14px; margin: 14px 0; color: #8b949e;\"\\1>"));
    return res;
}

QString HtmlClipboardProvider::inlineStyles(const QString& htmlFragment)
{
    QString res = inlineCodeStyles(htmlFragment);
    res = inlineTableStyles(res);
    return inlineBlockquoteStyles(res);
}

QString HtmlClipboardProvider::extractInnerHtml(const QString& rawHtml)
{
    static const QRegularExpression articleRegex("<article[^>]*id=[\"']content[\"'][^>]*>(.*)</article>", QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption);
    auto match = articleRegex.match(rawHtml);
    if (match.hasMatch()) {
        return match.captured(1).trimmed();
    }
    static const QRegularExpression bodyRegex("<body[^>]*>(.*)</body>", QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption);
    auto bodyMatch = bodyRegex.match(rawHtml);
    if (bodyMatch.hasMatch()) {
        return bodyMatch.captured(1).trimmed();
    }
    return rawHtml;
}

QMimeData* HtmlClipboardProvider::createFromMarkdown(const QString& markdown)
{
    QMimeData* mimeData = new QMimeData();
    const QString cleanMarkdown = normalizeSelection(markdown);
    mimeData->setText(cleanMarkdown);
    const QString fragment = Converter::toHtmlFragment(cleanMarkdown);
    const QString sanitized = sanitizeHtml(fragment);
    const QString styled = inlineStyles(sanitized);
    mimeData->setHtml(styled);
    return mimeData;
}

QMimeData* HtmlClipboardProvider::createFromPreviewHtml(const QString& rawHtml, const QString& markdownFallback)
{
    QMimeData* mimeData = new QMimeData();
    mimeData->setText(markdownFallback);
    const QString extracted = extractInnerHtml(rawHtml);
    const QString sanitized = sanitizeHtml(extracted);
    const QString styled = inlineStyles(sanitized);
    mimeData->setHtml(styled);
    return mimeData;
}

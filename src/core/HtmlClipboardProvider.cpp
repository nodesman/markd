#include "core/HtmlClipboardProvider.h"
#include "core/Converter.h"
#include <QRegularExpression>

namespace {

int tagDelta(const QRegularExpressionMatch& tokenMatch)
{
    if (tokenMatch.capturedView(1) == QLatin1StringView("/")) return -1;
    if (tokenMatch.capturedView(0).endsWith(QLatin1StringView("/>"))) return 0;
    return 1;
}

int findBalancedCloseEnd(const QString& html, const QString& tagName, int scanPos)
{
    const QRegularExpression tokenRegex(QStringLiteral("<(/?)%1\\b[^>]*>").arg(tagName), QRegularExpression::CaseInsensitiveOption);
    int depth = 1;
    int pos = scanPos;
    while (depth > 0) {
        auto match = tokenRegex.match(html, pos);
        if (!match.hasMatch()) return html.length();
        depth += tagDelta(match);
        pos = match.capturedEnd(0);
    }
    return pos;
}

QString combineCssRules(const QString& baseRules, const QString& existingRules)
{
    const QString trimmedBase = baseRules.trimmed();
    const QString trimmedExisting = existingRules.trimmed();
    if (trimmedExisting.isEmpty()) return trimmedBase;
    if (trimmedBase.endsWith(QLatin1Char(';'))) return trimmedBase + QLatin1Char(' ') + trimmedExisting;
    return trimmedBase + QStringLiteral("; ") + trimmedExisting;
}

QString rewriteOpeningTagWithStyle(const QString& fullTag, const QString& cssRules)
{
    static const QRegularExpression styleAttrRegex(
        QStringLiteral("\\bstyle\\s*=\\s*([\"'])(.*?)\\1"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    auto styleMatch = styleAttrRegex.match(fullTag);
    if (styleMatch.hasMatch()) {
        const QString merged = combineCssRules(cssRules, styleMatch.captured(2));
        QString updated = fullTag;
        updated.replace(styleMatch.capturedStart(0), styleMatch.capturedLength(0), QStringLiteral("style=\"%1\"").arg(merged));
        return updated;
    }
    if (fullTag.endsWith(QLatin1StringView("/>"))) {
        return fullTag.left(fullTag.length() - 2) + QStringLiteral(" style=\"%1\"/>").arg(cssRules);
    }
    const int closeBracket = fullTag.lastIndexOf(QLatin1Char('>'));
    return fullTag.left(closeBracket) + QStringLiteral(" style=\"%1\">").arg(cssRules);
}

QString rewriteMatchesWithStyle(const QString& html, const QString& pattern, const QString& cssRules)
{
    QString result = html;
    const QRegularExpression regex(pattern, QRegularExpression::CaseInsensitiveOption);
    int pos = 0;
    auto match = regex.match(result, pos);
    while (match.hasMatch()) {
        const QString replacement = rewriteOpeningTagWithStyle(match.captured(0), cssRules);
        result.replace(match.capturedStart(0), match.capturedLength(0), replacement);
        pos = match.capturedStart(0) + replacement.length();
        match = regex.match(result, pos);
    }
    return result;
}

QString inlineClassColor(const QString& html, const QString& className, const QString& color) {
    const QString pattern = QStringLiteral("<span\\b[^>]*\\bclass=[\"'][^\"']*(?<![\\w-])%1(?![\\w-])[^\"']*[\"'][^>]*>").arg(QRegularExpression::escape(className));
    return rewriteMatchesWithStyle(html, pattern, QStringLiteral("color: %1;").arg(color));
}

} // namespace

QString HtmlClipboardProvider::normalizeSelection(const QString& rawText) {
    QString normalized = rawText;
    normalized.replace(QChar(0x2029), QLatin1Char('\n'));
    normalized.replace(QChar(0x2028), QLatin1Char('\n'));
    return normalized;
}

QString HtmlClipboardProvider::stripElementsByClass(const QString& html, const QString& tagName, const QString& className) {
    QString result = html;
    const QString pattern = QStringLiteral("<%1\\b[^>]*\\bclass=[\"'][^\"']*(?<![\\w-])%2(?![\\w-])[^\"']*[\"'][^>]*>").arg(tagName, QRegularExpression::escape(className));
    const QRegularExpression openRegex(pattern, QRegularExpression::CaseInsensitiveOption);
    auto match = openRegex.match(result);
    while (match.hasMatch()) {
        const int startIdx = match.capturedStart(0);
        const bool selfClosing = match.capturedView(0).endsWith(QLatin1StringView("/>"));
        const int endIdx = selfClosing ? match.capturedEnd(0) : findBalancedCloseEnd(result, tagName, match.capturedEnd(0));
        result.remove(startIdx, endIdx - startIdx);
        match = openRegex.match(result, startIdx);
    }
    return result;
}

QString HtmlClipboardProvider::sanitizeExportHtml(const QString& rawHtml) {
    QString sanitized = rawHtml;
    static const QRegularExpression scriptRegex(QStringLiteral("<script[^>]*>.*?</script>"), QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption);
    sanitized.remove(scriptRegex);
    sanitized = stripElementsByClass(sanitized, QStringLiteral("button"), QStringLiteral("copy-btn"));
    sanitized = stripElementsByClass(sanitized, QStringLiteral("button"), QStringLiteral("table-expand-btn"));
    sanitized = stripElementsByClass(sanitized, QStringLiteral("div"), QStringLiteral("expandable-toolbar"));
    sanitized = stripElementsByClass(sanitized, QStringLiteral("div"), QStringLiteral("fullview-overlay"));
    sanitized = stripElementsByClass(sanitized, QStringLiteral("div"), QStringLiteral("fullview-toolbar"));
    return sanitized.trimmed();
}

QString HtmlClipboardProvider::sanitizeHtml(const QString& rawHtml) {
    QString sanitized = sanitizeExportHtml(rawHtml);
    sanitized = stripElementsByClass(sanitized, QStringLiteral("div"), QStringLiteral("code-block-header"));
    sanitized = stripElementsByClass(sanitized, QStringLiteral("span"), QStringLiteral("katex-mathml"));
    return sanitized.trimmed();
}

QString HtmlClipboardProvider::mergeTagStyle(const QString& html, const QString& tagName, const QString& cssRules) {
    return rewriteMatchesWithStyle(html, QStringLiteral("<%1\\b[^>]*>").arg(tagName), cssRules);
}

QString HtmlClipboardProvider::inlineCodeStyles(const QString& html) {
    const QString preRules = QStringLiteral("background: #0d1117; background-color: #0d1117; color: #f0f6fc; padding: 14px 16px; border-radius: 6px; border: 1px solid #1e242c; font-family: 'SF Mono', Menlo, Consolas, monospace; font-size: 13px; overflow-x: auto;");
    const QString pillCodeRules = QStringLiteral("background: #161b22; background-color: #161b22; color: #f0f6fc; padding: 2px 6px; border-radius: 4px; border: 1px solid #30363d; font-family: 'SF Mono', Menlo, monospace; font-size: 0.9em;");
    const QString innerCodeRules = QStringLiteral("background: transparent; border: none; padding: 0; color: inherit;");
    QString res = mergeTagStyle(html, QStringLiteral("pre"), preRules);
    static const QRegularExpression preCodeOpenRegex(QStringLiteral("(<pre\\b[^>]*>\\s*)<code\\b"), QRegularExpression::CaseInsensitiveOption);
    res.replace(preCodeOpenRegex, QStringLiteral("\\1<precode"));
    res = mergeTagStyle(res, QStringLiteral("code"), pillCodeRules);
    res = mergeTagStyle(res, QStringLiteral("precode"), innerCodeRules);
    return res.replace(QLatin1StringView("<precode"), QLatin1StringView("<code"), Qt::CaseInsensitive);
}

QString HtmlClipboardProvider::inlineSyntaxTokens(const QString& html) {
    QString res = inlineClassColor(html, QStringLiteral("hljs-keyword"), QStringLiteral("#ff7b72"));
    res = inlineClassColor(res, QStringLiteral("hljs-type"), QStringLiteral("#ff7b72"));
    res = inlineClassColor(res, QStringLiteral("hljs-string"), QStringLiteral("#a5d6ff"));
    res = inlineClassColor(res, QStringLiteral("hljs-regexp"), QStringLiteral("#a5d6ff"));
    res = inlineClassColor(res, QStringLiteral("hljs-title"), QStringLiteral("#d2a8ff"));
    res = inlineClassColor(res, QStringLiteral("hljs-attr"), QStringLiteral("#79c0ff"));
    res = inlineClassColor(res, QStringLiteral("hljs-number"), QStringLiteral("#79c0ff"));
    res = inlineClassColor(res, QStringLiteral("hljs-literal"), QStringLiteral("#79c0ff"));
    res = inlineClassColor(res, QStringLiteral("hljs-comment"), QStringLiteral("#8b949e"));
    return inlineClassColor(res, QStringLiteral("hljs-built_in"), QStringLiteral("#ffa657"));
}

QString HtmlClipboardProvider::inlineTableStyles(const QString& html) {
    QString res = mergeTagStyle(html, QStringLiteral("table"), QStringLiteral("border-collapse: collapse; border: 1px solid #30363d; margin: 16px 0; width: 100%;"));
    res = mergeTagStyle(res, QStringLiteral("th"), QStringLiteral("border: 1px solid #30363d; background: #161b22; color: #f0f6fc; padding: 8px 12px; font-weight: 600; text-align: left;"));
    return mergeTagStyle(res, QStringLiteral("td"), QStringLiteral("border: 1px solid #30363d; padding: 8px 12px; color: #c9d1d9;"));
}

QString HtmlClipboardProvider::inlineBlockquoteStyles(const QString& html) {
    return mergeTagStyle(html, QStringLiteral("blockquote"), QStringLiteral("border-left: 3px solid #30363d; padding-left: 14px; margin: 14px 0; color: #8b949e;"));
}

QString HtmlClipboardProvider::inlineStyles(const QString& htmlFragment) {
    QString res = inlineCodeStyles(htmlFragment);
    res = inlineSyntaxTokens(res);
    res = inlineTableStyles(res);
    return inlineBlockquoteStyles(res);
}

QString HtmlClipboardProvider::extractInnerHtml(const QString& rawHtml) {
    static const QRegularExpression articleRegex(QStringLiteral("<article[^>]*id=[\"']content[\"'][^>]*>(.*)</article>"), QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption);
    auto match = articleRegex.match(rawHtml);
    if (match.hasMatch()) return match.captured(1).trimmed();
    static const QRegularExpression bodyRegex(QStringLiteral("<body[^>]*>(.*)</body>"), QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption);
    auto bodyMatch = bodyRegex.match(rawHtml);
    if (bodyMatch.hasMatch()) return bodyMatch.captured(1).trimmed();
    return rawHtml;
}

QMimeData* HtmlClipboardProvider::createFromMarkdown(const QString& markdown) {
    QMimeData* mimeData = new QMimeData();
    const QString cleanMarkdown = normalizeSelection(markdown);
    mimeData->setText(cleanMarkdown);
    const QString sanitized = sanitizeHtml(Converter::toHtmlFragment(cleanMarkdown));
    mimeData->setHtml(inlineStyles(sanitized));
    return mimeData;
}

QMimeData* HtmlClipboardProvider::createFromPreviewHtml(const QString& rawHtml, const QString& markdownFallback) {
    QMimeData* mimeData = new QMimeData();
    mimeData->setText(markdownFallback);
    const QString sanitized = sanitizeHtml(extractInnerHtml(rawHtml));
    mimeData->setHtml(inlineStyles(sanitized));
    return mimeData;
}

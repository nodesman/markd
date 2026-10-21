#include "core/Converter.h"
#include "core/HtmlClipboardProvider.h"
#include <QFile>
#include <QTextDocument>
#include <QRegularExpression>
#include <QStringList>

namespace {

QString readResourceText(const QString& resourcePath)
{
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    return QString::fromUtf8(file.readAll());
}

QString stripRelativeFontFaces(const QString& css)
{
    QString cleaned = css;
    static const QRegularExpression fontFaceRegex(
        QStringLiteral("@font-face\\s*\\{[^}]*\\}"),
        QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption);
    cleaned.remove(fontFaceRegex);
    return cleaned;
}

} // namespace

QString Converter::escapeHtml(const QString& unescaped)
{
    QString escaped = unescaped.toHtmlEscaped();
    escaped.replace(QLatin1Char('\''), QLatin1StringView("&#39;"));
    return escaped;
}

QString Converter::toHtmlFragment(const QString& markdown)
{
    QString shielded = markdown;
    QStringList mathBlocks;
    static const QRegularExpression mathRegex(
        QStringLiteral("\\$\\$(.+?)\\$\\$|(?<!\\$)\\$([^\\$\\n]+?)\\$(?!\\$)"),
        QRegularExpression::DotMatchesEverythingOption);
    QRegularExpressionMatchIterator it = mathRegex.globalMatch(markdown);
    while (it.hasNext()) {
        mathBlocks.append(it.next().captured(0));
    }
    for (int i = 0; i < mathBlocks.size(); ++i) {
        int pos = shielded.indexOf(mathBlocks.at(i));
        if (pos != -1) {
            shielded.replace(pos, mathBlocks.at(i).length(), QStringLiteral("MARKDMATHSHIELD%1END").arg(i));
        }
    }

    QTextDocument doc;
    doc.setMarkdown(shielded, QTextDocument::MarkdownDialectGitHub);
    QString html = doc.toHtml();
    static const QRegularExpression bodyRegex(
        QStringLiteral("<body[^>]*>(.*)</body>"),
        QRegularExpression::DotMatchesEverythingOption);
    auto match = bodyRegex.match(html);
    QString fragment = match.hasMatch() ? match.captured(1).trimmed() : html;
    for (int i = 0; i < mathBlocks.size(); ++i) {
        fragment.replace(QStringLiteral("MARKDMATHSHIELD%1END").arg(i), mathBlocks.at(i).toHtmlEscaped());
    }
    return fragment;
}

QString Converter::toFullHtmlDocument(const QString& markdown)
{
    const QString bodyHtml = toHtmlFragment(markdown);
    return QString(
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head>\n"
        "    <meta charset=\"utf-8\">\n"
        "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
        "    <style>\n"
        "        html, body {\n"
        "            background-color: #050505 !important;\n"
        "            color: #f0f6fc;\n"
        "            margin: 0;\n"
        "            padding: 0;\n"
        "        }\n"
        "    </style>\n"
        "    <link rel=\"stylesheet\" href=\"qrc:/web/styles/obsidian.css\">\n"
        "    <link rel=\"stylesheet\" href=\"qrc:/web/vendor/highlight/github-dark.min.css\">\n"
        "    <link rel=\"stylesheet\" href=\"qrc:/web/vendor/katex/katex.min.css\">\n"
        "</head>\n"
        "<body style=\"background-color: #050505;\">\n"
        "    <article id=\"content\" class=\"markdown-rendered\">\n"
        "%1\n"
        "    </article>\n"
        "</body>\n"
        "</html>"
    ).arg(bodyHtml);
}

QString Converter::bundleStandaloneHtml(const QString& rawDomOrFragment, const QString& title)
{
    const QString innerHtml = HtmlClipboardProvider::extractInnerHtml(rawDomOrFragment);
    const QString sanitizedBody = HtmlClipboardProvider::sanitizeExportHtml(innerHtml);
    const QString obsidianCss = readResourceText(QStringLiteral(":/web/styles/obsidian.css"));
    const QString highlightCss = readResourceText(QStringLiteral(":/web/vendor/highlight/github-dark.min.css"));
    const QString katexCss = stripRelativeFontFaces(readResourceText(QStringLiteral(":/web/vendor/katex/katex.min.css")));
    const QString combinedCss = obsidianCss + QLatin1Char('\n') + highlightCss + QLatin1Char('\n') + katexCss;
    const QString safeTitle = escapeHtml(title.isEmpty() ? QStringLiteral("Markd Document") : title);

    return QStringLiteral(
        "<!DOCTYPE html><html lang=\"en\">\n"
        "<head>\n"
        "    <meta charset=\"utf-8\">\n"
        "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
        "    <title>%1</title>\n"
        "    <style>\n%2\n    </style>\n"
        "</head>\n"
        "<body style=\"background-color: #050505; color: #f0f6fc; margin: 0; padding: 0;\">\n"
        "    <article id=\"content\" class=\"markdown-rendered\">\n"
        "%3\n"
        "    </article>\n"
        "</body>\n"
        "</html>"
    ).arg(safeTitle, combinedCss, sanitizedBody);
}


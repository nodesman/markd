#include "Converter.h"
#include <QTextDocument>
#include <QRegularExpression>
#include <QStringList>

QString Converter::escapeHtml(const QString& unescaped)
{
    QString escaped = unescaped.toHtmlEscaped();
    escaped.replace(QLatin1Char('\''), QLatin1StringView("&#39;"));
    return escaped;
}

QString Converter::toHtmlFragment(const QString& markdown)
{
    // Use Qt's built-in Markdown-to-HTML parser (CommonMark / GitHub-flavored markdown)
    QTextDocument doc;
    doc.setMarkdown(markdown, QTextDocument::MarkdownDialectGitHub);
    
    // Extract body content or export HTML
    QString html = doc.toHtml();
    
    // Extract inner body content if needed
    static const QRegularExpression bodyRegex("<body[^>]*>(.*)</body>", QRegularExpression::DotMatchesEverythingOption);
    auto match = bodyRegex.match(html);
    if (match.hasMatch()) {
        return match.captured(1).trimmed();
    }
    
    return html;
}

QString Converter::toFullHtmlDocument(const QString& markdown)
{
    QString bodyHtml = toHtmlFragment(markdown);
    
    QString fullDoc = QString(
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
        "    <link rel=\"stylesheet\" href=\"qrc:/web/css/obsidian.css\">\n"
        "    <link rel=\"stylesheet\" href=\"qrc:/web/css/style.css\">\n"
        "    <link rel=\"stylesheet\" href=\"qrc:/web/js/github-dark.min.css\">\n"
        "    <link rel=\"stylesheet\" href=\"qrc:/web/js/katex.min.css\">\n"
        "</head>\n"
        "<body style=\"background-color: #050505;\">\n"
        "    <article id=\"content\">\n"
        "%1\n"
        "    </article>\n"
        "</body>\n"
        "</html>"
    ).arg(bodyHtml);

    return fullDoc;
}

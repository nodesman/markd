#include "parser/TocParser.h"
#include <QStringList>
#include <QRegularExpression>

QString TocParser::cleanHeadingText(const QString& raw)
{
    QString text = raw.trimmed();

    // 1. Strip trailing closing hashes (e.g. '## Title ##' -> 'Title')
    text.remove(QRegularExpression(R"(\s+#+\s*$)"));

    // 2. Strip images: ![alt](url) -> alt
    text.replace(QRegularExpression(R"(!\[([^\]]*)\]\([^\)]+\))"), QStringLiteral(R"(\1)"));

    // 3. Strip links: [text](url) -> text
    text.replace(QRegularExpression(R"(\[([^\]]+)\]\([^\)]+\))"), QStringLiteral(R"(\1)"));

    // 4. Strip inline math delimiters: $E=mc^2$ -> E=mc^2
    // Must be processed before HTML tags so math inequalities like $x < y$ are preserved
    text.replace(QRegularExpression(R"(\$([^\$]+)\$)"), QStringLiteral(R"(\1)"));

    // 5. Strip inline HTML tags: <span>b</span> -> b (require valid ASCII tag name starting with [a-zA-Z])
    text.remove(QRegularExpression(R"(</?[a-zA-Z][^>]*>)"));

    // 6. Strip balanced markdown formatting delimiters without breaking snake_case identifiers
    text.replace(QRegularExpression(R"(\*\*([^*]+)\*\*)"), QStringLiteral(R"(\1)")); // Bold **text**
    text.replace(QRegularExpression(R"(\*([^*]+)\*)"), QStringLiteral(R"(\1)"));     // Italic *text*
    text.replace(QRegularExpression(R"(~~([^~]+)~~)"), QStringLiteral(R"(\1)"));     // Strikethrough ~~text~~
    text.replace(QRegularExpression(R"(`([^`]+)`)"), QStringLiteral(R"(\1)"));       // Inline code `text`

    return text.trimmed();
}

QString TocParser::slugify(const QString& rawTitle, QMap<QString, int>& seenSlugs)
{
    // Matches render.js & markdownItAnchor:
    // s.trim().toLowerCase().replace(/\s+/g, '-').replace(/[^\w\u4e00-\u9fff-]/g, '').replace(/--+/g, '-').replace(/^-|-$/g, '')
    QString s = rawTitle.trimmed().toLower();
    s.replace(QRegularExpression(R"(\s+)"), QStringLiteral("-"));
    s.replace(QRegularExpression(R"([^a-zA-Z0-9_\x{4e00}-\x{9fff}-])"), QStringLiteral(""));
    s.replace(QRegularExpression(R"(--+)"), QStringLiteral("-"));
    s.replace(QRegularExpression(R"(^-+|-$)"), QStringLiteral(""));

    if (s.isEmpty()) {
        s = QStringLiteral("heading");
    }

    // markdownItAnchor duplicate disambiguation:
    // first occurrence: slug
    // subsequent occurrences: slug-1, slug-2, ...
    QString uniqueSlug = s;
    if (seenSlugs.contains(s)) {
        int count = seenSlugs[s];
        do {
            uniqueSlug = QStringLiteral("%1-%2").arg(s).arg(count);
            count++;
        } while (seenSlugs.contains(uniqueSlug));
        seenSlugs[s] = count;
        seenSlugs[uniqueSlug] = 1;
    } else {
        seenSlugs[s] = 1;
    }

    return uniqueSlug;
}

std::vector<TocItem> TocParser::extractHeadings(const QString& markdown)
{
    ++s_extractCallCount;
    std::vector<TocItem> items;
    QMap<QString, int> seenSlugs;

    // Normalize CRLF to prevent regex anchor failure on Windows line endings
    QString normalized = markdown;
    normalized.remove(QLatin1Char('\r'));
    QStringList lines = normalized.split(QLatin1Char('\n'));

    bool inFencedCode = false;
    QString fenceMarker;
    bool inHtmlComment = false;

    // ATX heading regex: strictly requires at least one whitespace after 1-6 hashes (#)
    QRegularExpression atxRegex(R"(^(#{1,6})[ \t]+(.+)$)");
    // CommonMark §4.5: opening fence may have up to 3 spaces of indentation, 3+ backticks or tildes
    QRegularExpression fenceStartRegex(R"(^[ ]{0,3}(`{3,}|~{3,})(.*)$)");
    // CommonMark §4.5: closing fence may have up to 3 spaces of indentation, same char, no info string
    QRegularExpression fenceEndRegex(R"(^[ ]{0,3}(`{3,}|~{3,})[ \t]*$)");
    QRegularExpression htmlCommentStart(R"(<!--)");
    QRegularExpression htmlCommentEnd(R"(-->)");
    QRegularExpression setextH1Regex(R"(^[ \t]*={3,}[ \t]*$)");
    QRegularExpression setextH2Regex(R"(^[ \t]*-{3,}[ \t]*$)");
    // List item check: marker (*, -, +, or digits.) followed by space/tab
    QRegularExpression listItemRegex(R"(^([*+-]|\d+[.)])[ \t]+)");
    // Thematic break regex (3 or more *, -, or _ with optional spaces)
    QRegularExpression thematicBreakRegex(R"(^[ \t]*([*-_])[ \t]*(?:\1[ \t]*){2,}[ \t]*$)");

    for (int i = 0; i < lines.size(); ++i) {
        const QString& line = lines[i];

        // 1. Check multi-line HTML comments
        if (!inFencedCode) {
            if (inHtmlComment) {
                if (line.contains(htmlCommentEnd)) {
                    inHtmlComment = false;
                }
                continue;
            } else if (line.trimmed().startsWith(QStringLiteral("<!--"))) {
                if (!line.contains(htmlCommentEnd)) {
                    inHtmlComment = true;
                }
                continue;
            }
        }

        // 2. Check fenced code blocks (``` or ~~~) with CommonMark §4.5 rules
        if (inFencedCode) {
            QRegularExpressionMatch endMatch = fenceEndRegex.match(line);
            if (endMatch.hasMatch()) {
                QString marker = endMatch.captured(1);
                // Must use same fence character and length >= opening fence length
                if (marker.at(0) == fenceMarker.at(0) && marker.length() >= fenceMarker.length()) {
                    inFencedCode = false;
                    fenceMarker.clear();
                }
            }
            continue; // Always skip lines inside fenced code
        } else {
            QRegularExpressionMatch startMatch = fenceStartRegex.match(line);
            if (startMatch.hasMatch()) {
                QString marker = startMatch.captured(1);
                QString info = startMatch.captured(2);
                // CommonMark §4.5: backtick fence info string cannot contain backticks
                if (marker.startsWith(QLatin1Char('`')) && info.contains(QLatin1Char('`'))) {
                    // Not a valid fence
                } else {
                    inFencedCode = true;
                    fenceMarker = marker;
                    continue;
                }
            }
        }

        // 3. CommonMark 4-space or tab indented code block check
        if (line.startsWith(QLatin1String("    ")) || line.startsWith(QLatin1Char('\t'))) {
            continue;
        }

        QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }

        // 4. Check Setext Headings (underlined with === for H1 or --- for H2)
        if (i + 1 < lines.size()) {
            const QString& nextLine = lines[i + 1];
            if (!inHtmlComment) {
                bool isSetextH1 = setextH1Regex.match(nextLine).hasMatch();
                bool isSetextH2 = setextH2Regex.match(nextLine).hasMatch();

                if (isSetextH1 || isSetextH2) {
                    // Disallow hr / thematic breaks, blockquotes, ATX headings, or list items
                    bool isDisallowed = trimmed.startsWith(QLatin1Char('#')) ||
                                        trimmed.startsWith(QLatin1Char('>')) ||
                                        thematicBreakRegex.match(trimmed).hasMatch() ||
                                        listItemRegex.match(trimmed).hasMatch();
                    if (!isDisallowed) {
                        int level = isSetextH1 ? 1 : 2;
                        QString cleanTitle = cleanHeadingText(trimmed);
                        QString slug = slugify(cleanTitle, seenSlugs);
                        items.push_back({level, i + 1, cleanTitle, slug});
                        ++i; // Skip underline line
                        continue;
                    }
                }
            }
        }

        // 5. Check Blockquote headings (> ## Heading)
        QString candidate = trimmed;
        while (candidate.startsWith(QLatin1Char('>'))) {
            candidate = candidate.mid(1).trimmed();
        }

        // 6. Check ATX Headings (1-6 '#' followed by whitespace)
        QRegularExpressionMatch atxMatch = atxRegex.match(candidate);
        if (atxMatch.hasMatch()) {
            int level = atxMatch.captured(1).length();
            QString rawTitle = atxMatch.captured(2);
            QString cleanTitle = cleanHeadingText(rawTitle);
            QString slug = slugify(cleanTitle, seenSlugs);

            items.push_back({level, i + 1, cleanTitle, slug});
        }
    }

    return items;
}

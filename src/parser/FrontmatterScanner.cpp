#include "parser/FrontmatterScanner.h"
#include <QRegularExpression>

namespace {

bool isOpeningFence(const QString& rawFirstLine)
{
    QString line = rawFirstLine;
    if (line.startsWith(QChar(0xFEFF))) line.remove(0, 1);
    static const QRegularExpression openRegex(QStringLiteral(R"(^---[ \t]*$)"));
    return openRegex.match(line).hasMatch();
}

bool isClosingFence(const QString& line)
{
    static const QRegularExpression closeRegex(QStringLiteral(R"(^(---|\.\.\.)[ \t]*$)"));
    return closeRegex.match(line).hasMatch();
}

int findClosingFenceLine(const QStringList& lines)
{
    for (int i = 1; i < lines.size(); ++i) {
        if (isClosingFence(lines.at(i))) return i;
    }
    return -1;
}

bool isIgnorableYamlLine(const QString& line)
{
    const QString trimmed = line.trimmed();
    return trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#'));
}

bool isTopLevelKeyLine(const QString& line)
{
    static const QRegularExpression keyRegex(QStringLiteral(R"(^[A-Za-z0-9_.-]+[ \t]*:)"));
    return keyRegex.match(line).hasMatch();
}

bool isMarkdownHeadingLine(const QString& line)
{
    static const QRegularExpression headingRegex(QStringLiteral(R"(^#{1,6}[ \t]+)"));
    return headingRegex.match(line.trimmed()).hasMatch();
}

int findFirstNonEmptyLine(const QStringList& lines, int startIndex, int endExclusive)
{
    for (int i = startIndex; i < endExclusive; ++i) {
        if (!lines.at(i).trimmed().isEmpty()) return i;
    }
    return -1;
}

int findFirstContentLine(const QStringList& lines, int closeIndex)
{
    for (int i = 1; i < closeIndex; ++i) {
        if (!isIgnorableYamlLine(lines.at(i))) return i;
    }
    return -1;
}

bool isHeadingFollowedByProseAt(const QStringList& lines, int index, int closeIndex)
{
    if (!isMarkdownHeadingLine(lines.at(index))) return false;
    const int nextIdx = findFirstNonEmptyLine(lines, index + 1, closeIndex);
    if (nextIdx < 0) return false;
    return !isIgnorableYamlLine(lines.at(nextIdx)) && !isTopLevelKeyLine(lines.at(nextIdx));
}

bool hasHeadingFollowedByProse(const QStringList& lines, int closeIndex)
{
    for (int i = 1; i < closeIndex; ++i) {
        if (isHeadingFollowedByProseAt(lines, i, closeIndex)) return true;
    }
    return false;
}

bool containsTopLevelKey(const QStringList& lines, int startIndex, int closeIndex)
{
    for (int i = startIndex; i < closeIndex; ++i) {
        if (isTopLevelKeyLine(lines.at(i))) return true;
    }
    return false;
}

bool isPlausibleFrontmatterBlock(const QStringList& lines, int closeIndex)
{
    if (hasHeadingFollowedByProse(lines, closeIndex)) return false;
    const int firstContent = findFirstContentLine(lines, closeIndex);
    if (firstContent < 0) return true;
    return containsTopLevelKey(lines, firstContent, closeIndex);
}

} // namespace

int FrontmatterScanner::bodyStartLine(const QStringList& normalizedLines)
{
    if (normalizedLines.size() < 2) return 0;
    if (!isOpeningFence(normalizedLines.at(0))) return 0;
    const int closeIndex = findClosingFenceLine(normalizedLines);
    if (closeIndex < 0) return 0;
    if (!isPlausibleFrontmatterBlock(normalizedLines, closeIndex)) return 0;
    return closeIndex + 1;
}

QString FrontmatterScanner::strip(const QString& markdown)
{
    QString normalized = markdown;
    normalized.remove(QLatin1Char('\r'));
    const QStringList lines = normalized.split(QLatin1Char('\n'));
    const int startLine = bodyStartLine(lines);
    if (startLine <= 0) return markdown;
    return lines.mid(startLine).join(QLatin1Char('\n'));
}

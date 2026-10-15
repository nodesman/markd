#include "core/TelemetryEngine.h"
#include <cmath>
#include <QLocale>

QString TelemetryEngine::stripMarkdownSyntax(const QString& markdown) {
    QString text = markdown;

    // 1. Strip HTML comments
    text.remove(QRegularExpression(R"(<!--[\s\S]*?-->)"));

    // 2. Shield math blocks: Display math ($$...$$) stripped BEFORE inline math ($...$)
    text.remove(QRegularExpression(R"(\$\$[\s\S]*?\$\$)"));

    // 3. Strip fenced code block fence lines
    text.remove(QRegularExpression(R"((?m)^[ ]{0,3}(`{3,}|~{3,}).*$)"));

    // 4. Strip inline math delimiters (preserve math formula text)
    text.replace(QRegularExpression(R"(\$([^\$]+)\$)"), QStringLiteral(R"(\1)"));

    // 5. Strip images
    text.remove(QRegularExpression(R"(!\[([^\]]*)\]\([^\)]+\))"));

    // 6. Strip links (preserve anchor link text)
    text.replace(QRegularExpression(R"(\[([^\]]+)\]\([^\)]+\))"), QStringLiteral(R"(\1)"));

    // 7. Strip inline HTML tags
    text.remove(QRegularExpression(R"(</?[a-zA-Z][^>]*>)"));

    // 8. Strip ATX headings
    text.remove(QRegularExpression(R"((?m)^#{1,6}[ \t]+)"));

    // 9. Strip blockquotes
    text.remove(QRegularExpression(R"((?m)^[ \t]*>[ \t]*)"));

    // 10. Strip formatting delimiters
    text.replace(QRegularExpression(R"(\*\*([^*]+)\*\*)"), QStringLiteral(R"(\1)"));
    text.replace(QRegularExpression(R"(\*([^*]+)\*)"), QStringLiteral(R"(\1)"));
    text.replace(QRegularExpression(R"(~~([^~]+)~~)"), QStringLiteral(R"(\1)"));
    text.replace(QRegularExpression(R"(`([^`]+)`)"), QStringLiteral(R"(\1)"));

    return text;
}

int TelemetryEngine::countValidTokens(const QStringList& tokens, const QRegularExpression& pattern) {
    int count = 0;
    for (const QString& token : tokens) {
        count += token.contains(pattern) ? 1 : 0;
    }
    return count;
}

QString TelemetryEngine::computeReadingVelocity(int wordCount, int& outMinutes) {
    if (wordCount <= 0) {
        outMinutes = 0;
        return QStringLiteral("0 min read");
    }
    if (wordCount < 100) {
        outMinutes = 1;
        return QStringLiteral("< 1 min read");
    }
    outMinutes = static_cast<int>(std::ceil(wordCount / 200.0));
    return QStringLiteral("%1 min read").arg(outMinutes);
}

QString TelemetryEngine::readingVelocityText(int wordCount) {
    int minutes = 0;
    return computeReadingVelocity(wordCount, minutes);
}


TelemetryMetrics TelemetryEngine::calculateMetrics(const QString& markdown) {
    TelemetryMetrics metrics;
    metrics.characterCount = static_cast<int>(markdown.toUcs4().size());

    const QString stripped = stripMarkdownSyntax(markdown);
    const auto whitespacePattern = QRegularExpression(R"(\s+)", QRegularExpression::UseUnicodePropertiesOption);
    const auto alphanumericPattern = QRegularExpression(R"([\p{L}\p{N}])", QRegularExpression::UseUnicodePropertiesOption);
    const QStringList rawTokens = stripped.split(whitespacePattern, Qt::SkipEmptyParts);

    metrics.wordCount = countValidTokens(rawTokens, alphanumericPattern);
    metrics.readingVelocityText = computeReadingVelocity(metrics.wordCount, metrics.readingTimeMinutes);

    const QString wordUnit = (metrics.wordCount == 1) ? QStringLiteral("word") : QStringLiteral("words");
    metrics.formattedTelemetry = QStringLiteral("%1 %2 · %3").arg(
        QLocale().toString(metrics.wordCount),
        wordUnit,
        metrics.readingVelocityText
    );

    const QString charUnit = (metrics.characterCount == 1) ? QStringLiteral("character") : QStringLiteral("characters");
    metrics.formattedCharacters = QStringLiteral("%1 %2").arg(
        QLocale().toString(metrics.characterCount),
        charUnit
    );

    return metrics;
}

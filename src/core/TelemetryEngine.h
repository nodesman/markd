#pragma once

#include <QString>
#include <QStringList>
#include <QRegularExpression>

struct TelemetryMetrics {
    int wordCount = 0;
    int readingTimeMinutes = 0;
    int characterCount = 0;
    QString readingVelocityText;
    QString formattedTelemetry;
    QString formattedCharacters;
};

class TelemetryEngine {
public:
    static TelemetryMetrics calculateMetrics(const QString& markdown);
    static QString computeReadingVelocity(int wordCount, int& outMinutes);
    static QString readingVelocityText(int wordCount);

private:
    static QString stripMarkdownSyntax(const QString& markdown);
    static int countValidTokens(const QStringList& tokens, const QRegularExpression& pattern);
};


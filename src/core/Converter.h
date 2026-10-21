#pragma once

#include <QString>

class Converter {
public:
    static QString toHtmlFragment(const QString& markdown);
    static QString toFullHtmlDocument(const QString& markdown);
    static QString bundleStandaloneHtml(const QString& rawDomOrFragment, const QString& title = QStringLiteral("Markd Document"));
    static QString escapeHtml(const QString& unescaped);
};


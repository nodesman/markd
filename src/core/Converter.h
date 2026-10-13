#pragma once

#include <QString>

class Converter {
public:
    static QString toHtmlFragment(const QString& markdown);
    static QString toFullHtmlDocument(const QString& markdown);
    static QString escapeHtml(const QString& unescaped);
};

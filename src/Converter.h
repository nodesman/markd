#pragma once

#include <QString>

class Converter {
public:
    // Converts raw markdown string into full HTML page with styling
    static QString markdownToFullHtml(const QString& markdown, bool darkMode = true);

    // Converts raw markdown string into an HTML fragment
    static QString markdownToHtmlFragment(const QString& markdown);

    // Default stylesheet for markdown rendering (Uber-black / GitHub Dark aesthetic)
    static QString defaultCss(bool darkMode = true);
};

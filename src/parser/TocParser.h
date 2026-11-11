#pragma once

#include <QString>
#include <QMap>
#include <cstdint>
#include <vector>
#include "parser/TocItem.h"

class TocParser {
public:
    static QString cleanHeadingText(const QString& raw);
    static QString slugify(const QString& rawTitle, QMap<QString, int>& seenSlugs);
    static std::vector<TocItem> extractHeadings(const QString& markdown);
    static uint64_t extractCallCount() { return s_extractCallCount; }
    static void resetExtractCallCount() { s_extractCallCount = 0; }

private:
    inline static uint64_t s_extractCallCount = 0;
};

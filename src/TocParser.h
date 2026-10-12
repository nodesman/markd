#pragma once

#include <QString>
#include <QMap>
#include <vector>
#include "TocItem.h"

class TocParser {
public:
    static QString cleanHeadingText(const QString& raw);
    static QString slugify(const QString& rawTitle, QMap<QString, int>& seenSlugs);
    static std::vector<TocItem> extractHeadings(const QString& markdown);
};

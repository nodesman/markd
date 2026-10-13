#pragma once

#include <QString>
#include <QMetaType>

struct TocItem {
    int level = 1;          // 1 to 6
    int lineNumber = 0;     // 1-based source line
    QString title;          // Plain text with markdown formatting stripped
    QString id;             // Slugified anchor matching render.js and markdownItAnchor

    bool operator==(const TocItem& other) const {
        return level == other.level &&
               lineNumber == other.lineNumber &&
               title == other.title &&
               id == other.id;
    }
};

Q_DECLARE_METATYPE(TocItem)

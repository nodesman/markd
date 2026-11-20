#pragma once

#include <QString>
#include <QStringList>

class FrontmatterScanner
{
public:
    static int bodyStartLine(const QStringList& normalizedLines);
    static QString strip(const QString& markdown);
};

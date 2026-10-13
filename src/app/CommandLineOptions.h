#pragma once

#include <QString>
#include <QStringList>

struct CommandLineOptions {
    bool showHelp = false;
    bool showVersion = false;
    QString filePath;
    bool hasFile = false;
    QString errorMessage;
    bool parseSuccess = true;

    static CommandLineOptions parse(const QStringList& arguments);
    static QString helpText(const QString& programName = "markd");
    static QString versionText();
};

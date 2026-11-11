#pragma once

#include <QString>
#include <QStringList>

struct CommandLineOptions {
    bool showHelp = false;
    bool showVersion = false;
    bool registerDefault = false;
    QString filePath;
    QStringList filePaths;
    bool hasFile = false;
    QString errorMessage;
    bool parseSuccess = true;

    static CommandLineOptions parse(const QStringList& arguments);
    static CommandLineOptions parseFromArgv(int argc, char* argv[]);
    static QString helpText(const QString& programName = "markd");
    static QString versionText();
};

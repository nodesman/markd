#include "core/ProvenanceDetector.h"
#include <QCoreApplication>

ProvenanceResult ProvenanceDetector::detect(const QString& appPath) {
    const QString targetPath = appPath.isEmpty()
        ? QCoreApplication::applicationFilePath()
        : appPath;

    if (targetPath.contains(QStringLiteral("brew")) || targetPath.contains(QStringLiteral("Cellar"))) {
        return ProvenanceResult{
            InstallationProvenance::Homebrew,
            QStringLiteral("brew upgrade markd"),
            QString()
        };
    }

    if (targetPath.contains(QStringLiteral("node_modules")) ||
        targetPath.contains(QStringLiteral(".nvm")) ||
        targetPath.contains(QStringLiteral("fnm"))) {
        return ProvenanceResult{
            InstallationProvenance::NpmGlobal,
            QStringLiteral("npm install -g markd@latest"),
            QString()
        };
    }

    return ProvenanceResult{
        InstallationProvenance::StandaloneBinary,
        QString(),
        QStringLiteral("https://aramudan.dev/markd")
    };
}

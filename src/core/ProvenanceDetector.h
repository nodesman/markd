#pragma once

#include <QString>

enum class InstallationProvenance {
    Homebrew,
    NpmGlobal,
    AptPackage,
    LinuxAppImage,
    StandaloneBinary
};

struct ProvenanceResult {
    InstallationProvenance provenance{InstallationProvenance::StandaloneBinary};
    QString upgradeCommand;
    QString downloadUrl;
    QString targetBinaryPath;
};

class ProvenanceDetector {
public:
    static QString resolveTargetBinaryPath(const QString& appPath = QString(),
                                           const QString& appImageEnv = QString());
    static ProvenanceResult detect(const QString& appPath = QString(),
                                   const QString& appImageEnv = QString());
};

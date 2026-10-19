#pragma once

#include <QString>

enum class InstallationProvenance {
    Homebrew,
    NpmGlobal,
    StandaloneBinary
};

struct ProvenanceResult {
    InstallationProvenance provenance{InstallationProvenance::StandaloneBinary};
    QString upgradeCommand;
    QString downloadUrl;
};

class ProvenanceDetector {
public:
    static ProvenanceResult detect(const QString& appPath = QString());
};

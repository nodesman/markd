#include "core/ProvenanceDetector.h"
#include <QCoreApplication>

QString ProvenanceDetector::resolveTargetBinaryPath(const QString& appPath, const QString& appImageEnv) {
    const QString envAppImage = appImageEnv.isEmpty() ? qEnvironmentVariable("APPIMAGE") : appImageEnv;
    const QString rawPath = appPath.isEmpty() ? QCoreApplication::applicationFilePath() : appPath;
    const bool isFuseMount = appPath.isEmpty() || rawPath.contains(QStringLiteral("/.mount_"));

    if (!envAppImage.isEmpty() && isFuseMount) {
        return envAppImage;
    }
    return rawPath;
}

ProvenanceResult ProvenanceDetector::detect(const QString& appPath, const QString& appImageEnv) {
    const QString resolvedPath = resolveTargetBinaryPath(appPath, appImageEnv);
    const bool isAppImageFile = resolvedPath.endsWith(QStringLiteral(".AppImage"), Qt::CaseInsensitive);
    const bool isAppImageMount = !appImageEnv.isEmpty() && appPath.contains(QStringLiteral("/.mount_"));

    if (isAppImageFile || isAppImageMount) {
        const QString cmd = QStringLiteral("curl -fsSL https://markd.aramudan.dev/install.sh | sh");
        const QString url = QStringLiteral("https://markd.aramudan.dev");
        return ProvenanceResult{InstallationProvenance::LinuxAppImage, cmd, url, resolvedPath};
    }

    if (resolvedPath.contains(QStringLiteral("brew")) || resolvedPath.contains(QStringLiteral("Cellar"))) {
        return ProvenanceResult{InstallationProvenance::Homebrew, QStringLiteral("brew upgrade markd"), QString(), resolvedPath};
    }

    const bool isNpm = resolvedPath.contains(QStringLiteral("node_modules")) ||
        resolvedPath.contains(QStringLiteral(".nvm")) ||
        resolvedPath.contains(QStringLiteral("fnm"));
    if (isNpm) {
        return ProvenanceResult{InstallationProvenance::NpmGlobal, QStringLiteral("npm install -g markd@latest"), QString(), resolvedPath};
    }

    const bool isApt = resolvedPath == QStringLiteral("/usr/bin/markd") ||
        resolvedPath.startsWith(QStringLiteral("/usr/lib/markd"));
    if (isApt) {
        const QString aptCmd = QStringLiteral("sudo apt update && sudo apt install --only-upgrade markd");
        return ProvenanceResult{InstallationProvenance::AptPackage, aptCmd, QString(), resolvedPath};
    }

    return ProvenanceResult{InstallationProvenance::StandaloneBinary, QString(), QStringLiteral("https://markd.aramudan.dev"), resolvedPath};
}

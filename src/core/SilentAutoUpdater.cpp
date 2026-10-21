#include "core/SilentAutoUpdater.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>

#if defined(Q_OS_MACOS)
#include <sys/xattr.h>
#endif

namespace {

void clearMacQuarantineAttribute(const QString& filePath) {
#if defined(Q_OS_MACOS)
    const QByteArray utf8Path = filePath.toUtf8();
    removexattr(utf8Path.constData(), "com.apple.quarantine", 0);
#else
    Q_UNUSED(filePath);
#endif
}

bool defaultDetachedRunner(const QString& program, const QStringList& args) {
    return QProcess::startDetached(program, args);
}

bool rotateExistingTargetToBackup(const QString& targetBinaryPath, const QString& backupPath, const QString& stagedPath) {
    if (!QFile::exists(targetBinaryPath)) return true;
    QFile::remove(backupPath);
    if (QFile::rename(targetBinaryPath, backupPath)) return true;
    QFile::remove(stagedPath);
    return false;
}

} // namespace

SilentAutoUpdater::SilentAutoUpdater(QObject* parent)
    : QObject(parent)
{
}

bool SilentAutoUpdater::sweepStaleBackup(const QString& targetBinaryPath) {
    if (targetBinaryPath.isEmpty()) return false;
    const QString oldPath = targetBinaryPath + QStringLiteral(".old");
    if (!QFile::exists(oldPath)) return false;
    return QFile::remove(oldPath);
}

SilentUpdateMode SilentAutoUpdater::determineUpdateMode(
    const QString& targetBinaryPath, InstallationProvenance provenance) {
    if (provenance == InstallationProvenance::AptPackage) {
        return SilentUpdateMode::ManualCommandRequired;
    }

    const QFileInfo binInfo(targetBinaryPath);
    const QFileInfo dirInfo(binInfo.absolutePath());
    if (!dirInfo.exists() || !dirInfo.isWritable()) {
        return SilentUpdateMode::ManualCommandRequired;
    }

    if (provenance == InstallationProvenance::Homebrew ||
        provenance == InstallationProvenance::NpmGlobal) {
        return SilentUpdateMode::BackgroundPackageCommand;
    }

    return SilentUpdateMode::DirectBinarySwap;
}

QString SilentAutoUpdater::computeSha256Hex(const QByteArray& data) {
    return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}

SilentUpdateResult SilentAutoUpdater::stageAndApplyBinaryUpdate(
    const QString& targetBinaryPath,
    const QByteArray& payload,
    const QString& expectedSha256Hex,
    const QString& targetVersion) {
    SilentUpdateResult result;
    result.mode = SilentUpdateMode::DirectBinarySwap;

    if (targetBinaryPath.isEmpty() || payload.isEmpty() || expectedSha256Hex.trimmed().isEmpty()) {
        result.errorMessage = QStringLiteral("Invalid update payload or target path.");
        return result;
    }

    const QString actualSha = computeSha256Hex(payload);
    if (actualSha.compare(expectedSha256Hex.trimmed(), Qt::CaseInsensitive) != 0) {
        result.errorMessage = QStringLiteral("SHA-256 verification failed for update payload.");
        return result;
    }

    const QString stagedPath = targetBinaryPath + QStringLiteral(".new");
    const QString backupPath = targetBinaryPath + QStringLiteral(".old");
    QFile::remove(stagedPath);

    QFile stagedFile(stagedPath);
    if (!stagedFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        result.errorMessage = QStringLiteral("Unable to stage update in target directory.");
        return result;
    }
    stagedFile.write(payload);
    stagedFile.close();

    const QFileDevice::Permissions execPerms =
        QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner |
        QFileDevice::ReadGroup | QFileDevice::ExeGroup |
        QFileDevice::ReadOther | QFileDevice::ExeOther;
    stagedFile.setPermissions(execPerms);
    clearMacQuarantineAttribute(stagedPath);

    if (!rotateExistingTargetToBackup(targetBinaryPath, backupPath, stagedPath)) {
        result.errorMessage = QStringLiteral("Unable to rotate active binary to backup.");
        return result;
    }

    if (!QFile::rename(stagedPath, targetBinaryPath)) {
        QFile::rename(backupPath, targetBinaryPath);
        QFile::remove(stagedPath);
        result.errorMessage = QStringLiteral("Unable to activate staged binary.");
        return result;
    }

    clearMacQuarantineAttribute(targetBinaryPath);
    QFile::remove(backupPath);

    result.applied = true;
    result.appliedVersion = targetVersion;
    result.statusText = QStringLiteral("✓ Updated to v%1 (active on next launch)").arg(targetVersion);
    return result;
}

SilentUpdateResult SilentAutoUpdater::executePackageUpgrade(
    InstallationProvenance provenance,
    const QString& targetVersion,
    CommandRunner runner) {
    SilentUpdateResult result;
    result.mode = SilentUpdateMode::BackgroundPackageCommand;
    CommandRunner activeRunner = runner ? runner : defaultDetachedRunner;

    if (provenance == InstallationProvenance::Homebrew) {
        qputenv("HOMEBREW_NO_AUTO_UPDATE", "1");
        result.applied = activeRunner(QStringLiteral("brew"), {QStringLiteral("upgrade"), QStringLiteral("markd")});
        result.appliedVersion = targetVersion;
        const QString okMsg = QStringLiteral("✓ Updating to v%1 via Homebrew in background").arg(targetVersion);
        result.statusText = result.applied ? okMsg : QStringLiteral("Run: brew upgrade markd");
        return result;
    }

    if (provenance == InstallationProvenance::NpmGlobal) {
        const QStringList npmArgs = QStringLiteral("install -g markd@latest --no-fund --no-audit --silent").split(QLatin1Char(' '));
        result.applied = activeRunner(QStringLiteral("npm"), npmArgs);
        result.appliedVersion = targetVersion;
        const QString okMsg = QStringLiteral("✓ Updating to v%1 via NPM in background").arg(targetVersion);
        result.statusText = result.applied ? okMsg : QStringLiteral("Run: npm install -g markd@latest");
        return result;
    }

    result.mode = SilentUpdateMode::ManualCommandRequired;
    result.errorMessage = QStringLiteral("Manual package upgrade required.");
    return result;
}

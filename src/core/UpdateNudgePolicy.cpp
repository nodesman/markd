#include "core/UpdateNudgePolicy.h"
#include <algorithm>

namespace {

qint64 resolveTimestamp(qint64 nowEpochSecs) {
    if (nowEpochSecs >= 0) {
        return nowEpochSecs;
    }
    return QDateTime::currentSecsSinceEpoch();
}

void writeNudgeBaseline(QSettings& settings, const QString& cleanVersion, qint64 timestamp) {
    settings.setValue(QStringLiteral("updates/lastNudgedVersion"), cleanVersion);
    settings.setValue(QStringLiteral("updates/runsSinceLastNudge"), 0);
    settings.setValue(QStringLiteral("updates/lastNudgeEpochSecs"), timestamp);
}

} // namespace

QString UpdateNudgePolicy::normalizeVersion(const QString& version) {
    QString clean = version.trimmed();
    const int markerIdx = clean.indexOf(QStringLiteral("Update v"), 0, Qt::CaseInsensitive);
    if (markerIdx >= 0) {
        clean = clean.mid(markerIdx + 8).trimmed();
        const int spaceIdx = clean.indexOf(QLatin1Char(' '));
        if (spaceIdx > 0) clean = clean.left(spaceIdx);
    }
    while (clean.startsWith(QLatin1Char('v'), Qt::CaseInsensitive)) {
        clean.remove(0, 1);
        clean = clean.trimmed();
    }
    if (clean.contains(QLatin1Char(' '))) {
        return QString();
    }
    return clean;
}

bool UpdateNudgePolicy::evaluateAndRecordRun(
    QSettings& settings,
    const QString& latestVersion,
    qint64 nowEpochSecs) {
    const QString cleanLatest = normalizeVersion(latestVersion);
    if (cleanLatest.isEmpty()) {
        return false;
    }

    const qint64 timestamp = resolveTimestamp(nowEpochSecs);
    const QString lastNudged = normalizeVersion(
        settings.value(QStringLiteral("updates/lastNudgedVersion")).toString());

    if (lastNudged != cleanLatest) {
        writeNudgeBaseline(settings, cleanLatest, timestamp);
        return true;
    }

    const int storedRuns = std::max(0, settings.value(QStringLiteral("updates/runsSinceLastNudge"), 0).toInt());
    const int nextRuns = storedRuns + 1;
    const qint64 lastEpoch = settings.value(QStringLiteral("updates/lastNudgeEpochSecs"), 0).toLongLong();
    const bool runsCooldownExpired = (nextRuns >= kCooldownRuns);
    const bool timeCooldownExpired = (lastEpoch > 0) && ((timestamp - lastEpoch) >= kCooldownSeconds);

    if (runsCooldownExpired || timeCooldownExpired) {
        writeNudgeBaseline(settings, cleanLatest, timestamp);
        return true;
    }

    settings.setValue(QStringLiteral("updates/runsSinceLastNudge"), nextRuns);
    return false;
}

bool UpdateNudgePolicy::evaluateAndRecordRun(const QString& latestVersion, qint64 nowEpochSecs) {
    QSettings settings;
    return evaluateAndRecordRun(settings, latestVersion, nowEpochSecs);
}

void UpdateNudgePolicy::recordDismissal(
    QSettings& settings,
    const QString& version,
    qint64 nowEpochSecs) {
    const qint64 timestamp = resolveTimestamp(nowEpochSecs);
    const QString cleanVer = normalizeVersion(version);
    const QString fallbackVer = normalizeVersion(
        settings.value(QStringLiteral("updates/lastNudgedVersion")).toString());
    const QString targetVer = cleanVer.isEmpty() ? fallbackVer : cleanVer;

    settings.setValue(QStringLiteral("updates/runsSinceLastNudge"), 0);
    settings.setValue(QStringLiteral("updates/lastNudgeEpochSecs"), timestamp);
    if (!targetVer.isEmpty()) {
        settings.setValue(QStringLiteral("updates/lastNudgedVersion"), targetVer);
        settings.setValue(QStringLiteral("updates/dismissedVersion"), targetVer);
    }
}

void UpdateNudgePolicy::recordDismissal(const QString& version, qint64 nowEpochSecs) {
    QSettings settings;
    recordDismissal(settings, version, nowEpochSecs);
}

QString UpdateNudgePolicy::resolveCopyPayload(const UpdateInfo& info) {
    const QString command = info.upgradeCommand.trimmed();
    if (!command.isEmpty()) {
        return command;
    }
    const QString url = info.downloadUrl.trimmed();
    if (!url.isEmpty()) {
        return url;
    }
    return QStringLiteral("https://markd.aramudan.dev");
}

QString UpdateNudgePolicy::formatNudgeMessage(const UpdateInfo& info) {
    const QString cleanVer = normalizeVersion(info.latestVersion);
    const QString payload = resolveCopyPayload(info);
    return QStringLiteral("↑ Update v%1 available · Click to copy '%2'").arg(cleanVer, payload);
}

QString UpdateNudgePolicy::formatCopiedConfirmation(const QString& copyPayload) {
    return QStringLiteral("✓ Copied '%1' to clipboard").arg(copyPayload);
}

QString UpdateNudgePolicy::formatAboutStatus(const UpdateInfo& info) {
    if (info.state == UpdateState::UpdateAvailable) {
        return QStringLiteral("↑ Update v%1 available").arg(normalizeVersion(info.latestVersion));
    }
    return QStringLiteral("✓ Up to date");
}

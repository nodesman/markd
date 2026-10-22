#pragma once

#include <QDateTime>
#include <QSettings>
#include <QString>
#include "core/UpdateChecker.h"

class UpdateNudgePolicy {
public:
    static constexpr int kCooldownRuns = 5;
    static constexpr qint64 kCooldownSeconds = 72LL * 3600LL;
    static constexpr int kNudgeDisplayTimeoutMs = 8000;
    static constexpr int kCopiedFeedbackTimeoutMs = 2000;

    static QString normalizeVersion(const QString& version);

    static bool evaluateAndRecordRun(QSettings& settings,
                                     const QString& latestVersion,
                                     qint64 nowEpochSecs = -1);

    static bool evaluateAndRecordRun(const QString& latestVersion,
                                     qint64 nowEpochSecs = -1);

    static void recordDismissal(QSettings& settings,
                                const QString& version,
                                qint64 nowEpochSecs = -1);

    static void recordDismissal(const QString& version,
                                qint64 nowEpochSecs = -1);

    static QString resolveCopyPayload(const UpdateInfo& info);
    static QString formatNudgeMessage(const UpdateInfo& info);
    static QString formatCopiedConfirmation(const QString& copyPayload);
    static QString formatAboutStatus(const UpdateInfo& info);
};

#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringList>
#include <functional>
#include "core/ProvenanceDetector.h"
#include "core/UpdateChecker.h"

enum class SilentUpdateMode {
    None,
    DirectBinarySwap,
    BackgroundPackageCommand,
    ManualCommandRequired
};

struct SilentUpdateResult {
    bool applied{false};
    SilentUpdateMode mode{SilentUpdateMode::None};
    QString appliedVersion;
    QString statusText;
    QString errorMessage;
};

class SilentAutoUpdater : public QObject {
    Q_OBJECT
public:
    using CommandRunner = std::function<bool(const QString& program, const QStringList& args)>;

    explicit SilentAutoUpdater(QObject* parent = nullptr);
    ~SilentAutoUpdater() override = default;

    static bool sweepStaleBackup(const QString& targetBinaryPath);
    static SilentUpdateMode determineUpdateMode(const QString& targetBinaryPath,
                                                InstallationProvenance provenance);
    static QString computeSha256Hex(const QByteArray& data);

    static SilentUpdateResult stageAndApplyBinaryUpdate(const QString& targetBinaryPath,
                                                        const QByteArray& payload,
                                                        const QString& expectedSha256Hex,
                                                        const QString& targetVersion);

    static SilentUpdateResult executePackageUpgrade(InstallationProvenance provenance,
                                                    const QString& targetVersion,
                                                    CommandRunner runner = nullptr);
};

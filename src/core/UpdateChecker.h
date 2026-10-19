#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <functional>
#include <memory>
#include "core/ProvenanceDetector.h"

enum class UpdateState {
    Checking,
    UpToDate,
    UpdateAvailable,
    CheckFailed
};

struct UpdateInfo {
    UpdateState state{UpdateState::Checking};
    QString currentVersion;
    QString latestVersion;
    QString upgradeCommand;
    QString downloadUrl;
    QString errorMessage;
    InstallationProvenance provenance{InstallationProvenance::StandaloneBinary};
};

class IUpdateTransport {
public:
    virtual ~IUpdateTransport() = default;
    virtual void fetch(const QUrl& url,
                       std::function<void(bool success, const QByteArray& data, const QString& error)> callback) = 0;
};

class UpdateChecker : public QObject {
    Q_OBJECT
public:
    explicit UpdateChecker(QObject* parent = nullptr);
    explicit UpdateChecker(std::shared_ptr<IUpdateTransport> transport, QObject* parent = nullptr);
    ~UpdateChecker() override;

    void checkForUpdates();
    void setTransport(std::shared_ptr<IUpdateTransport> transport);

    static bool isNewerVersion(const QString& current, const QString& latest);
    static QList<int> parseVersion(const QString& ver);

signals:
    void checkFinished(const UpdateInfo& info);
    void checkFailed(const QString& error);

private:
    void handleResponse(bool success, const QByteArray& data, const QString& error);

    std::shared_ptr<IUpdateTransport> m_transport;
};

#include "core/UpdateChecker.h"
#include "core/Version.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>

class NetworkUpdateTransport : public IUpdateTransport {
public:
    NetworkUpdateTransport()
        : m_manager(std::make_unique<QNetworkAccessManager>())
    {
    }

    ~NetworkUpdateTransport() override {
        if (m_reply) {
            m_reply->abort();
            m_reply->deleteLater();
        }
    }

    void fetch(const QUrl& url,
               std::function<void(bool success, const QByteArray& data, const QString& error)> callback) override {
        if (m_reply) {
            m_reply->abort();
            m_reply->deleteLater();
        }

        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Markd/%1").arg(MARKD_VERSION_STR));

        m_reply = m_manager->get(request);
        QPointer<QNetworkReply> guardedReply(m_reply);
        QObject::connect(m_reply.data(), &QNetworkReply::finished, [this, guardedReply, callback]() {
            if (!guardedReply) return;
            guardedReply->deleteLater();

            if (guardedReply->error() != QNetworkReply::NoError) {
                callback(false, QByteArray(), guardedReply->errorString());
                return;
            }

            callback(true, guardedReply->readAll(), QString());
        });
    }

private:
    std::unique_ptr<QNetworkAccessManager> m_manager;
    QPointer<QNetworkReply> m_reply;
};

UpdateChecker::UpdateChecker(QObject* parent)
    : UpdateChecker(std::make_shared<NetworkUpdateTransport>(), parent)
{
}

UpdateChecker::UpdateChecker(std::shared_ptr<IUpdateTransport> transport, QObject* parent)
    : QObject(parent)
    , m_transport(std::move(transport))
{
}

UpdateChecker::~UpdateChecker() = default;

void UpdateChecker::setTransport(std::shared_ptr<IUpdateTransport> transport) {
    m_transport = std::move(transport);
}

void UpdateChecker::checkForUpdates() {
    if (!m_transport) {
        emit checkFailed(QStringLiteral("No transport configured"));
        return;
    }

    QPointer<UpdateChecker> self(this);
    const QUrl endpoint(QStringLiteral("https://updates.markd.dev/v1/check"));
    m_transport->fetch(endpoint, [self](bool success, const QByteArray& data, const QString& error) {
        if (!self) return;
        self->handleResponse(success, data, error);
    });
}

void UpdateChecker::handleResponse(bool success, const QByteArray& data, const QString& error) {
    if (!success) {
        UpdateInfo info;
        info.state = UpdateState::CheckFailed;
        info.currentVersion = QString::fromUtf8(MARKD_VERSION_STR);
        info.errorMessage = error.isEmpty() ? QStringLiteral("Unable to check for updates.") : error;
        emit checkFailed(info.errorMessage);
        emit checkFinished(info);
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(data);
    const QString latest = doc.object().value(QStringLiteral("version")).toString();
    const QString current = QString::fromUtf8(MARKD_VERSION_STR);
    const ProvenanceResult prov = ProvenanceDetector::detect();

    UpdateInfo info;
    info.currentVersion = current;
    info.latestVersion = latest.isEmpty() ? current : latest;
    info.provenance = prov.provenance;
    info.upgradeCommand = prov.upgradeCommand;
    info.downloadUrl = prov.downloadUrl;

    if (isNewerVersion(current, latest)) {
        info.state = UpdateState::UpdateAvailable;
        emit checkFinished(info);
        return;
    }

    info.state = UpdateState::UpToDate;
    emit checkFinished(info);
}

QList<int> UpdateChecker::parseVersion(const QString& ver) {
    QString clean = ver.trimmed();
    if (clean.startsWith(QLatin1Char('v'), Qt::CaseInsensitive)) {
        clean.remove(0, 1);
    }

    const int dashIdx = clean.indexOf(QLatin1Char('-'));
    if (dashIdx != -1) {
        clean = clean.left(dashIdx);
    }

    const QStringList parts = clean.split(QLatin1Char('.'));
    const int major = parts.value(0).toInt();
    const int minor = parts.value(1).toInt();
    const int patch = parts.value(2).toInt();
    return QList<int>{major, minor, patch};
}

bool UpdateChecker::isNewerVersion(const QString& current, const QString& latest) {
    if (latest.isEmpty()) {
        return false;
    }

    const QList<int> curr = parseVersion(current);
    const QList<int> next = parseVersion(latest);

    if (next.value(0) > curr.value(0)) return true;
    if (next.value(0) < curr.value(0)) return false;
    if (next.value(1) > curr.value(1)) return true;
    if (next.value(1) < curr.value(1)) return false;
    if (next.value(2) > curr.value(2)) return true;
    return false;
}

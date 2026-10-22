#include "core/UpdateChecker.h"
#include "core/Version.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QSysInfo>

class NetworkUpdateTransport : public IUpdateTransport {
public:
    NetworkUpdateTransport()
        : m_manager(new QNetworkAccessManager())
    {
    }

    ~NetworkUpdateTransport() override {
        abortActiveReply();
        if (m_manager) m_manager->deleteLater();
    }

    void fetch(
        const QUrl& url,
        std::function<void(bool, const QByteArray&, const QString&)> callback) override {
        abortActiveReply();

        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Markd/%1").arg(MARKD_VERSION_STR));
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

        m_reply = m_manager->get(request);
        QPointer<QNetworkReply> guardedReply(m_reply);
        auto onFinished = [guardedReply, callback]() { dispatchReply(guardedReply, callback); };
        QObject::connect(m_reply.data(), &QNetworkReply::finished, m_manager.data(), onFinished);
    }

private:
    void abortActiveReply() {
        if (!m_reply) return;
        QPointer<QNetworkReply> activeReply = m_reply;
        m_reply.clear();
        activeReply->setProperty("markd_aborted", true);
        if (!activeReply->isFinished()) activeReply->abort();
        activeReply->deleteLater();
    }

    static void dispatchReply(
        QPointer<QNetworkReply> guardedReply,
        const std::function<void(bool, const QByteArray&, const QString&)>& callback) {
        if (!guardedReply) return;
        guardedReply->deleteLater();
        if (guardedReply->property("markd_aborted").toBool()) return;
        const bool hasError = guardedReply->error() != QNetworkReply::NoError;
        if (hasError) return callback(false, QByteArray(), guardedReply->errorString());
        callback(true, guardedReply->readAll(), QString());
    }

    QPointer<QNetworkAccessManager> m_manager;
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

QUrl UpdateChecker::resolvedEndpointUrl() {
    const QString envUrl = qEnvironmentVariable("MARKD_UPDATE_ENDPOINT");
    return QUrl(envUrl.isEmpty()
        ? QStringLiteral("https://markd.aramudan.dev/api/v1/updates/check")
        : envUrl);
}

QString UpdateChecker::currentPlatformArtifactKey() {
#if defined(Q_OS_MACOS)
    if (QSysInfo::currentCpuArchitecture().contains(QStringLiteral("arm"), Qt::CaseInsensitive)) {
        return QStringLiteral("darwin-arm64");
    }
    return QStringLiteral("darwin-x86_64");
#elif defined(Q_OS_WIN)
    return QStringLiteral("win32-x86_64");
#else
    if (QSysInfo::currentCpuArchitecture().contains(QStringLiteral("arm"), Qt::CaseInsensitive)) {
        return QStringLiteral("linux-arm64");
    }
    return QStringLiteral("linux-x86_64");
#endif
}

void UpdateChecker::checkForUpdates() {
    if (!m_transport) {
        emit checkFailed(QStringLiteral("No transport configured"));
        return;
    }

    QPointer<UpdateChecker> self(this);
    m_transport->fetch(resolvedEndpointUrl(), [self](bool success, const QByteArray& data, const QString& error) {
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

    const QJsonObject root = QJsonDocument::fromJson(data).object();
    const QString latest = root.value(QStringLiteral("version")).toString();
    const QString current = QString::fromUtf8(MARKD_VERSION_STR);
    const ProvenanceResult prov = ProvenanceDetector::detect();
    const QJsonObject artifactObj = root.value(QStringLiteral("artifacts"))
        .toObject()
        .value(currentPlatformArtifactKey())
        .toObject();

    UpdateInfo info;
    info.currentVersion = current;
    info.latestVersion = latest.isEmpty() ? current : latest;
    info.provenance = prov.provenance;
    info.upgradeCommand = prov.upgradeCommand;
    info.downloadUrl = prov.downloadUrl;
    info.artifactUrl = artifactObj.value(QStringLiteral("url")).toString();
    info.artifactSha256 = artifactObj.value(QStringLiteral("sha256")).toString();

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
    return QList<int>{parts.value(0).toInt(), parts.value(1).toInt(), parts.value(2).toInt()};
}

bool UpdateChecker::isNewerVersion(const QString& current, const QString& latest) {
    if (latest.isEmpty()) return false;
    const QList<int> curr = parseVersion(current);
    const QList<int> next = parseVersion(latest);

    if (next.value(0) > curr.value(0)) return true;
    if (next.value(0) < curr.value(0)) return false;
    if (next.value(1) > curr.value(1)) return true;
    if (next.value(1) < curr.value(1)) return false;
    return next.value(2) > curr.value(2);
}

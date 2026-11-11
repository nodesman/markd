#include "app/SingleInstanceCoordinator.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>

namespace {

QString normalizeSingleEntry(const QString& rawEntry, const QDir& base)
{
    const QString trimmed = rawEntry.trimmed();
    if (trimmed.isEmpty()) return QString();
    const QString candidate = QFileInfo(trimmed).isAbsolute() ? trimmed : base.filePath(trimmed);
    const QFileInfo info(candidate);
    const QString canonical = info.canonicalFilePath();
    return canonical.isEmpty() ? QDir::cleanPath(info.absoluteFilePath()) : canonical;
}

bool waitForSocketConnected(QLocalSocket& socket, int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    while (socket.state() == QLocalSocket::ConnectingState && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 10);
        if (socket.waitForConnected(10)) return true;
    }
    return socket.state() == QLocalSocket::ConnectedState;
}

bool waitForAckFrame(QLocalSocket& socket, int timeoutMs)
{
    QByteArray received;
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 10);
        socket.waitForReadyRead(10);
        received.append(socket.readAll());
        if (received.contains("ACK\n")) return true;
        if (socket.state() == QLocalSocket::UnconnectedState) break;
    }
    return received.contains("ACK\n");
}

QByteArray encodePayload(const QStringList& files, bool openEmptyWindow)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("files"), QJsonArray::fromStringList(files));
    obj.insert(QStringLiteral("openEmptyWindow"), openEmptyWindow);
    return QJsonDocument(obj).toJson(QJsonDocument::Compact) + '\n';
}

} // namespace

SingleInstanceCoordinator::SingleInstanceCoordinator(const QString& socketName, QObject* parent)
    : QObject(parent)
    , m_socketName(socketName.trimmed().isEmpty() ? defaultSocketName() : socketName.trimmed())
{
}

SingleInstanceCoordinator::~SingleInstanceCoordinator()
{
    if (m_server && m_server->isListening()) m_server->close();
}

QString SingleInstanceCoordinator::defaultSocketName()
{
    const QByteArray digest = QCryptographicHash::hash(
        QDir::homePath().toUtf8(), QCryptographicHash::Sha256).toHex().left(12);
    return QStringLiteral("markd-ipc-%1").arg(QString::fromLatin1(digest));
}

bool SingleInstanceCoordinator::hasCandidateServerSocket(const QString& socketName)
{
    const QString cleanName = socketName.trimmed();
    if (cleanName.isEmpty()) return false;
#if defined(Q_OS_WIN)
    return QFileInfo::exists(QStringLiteral("\\\\.\\pipe\\%1").arg(cleanName));
#else
    return QFileInfo::exists(QDir(QDir::tempPath()).filePath(cleanName));
#endif
}

bool SingleInstanceCoordinator::tryForwardToRunningInstance(
    int& argc, char** argv, const QStringList& rawFilePaths, bool openEmptyWindowWhenNoFiles)
{
    const QString name = defaultSocketName();
    if (!hasCandidateServerSocket(name)) return false;
    QCoreApplication ipcApp(argc, argv);
    SingleInstanceCoordinator client(name);
    const QStringList normalized = normalizeFilePaths(rawFilePaths, QDir::currentPath());
    return client.forwardToPrimary(normalized, openEmptyWindowWhenNoFiles) == ForwardResult::Delivered;
}

QStringList SingleInstanceCoordinator::normalizeFilePaths(const QStringList& rawPaths, const QString& baseDir)
{
    const QDir base(baseDir.isEmpty() ? QDir::currentPath() : baseDir);
    QStringList result;
    for (const QString& raw : rawPaths) {
        const QString normalized = normalizeSingleEntry(raw, base);
        if (!normalized.isEmpty() && !result.contains(normalized)) result.append(normalized);
    }
    return result;
}

SingleInstanceCoordinator::Role SingleInstanceCoordinator::activateOrListen(
    const QStringList& rawFilePaths, bool openEmptyWindowWhenNoFiles)
{
    const QStringList normalized = normalizeFilePaths(rawFilePaths, QDir::currentPath());
    const ForwardResult forwarded = forwardToPrimary(normalized, openEmptyWindowWhenNoFiles);
    if (forwarded == ForwardResult::Delivered) {
        m_isPrimary = false;
        return Role::SecondaryForwarded;
    }
    if (forwarded == ForwardResult::NoServer) QLocalServer::removeServer(m_socketName);
    if (!m_server) {
        m_server = new QLocalServer(this);
        m_server->setSocketOptions(QLocalServer::UserAccessOption);
        connect(m_server, &QLocalServer::newConnection, this, &SingleInstanceCoordinator::handleIncomingConnections);
    }
    m_isPrimary = m_server->listen(m_socketName);
    return Role::Primary;
}

SingleInstanceCoordinator::ForwardResult SingleInstanceCoordinator::forwardToPrimary(
    const QStringList& normalizedFiles, bool openEmptyWindow) const
{
    if (!hasCandidateServerSocket(m_socketName)) return ForwardResult::NoServer;
    QLocalSocket socket;
    socket.connectToServer(m_socketName);
    if (!waitForSocketConnected(socket, 150)) return ForwardResult::NoServer;
    socket.write(encodePayload(normalizedFiles, openEmptyWindow));
    socket.flush();
    socket.waitForBytesWritten(100);
    if (!waitForAckFrame(socket, 1200)) return ForwardResult::TransportTimeout;
    socket.disconnectFromServer();
    return ForwardResult::Delivered;
}

void SingleInstanceCoordinator::handleIncomingConnections()
{
    while (m_server && m_server->hasPendingConnections()) {
        QLocalSocket* client = m_server->nextPendingConnection();
        if (!client) continue;
        connect(client, &QLocalSocket::readyRead, this, [this, client]() { handleSocketReadyRead(client); });
        connect(client, &QLocalSocket::disconnected, client, &QObject::deleteLater);
        if (client->bytesAvailable() > 0) handleSocketReadyRead(client);
    }
}

void SingleInstanceCoordinator::handleSocketReadyRead(QLocalSocket* client)
{
    if (!client || !client->canReadLine()) return;
    const QByteArray line = client->readLine().trimmed();
    client->write("ACK\n");
    client->flush();
    const QJsonDocument doc = QJsonDocument::fromJson(line);
    if (!doc.isObject()) return;
    const QJsonObject obj = doc.object();
    QStringList files;
    for (const QJsonValue& val : obj.value(QStringLiteral("files")).toArray()) {
        const QString path = val.toString().trimmed();
        if (!path.isEmpty()) files.append(path);
    }
    if (!files.isEmpty()) {
        emit activationRequested(files);
        return;
    }
    if (obj.value(QStringLiteral("openEmptyWindow")).toBool()) emit emptyWindowRequested();
}

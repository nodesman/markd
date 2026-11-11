#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class QLocalServer;
class QLocalSocket;

class SingleInstanceCoordinator : public QObject {
    Q_OBJECT

public:
    enum class Role {
        Primary,
        SecondaryForwarded
    };

    explicit SingleInstanceCoordinator(
        const QString& socketName = defaultSocketName(),
        QObject* parent = nullptr);
    ~SingleInstanceCoordinator() override;

    static QString defaultSocketName();
    static bool hasCandidateServerSocket(const QString& socketName = defaultSocketName());
    static bool tryForwardToRunningInstance(
        int& argc, char** argv,
        const QStringList& rawFilePaths,
        bool openEmptyWindowWhenNoFiles);
    static QStringList normalizeFilePaths(const QStringList& rawPaths, const QString& baseDir);

    Role activateOrListen(const QStringList& rawFilePaths, bool openEmptyWindowWhenNoFiles = false);
    bool isPrimary() const { return m_isPrimary; }
    QString socketName() const { return m_socketName; }

signals:
    void activationRequested(const QStringList& filePaths);
    void emptyWindowRequested();

private:
    enum class ForwardResult {
        Delivered,
        NoServer,
        TransportTimeout
    };

    ForwardResult forwardToPrimary(const QStringList& normalizedFiles, bool openEmptyWindow) const;
    void handleIncomingConnections();
    void handleSocketReadyRead(QLocalSocket* client);

    QString m_socketName;
    QLocalServer* m_server = nullptr;
    bool m_isPrimary = false;
};

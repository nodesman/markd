#pragma once

#include <QString>
#include <QStringList>
#include <functional>

class FileAssociationManager {
public:
    using PlatformRegistrar = std::function<bool(const QString& binaryPath)>;

    FileAssociationManager();
    explicit FileAssociationManager(PlatformRegistrar customRegistrar);

    static QStringList supportedExtensions();
    static QStringList supportedMimeTypes();
    static QStringList supportedUtis();

    static QString formatDesktopEntry(const QString& binaryPath, const QString& iconPath);
    static QStringList formatWindowsRegistryCommands(const QString& binaryPath);

    bool registerAsDefault(const QString& binaryPath) const;

    static bool registerCurrentPlatformDefault(const QString& binaryPath);

private:
    PlatformRegistrar m_registrar;
};

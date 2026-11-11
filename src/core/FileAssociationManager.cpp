#include "core/FileAssociationManager.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

#if defined(Q_OS_MACOS)
#include <CoreFoundation/CoreFoundation.h>
#include <dlfcn.h>
#endif

namespace {

#if defined(Q_OS_MACOS)
using LSSetDefaultHandlerFn = OSStatus (*)(CFStringRef, uint32_t, CFStringRef);
using LSRegisterURLFn = OSStatus (*)(CFURLRef, Boolean);

void registerBundleWithCoreServices(LSRegisterURLFn regFn, const QString& bundlePath)
{
    if (!regFn) return;
    const QByteArray utf8Path = bundlePath.toUtf8();
    CFURLRef bundleUrl = CFURLCreateFromFileSystemRepresentation(
        kCFAllocatorDefault, reinterpret_cast<const UInt8*>(utf8Path.constData()), utf8Path.size(), true);
    if (!bundleUrl) return;
    regFn(bundleUrl, true);
    CFRelease(bundleUrl);
}

void registerActiveMacosBundleUrl(void* handle, const QString& bundlePath)
{
    if (bundlePath.isEmpty()) return;
    registerBundleWithCoreServices(reinterpret_cast<LSRegisterURLFn>(dlsym(handle, "LSRegisterURL")), bundlePath);
    const QString lsregisterBin = QStringLiteral(
        "/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister");
    if (QFileInfo::exists(lsregisterBin)) {
        QProcess::execute(lsregisterBin, { QStringLiteral("-f"), bundlePath });
    }
}

bool registerMacosLaunchServices(const QString& binaryPath)
{
    void* handle = dlopen("/System/Library/Frameworks/CoreServices.framework/CoreServices", RTLD_LAZY);
    if (!handle) return false;

    registerActiveMacosBundleUrl(handle, FileAssociationManager::resolveMacosAppBundlePath(binaryPath));
    auto setFn = reinterpret_cast<LSSetDefaultHandlerFn>(dlsym(handle, "LSSetDefaultRoleHandlerForContentType"));
    if (!setFn) {
        dlclose(handle);
        return false;
    }

    constexpr uint32_t kAllRoles = 0xFFFFFFFF;
    setFn(CFSTR("net.daringfireball.markdown"), kAllRoles, CFSTR("org.markd.app"));
    setFn(CFSTR("public.markdown"), kAllRoles, CFSTR("org.markd.app"));
    dlclose(handle);
    return true;
}
#endif

bool writeLinuxDesktopFile(const QString& binaryPath)
{
    const QString appsDir = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
    if (appsDir.isEmpty()) return false;
    QDir().mkpath(appsDir);

    QFile file(appsDir + QStringLiteral("/markd.desktop"));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    file.write(FileAssociationManager::formatDesktopEntry(binaryPath, QStringLiteral("markd")).toUtf8());
    file.close();

    QProcess::execute(QStringLiteral("xdg-mime"), {
        QStringLiteral("default"), QStringLiteral("markd.desktop"),
        QStringLiteral("text/markdown"), QStringLiteral("text/x-markdown")
    });
    return true;
}

bool runWindowsRegistryRegistration(const QString& binaryPath)
{
    const QStringList commands = FileAssociationManager::formatWindowsRegistryCommands(binaryPath);
    for (const QString& cmd : commands) {
        QProcess::execute(QStringLiteral("reg.exe"), cmd.split(QLatin1Char(' '), Qt::SkipEmptyParts));
    }
    return !commands.isEmpty();
}

} // namespace

FileAssociationManager::FileAssociationManager()
    : m_registrar(&FileAssociationManager::registerCurrentPlatformDefault)
{
}

FileAssociationManager::FileAssociationManager(PlatformRegistrar customRegistrar)
    : m_registrar(std::move(customRegistrar))
{
}

QStringList FileAssociationManager::supportedExtensions()
{
    return { QStringLiteral(".md"), QStringLiteral(".markdown"), QStringLiteral(".mdown"), QStringLiteral(".mkd") };
}

QStringList FileAssociationManager::supportedMimeTypes()
{
    return { QStringLiteral("text/markdown"), QStringLiteral("text/x-markdown") };
}

QStringList FileAssociationManager::supportedUtis()
{
    return { QStringLiteral("net.daringfireball.markdown"), QStringLiteral("public.markdown") };
}

QString FileAssociationManager::formatDesktopEntry(const QString& binaryPath, const QString& iconPath)
{
    QStringList lines;
    lines << QStringLiteral("[Desktop Entry]");
    lines << QStringLiteral("Type=Application");
    lines << QStringLiteral("Name=Markd");
    lines << QStringLiteral("Comment=Sovereign Markdown Reader and Editor");
    lines << QStringLiteral("Exec=\"%1\" %F").arg(binaryPath);
    lines << QStringLiteral("Icon=%1").arg(iconPath);
    lines << QStringLiteral("Terminal=false");
    lines << QStringLiteral("Categories=Office;TextEditor;Utility;");
    lines << QStringLiteral("MimeType=text/markdown;text/x-markdown;");
    return lines.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

QStringList FileAssociationManager::formatWindowsRegistryCommands(const QString& binaryPath)
{
    const QString nativeBin = QDir::toNativeSeparators(binaryPath);
    QStringList cmds;
    cmds << QStringLiteral("add HKCU\\Software\\Classes\\Markd.Document /ve /d \"Markdown Document\" /f");
    cmds << QStringLiteral("add HKCU\\Software\\Classes\\Markd.Document\\DefaultIcon /ve /d \"%1,0\" /f").arg(nativeBin);
    cmds << QStringLiteral("add HKCU\\Software\\Classes\\Markd.Document\\shell\\open\\command /ve /d \"\\\"%1\\\" \\\"%%1\\\"\" /f").arg(nativeBin);
    for (const QString& ext : supportedExtensions()) {
        cmds << QStringLiteral("add HKCU\\Software\\Classes\\%1 /ve /d Markd.Document /f").arg(ext);
        cmds << QStringLiteral("add HKCU\\Software\\Classes\\%1\\OpenWithProgids /v Markd.Document /t REG_NONE /f").arg(ext);
    }
    return cmds;
}

QString FileAssociationManager::resolveMacosAppBundlePath(const QString& binaryPath)
{
    const QString trimmed = binaryPath.trimmed();
    if (trimmed.isEmpty()) return QString();

    const QFileInfo info(trimmed);
    const QString canonical = info.exists() ? info.canonicalFilePath() : QString();
    const QString clean = canonical.isEmpty() ? QDir::cleanPath(trimmed) : canonical;

    const QString macosMarker = QStringLiteral(".app/Contents/MacOS/");
    const qsizetype markerIdx = clean.indexOf(macosMarker, 0, Qt::CaseInsensitive);
    if (markerIdx > 0) {
        return clean.left(markerIdx + 4);
    }
    if (clean.endsWith(QStringLiteral(".app"), Qt::CaseInsensitive)) {
        return clean;
    }

    const QString siblingBundle = QDir::cleanPath(QFileInfo(clean).dir().filePath(QStringLiteral("../libexec/Markd.app")));
    if (QFileInfo::exists(siblingBundle)) {
        return siblingBundle;
    }
    return QString();
}

bool FileAssociationManager::registerAsDefault(const QString& binaryPath) const
{
    if (binaryPath.trimmed().isEmpty() || !m_registrar) return false;
    return m_registrar(binaryPath);
}

bool FileAssociationManager::registerCurrentPlatformDefault(const QString& binaryPath)
{
    if (binaryPath.trimmed().isEmpty()) return false;
#if defined(Q_OS_MACOS)
    return registerMacosLaunchServices(binaryPath);
#elif defined(Q_OS_WIN)
    return runWindowsRegistryRegistration(binaryPath);
#else
    return writeLinuxDesktopFile(binaryPath);
#endif
}

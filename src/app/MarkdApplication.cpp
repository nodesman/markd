#include "app/MarkdApplication.h"
#include "app/CommandLineOptions.h"
#include "ui/MainWindow.h"
#include "core/Version.h"
#include "core/ProvenanceDetector.h"
#include "core/SilentAutoUpdater.h"
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QUrl>
#include <QUrlQuery>

namespace {
int& configureChromiumFlagsBeforeApp(int& argc) {
    MarkdApplication::applyChromiumStabilityFlags();
    return argc;
}
bool isReusableCleanUntitledWindow(const MainWindow* win) {
    return win && win->currentFilePath().isEmpty() && win->documentBuffer().isEmpty();
}
} // namespace

void MarkdApplication::applyChromiumStabilityFlags() {
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts, true);
    QByteArray flags = qgetenv("QTWEBENGINE_CHROMIUM_FLAGS");
    if (flags.contains("--disable-renderer-accessibility")) return;
    if (!flags.isEmpty()) flags.append(' ');
    flags.append("--disable-renderer-accessibility");
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", flags);
}

QString MarkdApplication::normalizeDocumentPath(const QString& filePath) {
    const QString trimmed = filePath.trimmed();
    if (trimmed.isEmpty()) return QString();
    const QFileInfo info(trimmed);
    const QString canonical = info.canonicalFilePath();
    return canonical.isEmpty() ? QDir::cleanPath(info.absoluteFilePath()) : canonical;
}

QString MarkdApplication::extractPathFromFileOpenEvent(const QFileOpenEvent* event) {
    if (!event) return QString();
    const QString direct = event->file().trimmed();
    if (!direct.isEmpty()) return direct;
    const QUrl url = event->url();
    if (url.isLocalFile()) return url.toLocalFile().trimmed();
    if (url.scheme().compare(QLatin1String("markd"), Qt::CaseInsensitive) != 0) return QString();
    return QUrlQuery(url).queryItemValue(QStringLiteral("path"), QUrl::FullyDecoded).trimmed();
}

MarkdApplication::MarkdApplication(int& argc, char** argv)
    : QApplication(configureChromiumFlagsBeforeApp(argc), argv) {
    setApplicationName(QStringLiteral("Markd"));
    setApplicationVersion(QString::fromUtf8(MARKD_VERSION_STR));
    setOrganizationDomain(QStringLiteral("markd.aramudan.dev"));
    setOrganizationName(QStringLiteral("Markd"));
    setWindowIcon(QIcon(QStringLiteral(":/branding/app_icon_64.png")));
    m_coalesceTimer = new QTimer(this);
    m_coalesceTimer->setSingleShot(true);
    connect(m_coalesceTimer, &QTimer::timeout, this, &MarkdApplication::handleCoalesceTimeout);
    QTimer::singleShot(2000, this, []() {
        SilentAutoUpdater::sweepStaleBackup(ProvenanceDetector::resolveTargetBinaryPath());
    });
}

MarkdApplication::~MarkdApplication() = default;

MarkdApplication* MarkdApplication::instance() {
    return qobject_cast<MarkdApplication*>(QCoreApplication::instance());
}

void MarkdApplication::activateAndRaiseWindow(MainWindow* window) {
    if (!window) return;
    window->setWindowState(window->windowState() & ~Qt::WindowMinimized);
    window->show();
    window->raise();
    window->activateWindow();
    m_lastActivatedWindow = window;
}

MainWindow* MarkdApplication::createWindow(const QString& filePath) {
    const bool hasFile = !filePath.isEmpty();
    auto* window = new MainWindow(filePath, hasFile ? MainWindow::Mode::Preview : MainWindow::Mode::Edit, hasFile);
    registerWindow(window);
    activateAndRaiseWindow(window);
    return window;
}

QList<MainWindow*> MarkdApplication::windows() const {
    QList<MainWindow*> result;
    for (const auto& ptr : m_windows) {
        if (ptr) result.append(ptr.data());
    }
    return result;
}

MainWindow* MarkdApplication::activeWindow() const {
    auto* active = qobject_cast<MainWindow*>(QApplication::activeWindow());
    if (active && m_windows.contains(active)) return active;
    for (const auto& ptr : m_windows) {
        if (ptr) return ptr.data();
    }
    return nullptr;
}

MainWindow* MarkdApplication::activeOrLastWindow() const {
    auto* active = qobject_cast<MainWindow*>(QApplication::activeWindow());
    if (active && m_windows.contains(active)) return active;
    if (m_lastActivatedWindow && m_windows.contains(m_lastActivatedWindow)) return m_lastActivatedWindow.data();
    return activeWindow();
}

MainWindow* MarkdApplication::findWindowForFile(const QString& filePath) const {
    const QString target = normalizeDocumentPath(filePath);
    if (target.isEmpty()) return nullptr;
    for (const auto& ptr : m_windows) {
        if (ptr && normalizeDocumentPath(ptr->currentFilePath()) == target) return ptr.data();
    }
    return nullptr;
}

MainWindow* MarkdApplication::openFileInWindow(const QString& filePath) {
    if (filePath.trimmed().isEmpty()) return nullptr;
    if (MainWindow* existing = findWindowForFile(filePath)) {
        activateAndRaiseWindow(existing);
        return existing;
    }
    for (const auto& ptr : m_windows) {
        if (!isReusableCleanUntitledWindow(ptr.data())) continue;
        ptr->loadFile(filePath);
        ptr->setMode(MainWindow::Mode::Preview);
        ptr->setSidebarCollapsed(true);
        activateAndRaiseWindow(ptr.data());
        return ptr.data();
    }
    return createWindow(filePath);
}

QList<MainWindow*> MarkdApplication::openFilesInWindows(const QStringList& filePaths) {
    QList<MainWindow*> opened;
    for (const QString& path : filePaths) {
        MainWindow* win = openFileInWindow(path);
        if (win && !opened.contains(win)) opened.append(win);
    }
    return opened;
}

void MarkdApplication::launchInitialWindows(const CommandLineOptions& options, bool coalescePlatformFileOpen) {
    m_coalesceTimer->stop();
    m_coalescingStartup = false;
    if (options.hasFile) {
        openFilesInWindows(options.filePaths);
        if (windows().isEmpty()) createWindow(QString());
        return;
    }
    if (!coalescePlatformFileOpen) {
        createWindow(QString());
        return;
    }
    m_coalescingStartup = true;
    m_coalesceTimer->start(35);
}

void MarkdApplication::handleCoalesceTimeout() {
    m_coalescingStartup = false;
    if (windows().isEmpty()) createWindow(QString());
}

void MarkdApplication::closeAllWindows() {
    const auto currentWindows = m_windows;
    for (const auto& ptr : currentWindows) {
        if (ptr) ptr->close();
    }
}

bool MarkdApplication::event(QEvent* event) {
    if (event->type() != QEvent::FileOpen) return QApplication::event(event);
    const bool wasCoalescing = m_coalescingStartup;
    m_coalescingStartup = false;
    m_coalesceTimer->stop();
    const QString resolved = extractPathFromFileOpenEvent(static_cast<QFileOpenEvent*>(event));
    if (!resolved.isEmpty() && (!wasCoalescing || QFileInfo::exists(resolved))) openFileInWindow(resolved);
    if (wasCoalescing && windows().isEmpty()) createWindow(QString());
    return true;
}

void MarkdApplication::registerWindow(MainWindow* window) {
    if (!window) return;
    window->setAttribute(Qt::WA_DeleteOnClose, true);
    m_windows.append(window);
    connect(window, &QObject::destroyed, this, [this, window]() { unregisterWindow(window); });
    emit windowCreated(window);
}

void MarkdApplication::unregisterWindow(MainWindow* window) {
    m_windows.removeAll(window);
    emit windowDestroyed(window);
}

#include "MarkdApplication.h"
#include "MainWindow.h"
#include <QFileInfo>

MarkdApplication::MarkdApplication(int& argc, char** argv)
    : QApplication(argc, argv)
{
    setApplicationName("Markd");
    setApplicationVersion("1.0.0");
    setOrganizationDomain("markd.org");
    setOrganizationName("Markd");
}

MarkdApplication::~MarkdApplication() = default;

MarkdApplication* MarkdApplication::instance() {
    return qobject_cast<MarkdApplication*>(QCoreApplication::instance());
}

MainWindow* MarkdApplication::createWindow(const QString& filePath) {
    auto* window = new MainWindow(filePath);
    registerWindow(window);
    window->show();
    window->raise();
    window->activateWindow();
    return window;
}

QList<MainWindow*> MarkdApplication::windows() const {
    QList<MainWindow*> result;
    for (const auto& ptr : m_windows) {
        if (ptr) {
            result.append(ptr.data());
        }
    }
    return result;
}

MainWindow* MarkdApplication::activeWindow() const {
    auto* active = qobject_cast<MainWindow*>(QApplication::activeWindow());
    if (active && m_windows.contains(active)) {
        return active;
    }
    for (const auto& ptr : m_windows) {
        if (ptr) {
            return ptr.data();
        }
    }
    return nullptr;
}

MainWindow* MarkdApplication::openFileInWindow(const QString& filePath) {
    if (filePath.isEmpty()) return nullptr;

    // Scan for clean, untitled window that can be reused
    for (const auto& ptr : m_windows) {
        if (ptr) {
            if (ptr->currentFilePath().isEmpty() && ptr->documentBuffer().isEmpty()) {
                ptr->loadFile(filePath);
                ptr->raise();
                ptr->activateWindow();
                return ptr.data();
            }
        }
    }

    // Otherwise spawn fresh window
    return createWindow(filePath);
}

void MarkdApplication::closeAllWindows() {
    auto currentWindows = m_windows;
    for (auto& ptr : currentWindows) {
        if (ptr) {
            ptr->close();
        }
    }
}

bool MarkdApplication::event(QEvent* event) {
    if (event->type() == QEvent::FileOpen) {
        auto* fileEvent = static_cast<QFileOpenEvent*>(event);
        openFileInWindow(fileEvent->file());
        return true;
    }
    return QApplication::event(event);
}

void MarkdApplication::registerWindow(MainWindow* window) {
    if (!window) return;

    window->setAttribute(Qt::WA_DeleteOnClose, true);
    m_windows.append(window);

    connect(window, &QObject::destroyed, this, [this, window]() {
        unregisterWindow(window);
    });

    emit windowCreated(window);
}

void MarkdApplication::unregisterWindow(MainWindow* window) {
    m_windows.removeAll(window);
    emit windowDestroyed(window);
}

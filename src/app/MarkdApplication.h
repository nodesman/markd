#pragma once

#include <QApplication>
#include <QEvent>
#include <QFileOpenEvent>
#include <QList>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QTimer>

struct CommandLineOptions;
class MainWindow;

class MarkdApplication : public QApplication {
    Q_OBJECT

public:
    MarkdApplication(int& argc, char** argv);
    ~MarkdApplication() override;

    static MarkdApplication* instance();
    static void applyChromiumStabilityFlags();
    static QString normalizeDocumentPath(const QString& filePath);
    static QString extractPathFromFileOpenEvent(const QFileOpenEvent* event);

    MainWindow* createWindow(const QString& filePath = QString());
    QList<MainWindow*> windows() const;
    MainWindow* activeWindow() const;
    MainWindow* activeOrLastWindow() const;
    MainWindow* findWindowForFile(const QString& filePath) const;
    MainWindow* openFileInWindow(const QString& filePath);
    QList<MainWindow*> openFilesInWindows(const QStringList& filePaths);
    void launchInitialWindows(const CommandLineOptions& options, bool coalescePlatformFileOpen = false);
    void closeAllWindows();

signals:
    void windowCreated(MainWindow* window);
    void windowDestroyed(MainWindow* window);

protected:
    bool event(QEvent* event) override;

private:
    void registerWindow(MainWindow* window);
    void unregisterWindow(MainWindow* window);
    void activateAndRaiseWindow(MainWindow* window);
    void handleCoalesceTimeout();

    QList<QPointer<MainWindow>> m_windows;
    QPointer<MainWindow> m_lastActivatedWindow;
    QTimer* m_coalesceTimer = nullptr;
    bool m_coalescingStartup = false;
};

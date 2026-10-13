#pragma once

#include <QApplication>
#include <QList>
#include <QPointer>
#include <QString>
#include <QEvent>
#include <QFileOpenEvent>

class MainWindow;

class MarkdApplication : public QApplication {
    Q_OBJECT

public:
    MarkdApplication(int& argc, char** argv);
    ~MarkdApplication() override;

    static MarkdApplication* instance();

    MainWindow* createWindow(const QString& filePath = QString());
    QList<MainWindow*> windows() const;
    MainWindow* activeWindow() const;
    MainWindow* openFileInWindow(const QString& filePath);
    void closeAllWindows();

signals:
    void windowCreated(MainWindow* window);
    void windowDestroyed(MainWindow* window);

protected:
    bool event(QEvent* event) override;

private:
    void registerWindow(MainWindow* window);
    void unregisterWindow(MainWindow* window);

    QList<QPointer<MainWindow>> m_windows;
};

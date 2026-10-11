#pragma once

#include <QMainWindow>
#include <QString>
#include <QPlainTextEdit>
#include "MenuBuilder.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(const QString& filePath = QString(), QWidget* parent = nullptr);
    ~MainWindow() override = default;

    bool loadFile(const QString& filePath);
    QString currentFilePath() const { return m_filePath; }
    QString documentBuffer() const;
    QPlainTextEdit* editor() const { return m_editor; }

private slots:
    void onOpen();
    void onSave();
    void onCloseWindow();
    void onToggleWordWrap();
    void onZoomIn();
    void onZoomOut();
    void onActualSize();
    void onAbout();

private:
    void setupUI();
    void updateWindowTitle();

    QString m_filePath;
    QPlainTextEdit* m_editor = nullptr;
    MenuBuilder::Actions m_actions;
    qreal m_currentZoom = 1.0;
};

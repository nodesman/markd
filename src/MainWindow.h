#pragma once

#include <QMainWindow>
#include <QString>
#include <QPlainTextEdit>
#include "MenuBuilder.h"
#include "HeaderBar.h"
#include "ModeSwitcher.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(const QString& filePath = QString(), QWidget* parent = nullptr);
    ~MainWindow() override = default;

    bool loadFile(const QString& filePath);
    QString currentFilePath() const { return m_filePath; }
    QString documentBuffer() const;
    QPlainTextEdit* editor() const { return m_editor; }
    HeaderBar* headerBar() const { return m_headerBar; }
    ModeSwitcher* modeSwitcher() const { return m_modeSwitcher; }

protected:
    void changeEvent(QEvent* event) override;

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
    HeaderBar* m_headerBar = nullptr;
    ModeSwitcher* m_modeSwitcher = nullptr;
    QPlainTextEdit* m_editor = nullptr;
    MenuBuilder::Actions m_actions;
    qreal m_currentZoom = 1.0;
};

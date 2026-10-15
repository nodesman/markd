#pragma once

#include <QMainWindow>
#include <QString>
#include <QPlainTextEdit>
#include <QStackedWidget>
#include <QSplitter>
#include <QTimer>
#include <QByteArray>
#include "ui/MenuBuilder.h"
#include "ui/HeaderBar.h"
#include "ui/ModeSwitcher.h"
#include "ui/PreviewView.h"
#include "ui/TocWidget.h"
#include "parser/TocParser.h"
#include "core/DocumentSession.h"
#include "ui/StatusBarTelemetry.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(const QString& filePath = QString(), QWidget* parent = nullptr);
    ~MainWindow() override = default;

    DocumentSession* session() const { return m_session; }
    bool loadFile(const QString& filePath);
    QString currentFilePath() const;
    QString documentBuffer() const;
    QPlainTextEdit* editor() const { return m_editor; }
    PreviewView* previewView() const { return m_previewView; }
    HeaderBar* headerBar() const { return m_headerBar; }
    ModeSwitcher* modeSwitcher() const { return m_modeSwitcher; }
    QStackedWidget* viewportStack() const { return m_viewportStack; }
    QSplitter* mainSplitter() const { return m_mainSplitter; }
    TocWidget* tocWidget() const { return m_tocWidget; }
    QPushButton* sidebarToggleButton() const { return m_sidebarToggleButton; }
    StatusBarTelemetry* statusBar() const { return m_statusBar; }

    using Mode = ModeSwitcher::DocumentMode;
    void setMode(Mode mode);

    bool isSidebarCollapsed() const { return m_collapsed; }
    int sidebarSavedWidth() const { return m_savedWidth; }
    void setSidebarSavedWidth(int w) { m_savedWidth = std::max(w, 200); }
    void setSidebarCollapsed(bool collapsed);
    void toggleSidebar();
    bool restoreSplitterState(const QByteArray& state);
    bool restoreSplitterState(bool collapsed, int targetWidth = 250);

    // Native Chromium Search API
    void findText(const QString& text, bool forward = true, bool caseSensitive = false);
    void findNext();
    void findPrevious();
    void clearFind();

    // Native Document Export & Print API
    void printToPdf(const QString& filePath);
    void exportHtml(const QString& filePath, const std::function<void(bool)>& callback = nullptr);
    void copyHtmlToClipboard(const std::function<void(bool)>& callback = nullptr);

signals:
    void sidebarToggled(bool collapsed);
    void findResult(int activeMatch, int totalMatches);
    void pdfPrintingFinished(const QString& filePath, bool success);

public slots:
    void updateToc();

protected:
    void changeEvent(QEvent* event) override;

private slots:
    void onOpen();
    void onSave();
    void onCloseWindow();
    void onPrintPdf();
    void onExportHtml();
    void onCopyHtml();
    void onFind();
    void onToggleWordWrap();
    void onZoomIn();
    void onZoomOut();
    void onActualSize();
    void onAbout();
    void onModeChanged(ModeSwitcher::DocumentMode mode);
    void onSplitterMoved(int pos, int index);
    void onHeadingActivated(const QString& id);
    void onFindResult(int activeMatch, int totalMatches);

private:
    void setupUI();
    void updateWindowTitle();

    DocumentSession* m_session = nullptr;
    HeaderBar* m_headerBar = nullptr;
    SidebarToggleButton* m_sidebarToggleButton = nullptr;
    ModeSwitcher* m_modeSwitcher = nullptr;
    QSplitter* m_mainSplitter = nullptr;
    TocWidget* m_tocWidget = nullptr;
    QStackedWidget* m_viewportStack = nullptr;
    QPlainTextEdit* m_editor = nullptr;
    PreviewView* m_previewView = nullptr;
    StatusBarTelemetry* m_statusBar = nullptr;
    MenuBuilder::Actions m_actions;

    int m_savedWidth = 250;
    bool m_collapsed = false;
    QString m_lastFindText;
    bool m_lastFindCaseSensitive = false;
};

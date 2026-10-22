#pragma once

#include <QMainWindow>
#include <QString>
#include "ui/EditorView.h"
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
#include "ui/FindBarOverlay.h"
#include "core/HtmlClipboardProvider.h"
#include "core/UpdateChecker.h"
#include <cstdint>
#include <memory>
#include <QPointer>

class AboutDialog;
class UpdateDialog;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    using Mode = ModeSwitcher::DocumentMode;

    explicit MainWindow(const QString& filePath = QString(), 
                        Mode initialMode = Mode::Edit, 
                        bool startCollapsed = false, 
                        QWidget* parent = nullptr);
    ~MainWindow() override;

    DocumentSession* session() const { return m_session; }
    bool loadFile(const QString& filePath);
    QString currentFilePath() const;
    QString documentBuffer() const;
    EditorView* editor() const { return m_editor; }
    PreviewView* previewView() const { return m_previewView; }
    HeaderBar* headerBar() const { return m_headerBar; }
    ModeSwitcher* modeSwitcher() const { return m_modeSwitcher; }
    QStackedWidget* viewportStack() const { return m_viewportStack; }
    QSplitter* mainSplitter() const { return m_mainSplitter; }
    TocWidget* tocWidget() const { return m_tocWidget; }
    QPushButton* sidebarToggleButton() const { return m_sidebarToggleButton; }
    StatusBarTelemetry* statusBar() const { return m_statusBar; }
    FindBarOverlay* findBarOverlay() const { return m_findBarOverlay; }

    void setMode(Mode mode);
    Mode currentMode() const { return m_currentMode; }

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
    void closeFindBar();

    // Native Document Export & Print API
    void printToPdf(const QString& filePath);
    void exportHtml(const QString& filePath, const std::function<void(bool)>& callback = nullptr);
    void copyHtmlToClipboard(const std::function<void(bool)>& callback = nullptr);
    void copyEditorHtmlToClipboard();
    void copyPreviewHtmlToClipboard(const std::function<void(bool)>& callback = nullptr);
    void updateActionRouting(ModeSwitcher::DocumentMode mode);
    bool isCopyingHtml() const { return m_isCopyingHtml; }
    void setIsCopyingHtml(bool copying) { m_isCopyingHtml = copying; }
    uint64_t clipboardOpGeneration() const { return m_clipboardOpGeneration; }

    void setUpdateTransport(std::shared_ptr<IUpdateTransport> transport);
    QTimer* startupUpdateTimer() const { return m_startupUpdateTimer; }
    UpdateInfo lastUpdateInfo() const { return m_lastUpdateInfo; }

signals:
    void sidebarToggled(bool collapsed);
    void findResult(int activeMatch, int totalMatches);
    void pdfPrintingFinished(const QString& filePath, bool success);

public slots:
    void updateToc();
    void onFind();
    void onToggleFullScreen();
    void runBackgroundUpdateCheck();

protected:
    void changeEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onOpen();
    void onSave();
    void onCloseWindow();
    void onPrintPdf();
    void onExportHtml();
    void onCopyHtml();
    void onToggleWordWrap();
    void onZoomIn();
    void onZoomOut();
    void onActualSize();
    void onAbout();
    void onCheckUpdates();
    void onMakeDefaultForMarkdown();
    void onOpenShowcase();
    void onModeChanged(ModeSwitcher::DocumentMode mode);
    void onSplitterMoved(int pos, int index);
    void onHeadingActivated(const QString& id);
    void onFindResult(int activeMatch, int totalMatches);
    void onDiskConflictDetected();
    void onDiskReloaded();

private:
    void setupUI();
    void setupClipboardBridge();
    void ensureUpdateChecker();
    void handleUpdateCheckFinished(const UpdateInfo& info);
    void handleUpdateCheckFailed(const QString& err);
    void repositionFindBarOverlay();
    void updateWindowTitle();
    void switchToEditMode();
    void switchToPreviewMode();
    void syncPreviewToEditor();
    void syncEditorToPreview();
    QString determineTargetHeading() const;
    qreal determineScrollRatio(const QString& targetHeadingId) const;
    void reloadEditorBuffer(const QString& md);
    void restoreActiveFocus();

    DocumentSession* m_session = nullptr;
    HeaderBar* m_headerBar = nullptr;
    SidebarToggleButton* m_sidebarToggleButton = nullptr;
    ModeSwitcher* m_modeSwitcher = nullptr;
    QSplitter* m_mainSplitter = nullptr;
    TocWidget* m_tocWidget = nullptr;
    QStackedWidget* m_viewportStack = nullptr;
    EditorView* m_editor = nullptr;
    PreviewView* m_previewView = nullptr;
    StatusBarTelemetry* m_statusBar = nullptr;
    FindBarOverlay* m_findBarOverlay = nullptr;
    MenuBuilder::Actions m_actions;

    QPointer<AboutDialog> m_aboutDialog;
    QPointer<UpdateDialog> m_updateDialog;
    QTimer* m_startupUpdateTimer = nullptr;
    UpdateInfo m_lastUpdateInfo;
    bool m_hasUpdateInfo = false;
    bool m_isBackgroundUpdateCheck = false;
    std::unique_ptr<UpdateChecker> m_updateChecker;

    int m_savedWidth = 250;
    bool m_collapsed = false;
    QString m_lastFindText;
    bool m_lastFindCaseSensitive = false;
    bool m_findCleared = false;
    bool m_isDiskReloading = false;
    uint64_t m_clipboardOpGeneration = 0;
    bool m_isCopyingHtml = false;
    ModeSwitcher::DocumentMode m_currentMode = ModeSwitcher::DocumentMode::Edit;
};

#include "ui/MainWindow.h"
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QFileDialog>
#include <QMessageBox>
#include <QFont>
#include <QTextBlock>
#include <QClipboard>
#include <QMimeData>
#include <QApplication>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QKeyEvent>
#include <QResizeEvent>
#include <algorithm>

MainWindow::MainWindow(const QString& filePath, QWidget* parent)
    : QMainWindow(parent)
{
    setupUI();

    if (!filePath.isEmpty()) {
        loadFile(filePath);
    } else {
        updateWindowTitle();
        if (m_session) {
            m_session->flushOutline();
        }
    }
}

void MainWindow::setupUI()
{
    qRegisterMetaType<TocItem>();

    // Strict Standalone Window Architecture:
    // Strictly Zero-Tabs (no QTabWidget, no tab bar).
    // Root container mounts HeaderBar above central horizontal QSplitter
    QWidget* rootContainer = new QWidget(this);
    rootContainer->setObjectName("rootCanvas");
    QVBoxLayout* rootLayout = new QVBoxLayout(rootContainer);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    m_headerBar = new HeaderBar(this);
    rootLayout->addWidget(m_headerBar);

    m_sidebarToggleButton = new SidebarToggleButton(QString::fromUtf8("◨"), this);
    m_sidebarToggleButton->setObjectName("sidebarToggleButton");
    m_sidebarToggleButton->setFixedSize(30, 28);
    m_sidebarToggleButton->setAccessibleName(tr("Toggle Table of Contents Outline"));
#if defined(Q_OS_MACOS)
    m_sidebarToggleButton->setToolTip(tr("Toggle Outline (Cmd+\\) / F9"));
#else
    m_sidebarToggleButton->setToolTip(tr("Toggle Outline (Ctrl+\\) / F9"));
#endif
    m_sidebarToggleButton->setFocusPolicy(Qt::NoFocus);
    m_sidebarToggleButton->setCheckable(true);
    m_sidebarToggleButton->setChecked(true);
    m_headerBar->leadingSlot()->layout()->addWidget(m_sidebarToggleButton);
    m_headerBar->balanceFlanks();

    m_modeSwitcher = new ModeSwitcher(m_headerBar->centerSlot());
    m_headerBar->centerSlot()->layout()->addWidget(m_modeSwitcher);

    // Mount horizontal QSplitter (mainSplitter) beneath m_headerBar
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    m_mainSplitter->setObjectName("mainSplitter");
    m_mainSplitter->setChildrenCollapsible(false);

    // Index 0: TocWidget (Outline Sidebar)
    m_tocWidget = new TocWidget(this);
    m_tocWidget->setObjectName("tocSidebar");
    m_tocWidget->setAccessibleName(tr("Table of Contents"));
    m_tocWidget->setMinimumWidth(200);
    m_tocWidget->setMaximumWidth(600);
    m_mainSplitter->addWidget(m_tocWidget);

    // Index 1: QStackedWidget for Dual Viewports (Edit = 0, Preview = 1)
    m_viewportStack = new QStackedWidget(this);
    m_viewportStack->setObjectName("viewportStack");
    m_viewportStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_viewportStack->setMinimumWidth(320); // Central reading pane minimum width constraint

    m_editor = new QPlainTextEdit(this);
    m_editor->setObjectName("central_editor");
    m_editor->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_editor->setMinimumSize(0, 0);
    m_viewportStack->addWidget(m_editor); // Index 0: Edit

    m_previewView = new PreviewView(this);
    m_viewportStack->addWidget(m_previewView); // Index 1: Preview

    m_viewportStack->setCurrentIndex(0);
    m_mainSplitter->addWidget(m_viewportStack);

    // Viewport Protection Invariants & Stretch Factors
    m_mainSplitter->setCollapsible(0, true);
    m_mainSplitter->setCollapsible(1, false);
    m_mainSplitter->setStretchFactor(0, 0);
    m_mainSplitter->setStretchFactor(1, 1);
    m_mainSplitter->setSizes({m_savedWidth, 900 - m_savedWidth});

    rootLayout->addWidget(m_mainSplitter);
    setCentralWidget(rootContainer);

    // Connect splitter moved slot
    connect(m_mainSplitter, &QSplitter::splitterMoved, this, &MainWindow::onSplitterMoved);

    // Connect mode switching signal
    connect(m_modeSwitcher, &ModeSwitcher::modeChanged, this, &MainWindow::onModeChanged);

    // Connect TOC jump and scroll-spy
    connect(m_tocWidget, &TocWidget::headingActivated, this, &MainWindow::onHeadingActivated);
    connect(m_previewView, &PreviewView::activeHeadingChanged, m_tocWidget, &TocWidget::setActiveHeading);
    connect(m_previewView, &PreviewView::activeHeadingChanged, this, [this](const QString& id) {
        if (m_session && m_session->activeHeading() != id) {
            m_session->setActiveHeading(id);
        }
    });
    connect(m_previewView, &PreviewView::findResult, this, &MainWindow::onFindResult);
    connect(m_previewView, &PreviewView::pdfPrintingFinished, this, &MainWindow::pdfPrintingFinished);

    // Initialize Headless DocumentSession Single Source of Truth
    m_session = new DocumentSession(this);
    m_previewView->setSession(m_session);

    // Initialize 24px Bottom Status Bar & Telemetry Engine (Task 11)
    m_statusBar = new StatusBarTelemetry(this);
    setStatusBar(m_statusBar);

    connect(m_session, &DocumentSession::telemetryUpdated, m_statusBar, &StatusBarTelemetry::updateMetrics);
    connect(m_session, &DocumentSession::zoomFactorChanged, m_statusBar, &StatusBarTelemetry::setZoomFactor);
    m_statusBar->updateMetrics(m_session->telemetryMetrics());
    m_statusBar->setZoomFactor(m_session->zoomFactor());

    connect(m_session, &DocumentSession::activeHeadingChanged, this, [this](const QString& id) {
        if (m_tocWidget && m_tocWidget->headings().empty() && m_session && !m_session->markdown().isEmpty()) {
            m_session->flushOutline();
        }
        if (m_tocWidget) {
            m_tocWidget->setActiveHeading(id);
        }
        QString title = m_tocWidget ? m_tocWidget->activeHeadingTitle() : QString();
        m_statusBar->setSectionTitle(title);
        m_statusBar->setSectionVisible(isSidebarCollapsed() && !title.isEmpty());
    });

    connect(this, &MainWindow::sidebarToggled, this, [this](bool collapsed) {
        if (m_tocWidget && m_tocWidget->headings().empty() && m_session && !m_session->markdown().isEmpty()) {
            m_session->flushOutline();
        }
        QString title = m_tocWidget ? m_tocWidget->activeHeadingTitle() : QString();
        m_statusBar->setSectionTitle(title);
        m_statusBar->setSectionVisible(collapsed && !title.isEmpty());
    });

    // Reactive observation of DocumentSession
    connect(m_session, &DocumentSession::markdownChanged, this, [this](const QString& md) {
        if (m_isDiskReloading) {
            m_isDiskReloading = false;
            return;
        }
        if (m_editor && m_editor->toPlainText() != md) {
            m_editor->setPlainText(md);
        }
        if (m_previewView && m_previewView->currentMarkdown() != md) {
            m_previewView->setMarkdown(md);
        }
    });

    connect(m_session, &DocumentSession::themeChanged, m_previewView, &PreviewView::setTheme);

    connect(m_session, &DocumentSession::filePathChanged, this, &MainWindow::updateWindowTitle);
    connect(m_session, &DocumentSession::dirtyChanged, this, &MainWindow::updateWindowTitle);
    connect(m_session, &DocumentSession::diskConflictDetected, this, &MainWindow::onDiskConflictDetected);
    connect(m_session, &DocumentSession::diskReloaded, this, &MainWindow::onDiskReloaded);

    connect(m_session, &DocumentSession::tocChanged, this, [this](const std::vector<TocItem>& headings) {
        if (m_tocWidget) {
            m_tocWidget->setHeadings(headings);
            QString title = m_tocWidget->activeHeadingTitle();
            m_statusBar->setSectionTitle(title);
            m_statusBar->setSectionVisible(isSidebarCollapsed() && !title.isEmpty());
        }
    });

    connect(m_session, &DocumentSession::zoomFactorChanged, this, [this](qreal factor) {
        if (m_editor) {
            QFont f = m_editor->font();
            f.setPointSizeF(13.0 * factor);
            m_editor->setFont(f);
        }
        if (m_previewView) {
            m_previewView->setZoomFactor(factor);
        }
    });

    // Editor buffer mutation notifies DocumentSession
    connect(m_editor, &QPlainTextEdit::textChanged, this, [this]() {
        if (m_session && m_session->markdown() != m_editor->toPlainText()) {
            m_session->setMarkdown(m_editor->toPlainText());
        }
    });

    setupClipboardBridge();

    // Apply native platform window hooks (Cocoa style masks, full size content view)
    PlatformFrame::setupWindow(this);

    // Build declarative menubar matching menu_items.yml
    m_actions = MenuBuilder::buildMenuBar(menuBar(), this);

    // Connect View Mode Action (Cmd+E / Ctrl+E)
    if (m_actions.toggleModeAction) {
        connect(m_actions.toggleModeAction, &QAction::triggered, m_modeSwitcher, &ModeSwitcher::toggleMode);
    }

    // Connect Toggle Sidebar Action (Cmd+\ / Ctrl+\, F9)
    if (m_actions.toggleSidebarAction) {
        connect(m_actions.toggleSidebarAction, &QAction::triggered, this, &MainWindow::toggleSidebar);
    }

    // Connect Sidebar Toggle Button
    if (m_sidebarToggleButton) {
        connect(m_sidebarToggleButton, &QPushButton::clicked, this, &MainWindow::toggleSidebar);
    }

    // Synchronize UI highlight with sidebar collapse state
    connect(this, &MainWindow::sidebarToggled, this, [this](bool collapsed) {
        if (m_sidebarToggleButton) {
            m_sidebarToggleButton->setChecked(!collapsed);
        }
        if (m_actions.toggleSidebarAction) {
            m_actions.toggleSidebarAction->setChecked(!collapsed);
        }
    });

    // Connect Standard File Actions
    if (m_actions.openAction) {
        connect(m_actions.openAction, &QAction::triggered, this, &MainWindow::onOpen);
    }
    if (m_actions.saveAction) {
        connect(m_actions.saveAction, &QAction::triggered, this, &MainWindow::onSave);
    }
    if (m_actions.exportHtmlAction) {
        connect(m_actions.exportHtmlAction, &QAction::triggered, this, &MainWindow::onExportHtml);
    }
    if (m_actions.printPdfAction) {
        connect(m_actions.printPdfAction, &QAction::triggered, this, &MainWindow::onPrintPdf);
    }
    if (m_actions.closeWindowAction) {
        connect(m_actions.closeWindowAction, &QAction::triggered, this, &MainWindow::onCloseWindow);
    }

    // Connect Standard Edit Actions to QPlainTextEdit built-in slots
    if (m_actions.undoAction) {
        connect(m_actions.undoAction, &QAction::triggered, m_editor, &QPlainTextEdit::undo);
    }
    if (m_actions.redoAction) {
        connect(m_actions.redoAction, &QAction::triggered, m_editor, &QPlainTextEdit::redo);
    }
    if (m_actions.cutAction) {
        connect(m_actions.cutAction, &QAction::triggered, m_editor, &QPlainTextEdit::cut);
    }
    if (m_actions.copyAction) {
        connect(m_actions.copyAction, &QAction::triggered, m_editor, &QPlainTextEdit::copy);
    }
    if (m_actions.copyHtmlAction) {
        connect(m_actions.copyHtmlAction, &QAction::triggered, this, &MainWindow::onCopyHtml);
    }
    if (m_actions.pasteAction) {
        connect(m_actions.pasteAction, &QAction::triggered, m_editor, &QPlainTextEdit::paste);
    }
    if (m_actions.selectAllAction) {
        connect(m_actions.selectAllAction, &QAction::triggered, m_editor, &QPlainTextEdit::selectAll);
    }
    if (m_actions.findAction) {
        connect(m_actions.findAction, &QAction::triggered, this, &MainWindow::onFind);
    }

    // Connect View Actions
    if (m_actions.toggleWordWrapAction) {
        connect(m_actions.toggleWordWrapAction, &QAction::triggered, this, &MainWindow::onToggleWordWrap);
    }
    if (m_actions.zoomInAction) {
        connect(m_actions.zoomInAction, &QAction::triggered, this, &MainWindow::onZoomIn);
    }
    if (m_actions.zoomOutAction) {
        connect(m_actions.zoomOutAction, &QAction::triggered, this, &MainWindow::onZoomOut);
    }
    if (m_actions.actualSizeAction) {
        connect(m_actions.actualSizeAction, &QAction::triggered, this, &MainWindow::onActualSize);
    }

    // Help Action
    if (m_actions.aboutAction) {
        connect(m_actions.aboutAction, &QAction::triggered, this, &MainWindow::onAbout);
    }

    m_findBarOverlay = new FindBarOverlay(this);
    connect(m_findBarOverlay, &FindBarOverlay::queryChanged, this, [this](const QString& text) {
        findText(text, true, m_lastFindCaseSensitive);
    });
    connect(m_findBarOverlay, &FindBarOverlay::findNextRequested, this, &MainWindow::findNext);
    connect(m_findBarOverlay, &FindBarOverlay::findPreviousRequested, this, &MainWindow::findPrevious);
    connect(m_findBarOverlay, &FindBarOverlay::closeRequested, this, &MainWindow::closeFindBar);

    resize(900, 650);
    repositionFindBarOverlay();
}

void MainWindow::onSplitterMoved(int pos, int index)
{
    if (index == 1) { // Splitter handle between pane 0 (sidebar) and pane 1 (viewport)
        int total = m_mainSplitter->sizes().value(0, 0) + m_mainSplitter->sizes().value(1, 0);
        if (total <= 0) total = width();

        if (pos > 0) {
            m_savedWidth = std::max(pos, 200);
            if (m_collapsed) {
                m_collapsed = false;
                m_tocWidget->setMinimumWidth(200);
                m_mainSplitter->setSizes({pos, std::max(320, total - pos)});
                emit sidebarToggled(false);
            }
        } else if (pos == 0) {
            // Interactive Drag-to-Zero Guard: mark collapsed without wiping m_savedWidth
            if (!m_collapsed) {
                m_collapsed = true;
                m_tocWidget->setMinimumWidth(0);
                m_mainSplitter->setSizes({0, total});
                emit sidebarToggled(true);
            }
        }
    }
}

void MainWindow::setSidebarCollapsed(bool collapsed)
{
    if (m_collapsed == collapsed) return;

    QList<int> currentSizes = m_mainSplitter->sizes();
    int totalWidth = currentSizes.value(0, 0) + currentSizes.value(1, 0);
    if (totalWidth <= 0) totalWidth = width();

    if (collapsed) {
        if (currentSizes.value(0, 0) > 0) {
            m_savedWidth = std::max(currentSizes.at(0), 200);
        }
        m_collapsed = true;
        m_tocWidget->setMinimumWidth(0);
        m_mainSplitter->setSizes({0, totalWidth});
        emit sidebarToggled(true);
    } else {
        m_collapsed = false;
        m_tocWidget->setMinimumWidth(200);
        int targetWidth = std::max(m_savedWidth, 200);
        int rest = std::max(320, totalWidth - targetWidth);
        m_mainSplitter->setSizes({targetWidth, rest});
        emit sidebarToggled(false);
    }
}

void MainWindow::toggleSidebar()
{
    setSidebarCollapsed(!m_collapsed);
}

bool MainWindow::restoreSplitterState(const QByteArray& state)
{
    bool ok = m_mainSplitter->restoreState(state);
    if (ok) {
        int currentWidth = m_mainSplitter->sizes().value(0, 0);
        m_collapsed = (currentWidth == 0);
        if (m_collapsed) {
            m_tocWidget->setMinimumWidth(0);
        } else {
            m_tocWidget->setMinimumWidth(200);
            m_savedWidth = std::max(currentWidth, 200);
        }
        emit sidebarToggled(m_collapsed);
    }
    return ok;
}

bool MainWindow::restoreSplitterState(bool collapsed, int targetWidth)
{
    m_savedWidth = std::max(targetWidth, 200);
    int totalWidth = m_mainSplitter->width();
    if (totalWidth <= 0) totalWidth = width();
    if (totalWidth <= 0) totalWidth = 1024;

    if (collapsed) {
        m_collapsed = true;
        m_tocWidget->setMinimumWidth(0);
        m_mainSplitter->setSizes({0, totalWidth});
        emit sidebarToggled(true);
    } else {
        m_collapsed = false;
        m_tocWidget->setMinimumWidth(200);
        int rest = std::max(320, totalWidth - m_savedWidth);
        m_mainSplitter->setSizes({m_savedWidth, rest});
        emit sidebarToggled(false);
    }
    return true;
}

void MainWindow::onHeadingActivated(const QString& id)
{
    const auto& items = m_tocWidget->headings();
    auto it = std::find_if(items.begin(), items.end(), [&id](const TocItem& item) {
        return item.id == id;
    });

    if (it == items.end()) return;

    if (m_session) {
        m_session->setSelectedHeading(id);
    }

    if (m_modeSwitcher->currentMode() == ModeSwitcher::DocumentMode::Edit) {
        int line = it->lineNumber;
        QTextDocument* doc = m_editor->document();
        QTextBlock block = doc->findBlockByLineNumber(line - 1);
        if (block.isValid()) {
            QTextCursor cursor(block);
            m_editor->setTextCursor(cursor);
            m_editor->centerCursor();
            m_editor->setFocus();
        }
    } else {
        m_previewView->scrollToHeading(id);
    }
}

void MainWindow::updateToc()
{
    if (m_session) {
        m_session->flushOutline();
    }
}

bool MainWindow::loadFile(const QString& filePath)
{
    if (!m_session) return false;
    return m_session->loadFile(filePath);
}

QString MainWindow::currentFilePath() const
{
    return m_session ? m_session->filePath() : QString();
}

QString MainWindow::documentBuffer() const
{
    return m_session ? m_session->markdown() : QString();
}

void MainWindow::updateWindowTitle()
{
    QString path = currentFilePath();
    bool dirty = m_session ? m_session->isDirty() : false;
    if (path.isEmpty()) {
        setWindowTitle(dirty ? "Markd*" : "Markd");
    } else {
        QFileInfo fi(path);
        setWindowTitle(QString("%1%2 — Markd").arg(fi.fileName(), dirty ? "*" : ""));
    }
}

void MainWindow::onOpen()
{
    QString selectedFile = QFileDialog::getOpenFileName(
        this,
        tr("Open Markdown Document"),
        QString(),
        tr("Markdown Files (*.md *.markdown *.mdown);;All Files (*)")
    );

    if (!selectedFile.isEmpty()) {
        loadFile(selectedFile);
    }
}

void MainWindow::onSave()
{
    if (!m_session) return;
    if (m_session->filePath().isEmpty()) {
        QString selectedFile = QFileDialog::getSaveFileName(
            this,
            tr("Save Markdown Document"),
            QString(),
            tr("Markdown Files (*.md *.markdown);;All Files (*)")
        );
        if (selectedFile.isEmpty()) return;
        m_session->saveAs(selectedFile);
    } else {
        m_session->save();
    }
}

void MainWindow::onCloseWindow()
{
    close();
}

void MainWindow::onToggleWordWrap()
{
    if (m_editor->lineWrapMode() == QPlainTextEdit::NoWrap) {
        m_editor->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    } else {
        m_editor->setLineWrapMode(QPlainTextEdit::NoWrap);
    }
}

void MainWindow::onZoomIn()
{
    if (m_session) {
        m_session->zoomIn();
    }
}

void MainWindow::onZoomOut()
{
    if (m_session) {
        m_session->zoomOut();
    }
}

void MainWindow::onActualSize()
{
    if (m_session) {
        m_session->resetZoom();
    }
}

void MainWindow::onAbout()
{
    QMessageBox::about(
        this,
        tr("About Markd"),
        tr("<h3>Markd 1.0.0</h3>"
           "<p>High-performance native Markdown viewer and editor.</p>"
           "<p>Engineered for command-line developers and power users.</p>")
    );
}

void MainWindow::setMode(Mode mode)
{
    if (m_modeSwitcher) {
        m_modeSwitcher->setMode(mode);
    }
}

void MainWindow::setupClipboardBridge()
{
    if (!m_session) return;
    connect(m_session, &DocumentSession::clipboardCopyRequested, this, [this](const QString& text) {
        QApplication::clipboard()->setText(text);
        if (m_statusBar) {
            m_statusBar->showNotification(tr("Copied to clipboard"), QStringLiteral("info"), 1500);
        }
    });
}

void MainWindow::onModeChanged(ModeSwitcher::DocumentMode mode)
{
    if (!m_viewportStack) return;

    if (m_session) {
        m_session->setMode(static_cast<int>(mode));
        m_session->flushOutline();
    }

    if (mode == ModeSwitcher::DocumentMode::Edit) {
        switchToEditMode();
    }
    if (mode == ModeSwitcher::DocumentMode::Preview) {
        switchToPreviewMode();
    }

    if (!m_findBarOverlay || !m_findBarOverlay->isVisible() || m_lastFindText.isEmpty()) {
        return;
    }

    if (mode == ModeSwitcher::DocumentMode::Edit && m_previewView) {
        m_previewView->clearFind();
    }
    if (mode == ModeSwitcher::DocumentMode::Preview && m_editor) {
        QTextCursor cur = m_editor->textCursor();
        cur.clearSelection();
        m_editor->setTextCursor(cur);
        m_editor->setExtraSelections({});
    }

    const QString activeQuery = m_lastFindText;
    m_lastFindText.clear();
    findText(activeQuery, true, m_lastFindCaseSensitive);
    m_findBarOverlay->raise();
}

void MainWindow::switchToEditMode()
{
    m_viewportStack->setCurrentIndex(0);
    if (!m_editor) return;
    activateWindow();
    m_editor->setFocus(Qt::OtherFocusReason);
    syncPreviewToEditor();
}

void MainWindow::syncPreviewToEditor()
{
    if (!m_session || !m_tocWidget || !m_editor) return;
    QString activeId = m_session->activeHeading();
    if (activeId.isEmpty()) return;

    const auto& headings = m_tocWidget->headings();
    auto it = std::find_if(headings.begin(), headings.end(), [&activeId](const TocItem& item) {
        return item.id == activeId;
    });
    if (it == headings.end()) return;

    QTextDocument* doc = m_editor->document();
    if (!doc) return;
    QTextBlock block = doc->findBlockByLineNumber(it->lineNumber - 1);
    if (!block.isValid()) return;

    QTextCursor cursor(block);
    m_editor->setTextCursor(cursor);
    m_editor->centerCursor();
}

void MainWindow::switchToPreviewMode()
{
    m_viewportStack->setCurrentIndex(1);
    if (!m_previewView || !m_session) return;
    activateWindow();
    if (m_previewView->webView()) {
        m_previewView->webView()->setFocus(Qt::OtherFocusReason);
    }
    syncEditorToPreview();
}

void MainWindow::syncEditorToPreview()
{
    QString targetHeadingId = determineTargetHeading();
    qreal scrollRatio = determineScrollRatio(targetHeadingId);

    QJsonObject renderOpts;
    if (!targetHeadingId.isEmpty()) {
        renderOpts[QStringLiteral("targetHeading")] = targetHeadingId;
    }
    if (targetHeadingId.isEmpty() && scrollRatio > 0.0) {
        renderOpts[QStringLiteral("scrollRatio")] = scrollRatio;
    }

    if (m_previewView->currentMarkdown() != m_session->markdown()) {
        m_previewView->setMarkdown(m_session->markdown(), renderOpts);
        return;
    }
    if (!targetHeadingId.isEmpty()) {
        m_previewView->scrollToHeading(targetHeadingId);
        return;
    }
    if (scrollRatio > 0.0) {
        m_previewView->scrollToRatio(scrollRatio);
    }
}

QString MainWindow::determineTargetHeading() const
{
    if (!m_editor || !m_tocWidget) return QString();
    int currentLine = m_editor->textCursor().blockNumber() + 1;
    const auto& headings = m_tocWidget->headings();
    QString targetId;
    for (const auto& item : headings) {
        if (item.lineNumber > currentLine) break;
        targetId = item.id;
    }
    return targetId;
}

qreal MainWindow::determineScrollRatio(const QString& targetHeadingId) const
{
    if (!targetHeadingId.isEmpty() || !m_editor) return 0.0;
    QScrollBar* bar = m_editor->verticalScrollBar();
    if (!bar || bar->maximum() <= 0) return 0.0;
    return static_cast<qreal>(bar->value()) / bar->maximum();
}

void MainWindow::changeEvent(QEvent* event)
{
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange) {
        if (m_headerBar) {
            m_headerBar->updateMargins();
        }
    }
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    repositionFindBarOverlay();
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape && m_findBarOverlay && m_findBarOverlay->isVisible()) {
        closeFindBar();
        event->accept();
        return;
    }
    QMainWindow::keyPressEvent(event);
}

void MainWindow::repositionFindBarOverlay()
{
    if (!m_findBarOverlay) return;
    const int x = std::max(16, width() - m_findBarOverlay->width() - 16);
    const int y = PlatformFrame::headerHeight() + 6;
    m_findBarOverlay->move(x, y);
}

void MainWindow::onFindResult(int activeMatch, int totalMatches)
{
    if (m_findCleared) return;
    if (m_findBarOverlay) {
        m_findBarOverlay->updateMatchCount(activeMatch, totalMatches);
    }
    emit findResult(activeMatch, totalMatches);
}

void MainWindow::findText(const QString& text, bool forward, bool caseSensitive)
{
    const bool isNewQuery = (text != m_lastFindText);
    m_lastFindText = text;
    m_lastFindCaseSensitive = caseSensitive;

    if (text.isEmpty()) {
        clearFind();
        return;
    }

    m_findCleared = false;
    if (m_findBarOverlay && m_findBarOverlay->query() != text) {
        const QSignalBlocker blocker(m_findBarOverlay->findInput());
        m_findBarOverlay->findInput()->setText(text);
    }

    const bool inPreview = m_modeSwitcher && m_modeSwitcher->currentMode() == ModeSwitcher::DocumentMode::Preview;
    if (inPreview && m_previewView) {
        m_previewView->findText(text, forward, caseSensitive);
        return;
    }

    if (!m_editor) return;
    const QPair<int, int> counts = FindBarOverlay::executeEditorSearch(
        m_editor, text, forward, caseSensitive, isNewQuery, true);
    if (m_findBarOverlay) {
        m_findBarOverlay->updateMatchCount(counts.first, counts.second);
    }
    emit findResult(counts.first, counts.second);
}

void MainWindow::findNext()
{
    if (m_lastFindText.isEmpty()) return;
    const bool inPreview = m_modeSwitcher && m_modeSwitcher->currentMode() == ModeSwitcher::DocumentMode::Preview;
    if (inPreview && m_previewView) {
        m_previewView->findNext();
        return;
    }
    findText(m_lastFindText, true, m_lastFindCaseSensitive);
}

void MainWindow::findPrevious()
{
    if (m_lastFindText.isEmpty()) return;
    const bool inPreview = m_modeSwitcher && m_modeSwitcher->currentMode() == ModeSwitcher::DocumentMode::Preview;
    if (inPreview && m_previewView) {
        m_previewView->findPrevious();
        return;
    }
    findText(m_lastFindText, false, m_lastFindCaseSensitive);
}

void MainWindow::clearFind()
{
    m_lastFindText.clear();
    m_findCleared = true;
    if (m_previewView) {
        m_previewView->clearFind();
    }
    if (m_editor) {
        QTextCursor cur = m_editor->textCursor();
        cur.clearSelection();
        m_editor->setTextCursor(cur);
        m_editor->setExtraSelections({});
    }
    if (m_findBarOverlay) {
        m_findBarOverlay->updateMatchCount(0, 0);
    }
    emit findResult(0, 0);
}

void MainWindow::onFind()
{
    if (!m_findBarOverlay) return;
    repositionFindBarOverlay();
    QString seedText;
    const bool inEditMode = m_modeSwitcher && m_modeSwitcher->currentMode() == ModeSwitcher::DocumentMode::Edit;
    if (inEditMode && m_editor && m_editor->textCursor().hasSelection()) {
        const QString selected = m_editor->textCursor().selectedText();
        if (!selected.contains(QLatin1Char('\n')) && !selected.contains(QChar(0x2029))) {
            seedText = selected;
        }
    }
    const QString previousQuery = m_findBarOverlay->query();
    m_findBarOverlay->activate(seedText);
    if (!m_findBarOverlay->query().isEmpty() && (seedText.isEmpty() || seedText == previousQuery)) {
        const QString queryToRun = m_findBarOverlay->query();
        m_lastFindText.clear();
        findText(queryToRun, true, m_lastFindCaseSensitive);
    }
}

void MainWindow::closeFindBar()
{
    if (m_findBarOverlay) {
        m_findBarOverlay->hide();
    }
    clearFind();
    activateWindow();
    const bool inPreview = m_modeSwitcher && m_modeSwitcher->currentMode() == ModeSwitcher::DocumentMode::Preview;
    if (inPreview && m_previewView && m_previewView->webView()) {
        m_previewView->webView()->setFocus(Qt::OtherFocusReason);
        return;
    }
    if (m_editor) {
        m_editor->setFocus(Qt::OtherFocusReason);
    }
}

void MainWindow::printToPdf(const QString& filePath)
{
    if (m_previewView) {
        m_previewView->printToPdf(filePath);
    }
}

void MainWindow::exportHtml(const QString& filePath, const std::function<void(bool)>& callback)
{
    if (!m_previewView) {
        if (callback) callback(false);
        return;
    }
    m_previewView->toHtml([filePath, callback](const QString& html) {
        QFile file(filePath);
        bool ok = false;
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << html;
            file.close();
            ok = true;
        }
        if (callback) callback(ok);
    });
}

void MainWindow::copyHtmlToClipboard(const std::function<void(bool)>& callback)
{
    if (!m_previewView) {
        if (callback) callback(false);
        return;
    }
    m_previewView->toHtml([callback](const QString& html) {
        QMimeData* mimeData = new QMimeData();
        mimeData->setHtml(html);
        mimeData->setText(html);
        QApplication::clipboard()->setMimeData(mimeData);
        if (callback) callback(true);
    });
}

void MainWindow::onPrintPdf()
{
    QString defaultPath;
    if (m_session && !m_session->filePath().isEmpty()) {
        QFileInfo fi(m_session->filePath());
        defaultPath = fi.absolutePath() + "/" + fi.completeBaseName() + ".pdf";
    }
    QString selectedFile = QFileDialog::getSaveFileName(
        this,
        tr("Print / Export PDF"),
        defaultPath,
        tr("PDF Files (*.pdf);;All Files (*)")
    );
    if (!selectedFile.isEmpty()) {
        printToPdf(selectedFile);
    }
}

void MainWindow::onExportHtml()
{
    QString defaultPath;
    if (m_session && !m_session->filePath().isEmpty()) {
        QFileInfo fi(m_session->filePath());
        defaultPath = fi.absolutePath() + "/" + fi.completeBaseName() + ".html";
    }
    QString selectedFile = QFileDialog::getSaveFileName(
        this,
        tr("Export to HTML"),
        defaultPath,
        tr("HTML Files (*.html *.htm);;All Files (*)")
    );
    if (!selectedFile.isEmpty()) {
        exportHtml(selectedFile);
    }
}

void MainWindow::onCopyHtml()
{
    copyHtmlToClipboard();
}

void MainWindow::onDiskConflictDetected()
{
    if (!m_statusBar) return;
    m_statusBar->showNotification(
        tr("Disk file changed externally; unsaved edits preserved"),
        QStringLiteral("warning"),
        3000
    );
}

void MainWindow::reloadEditorBuffer(const QString& md)
{
    if (!m_editor) return;
    const int scrollPosition = m_editor->verticalScrollBar()->value();
    const QTextCursor oldCursor = m_editor->textCursor();
    const int cursorPosition = oldCursor.hasSelection() ? oldCursor.selectionStart() : oldCursor.position();
    {
        const QSignalBlocker blocker(m_editor);
        m_editor->setPlainText(md);
        QTextCursor cursor = m_editor->textCursor();
        cursor.setPosition(std::min(cursorPosition, static_cast<int>(md.length())));
        m_editor->setTextCursor(cursor);
    }
    m_editor->verticalScrollBar()->setValue(scrollPosition);
}

void MainWindow::onDiskReloaded()
{
    if (!m_session) return;
    m_isDiskReloading = true;
    const QString& md = m_session->markdown();
    reloadEditorBuffer(md);
    if (m_previewView) {
        m_previewView->setMarkdown(md, /*preserveScroll=*/true);
    }
    if (!m_findBarOverlay || !m_findBarOverlay->isVisible() || m_lastFindText.isEmpty() || !m_editor) {
        return;
    }
    const QPair<int, int> counts = FindBarOverlay::executeEditorSearch(
        m_editor, m_lastFindText, true, m_lastFindCaseSensitive, false, false);
    m_findBarOverlay->updateMatchCount(counts.first, counts.second);
    emit findResult(counts.first, counts.second);
}

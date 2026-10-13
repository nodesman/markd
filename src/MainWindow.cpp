#include "MainWindow.h"
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QFileDialog>
#include <QMessageBox>
#include <QFont>
#include <QTextBlock>
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

    // Initialize Headless DocumentSession Single Source of Truth
    m_session = new DocumentSession(this);

    // Reactive observation of DocumentSession
    connect(m_session, &DocumentSession::markdownChanged, this, [this](const QString& md) {
        if (m_editor && m_editor->toPlainText() != md) {
            m_editor->setPlainText(md);
        }
        if (m_previewView) {
            m_previewView->setMarkdown(md);
        }
    });

    connect(m_session, &DocumentSession::filePathChanged, this, &MainWindow::updateWindowTitle);
    connect(m_session, &DocumentSession::dirtyChanged, this, &MainWindow::updateWindowTitle);

    connect(m_session, &DocumentSession::tocChanged, this, [this](const std::vector<TocItem>& headings) {
        if (m_tocWidget) {
            m_tocWidget->setHeadings(headings);
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

    // Apply native platform window hooks (Cocoa style masks, full size content view)
    PlatformFrame::setupWindow(this);

    // Build declarative menubar matching menu_items.yml
    m_actions = MenuBuilder::buildMenuBar(menuBar(), this);

    // Connect View Mode Action (Cmd+E / Ctrl+E)
    if (m_actions.toggleModeAction) {
        connect(m_actions.toggleModeAction, &QAction::triggered, m_modeSwitcher, &ModeSwitcher::toggleMode);
    }

    // Connect Toggle Sidebar Action (Cmd+\ / Ctrl+\)
    if (m_actions.toggleSidebarAction) {
        connect(m_actions.toggleSidebarAction, &QAction::triggered, this, &MainWindow::toggleSidebar);
    }

    // Connect Standard File Actions
    if (m_actions.openAction) {
        connect(m_actions.openAction, &QAction::triggered, this, &MainWindow::onOpen);
    }
    if (m_actions.saveAction) {
        connect(m_actions.saveAction, &QAction::triggered, this, &MainWindow::onSave);
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
    if (m_actions.pasteAction) {
        connect(m_actions.pasteAction, &QAction::triggered, m_editor, &QPlainTextEdit::paste);
    }
    if (m_actions.selectAllAction) {
        connect(m_actions.selectAllAction, &QAction::triggered, m_editor, &QPlainTextEdit::selectAll);
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

    resize(900, 650);
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
    }
    return ok;
}

void MainWindow::onHeadingActivated(const QString& id)
{
    const auto& items = m_tocWidget->headings();
    auto it = std::find_if(items.begin(), items.end(), [&id](const TocItem& item) {
        return item.id == id;
    });

    if (it == items.end()) return;

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

void MainWindow::onModeChanged(ModeSwitcher::DocumentMode mode)
{
    if (!m_viewportStack) return;

    if (m_session) {
        m_session->setMode(static_cast<int>(mode));
        m_session->flushOutline();
    }

    if (mode == ModeSwitcher::DocumentMode::Edit) {
        m_viewportStack->setCurrentIndex(0);
        if (m_editor) {
            activateWindow();
            m_editor->setFocus(Qt::OtherFocusReason);
        }
    } else {
        m_viewportStack->setCurrentIndex(1);
        if (m_previewView && m_session) {
            m_previewView->setMarkdown(m_session->markdown());
        }
    }
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

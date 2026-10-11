#include "MainWindow.h"
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QFileDialog>
#include <QMessageBox>
#include <QFont>

MainWindow::MainWindow(const QString& filePath, QWidget* parent)
    : QMainWindow(parent)
{
    setupUI();

    if (!filePath.isEmpty()) {
        loadFile(filePath);
    } else {
        updateWindowTitle();
    }
}

void MainWindow::setupUI()
{
    // Strict Standalone Window Architecture:
    // Strictly Zero-Tabs (no QTabWidget, no tab bar).
    // Root container mounts HeaderBar above central viewport stack
    QWidget* rootContainer = new QWidget(this);
    rootContainer->setObjectName("rootCanvas");
    QVBoxLayout* rootLayout = new QVBoxLayout(rootContainer);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    m_headerBar = new HeaderBar(this);
    rootLayout->addWidget(m_headerBar);

    m_modeSwitcher = new ModeSwitcher(m_headerBar->centerSlot());
    m_headerBar->centerSlot()->layout()->addWidget(m_modeSwitcher);

    // Mount QStackedWidget for Dual Viewports (Edit = 0, Preview = 1)
    m_viewportStack = new QStackedWidget(this);
    m_viewportStack->setObjectName("viewportStack");
    m_viewportStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_viewportStack->setMinimumSize(0, 0);

    m_editor = new QPlainTextEdit(this);
    m_editor->setObjectName("central_editor");
    m_editor->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_editor->setMinimumSize(0, 0);
    m_viewportStack->addWidget(m_editor); // Index 0: Edit

    m_previewView = new PreviewView(this);
    m_viewportStack->addWidget(m_previewView); // Index 1: Preview

    m_viewportStack->setCurrentIndex(0);
    rootLayout->addWidget(m_viewportStack);

    setCentralWidget(rootContainer);

    // Connect mode switching signal
    connect(m_modeSwitcher, &ModeSwitcher::modeChanged, this, &MainWindow::onModeChanged);

    // Apply native platform window hooks (Cocoa style masks, full size content view)
    PlatformFrame::setupWindow(this);

    // Build declarative menubar matching menu_items.yml
    m_actions = MenuBuilder::buildMenuBar(menuBar(), this);

    // Connect View Mode Action (Cmd+E / Ctrl+E)
    if (m_actions.toggleModeAction) {
        connect(m_actions.toggleModeAction, &QAction::triggered, m_modeSwitcher, &ModeSwitcher::toggleMode);
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

bool MainWindow::loadFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_filePath = filePath;
        updateWindowTitle();
        return false;
    }

    QTextStream in(&file);
    QString content = in.readAll();
    m_editor->setPlainText(content);
    m_filePath = filePath;
    updateWindowTitle();
    return true;
}

QString MainWindow::documentBuffer() const
{
    return m_editor ? m_editor->toPlainText() : QString();
}

void MainWindow::updateWindowTitle()
{
    if (m_filePath.isEmpty()) {
        setWindowTitle("Markd");
    } else {
        QFileInfo fi(m_filePath);
        setWindowTitle(QString("%1 — Markd").arg(fi.fileName()));
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
    if (m_filePath.isEmpty()) {
        QString selectedFile = QFileDialog::getSaveFileName(
            this,
            tr("Save Markdown Document"),
            QString(),
            tr("Markdown Files (*.md *.markdown);;All Files (*)")
        );
        if (selectedFile.isEmpty()) return;
        m_filePath = selectedFile;
    }

    QFile file(m_filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << m_editor->toPlainText();
        updateWindowTitle();
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
    m_editor->zoomIn(1);
}

void MainWindow::onZoomOut()
{
    m_editor->zoomOut(1);
}

void MainWindow::onActualSize()
{
    // Reset font point size to standard default (e.g. 13pt)
    QFont f = m_editor->font();
    f.setPointSize(13);
    m_editor->setFont(f);
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

    if (mode == ModeSwitcher::DocumentMode::Edit) {
        m_viewportStack->setCurrentIndex(0);
        if (m_editor) {
            activateWindow();
            m_editor->setFocus(Qt::OtherFocusReason);
        }
    } else {
        m_viewportStack->setCurrentIndex(1);
        if (m_previewView && m_editor) {
            m_previewView->setMarkdown(m_editor->toPlainText());
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

#include "MenuBuilder.h"
#include <QKeySequence>

MenuBuilder::Actions MenuBuilder::buildMenuBar(QMenuBar* menuBar, QObject* parent) {
    Actions actions;
    if (!menuBar) return actions;
    if (!parent) parent = menuBar;

    // Clear any default items
    menuBar->clear();

    // ==========================================
    // 1. File Menu
    // ==========================================
    QMenu* fileMenu = menuBar->addMenu(QObject::tr("&File"));

    // Open… (Cmd+O / Ctrl+O)
    actions.openAction = new QAction(QObject::tr("&Open…"), parent);
    actions.openAction->setObjectName("action_open");
    actions.openAction->setShortcut(QKeySequence::Open);
    fileMenu->addAction(actions.openAction);

    // Save (Cmd+S / Ctrl+S)
    actions.saveAction = new QAction(QObject::tr("&Save"), parent);
    actions.saveAction->setObjectName("action_save");
    actions.saveAction->setShortcut(QKeySequence::Save);
    fileMenu->addAction(actions.saveAction);

    fileMenu->addSeparator();

    // Export to HTML… (Shift+Cmd+E / Ctrl+Shift+E)
    actions.exportHtmlAction = new QAction(QObject::tr("&Export to HTML…"), parent);
    actions.exportHtmlAction->setObjectName("action_export_html");
#if defined(Q_OS_MACOS)
    actions.exportHtmlAction->setShortcut(QKeySequence(Qt::META | Qt::SHIFT | Qt::Key_E));
#else
    actions.exportHtmlAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E));
#endif
    fileMenu->addAction(actions.exportHtmlAction);

    // Print / Export PDF… (Cmd+P / Ctrl+P)
    actions.printPdfAction = new QAction(QObject::tr("&Print / Export PDF…"), parent);
    actions.printPdfAction->setObjectName("action_print_pdf");
    actions.printPdfAction->setShortcut(QKeySequence::Print);
    fileMenu->addAction(actions.printPdfAction);

    fileMenu->addSeparator();

    // Close Window / Exit (Cmd+W on macOS, Alt+F4 on Win, Ctrl+Q on Linux)
    actions.closeWindowAction = new QAction(QObject::tr("&Close Window / Exit"), parent);
    actions.closeWindowAction->setObjectName("action_close_window");
#if defined(Q_OS_MACOS)
    actions.closeWindowAction->setShortcut(QKeySequence(Qt::META | Qt::Key_W));
#elif defined(Q_OS_WIN)
    actions.closeWindowAction->setShortcut(QKeySequence(Qt::ALT | Qt::Key_F4));
#else
    actions.closeWindowAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Q));
#endif
    fileMenu->addAction(actions.closeWindowAction);

    // ==========================================
    // 2. Edit Menu
    // ==========================================
    QMenu* editMenu = menuBar->addMenu(QObject::tr("&Edit"));

    // Undo (Cmd+Z / Ctrl+Z)
    actions.undoAction = new QAction(QObject::tr("&Undo"), parent);
    actions.undoAction->setObjectName("action_undo");
    actions.undoAction->setShortcut(QKeySequence::Undo);
    editMenu->addAction(actions.undoAction);

    // Redo (Shift+Cmd+Z on macOS/Linux, Ctrl+Y on Win)
    actions.redoAction = new QAction(QObject::tr("&Redo"), parent);
    actions.redoAction->setObjectName("action_redo");
    actions.redoAction->setShortcut(QKeySequence::Redo);
    editMenu->addAction(actions.redoAction);

    editMenu->addSeparator();

    // Cut (Cmd+X / Ctrl+X)
    actions.cutAction = new QAction(QObject::tr("Cu&t"), parent);
    actions.cutAction->setObjectName("action_cut");
    actions.cutAction->setShortcut(QKeySequence::Cut);
    editMenu->addAction(actions.cutAction);

    // Copy (Cmd+C / Ctrl+C)
    actions.copyAction = new QAction(QObject::tr("&Copy"), parent);
    actions.copyAction->setObjectName("action_copy");
    actions.copyAction->setShortcut(QKeySequence::Copy);
    editMenu->addAction(actions.copyAction);

    // Copy as HTML (Opt+Cmd+C on macOS, Ctrl+Shift+C on Win/Linux)
    actions.copyHtmlAction = new QAction(QObject::tr("Copy as &HTML"), parent);
    actions.copyHtmlAction->setObjectName("action_copy_html");
#if defined(Q_OS_MACOS)
    actions.copyHtmlAction->setShortcut(QKeySequence(Qt::META | Qt::ALT | Qt::Key_C));
#else
    actions.copyHtmlAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C));
#endif
    editMenu->addAction(actions.copyHtmlAction);

    // Paste (Cmd+V / Ctrl+V)
    actions.pasteAction = new QAction(QObject::tr("&Paste"), parent);
    actions.pasteAction->setObjectName("action_paste");
    actions.pasteAction->setShortcut(QKeySequence::Paste);
    editMenu->addAction(actions.pasteAction);

    // Select All (Cmd+A / Ctrl+A)
    actions.selectAllAction = new QAction(QObject::tr("Select &All"), parent);
    actions.selectAllAction->setObjectName("action_select_all");
    actions.selectAllAction->setShortcut(QKeySequence::SelectAll);
    editMenu->addAction(actions.selectAllAction);

    editMenu->addSeparator();

    // Find in Document (Cmd+F / Ctrl+F)
    actions.findAction = new QAction(QObject::tr("&Find in Document"), parent);
    actions.findAction->setObjectName("action_find");
    actions.findAction->setShortcut(QKeySequence::Find);
    editMenu->addAction(actions.findAction);

    // ==========================================
    // 3. View Menu
    // ==========================================
    QMenu* viewMenu = menuBar->addMenu(QObject::tr("&View"));

    // Mode: Edit / Preview (Cmd+E / Ctrl+E)
    actions.toggleModeAction = new QAction(QObject::tr("&Mode: Edit / Preview"), parent);
    actions.toggleModeAction->setObjectName("action_toggle_mode");
#if defined(Q_OS_MACOS)
    actions.toggleModeAction->setShortcut(QKeySequence(Qt::META | Qt::Key_E));
#else
    actions.toggleModeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
#endif
    viewMenu->addAction(actions.toggleModeAction);

    // Toggle Sidebar (Cmd+\ / Ctrl+\)
    actions.toggleSidebarAction = new QAction(QObject::tr("Toggle &Sidebar"), parent);
    actions.toggleSidebarAction->setObjectName("action_toggle_sidebar");
#if defined(Q_OS_MACOS)
    actions.toggleSidebarAction->setShortcut(QKeySequence(Qt::META | Qt::Key_Backslash));
#else
    actions.toggleSidebarAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Backslash));
#endif
    viewMenu->addAction(actions.toggleSidebarAction);

    // Toggle Word Wrap (Opt+Z / Alt+Z)
    actions.toggleWordWrapAction = new QAction(QObject::tr("Toggle &Word Wrap"), parent);
    actions.toggleWordWrapAction->setObjectName("action_toggle_word_wrap");
    actions.toggleWordWrapAction->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Z));
    viewMenu->addAction(actions.toggleWordWrapAction);

    viewMenu->addSeparator();

    // Zoom In (Cmd++ / Ctrl++)
    actions.zoomInAction = new QAction(QObject::tr("Zoom &In"), parent);
    actions.zoomInAction->setObjectName("action_zoom_in");
    actions.zoomInAction->setShortcut(QKeySequence::ZoomIn);
    viewMenu->addAction(actions.zoomInAction);

    // Zoom Out (Cmd+- / Ctrl+-)
    actions.zoomOutAction = new QAction(QObject::tr("Zoom &Out"), parent);
    actions.zoomOutAction->setObjectName("action_zoom_out");
    actions.zoomOutAction->setShortcut(QKeySequence::ZoomOut);
    viewMenu->addAction(actions.zoomOutAction);

    // Actual Size (100%) (Cmd+0 / Ctrl+0)
    actions.actualSizeAction = new QAction(QObject::tr("&Actual Size (100%)"), parent);
    actions.actualSizeAction->setObjectName("action_actual_size");
#if defined(Q_OS_MACOS)
    actions.actualSizeAction->setShortcut(QKeySequence(Qt::META | Qt::Key_0));
#else
    actions.actualSizeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
#endif
    viewMenu->addAction(actions.actualSizeAction);

    // ==========================================
    // 4. Help Menu (or macOS App Menu integration)
    // ==========================================
    QMenu* helpMenu = menuBar->addMenu(QObject::tr("&Help"));

    // About Markd
    actions.aboutAction = new QAction(QObject::tr("&About Markd"), parent);
    actions.aboutAction->setObjectName("action_about");
    actions.aboutAction->setMenuRole(QAction::AboutRole);
    helpMenu->addAction(actions.aboutAction);

    // Check for Updates…
    actions.checkUpdatesAction = new QAction(QObject::tr("Check for &Updates…"), parent);
    actions.checkUpdatesAction->setObjectName("action_check_updates");
    actions.checkUpdatesAction->setMenuRole(QAction::ApplicationSpecificRole);
    helpMenu->addAction(actions.checkUpdatesAction);

    return actions;
}

#include "ui/MenuBuilder.h"
#include <QKeySequence>

MenuBuilder::Actions MenuBuilder::buildMenuBar(QMenuBar* menuBar, QObject* parent)
{
    Actions actions;
    if (!menuBar) return actions;
    if (!parent) parent = menuBar;

    menuBar->clear();
    buildFileMenu(menuBar, parent, actions);
    buildEditMenu(menuBar, parent, actions);
    buildViewMenu(menuBar, parent, actions);
    buildHelpMenu(menuBar, parent, actions);
    return actions;
}

void MenuBuilder::buildFileMenu(QMenuBar* menuBar, QObject* parent, Actions& actions)
{
    QMenu* fileMenu = menuBar->addMenu(QObject::tr("&File"));

    actions.openAction = new QAction(QObject::tr("&Open…"), parent);
    actions.openAction->setObjectName("action_open");
    actions.openAction->setShortcut(QKeySequence::Open);
    fileMenu->addAction(actions.openAction);

    actions.saveAction = new QAction(QObject::tr("&Save"), parent);
    actions.saveAction->setObjectName("action_save");
    actions.saveAction->setShortcut(QKeySequence::Save);
    fileMenu->addAction(actions.saveAction);

    fileMenu->addSeparator();

    actions.exportHtmlAction = new QAction(QObject::tr("&Export to HTML…"), parent);
    actions.exportHtmlAction->setObjectName("action_export_html");
#if defined(Q_OS_MACOS)
    actions.exportHtmlAction->setShortcut(QKeySequence(Qt::META | Qt::SHIFT | Qt::Key_E));
#else
    actions.exportHtmlAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E));
#endif
    fileMenu->addAction(actions.exportHtmlAction);

    actions.printPdfAction = new QAction(QObject::tr("&Print / Export PDF…"), parent);
    actions.printPdfAction->setObjectName("action_print_pdf");
    actions.printPdfAction->setShortcut(QKeySequence::Print);
    fileMenu->addAction(actions.printPdfAction);

    fileMenu->addSeparator();

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
}

void MenuBuilder::buildEditMenu(QMenuBar* menuBar, QObject* parent, Actions& actions)
{
    QMenu* editMenu = menuBar->addMenu(QObject::tr("&Edit"));

    actions.undoAction = new QAction(QObject::tr("&Undo"), parent);
    actions.undoAction->setObjectName("action_undo");
    actions.undoAction->setShortcut(QKeySequence::Undo);
    editMenu->addAction(actions.undoAction);

    actions.redoAction = new QAction(QObject::tr("&Redo"), parent);
    actions.redoAction->setObjectName("action_redo");
    actions.redoAction->setShortcut(QKeySequence::Redo);
    editMenu->addAction(actions.redoAction);

    editMenu->addSeparator();

    actions.cutAction = new QAction(QObject::tr("Cu&t"), parent);
    actions.cutAction->setObjectName("action_cut");
    actions.cutAction->setShortcut(QKeySequence::Cut);
    editMenu->addAction(actions.cutAction);

    actions.copyAction = new QAction(QObject::tr("&Copy"), parent);
    actions.copyAction->setObjectName("action_copy");
    actions.copyAction->setShortcut(QKeySequence::Copy);
    editMenu->addAction(actions.copyAction);

    actions.copyHtmlAction = new QAction(QObject::tr("Copy as &HTML"), parent);
    actions.copyHtmlAction->setObjectName("action_copy_html");
#if defined(Q_OS_MACOS)
    actions.copyHtmlAction->setShortcut(QKeySequence(Qt::META | Qt::ALT | Qt::Key_C));
#else
    actions.copyHtmlAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C));
#endif
    editMenu->addAction(actions.copyHtmlAction);

    actions.pasteAction = new QAction(QObject::tr("&Paste"), parent);
    actions.pasteAction->setObjectName("action_paste");
    actions.pasteAction->setShortcut(QKeySequence::Paste);
    editMenu->addAction(actions.pasteAction);

    actions.selectAllAction = new QAction(QObject::tr("Select &All"), parent);
    actions.selectAllAction->setObjectName("action_select_all");
    actions.selectAllAction->setShortcut(QKeySequence::SelectAll);
    editMenu->addAction(actions.selectAllAction);

    editMenu->addSeparator();

    actions.findAction = new QAction(QObject::tr("&Find in Document"), parent);
    actions.findAction->setObjectName("action_find");
    actions.findAction->setShortcut(QKeySequence::Find);
    editMenu->addAction(actions.findAction);
}

void MenuBuilder::buildViewMenu(QMenuBar* menuBar, QObject* parent, Actions& actions)
{
    QMenu* viewMenu = menuBar->addMenu(QObject::tr("&View"));

    actions.toggleModeAction = new QAction(QObject::tr("&Mode: Edit / Preview"), parent);
    actions.toggleModeAction->setObjectName("action_toggle_mode");
#if defined(Q_OS_MACOS)
    actions.toggleModeAction->setShortcut(QKeySequence(Qt::META | Qt::Key_E));
#else
    actions.toggleModeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
#endif
    viewMenu->addAction(actions.toggleModeAction);

    actions.toggleSidebarAction = new QAction(QObject::tr("Toggle &Sidebar"), parent);
    actions.toggleSidebarAction->setObjectName("action_toggle_sidebar");
    actions.toggleSidebarAction->setCheckable(true);
    actions.toggleSidebarAction->setChecked(true);
#if defined(Q_OS_MACOS)
    QKeySequence primarySidebarShortcut(Qt::META | Qt::Key_Backslash);
#else
    QKeySequence primarySidebarShortcut(Qt::CTRL | Qt::Key_Backslash);
#endif
    actions.toggleSidebarAction->setShortcuts({primarySidebarShortcut, QKeySequence(Qt::Key_F9)});
    viewMenu->addAction(actions.toggleSidebarAction);

    actions.toggleWordWrapAction = new QAction(QObject::tr("Toggle &Word Wrap"), parent);
    actions.toggleWordWrapAction->setObjectName("action_toggle_word_wrap");
    actions.toggleWordWrapAction->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Z));
    viewMenu->addAction(actions.toggleWordWrapAction);

    viewMenu->addSeparator();

    actions.zoomInAction = new QAction(QObject::tr("Zoom &In"), parent);
    actions.zoomInAction->setObjectName("action_zoom_in");
    actions.zoomInAction->setShortcut(QKeySequence::ZoomIn);
    viewMenu->addAction(actions.zoomInAction);

    actions.zoomOutAction = new QAction(QObject::tr("Zoom &Out"), parent);
    actions.zoomOutAction->setObjectName("action_zoom_out");
    actions.zoomOutAction->setShortcut(QKeySequence::ZoomOut);
    viewMenu->addAction(actions.zoomOutAction);

    actions.actualSizeAction = new QAction(QObject::tr("&Actual Size (100%)"), parent);
    actions.actualSizeAction->setObjectName("action_actual_size");
#if defined(Q_OS_MACOS)
    actions.actualSizeAction->setShortcut(QKeySequence(Qt::META | Qt::Key_0));
#else
    actions.actualSizeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
#endif
    viewMenu->addAction(actions.actualSizeAction);

    viewMenu->addSeparator();

    actions.toggleFullScreenAction = new QAction(QObject::tr("Toggle &Full Screen"), parent);
    actions.toggleFullScreenAction->setObjectName("action_toggle_fullscreen");
    actions.toggleFullScreenAction->setCheckable(true);
    actions.toggleFullScreenAction->setShortcutContext(Qt::ApplicationShortcut);
    actions.toggleFullScreenAction->setShortcuts({
        QKeySequence::FullScreen,
        QKeySequence(Qt::ALT | Qt::Key_Return),
        QKeySequence(Qt::ALT | Qt::Key_Enter)
    });
    viewMenu->addAction(actions.toggleFullScreenAction);
}

void MenuBuilder::buildHelpMenu(QMenuBar* menuBar, QObject* parent, Actions& actions)
{
    QMenu* helpMenu = menuBar->addMenu(QObject::tr("&Help"));

    actions.aboutAction = new QAction(QObject::tr("&About Markd"), parent);
    actions.aboutAction->setObjectName("action_about");
    actions.aboutAction->setMenuRole(QAction::AboutRole);
    helpMenu->addAction(actions.aboutAction);

    actions.checkUpdatesAction = new QAction(QObject::tr("Check for &Updates…"), parent);
    actions.checkUpdatesAction->setObjectName("action_check_updates");
    actions.checkUpdatesAction->setMenuRole(QAction::ApplicationSpecificRole);
    helpMenu->addAction(actions.checkUpdatesAction);

    actions.makeDefaultAction = new QAction(QObject::tr("Make Default for &Markdown…"), parent);
    actions.makeDefaultAction->setObjectName("action_make_default_md");
    actions.makeDefaultAction->setMenuRole(QAction::ApplicationSpecificRole);
    helpMenu->addAction(actions.makeDefaultAction);

    actions.openShowcaseAction = new QAction(QObject::tr("Open Demo &Showcase"), parent);
    actions.openShowcaseAction->setObjectName("action_open_showcase");
    helpMenu->addAction(actions.openShowcaseAction);
}

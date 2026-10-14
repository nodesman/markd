#pragma once

#include <cassert>
#include <iostream>
#include <QTemporaryFile>
#include <QFileInfo>
#include <QTextStream>
#include <QMenuBar>
#include <QAction>
#include <QList>
#include "Zinsser.h"
#include "ui/MainWindow.h"
#include "ui/MenuBuilder.h"
#include "ui/HeaderBar.h"

inline void testWindowAndMenuIntegrity() {
    using Zinsser::describe;
    using Zinsser::it;

    describe("Window initialization and title formatting", [&]() {
        it("initializes untitled document with zero-tabs root canvas", [&]() {
            MainWindow win;
            assert(win.windowTitle() == "Markd");
            assert(win.documentBuffer().isEmpty());
            assert(win.centralWidget() != nullptr);
            // Verify Zero-Tabs: centralWidget is rootCanvas QWidget containing header and editor, not QTabWidget
            assert(!win.centralWidget()->inherits("QTabWidget"));
            assert(win.headerBar() != nullptr);
            assert(win.editor() != nullptr);
        });

        it("formats window title and populates document buffer from file", [&]() {
            QTemporaryFile tempFile;
            assert(tempFile.open());
            QString content = "# Hello Markd\n\nThis is a test buffer.";
            QTextStream out(&tempFile);
            out << content;
            out.flush();
            QString tempPath = tempFile.fileName();
            QFileInfo fi(tempPath);

            MainWindow win(tempPath);
            assert(win.windowTitle() == QString("%1 — Markd").arg(fi.fileName()));
            assert(win.documentBuffer() == content);
        });
    });

    describe("Declarative menu bar and accelerator bindings", [&]() {
        it("constructs declarative menu hierarchy matching specification", [&]() {
            QMenuBar menuBar;
            MenuBuilder::Actions actions = MenuBuilder::buildMenuBar(&menuBar);

            // Verify actions exist
            assert(actions.openAction != nullptr);
            assert(actions.saveAction != nullptr);
            assert(actions.exportHtmlAction != nullptr);
            assert(actions.printPdfAction != nullptr);
            assert(actions.closeWindowAction != nullptr);

            assert(actions.undoAction != nullptr);
            assert(actions.redoAction != nullptr);
            assert(actions.cutAction != nullptr);
            assert(actions.copyAction != nullptr);
            assert(actions.copyHtmlAction != nullptr);
            assert(actions.pasteAction != nullptr);
            assert(actions.selectAllAction != nullptr);
            assert(actions.findAction != nullptr);

            assert(actions.toggleModeAction != nullptr);
            assert(actions.toggleSidebarAction != nullptr);
            assert(actions.toggleWordWrapAction != nullptr);
            assert(actions.zoomInAction != nullptr);
            assert(actions.zoomOutAction != nullptr);
            assert(actions.actualSizeAction != nullptr);

            assert(actions.aboutAction != nullptr);
            assert(actions.checkUpdatesAction != nullptr);

            // Verify Menus
            QList<QAction*> menus = menuBar.actions();
            // File, Edit, View, Help (4 menus)
            assert(menus.size() >= 4);
        });

        it("binds standard platform accelerators across menu items", [&]() {
            QMenuBar menuBar;
            MenuBuilder::Actions actions = MenuBuilder::buildMenuBar(&menuBar);

            assert(actions.openAction->shortcut() == QKeySequence::Open);
            assert(actions.saveAction->shortcut() == QKeySequence::Save);
            assert(actions.undoAction->shortcut() == QKeySequence::Undo);
            assert(actions.redoAction->shortcut() == QKeySequence::Redo);
            assert(actions.cutAction->shortcut() == QKeySequence::Cut);
            assert(actions.copyAction->shortcut() == QKeySequence::Copy);
            assert(actions.pasteAction->shortcut() == QKeySequence::Paste);
            assert(actions.selectAllAction->shortcut() == QKeySequence::SelectAll);
            assert(actions.findAction->shortcut() == QKeySequence::Find);
            assert(actions.printPdfAction->shortcut() == QKeySequence::Print);
            assert(actions.zoomInAction->shortcut() == QKeySequence::ZoomIn);
            assert(actions.zoomOutAction->shortcut() == QKeySequence::ZoomOut);

#if defined(Q_OS_MACOS)
            assert(actions.closeWindowAction->shortcut() == QKeySequence(Qt::META | Qt::Key_W));
            assert(actions.exportHtmlAction->shortcut() == QKeySequence(Qt::META | Qt::SHIFT | Qt::Key_E));
            assert(actions.copyHtmlAction->shortcut() == QKeySequence(Qt::META | Qt::ALT | Qt::Key_C));
            assert(actions.toggleModeAction->shortcut() == QKeySequence(Qt::META | Qt::Key_E));
            assert(actions.toggleSidebarAction->shortcut() == QKeySequence(Qt::META | Qt::Key_Backslash));
            assert(actions.actualSizeAction->shortcut() == QKeySequence(Qt::META | Qt::Key_0));
#endif
            assert(actions.toggleWordWrapAction->shortcut() == QKeySequence(Qt::ALT | Qt::Key_Z));
        });
    });
}

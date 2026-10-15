#pragma once

#include <cassert>
#include <iostream>
#include <QTemporaryFile>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <QMenuBar>
#include <QAction>
#include <QList>
#include <QDialog>
#include <QTimer>
#include <QWindowStateChangeEvent>
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

            // Reflects dirty marker in window title
            win.session()->setMarkdown(content + " extra");
            assert(win.windowTitle() == QString("%1* — Markd").arg(fi.fileName()));

            // Untitled dirty window title format
            MainWindow untitledWin;
            untitledWin.session()->setMarkdown("dirty draft");
            assert(untitledWin.windowTitle() == "Markd*");

            // Splitter state serialization and programmatic restoration
            QByteArray state = win.mainSplitter()->saveState();
            assert(win.restoreSplitterState(state));
            assert(win.restoreSplitterState(false, 240));
            assert(!win.isSidebarCollapsed());
            assert(win.sidebarSavedWidth() == 240);
            assert(win.restoreSplitterState(true, 240));
            assert(win.isSidebarCollapsed());

            // HTML export and PDF print triggers
            QTemporaryFile htmlOut(QDir::tempPath() + "/markd_export_XXXXXX.html");
            assert(htmlOut.open());
            htmlOut.close();
            win.exportHtml(htmlOut.fileName(), [](bool ok) {
                assert(ok);
            });
            win.copyHtmlToClipboard([](bool ok) {
                assert(ok);
            });

            QTemporaryFile pdfOut(QDir::tempPath() + "/markd_print_XXXXXX.pdf");
            assert(pdfOut.open());
            pdfOut.close();
            win.printToPdf(pdfOut.fileName());
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

        it("triggers menu action dispatchers for find, copy-html, and window closure", [&]() {
            MainWindow win;
            QAction* findAct = win.findChild<QAction*>("action_find");
            assert(findAct != nullptr);
            findAct->trigger();

            QAction* copyHtmlAct = win.findChild<QAction*>("action_copy_html");
            assert(copyHtmlAct != nullptr);
            copyHtmlAct->trigger();

            QAction* closeAct = win.findChild<QAction*>("action_close_window");
            assert(closeAct != nullptr);
            closeAct->trigger();
        });

        it("persists modified file buffers upon save action dispatch", [&]() {
            QTemporaryFile tempFile;
            assert(tempFile.open());
            tempFile.write("# Initial Buffer");
            tempFile.flush();
            MainWindow win(tempFile.fileName());
            win.session()->setMarkdown("# Mutated Buffer");
            assert(win.session()->isDirty());

            QAction* saveAct = win.findChild<QAction*>("action_save");
            assert(saveAct != nullptr);
            saveAct->trigger();

            assert(!win.session()->isDirty());
            assert(win.session()->markdown() == "# Mutated Buffer");
        });

        it("adapts layout margins on window state change events", [&]() {
            MainWindow win;
            QWindowStateChangeEvent stateEv(Qt::WindowNoState);
            QCoreApplication::sendEvent(&win, &stateEv);
            assert(win.headerBar() != nullptr);
        });

        it("relays native find results through window signal bus", [&]() {
            MainWindow win;
            int receivedActive = -1;
            int receivedTotal = -1;
            QObject::connect(&win, &MainWindow::findResult, [&](int active, int total) {
                receivedActive = active;
                receivedTotal = total;
            });
            QMetaObject::invokeMethod(&win, "onFindResult", Q_ARG(int, 2), Q_ARG(int, 5));
            assert(receivedActive == 2);
            assert(receivedTotal == 5);
        });

        it("toggles editor line wrap modes across menu actions", [&]() {
            MainWindow win;
            QAction* wrapAct = win.findChild<QAction*>("action_toggle_word_wrap");
            assert(wrapAct != nullptr);
            auto initialWrap = win.editor()->lineWrapMode();
            wrapAct->trigger();
            assert(win.editor()->lineWrapMode() != initialWrap);
            wrapAct->trigger();
            assert(win.editor()->lineWrapMode() == initialWrap);
        });

        it("adjusts magnification factors via zoom menu actions", [&]() {
            MainWindow win;
            double initialZoom = win.session()->zoomFactor();
            QAction* zoomIn = win.findChild<QAction*>("action_zoom_in");
            assert(zoomIn != nullptr);
            zoomIn->trigger();
            assert(win.session()->zoomFactor() > initialZoom);

            QAction* zoomOut = win.findChild<QAction*>("action_zoom_out");
            assert(zoomOut != nullptr);
            zoomOut->trigger();
            assert(std::abs(win.session()->zoomFactor() - initialZoom) < 0.01);

            QAction* resetZoom = win.findChild<QAction*>("action_actual_size");
            assert(resetZoom != nullptr);
            resetZoom->trigger();
            assert(std::abs(win.session()->zoomFactor() - 1.0) < 0.001);
        });

        it("dismisses informational about dialog without blocking execution", [&]() {
            MainWindow win;
            QTimer::singleShot(20, []() {
                for (QWidget* w : QApplication::topLevelWidgets()) {
                    if (qobject_cast<QDialog*>(w) || w->isModal()) {
                        w->close();
                    }
                }
            });
            QMetaObject::invokeMethod(&win, "onAbout");
        });

        it("dismisses native file dialog invocations without blocking", [&]() {
            QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs, true);
            MainWindow win;
            auto dismiss = []() {
                QTimer::singleShot(20, []() {
                    for (QWidget* w : QApplication::topLevelWidgets()) {
                        if (qobject_cast<QDialog*>(w) || w->isModal()) {
                            w->close();
                        }
                    }
                });
            };

            // Test onOpen
            dismiss();
            QMetaObject::invokeMethod(&win, "onOpen");

            // Test onPrintPdf
            dismiss();
            QMetaObject::invokeMethod(&win, "onPrintPdf");

            // Test onExportHtml
            dismiss();
            QMetaObject::invokeMethod(&win, "onExportHtml");

            // Test onSave with untitled window
            MainWindow untitledWin;
            dismiss();
            QMetaObject::invokeMethod(&untitledWin, "onSave");
        });
    });
}

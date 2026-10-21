#pragma once

#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QObject>

class MenuBuilder {
public:
    struct Actions {
        // File
        QAction* openAction = nullptr;
        QAction* saveAction = nullptr;
        QAction* exportHtmlAction = nullptr;
        QAction* printPdfAction = nullptr;
        QAction* closeWindowAction = nullptr;

        // Edit
        QAction* undoAction = nullptr;
        QAction* redoAction = nullptr;
        QAction* cutAction = nullptr;
        QAction* copyAction = nullptr;
        QAction* copyHtmlAction = nullptr;
        QAction* pasteAction = nullptr;
        QAction* selectAllAction = nullptr;
        QAction* findAction = nullptr;

        // View
        QAction* toggleModeAction = nullptr;
        QAction* toggleSidebarAction = nullptr;
        QAction* toggleWordWrapAction = nullptr;
        QAction* zoomInAction = nullptr;
        QAction* zoomOutAction = nullptr;
        QAction* actualSizeAction = nullptr;
        QAction* toggleFullScreenAction = nullptr;

        // Help / App
        QAction* aboutAction = nullptr;
        QAction* checkUpdatesAction = nullptr;
        QAction* makeDefaultAction = nullptr;
        QAction* openShowcaseAction = nullptr;
    };

    // Builds and attaches standard File, Edit, View, and Help menus to the menuBar.
    // Populates and returns the Actions struct for connecting slots.
    static Actions buildMenuBar(QMenuBar* menuBar, QObject* parent = nullptr);

private:
    static void buildFileMenu(QMenuBar* menuBar, QObject* parent, Actions& actions);
    static void buildEditMenu(QMenuBar* menuBar, QObject* parent, Actions& actions);
    static void buildViewMenu(QMenuBar* menuBar, QObject* parent, Actions& actions);
    static void buildHelpMenu(QMenuBar* menuBar, QObject* parent, Actions& actions);
};

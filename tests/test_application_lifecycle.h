#pragma once

#include <QSignalSpy>
#include <QTemporaryFile>
#include <QDir>
#include <QFileOpenEvent>
#include <QTest>
#include <cassert>
#include <iostream>
#include "Zinsser.h"
#include "app/MarkdApplication.h"
#include "ui/MainWindow.h"

inline void testMarkdApplication(MarkdApplication& app) {
    using Zinsser::describe;
    using Zinsser::it;

    qRegisterMetaType<MainWindow*>("MainWindow*");

    describe("Application singleton instance and window registration", [&]() {
        it("exposes singleton instance and application identity metadata", [&]() {
            assert(MarkdApplication::instance() == &app);
            assert(app.applicationName() == "Markd");
            assert(app.organizationName() == "Markd");
        });

        it("registers newly created window instances in application registry", [&]() {
            QSignalSpy createdSpy(&app, &MarkdApplication::windowCreated);
            int initialCount = app.windows().size();
            MainWindow* win1 = app.createWindow();
            assert(win1 != nullptr);
            assert(app.windows().size() == initialCount + 1);
            assert(app.windows().contains(win1));
            assert(createdSpy.count() == 1);
        });
    });

    describe("Window reuse and document spawning semantics", [&]() {
        it("reuses existing untitled clean window when opening document", [&]() {
            int initialCount = app.windows().size();
            MainWindow* win1 = app.windows().first();

            QTemporaryFile temp1(QDir::tempPath() + "/markd_app_reuse_XXXXXX.md");
            assert(temp1.open());
            temp1.write("# Reused Window Content");
            temp1.close();

            MainWindow* reusedWin = app.openFileInWindow(temp1.fileName());
            assert(reusedWin == win1);
            assert(app.windows().size() == initialCount);
            assert(reusedWin->currentFilePath() == temp1.fileName());
            assert(reusedWin->documentBuffer().contains("# Reused Window Content"));
        });

        it("spawns distinct window instance when active window contains dirty or loaded content", [&]() {
            int initialCount = app.windows().size();
            MainWindow* win1 = app.windows().first();

            QTemporaryFile temp2(QDir::tempPath() + "/markd_app_spawn_XXXXXX.md");
            assert(temp2.open());
            temp2.write("# Fresh Window Content");
            temp2.close();

            MainWindow* win2 = app.openFileInWindow(temp2.fileName());
            assert(win2 != win1);
            assert(win2 != nullptr);
            assert(app.windows().size() == initialCount + 1);
            assert(win2->currentFilePath() == temp2.fileName());
            assert(win2->documentBuffer().contains("# Fresh Window Content"));
        });

        it("handles macOS file open events by opening document in window", [&]() {
            int initialCount = app.windows().size();

            QTemporaryFile temp3(QDir::tempPath() + "/markd_app_event_XXXXXX.md");
            assert(temp3.open());
            temp3.write("# Event Dispatched Content");
            temp3.close();

            QFileOpenEvent openEvent(temp3.fileName());
            bool handled = QCoreApplication::sendEvent(&app, &openEvent);
            assert(handled);
            assert(app.windows().size() == initialCount + 1);
            MainWindow* eventWin = app.windows().last();
            assert(eventWin->currentFilePath() == temp3.fileName());
            assert(eventWin->documentBuffer().contains("# Event Dispatched Content"));
        });
    });

    describe("Window destruction and application termination", [&]() {
        it("unregisters closed window instances from application registry", [&]() {
            QSignalSpy destroyedSpy(&app, &MarkdApplication::windowDestroyed);
            MainWindow* winToClose = app.windows().last();
            int countBeforeClose = app.windows().size();

            winToClose->close();
            QTest::qWait(50);
            assert(app.windows().size() == countBeforeClose - 1);
            assert(!app.windows().contains(winToClose));
            assert(destroyedSpy.count() >= 1);
        });

        it("terminates all active window instances on bulk close request", [&]() {
            app.closeAllWindows();
            QTest::qWait(50);
            assert(app.windows().isEmpty());
        });
    });
}

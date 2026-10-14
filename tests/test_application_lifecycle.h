#pragma once

#include <QSignalSpy>
#include <QTemporaryFile>
#include <QDir>
#include <QFileOpenEvent>
#include <QTest>
#include <cassert>
#include <iostream>
#include "app/MarkdApplication.h"
#include "ui/MainWindow.h"

inline void testMarkdApplication(MarkdApplication& app) {
    qRegisterMetaType<MainWindow*>("MainWindow*");

    // 1. exposes singleton instance and metadata
    assert(MarkdApplication::instance() == &app);
    assert(app.applicationName() == "Markd");
    assert(app.organizationName() == "Markd");

    // 2. registers newly created window in registry
    QSignalSpy createdSpy(&app, &MarkdApplication::windowCreated);
    QSignalSpy destroyedSpy(&app, &MarkdApplication::windowDestroyed);

    int initialCount = app.windows().size();
    MainWindow* win1 = app.createWindow();
    assert(win1 != nullptr);
    assert(app.windows().size() == initialCount + 1);
    assert(app.windows().contains(win1));
    assert(createdSpy.count() == 1);

    // 3. reuses clean untitled window for file opening
    QTemporaryFile temp1(QDir::tempPath() + "/markd_app_reuse_XXXXXX.md");
    assert(temp1.open());
    temp1.write("# Reused Window Content");
    temp1.close();

    MainWindow* reusedWin = app.openFileInWindow(temp1.fileName());
    assert(reusedWin == win1);
    assert(app.windows().size() == initialCount + 1);
    assert(reusedWin->currentFilePath() == temp1.fileName());
    assert(reusedWin->documentBuffer().contains("# Reused Window Content"));

    // 4. spawns distinct window when existing window contains file or content
    QTemporaryFile temp2(QDir::tempPath() + "/markd_app_spawn_XXXXXX.md");
    assert(temp2.open());
    temp2.write("# Fresh Window Content");
    temp2.close();

    MainWindow* win2 = app.openFileInWindow(temp2.fileName());
    assert(win2 != win1);
    assert(win2 != nullptr);
    assert(app.windows().size() == initialCount + 2);
    assert(win2->currentFilePath() == temp2.fileName());
    assert(win2->documentBuffer().contains("# Fresh Window Content"));

    // 5. intercepts and processes QFileOpenEvent (macOS Finder / Dock drops)
    QTemporaryFile temp3(QDir::tempPath() + "/markd_app_event_XXXXXX.md");
    assert(temp3.open());
    temp3.write("# Event Dispatched Content");
    temp3.close();

    QFileOpenEvent openEvent(temp3.fileName());
    bool handled = QCoreApplication::sendEvent(&app, &openEvent);
    assert(handled);
    assert(app.windows().size() == initialCount + 3);
    MainWindow* eventWin = app.windows().last();
    assert(eventWin->currentFilePath() == temp3.fileName());
    assert(eventWin->documentBuffer().contains("# Event Dispatched Content"));

    // 6. unregisters window upon closing and destruction
    int countBeforeClose = app.windows().size();
    win2->close();
    QTest::qWait(50); // process WA_DeleteOnClose
    assert(app.windows().size() == countBeforeClose - 1);
    assert(!app.windows().contains(win2));
    assert(destroyedSpy.count() >= 1);

    // 7. closes all registered windows cleanly
    app.closeAllWindows();
    QTest::qWait(50);
    assert(app.windows().isEmpty());

    std::cout << "PASS: William Zinsser MarkdApplication domain tests passed!\n";
}

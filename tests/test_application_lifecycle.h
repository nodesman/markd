#pragma once

#include <QSignalSpy>
#include <QTemporaryFile>
#include <QTemporaryDir>
#include <QImage>
#include <QProcess>
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

        it("binds monolith application window icon from bundled single-source png", [&]() {
            assert(!app.windowIcon().isNull());
            QImage iconImg(QStringLiteral(":/branding/app_icon.png"));
            assert(!iconImg.isNull());
            assert(iconImg.width() == iconImg.height());
            assert(iconImg.width() >= 512);
        });

        it("validates icon dimensions and generates ico artifact via ffmpeg script", [&]() {
            QString scriptPath = QFileInfo(QStringLiteral(__FILE__)).absoluteDir().filePath(QStringLiteral("../scripts/generate_icons.sh"));
            assert(QFileInfo::exists(scriptPath));

            QTemporaryDir tempDir;
            assert(tempDir.isValid());

            // 1. Rejects non-square image with exit code 4
            QString nonSquarePath = tempDir.filePath(QStringLiteral("non_square.png"));
            QImage nonSquare(600, 400, QImage::Format_RGBA8888);
            nonSquare.fill(Qt::black);
            assert(nonSquare.save(nonSquarePath, "PNG"));
            int rcNonSquare = QProcess::execute(QStringLiteral("bash"), {scriptPath, nonSquarePath, tempDir.path()});
            assert(rcNonSquare == 4);

            // 2. Rejects undersized (<512x512) square image with exit code 4
            QString smallPath = tempDir.filePath(QStringLiteral("small_square.png"));
            QImage smallSquare(128, 128, QImage::Format_RGBA8888);
            smallSquare.fill(Qt::black);
            assert(smallSquare.save(smallPath, "PNG"));
            int rcSmall = QProcess::execute(QStringLiteral("bash"), {scriptPath, smallPath, tempDir.path()});
            assert(rcSmall == 4);

            // 3. Accepts valid >=512x512 square PNG and generates valid .ico with magic header 00 00 01 00
            QString validPath = tempDir.filePath(QStringLiteral("valid_512.png"));
            QImage validSquare(512, 512, QImage::Format_RGBA8888);
            validSquare.fill(QColor(5, 5, 5));
            assert(validSquare.save(validPath, "PNG"));
            QString outDir = tempDir.filePath(QStringLiteral("out_icons"));
            int rcValid = QProcess::execute(QStringLiteral("bash"), {scriptPath, validPath, outDir});
            assert(rcValid == 0);

            QFile icoFile(outDir + QStringLiteral("/markd.ico"));
            assert(icoFile.exists());
            assert(icoFile.open(QIODevice::ReadOnly));
            QByteArray header = icoFile.read(4);
            assert(header.size() == 4);
            assert(static_cast<unsigned char>(header[0]) == 0x00);
            assert(static_cast<unsigned char>(header[1]) == 0x00);
            assert(static_cast<unsigned char>(header[2]) == 0x01);
            assert(static_cast<unsigned char>(header[3]) == 0x00);
        });

        it("injects chromium renderer accessibility guard into environment before engine initialization", [&]() {
            QByteArray activeFlags = qgetenv("QTWEBENGINE_CHROMIUM_FLAGS");
            assert(activeFlags.contains("--disable-renderer-accessibility"));

            // Verify idempotency when already present
            MarkdApplication::applyChromiumStabilityFlags();
            assert(qgetenv("QTWEBENGINE_CHROMIUM_FLAGS") == activeFlags);

            // Verify clean injection when starting from empty environment variable
            qputenv("QTWEBENGINE_CHROMIUM_FLAGS", "");
            MarkdApplication::applyChromiumStabilityFlags();
            assert(qgetenv("QTWEBENGINE_CHROMIUM_FLAGS") == QByteArray("--disable-renderer-accessibility"));

            // Restore test harness flags and verify space-delimited append
            qputenv("QTWEBENGINE_CHROMIUM_FLAGS", "--no-sandbox --single-process");
            MarkdApplication::applyChromiumStabilityFlags();
            assert(qgetenv("QTWEBENGINE_CHROMIUM_FLAGS") == QByteArray("--no-sandbox --single-process --disable-renderer-accessibility"));
        });

        it("registers newly created window instances in application registry", [&]() {
            QSignalSpy createdSpy(&app, &MarkdApplication::windowCreated);
            int initialCount = app.windows().size();
            MainWindow* win1 = app.createWindow();
            assert(win1 != nullptr);
            assert(app.windows().size() == initialCount + 1);
            assert(app.windows().contains(win1));
            assert(createdSpy.count() == 1);

            // Active window query retrieves active or first available registered window
            win1->activateWindow();
            MainWindow* active = app.activeWindow();
            assert(active == win1);

            // Null window registration guard safely ignores null pointer
            QMetaObject::invokeMethod(&app, "registerWindow", Q_ARG(MainWindow*, nullptr));

            // Empty file path safely returns null
            assert(app.openFileInWindow("") == nullptr);
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
            assert(reusedWin->currentMode() == MainWindow::Mode::Preview);
            assert(reusedWin->isSidebarCollapsed());
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
            assert(win2->currentMode() == MainWindow::Mode::Preview);
            assert(win2->isSidebarCollapsed());
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
            assert(eventWin->currentMode() == MainWindow::Mode::Preview);
            assert(eventWin->isSidebarCollapsed());
        });

        it("defaults to preview mode and collapses sidebar when application creates window with document file argument", [&]() {
            QTemporaryFile doc(QDir::tempPath() + "/markd_init_preview_XXXXXX.md");
            assert(doc.open());
            doc.write("# CLI Document\n\nPreview body text.");
            doc.close();

            MainWindow* win = app.createWindow(doc.fileName());
            assert(win != nullptr);
            assert(win->currentMode() == MainWindow::Mode::Preview);
            assert(win->isSidebarCollapsed());
        });

        it("retains edit mode and uncollapsed sidebar when application creates empty untitled window", [&]() {
            MainWindow* blankWin = app.createWindow();
            assert(blankWin != nullptr);
            assert(blankWin->currentMode() == MainWindow::Mode::Edit);
            assert(!blankWin->isSidebarCollapsed());
        });

        it("honors explicit initial mode and sidebar collapse parameters during construction", [&]() {
            QTemporaryFile doc(QDir::tempPath() + "/markd_explicit_init_XXXXXX.md");
            assert(doc.open());
            doc.write("# Explicit Init");
            doc.close();

            MainWindow previewWin(doc.fileName(), MainWindow::Mode::Preview, true);
            assert(previewWin.currentMode() == MainWindow::Mode::Preview);
            assert(previewWin.isSidebarCollapsed());

            MainWindow editWin(doc.fileName(), MainWindow::Mode::Edit, false);
            assert(editWin.currentMode() == MainWindow::Mode::Edit);
            assert(!editWin.isSidebarCollapsed());

            MainWindow defaultWin(doc.fileName());
            assert(defaultWin.currentMode() == MainWindow::Mode::Edit);
            assert(!defaultWin.isSidebarCollapsed());
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

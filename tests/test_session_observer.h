#pragma once

#include <QTemporaryFile>
#include <QDir>
#include <QFileInfo>
#include <cassert>
#include <iostream>
#include <QScrollBar>
#include <QLabel>
#include "Zinsser.h"
#include "app/MarkdApplication.h"
#include "ui/MainWindow.h"
#include "ui/TocWidget.h"
#include "ui/StatusBarTelemetry.h"
#include "core/DocumentSession.h"

inline void testMainWindowSessionObserver(MarkdApplication& app) {
    Q_UNUSED(app);
    using Zinsser::describe;
    using Zinsser::it;

    MainWindow window;

    describe("Main window document session observation and synchronization", [&]() {
        it("exposes active document session instance on window creation", [&]() {
            assert(window.session() != nullptr);
            assert(window.currentFilePath().isEmpty());
            assert(window.documentBuffer().isEmpty());
        });

        it("reflects session markdown mutations into editor without recursive loops", [&]() {
            window.session()->setMarkdown("# Injected Heading");
            assert(window.documentBuffer() == "# Injected Heading");
            assert(window.editor()->toPlainText() == "# Injected Heading");
        });

        it("synchronizes editor edits into session buffer and updates dirty window title", [&]() {
            window.editor()->setPlainText("# User Edited Heading\n\nBody paragraph.");
            assert(window.session()->markdown() == "# User Edited Heading\n\nBody paragraph.");
            assert(window.session()->isDirty());
            assert(window.windowTitle().contains("*"));
        });

        it("synchronizes outline extraction from session into table of contents widget", [&]() {
            window.session()->flushOutline();
            assert(window.tocWidget()->headingCount() == 1);
            assert(window.tocWidget()->headings()[0].title == "User Edited Heading");
        });

        it("propagates zoom factor mutations to editor font size", [&]() {
            qreal initialPointSize = window.editor()->font().pointSizeF();
            window.session()->zoomIn();
            assert(window.editor()->font().pointSizeF() > initialPointSize);
            window.session()->resetZoom();
            assert(std::abs(window.editor()->font().pointSizeF() - 13.0) < 0.01);
        });

        it("loads file via session and updates window title and clean state", [&]() {
            QTemporaryFile temp(QDir::tempPath() + "/markd_obs_test_XXXXXX.md");
            assert(temp.open());
            temp.write("# Observer Loaded Document");
            temp.close();

            bool loaded = window.loadFile(temp.fileName());
            assert(loaded);
            assert(window.currentFilePath() == temp.fileName());
            assert(window.documentBuffer() == "# Observer Loaded Document");
            assert(!window.session()->isDirty());
            assert(!window.windowTitle().contains("*"));
            assert(window.windowTitle().contains(QFileInfo(temp.fileName()).fileName()));
        });
    });

    describe("External disk reload observation and viewport preservation", [&]() {
        it("notifies status bar telemetry when external disk modification conflicts with dirty buffer", [&]() {
            MainWindow conflictWindow;
            conflictWindow.resize(1000, 650);
            conflictWindow.show();
            app.processEvents();

            QTemporaryFile temp(QDir::tempPath() + "/markd_conflict_obs_XXXXXX.md");
            temp.setAutoRemove(true);
            assert(temp.open());
            temp.write("# Initial Disk Content\n\nPreserved line.");
            temp.close();

            assert(conflictWindow.loadFile(temp.fileName()));
            conflictWindow.editor()->setPlainText("# Dirty Modified Buffer");
            assert(conflictWindow.session()->isDirty());

            {
                QFile diskFile(temp.fileName());
                assert(diskFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
                diskFile.write("# External Conflict Intrusion");
                diskFile.close();
            }

            QMetaObject::invokeMethod(conflictWindow.session(), "onWatcherTimeout");
            app.processEvents();

            assert(conflictWindow.session()->isDirty());
            assert(conflictWindow.editor()->toPlainText() == "# Dirty Modified Buffer");
            assert(conflictWindow.statusBar() != nullptr);
            assert(conflictWindow.statusBar()->notificationLabel() != nullptr);
            assert(conflictWindow.statusBar()->notificationLabel()->isVisible());
            assert(conflictWindow.statusBar()->notificationLabel()->text().contains("Disk file changed externally"));
        });

        it("preserves active viewport scroll offset across external disk reloads", [&]() {
            MainWindow scrollWindow;
            scrollWindow.resize(1000, 650);
            scrollWindow.show();
            app.processEvents();

            QTemporaryFile temp(QDir::tempPath() + "/markd_scroll_obs_XXXXXX.md");
            temp.setAutoRemove(true);
            assert(temp.open());
            QString longDocument = "# Scroll Document Heading\n\n";
            for (int i = 0; i < 80; ++i) {
                longDocument += QString("Paragraph index %1 with sufficient vertical content height for scrolling.\n\n").arg(i);
            }
            temp.write(longDocument.toUtf8());
            temp.close();

            assert(scrollWindow.loadFile(temp.fileName()));
            app.processEvents();
            scrollWindow.editor()->verticalScrollBar()->setValue(100);
            app.processEvents();
            const int targetScroll = scrollWindow.editor()->verticalScrollBar()->value();
            assert(targetScroll > 0);

            {
                QFile diskFile(temp.fileName());
                assert(diskFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
                QString appendedDocument = longDocument + "# Appended Tail Section\n\nFurther content.";
                diskFile.write(appendedDocument.toUtf8());
                diskFile.close();
            }

            QMetaObject::invokeMethod(scrollWindow.session(), "onWatcherTimeout");
            app.processEvents();

            assert(!scrollWindow.session()->isDirty());
            assert(scrollWindow.editor()->toPlainText().contains("Appended Tail Section"));
            assert(scrollWindow.editor()->verticalScrollBar()->value() == targetScroll);
        });

        it("preserves sidebar collapse state and saved width across disk reloads", [&]() {
            MainWindow sidebarWindow;
            sidebarWindow.resize(1000, 650);
            sidebarWindow.show();
            app.processEvents();

            QTemporaryFile temp(QDir::tempPath() + "/markd_sidebar_obs_XXXXXX.md");
            temp.setAutoRemove(true);
            assert(temp.open());
            temp.write("# Sidebar State Document\n\nInitial content.");
            temp.close();

            assert(sidebarWindow.loadFile(temp.fileName()));
            app.processEvents();
            sidebarWindow.setSidebarCollapsed(true);
            sidebarWindow.setSidebarSavedWidth(340);
            assert(sidebarWindow.isSidebarCollapsed());
            assert(sidebarWindow.sidebarSavedWidth() == 340);

            {
                QFile diskFile(temp.fileName());
                assert(diskFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
                diskFile.write("# Sidebar State Document\n\nUpdated disk content.");
                diskFile.close();
            }

            QMetaObject::invokeMethod(sidebarWindow.session(), "onWatcherTimeout");
            app.processEvents();

            assert(!sidebarWindow.session()->isDirty());
            assert(sidebarWindow.editor()->toPlainText().contains("Updated disk content."));
            assert(sidebarWindow.isSidebarCollapsed());
            assert(sidebarWindow.sidebarSavedWidth() == 340);
        });
    });
}

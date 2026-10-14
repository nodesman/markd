#pragma once

#include <QTemporaryFile>
#include <QDir>
#include <QFileInfo>
#include <cassert>
#include <iostream>
#include <cmath>
#include "Zinsser.h"
#include "app/MarkdApplication.h"
#include "ui/MainWindow.h"
#include "ui/TocWidget.h"
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
}

#pragma once

#include <QTemporaryFile>
#include <QDir>
#include <QFileInfo>
#include <cassert>
#include <iostream>
#include <cmath>
#include "app/MarkdApplication.h"
#include "ui/MainWindow.h"
#include "ui/TocWidget.h"
#include "core/DocumentSession.h"

inline void testMainWindowSessionObserver(MarkdApplication& app) {
    Q_UNUSED(app);

    MainWindow window;

    // 1. exposes active DocumentSession instance
    assert(window.session() != nullptr);
    assert(window.currentFilePath().isEmpty());
    assert(window.documentBuffer().isEmpty());

    // 2. reflects session markdown mutations in editor without recursive loops
    window.session()->setMarkdown("# Injected Heading");
    assert(window.documentBuffer() == "# Injected Heading");
    assert(window.editor()->toPlainText() == "# Injected Heading");

    // 3. synchronizes editor typing into session buffer and updates dirty state
    window.editor()->setPlainText("# User Edited Heading\n\nBody paragraph.");
    assert(window.session()->markdown() == "# User Edited Heading\n\nBody paragraph.");
    assert(window.session()->isDirty());
    assert(window.windowTitle().contains("*"));

    // 4. synchronizes outline extraction from session into toc widget
    window.session()->flushOutline();
    assert(window.tocWidget()->headingCount() == 1);
    assert(window.tocWidget()->headings()[0].title == "User Edited Heading");

    // 5. propagates zoom factor mutations to editor font and preview
    qreal initialPointSize = window.editor()->font().pointSizeF();
    window.session()->zoomIn();
    assert(window.editor()->font().pointSizeF() > initialPointSize);
    window.session()->resetZoom();
    assert(std::abs(window.editor()->font().pointSizeF() - 13.0) < 0.01);

    // 6. loads file via session and updates window title and clean state
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

    std::cout << "PASS: William Zinsser MainWindow DocumentSession observer tests passed!\n";
}

#include <QApplication>
#include <cassert>
#include <iostream>
#include <QTemporaryFile>
#include <QFileInfo>
#include <QTextStream>
#include <QAction>
#include <QMenuBar>
#include <QFile>
#include "../src/CommandLineOptions.h"
#include "../src/MainWindow.h"
#include "../src/MenuBuilder.h"
#include "../src/Theme.h"
#include "../src/PlatformFrame.h"
#include "../src/HeaderBar.h"

void testCommandLineOptions() {
    // Test 1: Empty args
    {
        QStringList args = {"markd"};
        CommandLineOptions opts = CommandLineOptions::parse(args);
        assert(opts.parseSuccess);
        assert(!opts.showHelp);
        assert(!opts.showVersion);
        assert(!opts.hasFile);
    }

    // Test 2: Help flag
    {
        QStringList args = {"markd", "--help"};
        CommandLineOptions opts = CommandLineOptions::parse(args);
        assert(opts.parseSuccess);
        assert(opts.showHelp);
        assert(!opts.showVersion);
        assert(!opts.hasFile);
    }

    // Test 3: Short help flag
    {
        QStringList args = {"markd", "-h"};
        CommandLineOptions opts = CommandLineOptions::parse(args);
        assert(opts.parseSuccess);
        assert(opts.showHelp);
    }

    // Test 4: Version flag
    {
        QStringList args = {"markd", "--version"};
        CommandLineOptions opts = CommandLineOptions::parse(args);
        assert(opts.parseSuccess);
        assert(!opts.showHelp);
        assert(opts.showVersion);
    }

    // Test 5: File argument
    {
        QStringList args = {"markd", "README.md"};
        CommandLineOptions opts = CommandLineOptions::parse(args);
        assert(opts.parseSuccess);
        assert(!opts.showHelp);
        assert(!opts.showVersion);
        assert(opts.hasFile);
        assert(opts.filePath == "README.md");
    }

    // Test 6: Unknown option
    {
        QStringList args = {"markd", "--unknown-flag"};
        CommandLineOptions opts = CommandLineOptions::parse(args);
        assert(!opts.parseSuccess);
        assert(opts.errorMessage.contains("Unknown option"));
    }

    // Test 7: Multiple positional arguments
    {
        QStringList args = {"markd", "file1.md", "file2.md"};
        CommandLineOptions opts = CommandLineOptions::parse(args);
        assert(!opts.parseSuccess);
        assert(opts.errorMessage.contains("Unexpected positional argument"));
    }

    std::cout << "PASS: CommandLineOptions tests passed!\n";
}

void testWindowAndMenuIntegrity() {
    // Test MainWindow initialization without file (untitled)
    {
        MainWindow win;
        assert(win.windowTitle() == "Markd");
        assert(win.documentBuffer().isEmpty());
        assert(win.centralWidget() != nullptr);
        // Verify Zero-Tabs: centralWidget is rootCanvas QWidget containing header and editor, not QTabWidget
        assert(!win.centralWidget()->inherits("QTabWidget"));
        assert(win.headerBar() != nullptr);
        assert(win.editor() != nullptr);
    }

    // Test MainWindow with file loading and title formatting
    {
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
    }

    // Test Declarative MenuBuilder matching menu_items.yml
    {
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

        // Verify shortcuts
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
    }

    std::cout << "PASS: Window and Menu integrity tests passed!\n";
}

void testThemeEngine(QApplication& app) {
    // 1. Verify resource exists in Qt VFS
    assert(QFile::exists(":/theme/theme.qss"));

    // 2. Apply theme
    Theme::apply(app);

    // 3. Verify stylesheet is loaded and non-empty
    QString qss = app.styleSheet();
    assert(!qss.isEmpty());

    // 4. Verify Obsidian palette color tokens in stylesheet
    assert(qss.contains("#050505"));
    assert(qss.contains("#0d1117"));
    assert(qss.contains("#1e242c"));
    assert(qss.contains("#161b22"));

    // 5. Verify accessible, comfortable translucent scrollbar styling in stylesheet
    assert(qss.contains("QScrollBar:vertical"));
    assert(qss.contains("width: 12px"));
    assert(qss.contains("rgba(255, 255, 255, 0.18)"));
    assert(qss.contains("border-radius: 4px"));

    // 6. Verify QPalette fallback window color
    assert(app.palette().color(QPalette::Window) == QColor("#050505"));
    assert(app.palette().color(QPalette::Base) == QColor("#050505"));
    assert(app.palette().color(QPalette::Text) == QColor("#f0f6fc"));
    assert(app.palette().color(QPalette::Highlight) == QColor("#161b22"));

    std::cout << "PASS: Theme engine tests passed!\n";
}

void testPlatformFrame(QApplication& app) {
    Q_UNUSED(app);

    // 1. Header height contract
    assert(PlatformFrame::headerHeight() == 38);

    // 2. Safe area contract
    {
        MainWindow win;
        win.resize(900, 650);

        // Windowed mode safe area
#if defined(Q_OS_MACOS)
        assert(PlatformFrame::safeArea(&win).leftMargin == 76);
#endif

        // Fullscreen mode safe area
        win.setWindowState(Qt::WindowFullScreen);
        assert(PlatformFrame::safeArea(&win).leftMargin == 0);

        // Reset to normal
        win.setWindowState(Qt::WindowNoState);
#if defined(Q_OS_MACOS)
        assert(PlatformFrame::safeArea(&win).leftMargin == 76);
#endif
    }

    // 3. Draggable region contract
    {
        // 0 <= y <= 4: resize buffer, should not be draggable
        assert(PlatformFrame::isDraggableRegion(QPoint(100, 2)) == false);
        assert(PlatformFrame::isDraggableRegion(QPoint(100, 4)) == false);

        // y > 4 && y <= 38: draggable header region
        assert(PlatformFrame::isDraggableRegion(QPoint(100, 5)) == true);
        assert(PlatformFrame::isDraggableRegion(QPoint(100, 10)) == true);
        assert(PlatformFrame::isDraggableRegion(QPoint(100, 38)) == true);

        // y > 38: below header, not draggable
        assert(PlatformFrame::isDraggableRegion(QPoint(100, 39)) == false);
        assert(PlatformFrame::isDraggableRegion(QPoint(100, 100)) == false);
    }

    // 4. HeaderBar container and true centering geometry
    {
        MainWindow win;
        win.resize(900, 650);
        HeaderBar* header = win.headerBar();
        assert(header != nullptr);
        assert(header->objectName() == "windowHeaderBar");
        assert(header->height() == 38);

        // Verify slot hooks exist
        assert(header->leadingSlot() != nullptr);
        assert(header->centerSlot() != nullptr);
        assert(header->trailingSlot() != nullptr);

        // Test geometry layout and centering calculation
        header->resize(900, 38);
        header->updateMargins();

        // In windowed mode on macOS, leading margin is 76px, so leadingSlot minimum width is >= 76px
        // TrailingSlot minimum width is also set to 76px to counter-balance flanking spacers
#if defined(Q_OS_MACOS)
        assert(header->leadingSlot()->minimumWidth() >= 76);
        assert(header->trailingSlot()->minimumWidth() >= 76);
        assert(header->leadingSlot()->minimumWidth() == header->trailingSlot()->minimumWidth());

        // Force layout pass to calculate physical widget geometries
        header->layout()->activate();
        // Mathematical True-Centering Assertion:
        // centerSlot.x() + centerSlot.width()/2 == header.width()/2 within 1px rounding tolerance
        int centerSlotMid = header->centerSlot()->x() + (header->centerSlot()->width() / 2);
        int windowMid = header->width() / 2;
        assert(std::abs(centerSlotMid - windowMid) <= 1);
#endif
    }

    std::cout << "PASS: PlatformFrame and HeaderBar tests passed!\n";
}

int main(int argc, char* argv[]) {
    // Run QApplication in offscreen platform mode for headless automated test suite
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    testCommandLineOptions();
    testWindowAndMenuIntegrity();
    testThemeEngine(app);
    testPlatformFrame(app);

    return 0;
}

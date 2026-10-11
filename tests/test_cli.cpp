#include <QApplication>
#include <cassert>
#include <iostream>
#include <QTemporaryFile>
#include <QFileInfo>
#include <QTextStream>
#include <QAction>
#include <QMenuBar>
#include "../src/CommandLineOptions.h"
#include "../src/MainWindow.h"
#include "../src/MenuBuilder.h"

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
        // Verify Zero-Tabs: centralWidget is QPlainTextEdit, not QTabWidget
        assert(win.centralWidget()->inherits("QPlainTextEdit"));
        assert(!win.centralWidget()->inherits("QTabWidget"));
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

int main(int argc, char* argv[]) {
    // Run QApplication in offscreen platform mode for headless automated test suite
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    testCommandLineOptions();
    testWindowAndMenuIntegrity();

    return 0;
}

#include <QApplication>
#include <cassert>
#include <iostream>
#include <QTemporaryFile>
#include <QFileInfo>
#include <QTextStream>
#include <QAction>
#include <QMenuBar>
#include <QFile>
#include <QStackedWidget>
#include <QPlainTextEdit>
#include <QWebEngineView>
#include <QTabBar>
#include "../src/CommandLineOptions.h"
#include "../src/MainWindow.h"
#include "../src/MenuBuilder.h"
#include "../src/Theme.h"
#include "../src/PlatformFrame.h"
#include "../src/HeaderBar.h"
#include "../src/ModeSwitcher.h"
#include "../src/PreviewView.h"
#include "../src/Converter.h"

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
    assert(qss.contains("width: 8px"));
    assert(qss.contains("rgba(255, 255, 255, 0.22)"));
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

void testModeSwitcher(QApplication& app) {
    Q_UNUSED(app);

    MainWindow win;
    win.resize(900, 650);

    // 1. ModeSwitcher existence in win.headerBar()->centerSlot()
    HeaderBar* header = win.headerBar();
    assert(header != nullptr);
    QWidget* centerSlot = header->centerSlot();
    assert(centerSlot != nullptr);

    ModeSwitcher* switcher = win.modeSwitcher();
    assert(switcher != nullptr);
    assert(switcher->parentWidget() == centerSlot);
    assert(switcher->objectName() == "modeSwitcher");

    // 2. Default state Edit
    assert(switcher->currentMode() == ModeSwitcher::DocumentMode::Edit);

    // 3. Mathematical true-centering assertion
    header->resize(900, 38);
    header->updateMargins();
    header->layout()->activate();
    centerSlot->layout()->activate();

    int switcherMid = switcher->mapTo(header, QPoint(switcher->width() / 2, 0)).x();
    int windowMid = header->width() / 2;
    assert(std::abs(switcherMid - windowMid) <= 1);

    // 4. Click event isolation (event->isAccepted() == true)
    // Left-click on preview segment region (x > width/2)
    QMouseEvent clickEvent(
        QEvent::MouseButtonPress,
        QPointF(100.0, 13.0),
        QPointF(100.0, 13.0),
        Qt::LeftButton,
        Qt::LeftButton,
        Qt::NoModifier
    );
    QApplication::sendEvent(switcher, &clickEvent);
    assert(clickEvent.isAccepted() == true);
    assert(switcher->currentMode() == ModeSwitcher::DocumentMode::Preview);

    // 5. Signal emission and state transitions
    int signalCount = 0;
    ModeSwitcher::DocumentMode lastEmittedMode = ModeSwitcher::DocumentMode::Preview;
    QObject::connect(switcher, &ModeSwitcher::modeChanged, [&](ModeSwitcher::DocumentMode mode) {
        signalCount++;
        lastEmittedMode = mode;
    });

    // Toggle via public slot / toggleMode()
    switcher->toggleMode();
    assert(switcher->currentMode() == ModeSwitcher::DocumentMode::Edit);
    assert(signalCount == 1);
    assert(lastEmittedMode == ModeSwitcher::DocumentMode::Edit);

    // 6. Keyboard action sync: triggering toggle action switches mode to Preview and fires modeChanged
    QAction* toggleAction = win.findChild<QAction*>("action_toggle_mode");
    assert(toggleAction != nullptr);
    toggleAction->trigger();
    assert(switcher->currentMode() == ModeSwitcher::DocumentMode::Preview);
    assert(signalCount == 2);
    assert(lastEmittedMode == ModeSwitcher::DocumentMode::Preview);

    // 7. Fullscreen safe margin invariance & centering stability
    win.setWindowState(Qt::WindowFullScreen);
    header->updateMargins();
    header->layout()->activate();
    centerSlot->layout()->activate();
    int fsSwitcherMid = switcher->mapTo(header, QPoint(switcher->width() / 2, 0)).x();
    int fsWindowMid = header->width() / 2;
    assert(std::abs(fsSwitcherMid - fsWindowMid) <= 1);

    win.setWindowState(Qt::WindowNoState);
    header->updateMargins();
    header->layout()->activate();
    centerSlot->layout()->activate();
    int restoredSwitcherMid = switcher->mapTo(header, QPoint(switcher->width() / 2, 0)).x();
    int restoredWindowMid = header->width() / 2;
    assert(std::abs(restoredSwitcherMid - restoredWindowMid) <= 1);

    // 8. Visual token & Brackets verification
    // Verify no brackets [] in buttons or switcher
    QList<QPushButton*> buttons = switcher->findChildren<QPushButton*>();
    assert(buttons.size() == 2);
    for (QPushButton* btn : buttons) {
        assert(!btn->text().contains('['));
        assert(!btn->text().contains(']'));
        assert(btn->text() == "Edit" || btn->text() == "Preview");
    }

    // Verify fixed dimensions and sizeHint
    assert(switcher->height() == 26);
    assert(switcher->width() == 140);
    assert(switcher->sizeHint().height() == 26);
    assert(switcher->sizeHint().width() == 140);

    std::cout << "PASS: ModeSwitcher tests passed!\n";
}

void testOfflineAssetSelfContainment() {
    // Verify all presentation engine assets exist in compiled Qt VFS (zero network egress)
    assert(QFile::exists(":/web/template.html"));
    assert(QFile::exists(":/web/css/obsidian.css"));
    assert(QFile::exists(":/web/css/style.css"));
    assert(QFile::exists(":/web/js/qwebchannel.js"));
    assert(QFile::exists(":/web/js/katex.min.css"));
    assert(QFile::exists(":/web/js/katex.min.js"));
    assert(QFile::exists(":/web/js/highlight.min.js"));
    assert(QFile::exists(":/web/js/markdown-it.min.js"));
    assert(QFile::exists(":/web/js/render.js"));

    std::cout << "PASS: Offline asset self-containment tests passed!\n";
}

void testConverterPipeline() {
    // 1. HTML escaping
    assert(Converter::escapeHtml("<div>&\"'</div>") == "&lt;div&gt;&amp;&quot;&#39;&lt;/div&gt;");

    // 2. Markdown to HTML fragment
    QString md = "# Title\n\n**Bold Text** and `code`";
    QString html = Converter::toHtmlFragment(md);
    assert(html.contains("Title"));
    assert(html.contains("Bold Text"));
    assert(html.contains("code"));

    // 3. Full document generation with Obsidian stylesheet
    QString fullDoc = Converter::toFullHtmlDocument(md);
    assert(fullDoc.contains("<!DOCTYPE html>"));
    assert(fullDoc.contains("#050505"));
    assert(fullDoc.contains("qrc:/web/css/obsidian.css"));
    assert(fullDoc.contains("Title"));

    std::cout << "PASS: Converter pipeline tests passed!\n";
}

void testDualViewports(QApplication& app) {
    MainWindow win;
    win.resize(900, 650);
    win.show();
    win.activateWindow();
    app.processEvents();

    // 1. Structural Containment & Zero-Tabs Assertion
    assert(!win.centralWidget()->inherits("QTabWidget"));
    assert(win.findChild<QTabBar*>() == nullptr);

    auto* stack = win.findChild<QStackedWidget*>("viewportStack");
    assert(stack != nullptr);
    assert(stack->count() == 2);
    assert(win.viewportStack() == stack);

    // 2. Public API Non-Regression
    assert(win.editor() != nullptr);
    assert(win.editor() == stack->widget(0)); // Edit mode at index 0

    // PreviewView at index 1
    assert(win.previewView() != nullptr);
    assert(win.previewView() == stack->widget(1));

    // 3. Buffer Permanence Test
    QString testPayload = "# Sovereign Header\n\nPreserved buffer test with formula: $$x^2 + y^2 = z^2$$";
    win.editor()->setPlainText(testPayload);
    assert(win.documentBuffer() == testPayload);

    // 4. Deterministic Stack Mapping
    assert(win.modeSwitcher()->currentMode() == ModeSwitcher::DocumentMode::Edit);
    assert(stack->currentIndex() == 0);

    // 5. Background Color Pre-Clear (White-Flash Guard)
    auto* preview = win.findChild<QWebEngineView*>("previewView");
    assert(preview != nullptr);
    assert(preview->page()->backgroundColor() == QColor("#050505"));

    // 6. Viewport Dynamic Size Hints
    assert(stack->sizePolicy().horizontalPolicy() == QSizePolicy::Expanding);
    assert(stack->sizePolicy().verticalPolicy() == QSizePolicy::Expanding);
    assert(stack->minimumSize() == QSize(0, 0));
    assert(win.editor()->minimumSize() == QSize(0, 0));
    assert(win.previewView()->minimumSize() == QSize(0, 0));

    // 7. IPC Architecture Bridge Pre-wiring
    assert(win.previewView()->bridge() != nullptr);
    assert(win.previewView()->channel() != nullptr);
    assert(win.previewView()->channel()->registeredObjects().contains("markdBridge"));

    // Toggle to Preview
    win.modeSwitcher()->setMode(ModeSwitcher::DocumentMode::Preview);
    app.processEvents();
    assert(stack->currentIndex() == 1);
    assert(win.documentBuffer() == testPayload); // Buffer remains completely intact!

    // Toggle back to Edit & Focus Check
    win.modeSwitcher()->setMode(ModeSwitcher::DocumentMode::Edit);
    app.processEvents();
    assert(stack->currentIndex() == 0);
    assert(win.documentBuffer() == testPayload);
    assert(win.editor()->hasFocus() || win.focusWidget() == win.editor());

    // Keyboard shortcut action toggle
    QAction* toggleAction = win.findChild<QAction*>("action_toggle_mode");
    assert(toggleAction != nullptr);
    toggleAction->trigger();
    app.processEvents();
    assert(stack->currentIndex() == 1);
    assert(win.documentBuffer() == testPayload);

    toggleAction->trigger();
    app.processEvents();
    assert(stack->currentIndex() == 0);
    assert(win.documentBuffer() == testPayload);
    assert(win.editor()->hasFocus() || win.focusWidget() == win.editor());

    std::cout << "PASS: Dual viewports tests passed!\n";
}

int main(int argc, char* argv[]) {
    // Run QApplication in offscreen platform mode for headless automated test suite
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    testCommandLineOptions();
    testWindowAndMenuIntegrity();
    testThemeEngine(app);
    testPlatformFrame(app);
    testModeSwitcher(app);
    testOfflineAssetSelfContainment();
    testConverterPipeline();
    testDualViewports(app);

    return 0;
}

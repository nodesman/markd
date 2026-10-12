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
#include <QSplitter>
#include <QSignalSpy>
#include <QLabel>
#include <QDir>
#include "../src/CommandLineOptions.h"
#include "../src/MainWindow.h"
#include "../src/MenuBuilder.h"
#include "../src/Theme.h"
#include "../src/PlatformFrame.h"
#include "../src/HeaderBar.h"
#include "../src/ModeSwitcher.h"
#include "../src/PreviewView.h"
#include "../src/Converter.h"
#include "../src/TocItem.h"
#include "../src/TocParser.h"
#include "../src/TocWidget.h"

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
    assert(stack->minimumWidth() >= 320);
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

void testTask9TocSidebarIntegration(QApplication& app) {
    MainWindow win;
    win.resize(1000, 650);
    win.show();
    win.activateWindow();
    app.processEvents();

    // --- Assertion 1: Splitter Integration, Viewport Protection & Stretch Factors ---
    auto* splitter = win.findChild<QSplitter*>("mainSplitter");
    assert(splitter != nullptr && splitter->count() == 2);
    assert(splitter->widget(0)->objectName() == "tocSidebar");
    assert(splitter->widget(1) == win.viewportStack());
    assert(splitter->isCollapsible(0) == true);
    assert(splitter->isCollapsible(1) == false); // Central viewport NEVER collapses to 0!
    assert(win.viewportStack()->minimumWidth() >= 320); // Central reading pane minimum width constraint
    assert(splitter->widget(0)->accessibleName() == "Table of Contents" || win.findChild<QWidget*>("tocSidebar")->accessibleName() == "Table of Contents"); // Accessibility landmark
    // Stretch factors: sidebar stays fixed width on window resize, viewport absorbs 100%
    int prevSidebarW = splitter->sizes().at(0);
    int prevViewportW = splitter->sizes().at(1);
    win.resize(1200, 650);
    app.processEvents();
    assert(splitter->sizes().at(0) == prevSidebarW); // Sidebar stays fixed width on window resize
    assert(splitter->sizes().at(1) == prevViewportW + 200); // Viewport absorbs 100% of window resize
    win.resize(1000, 650);
    app.processEvents();
    assert(win.findChild<QTabBar*>() == nullptr && !win.centralWidget()->inherits("QTabWidget"));

    // --- Assertion 2: Signal Contract & Action Trigger (Setting up Task 10) ---
    QSignalSpy toggleSpy(&win, &MainWindow::sidebarToggled);
    QAction* toggleAct = win.findChild<QAction*>("action_toggle_sidebar");
    assert(toggleAct != nullptr);
    toggleAct->trigger(); // Collapse via Cmd+\
    assert(win.isSidebarCollapsed() == true);
    assert(toggleSpy.count() == 1 && toggleSpy.takeFirst().at(0).toBool() == true);

    toggleAct->trigger(); // Uncollapse via Cmd+\
    assert(win.isSidebarCollapsed() == false);
    assert(toggleSpy.count() == 1 && toggleSpy.takeFirst().at(0).toBool() == false);

    // --- Assertion 3: Splitter Width Memory Retention, Interactive Drag-to-Zero & Drag-Reopen Guard ---
    // Note: splitter->setSizes() does NOT emit splitterMoved(); simulate drag interactions explicitly:
    emit splitter->splitterMoved(280, 1); // Drag handler with 280 (recording m_savedWidth = 280)
    assert(win.sidebarSavedWidth() == 280);
    assert(win.isSidebarCollapsed() == false);

    emit splitter->splitterMoved(0, 1);   // Drag handler with 0 (recording collapse)
    assert(win.isSidebarCollapsed() == true);
    assert(win.sidebarSavedWidth() == 280); // Maintained at 280, not overwritten with 0!
    assert(splitter->sizes().at(0) == 0);

    // Dragging sidebar back open from collapsed state:
    QSignalSpy dragOpenSpy(&win, &MainWindow::sidebarToggled);
    emit splitter->splitterMoved(250, 1); // Drag back open (pos > 0)
    assert(win.isSidebarCollapsed() == false);
    assert(win.sidebarSavedWidth() == 250);
    assert(dragOpenSpy.count() == 1 && dragOpenSpy.takeFirst().at(0).toBool() == false);

    win.toggleSidebar(); // Collapse again via shortcut
    assert(win.isSidebarCollapsed() == true);
    win.toggleSidebar(); // Uncollapse via shortcut: must restore 250px, NOT 0px!
    assert(win.isSidebarCollapsed() == false);
    assert(std::abs(splitter->sizes().at(0) - 250) <= 1);

    // Minimum width clamp
    assert(splitter->widget(0)->minimumWidth() >= 200 || win.isSidebarCollapsed());

    // --- Assertion 4: Cross-Session Collapsed State Persistence Roundtrip & State Re-derivation ---
    int savedWidth = win.sidebarSavedWidth();
    assert(savedWidth >= 200);
    win.setSidebarCollapsed(true);
    assert(win.isSidebarCollapsed() == true);
    QByteArray state = splitter->saveState();
    splitter->restoreState(state);
    win.restoreSplitterState(state); // Re-derives m_collapsed from restored dimensions
    assert(win.isSidebarCollapsed() == (splitter->sizes().at(0) == 0));
    win.setSidebarSavedWidth(savedWidth);
    win.toggleSidebar();
    assert(std::abs(splitter->sizes().at(0) - savedWidth) <= 1);

    // --- Assertion 5: Code Fence Shielding, Indented Code, Setext, Comments & Slug Disambiguation ---
    QString complexDoc =
        "Markd Document Title\n"
        "===================\n\n"
        "<!-- \n# Multi-line Comment Ignored\n -->\n\n"
        "~~~markdown\n# Tilde Fence Ignored\n~~~\n\n"
        "````\n# Multi-Backtick Ignored\n````\n\n"
        "    # 4-Space Indented Code Block Ignored\n\n"
        "#NoSpace Ignored\n\n"
        "Subtitle Section\n"
        "----------------\n\n"
        "## Overview ##\n\n"
        "## Overview-1\n\n"
        "## Overview\n\n"
        "> ## Blockquote Heading\n\n"
        "## Energy $E=mc^2$\n\n"
        "## A <span>b</span>\n\n"
        "## Header with ![logo](img.png)\n\n"
        "### 1. அறம் & Code";

    auto headings = TocParser::extractHeadings(complexDoc);
    assert(headings.size() == 10);
    assert(headings[0].level == 1 && headings[0].title == "Markd Document Title" && headings[0].id == "markd-document-title");
    assert(headings[1].level == 2 && headings[1].title == "Subtitle Section" && headings[1].id == "subtitle-section");
    assert(headings[2].level == 2 && headings[2].title == "Overview" && headings[2].id == "overview"); // Trailing ## stripped
    assert(headings[3].level == 2 && headings[3].title == "Overview-1" && headings[3].id == "overview-1");
    assert(headings[4].level == 2 && headings[4].title == "Overview" && headings[4].id == "overview-2"); // Disambiguated
    assert(headings[5].level == 2 && headings[5].title == "Blockquote Heading" && headings[5].id == "blockquote-heading");
    assert(headings[6].level == 2 && headings[6].title == "Energy E=mc^2" && headings[6].id == "energy-emc2"); // Math delimiters stripped
    assert(headings[7].level == 2 && headings[7].title == "A b" && headings[7].id == "a-b"); // Inline HTML stripped
    assert(headings[8].level == 2 && headings[8].title == "Header with logo" && headings[8].id == "header-with-logo"); // Image stripped
    assert(headings[9].level == 3 && headings[9].title == "1. அறம் & Code" && headings[9].id == "1-code"); // Collapses consecutive dashes

    // --- Assertion 6: Scroll-Spy Ping-Pong Isolation & Unknown Heading Graceful Guard ---
    auto* tocWidget = win.findChild<QWidget*>("tocSidebar");
    assert(tocWidget != nullptr);
    // Setting active heading from scroll-spy must NOT trigger outbound jump signal
    QSignalSpy jumpSpy(tocWidget, SIGNAL(headingActivated(QString)));
    QMetaObject::invokeMethod(tocWidget, "setActiveHeading", Q_ARG(QString, "whats-new-in-markd-v10"));
    assert(jumpSpy.count() == 0); // Strictly 0; avoids infinite scroll feedback loop

    // Unknown heading ID must safely no-op without crashing or jumping
    QMetaObject::invokeMethod(tocWidget, "setActiveHeading", Q_ARG(QString, "unknown-heading-id-999"));
    assert(jumpSpy.count() == 0);

    // --- Assertion 7: Empty Document UX & Headless Offscreen Stability ---
    QTemporaryFile tempEmptyFile(QDir::tempPath() + "/markd_empty_XXXXXX.md");
    tempEmptyFile.setAutoRemove(true);
    assert(tempEmptyFile.open());
    tempEmptyFile.close();
    win.loadFile(tempEmptyFile.fileName());
    app.processEvents();
    auto* placeholder = win.findChild<QLabel*>("tocPlaceholderLabel");
    assert(placeholder != nullptr && placeholder->isVisible());
    assert(placeholder->text().contains("No headings", Qt::CaseInsensitive));

    std::cout << "PASS: Task 9 TOC sidebar integration tests passed!\n";
}

void testTask9EdgeCases(QApplication& app) {
    MainWindow win;
    win.resize(1000, 650);
    win.show();
    win.activateWindow();
    app.processEvents();

    // 1. Empty string parsing
    auto emptyHeadings = TocParser::extractHeadings("");
    assert(emptyHeadings.empty());

    // 2. CRLF (\r\n) line endings parsing
    QString crlfDoc = "# Title\r\n\r\nParagraph\r\n\r\n## Section\r\n";
    auto crlfHeadings = TocParser::extractHeadings(crlfDoc);
    assert(crlfHeadings.size() == 2);
    assert(crlfHeadings[0].level == 1 && crlfHeadings[0].title == "Title" && crlfHeadings[0].lineNumber == 1);
    assert(crlfHeadings[1].level == 2 && crlfHeadings[1].title == "Section" && crlfHeadings[1].lineNumber == 5);

    // 3. Snake_case identifiers preserved
    QString snakeDoc = "## snake_case_identifier_test\n\n### CamelCase_Test_123";
    auto snakeHeadings = TocParser::extractHeadings(snakeDoc);
    assert(snakeHeadings.size() == 2);
    assert(snakeHeadings[0].title == "snake_case_identifier_test");
    assert(snakeHeadings[0].id == "snake_case_identifier_test");
    assert(snakeHeadings[1].title == "CamelCase_Test_123");
    assert(snakeHeadings[1].id == "camelcase_test_123");

    // 4. Formatting delimiters stripped without breaking text
    QString formatDoc = "## **Bold** *Italic* ~~Strike~~ `code`";
    auto formatHeadings = TocParser::extractHeadings(formatDoc);
    assert(formatHeadings.size() == 1);
    assert(formatHeadings[0].title == "Bold Italic Strike code");
    assert(formatHeadings[0].id == "bold-italic-strike-code");

    // 5. Unclosed HTML comment guard
    QString unclosedComment = "<!--\n# Commented Heading\nStill in comment";
    auto unclosedHeadings = TocParser::extractHeadings(unclosedComment);
    assert(unclosedHeadings.empty());

    // 6. 7 hashes rejected (CommonMark only supports 1-6)
    QString deepDoc = "####### Too Deep\n###### Six Hashes Ok";
    auto deepHeadings = TocParser::extractHeadings(deepDoc);
    assert(deepHeadings.size() == 1);
    assert(deepHeadings[0].level == 6 && deepHeadings[0].title == "Six Hashes Ok");

    // 7. TocWidget selection persistence on invalid ID
    TocWidget tocWidget;
    std::vector<TocItem> items = {{1, 1, "Section 1", "section-1"}, {2, 5, "Section 2", "section-2"}};
    tocWidget.setHeadings(items);
    assert(tocWidget.headingCount() == 2);
    tocWidget.setActiveHeading("");
    assert(tocWidget.activeHeadingId().isEmpty());
    tocWidget.setActiveHeading("section-2");
    assert(tocWidget.activeHeadingId() == "section-2");
    tocWidget.setActiveHeading("non-existent-id");
    assert(tocWidget.activeHeadingId() == "section-2"); // Not corrupted by unknown id

    // 8. Sidebar saved width minimum clamp
    win.setSidebarSavedWidth(50);
    assert(win.sidebarSavedWidth() == 200); // Clamped to 200

    // 9. Redundant collapse/uncollapse signal suppression
    win.setSidebarCollapsed(true);
    QSignalSpy dupSpy(&win, &MainWindow::sidebarToggled);
    win.setSidebarCollapsed(true); // Redundant
    assert(dupSpy.count() == 0);
    win.setSidebarCollapsed(false);
    assert(dupSpy.count() == 1 && dupSpy.takeFirst().at(0).toBool() == false);
    win.setSidebarCollapsed(false); // Redundant
    assert(dupSpy.count() == 0);

    // 10. Click-to-jump in Edit mode moves cursor to exact line
    win.modeSwitcher()->setMode(ModeSwitcher::DocumentMode::Edit);
    win.editor()->setPlainText("# First Line\n\nLine 3\nLine 4\n\n## Target Section\n\nLine 8");
    win.updateToc();
    assert(win.tocWidget()->headingCount() == 2);
    emit win.tocWidget()->headingActivated("target-section");
    app.processEvents();
    assert(win.editor()->textCursor().blockNumber() == 5); // Line 6 (1-based) is block index 5

    std::cout << "PASS: Task 9 edge cases tests passed!\n";
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
    testTask9TocSidebarIntegration(app);
    testTask9EdgeCases(app);

    return 0;
}

#pragma once

#include <QApplication>
#include <QSplitter>
#include <QSignalSpy>
#include <QAction>
#include <QLabel>
#include <QTemporaryFile>
#include <QDir>
#include <QTabBar>
#include <cassert>
#include <iostream>
#include <cmath>
#include <vector>
#include "Zinsser.h"
#include "ui/MainWindow.h"
#include "parser/TocParser.h"
#include "parser/TocItem.h"
#include "ui/TocWidget.h"
#include "ui/ModeSwitcher.h"

inline void testTocSidebarIntegration(QApplication& app) {
    using Zinsser::describe;
    using Zinsser::it;

    MainWindow win;
    win.resize(1000, 650);
    win.show();
    win.activateWindow();
    app.processEvents();

    auto* splitter = win.findChild<QSplitter*>("mainSplitter");
    assert(splitter != nullptr && splitter->count() == 2);

    describe("Splitter layout constraints and stretch allocations", [&]() {
        it("allocates fixed width to sidebar and assigns window resize expansion to reading pane", [&]() {
            assert(splitter->widget(0)->objectName() == "tocSidebar");
            assert(splitter->widget(1) == win.viewportStack());
            assert(splitter->isCollapsible(0) == true);
            assert(splitter->isCollapsible(1) == false);
            assert(win.viewportStack()->minimumWidth() >= 320);
            assert(splitter->widget(0)->accessibleName() == "Table of Contents" || win.findChild<QWidget*>("tocSidebar")->accessibleName() == "Table of Contents");

            int prevSidebarW = splitter->sizes().at(0);
            int prevViewportW = splitter->sizes().at(1);
            win.resize(1200, 650);
            app.processEvents();
            assert(splitter->sizes().at(0) == prevSidebarW);
            assert(splitter->sizes().at(1) == prevViewportW + 200);

            win.resize(1000, 650);
            app.processEvents();
            assert(win.findChild<QTabBar*>() == nullptr && !win.centralWidget()->inherits("QTabWidget"));
        });
    });

    describe("Sidebar outline collapse and state persistence", [&]() {
        it("dispatches sidebarToggled signal upon programmatic and shortcut collapse", [&]() {
            QSignalSpy toggleSpy(&win, &MainWindow::sidebarToggled);
            QAction* toggleAct = win.findChild<QAction*>("action_toggle_sidebar");
            assert(toggleAct != nullptr);

            toggleAct->trigger();
            assert(win.isSidebarCollapsed() == true);
            assert(toggleSpy.count() == 1 && toggleSpy.takeFirst().at(0).toBool() == true);

            toggleAct->trigger();
            assert(win.isSidebarCollapsed() == false);
            assert(toggleSpy.count() == 1 && toggleSpy.takeFirst().at(0).toBool() == false);
        });

        it("memorizes sidebar width across drag-to-zero interactions without zero overwrite", [&]() {
            emit splitter->splitterMoved(280, 1);
            assert(win.sidebarSavedWidth() == 280);
            assert(win.isSidebarCollapsed() == false);

            emit splitter->splitterMoved(0, 1);
            assert(win.isSidebarCollapsed() == true);
            assert(win.sidebarSavedWidth() == 280);
            assert(splitter->sizes().at(0) == 0);

            QSignalSpy dragOpenSpy(&win, &MainWindow::sidebarToggled);
            emit splitter->splitterMoved(250, 1);
            assert(win.isSidebarCollapsed() == false);
            assert(win.sidebarSavedWidth() == 250);
            assert(dragOpenSpy.count() == 1 && dragOpenSpy.takeFirst().at(0).toBool() == false);

            win.toggleSidebar();
            assert(win.isSidebarCollapsed() == true);
            win.toggleSidebar();
            assert(win.isSidebarCollapsed() == false);
            assert(std::abs(splitter->sizes().at(0) - 250) <= 1);
            assert(splitter->widget(0)->minimumWidth() >= 200 || win.isSidebarCollapsed());
        });

        it("re-derives collapse status across serialized splitter state roundtrips", [&]() {
            int savedWidth = win.sidebarSavedWidth();
            assert(savedWidth >= 200);
            win.setSidebarCollapsed(true);
            assert(win.isSidebarCollapsed() == true);

            QByteArray state = splitter->saveState();
            splitter->restoreState(state);
            win.restoreSplitterState(state);
            assert(win.isSidebarCollapsed() == (splitter->sizes().at(0) == 0));

            win.setSidebarSavedWidth(savedWidth);
            win.toggleSidebar();
            assert(std::abs(splitter->sizes().at(0) - savedWidth) <= 1);
        });
    });

    describe("CommonMark heading extraction and AST normalization", [&]() {
        it("parses Setext, ATX, and code-fence-shielded headings with slug disambiguation", [&]() {
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
            assert(headings[2].level == 2 && headings[2].title == "Overview" && headings[2].id == "overview");
            assert(headings[3].level == 2 && headings[3].title == "Overview-1" && headings[3].id == "overview-1");
            assert(headings[4].level == 2 && headings[4].title == "Overview" && headings[4].id == "overview-2");
            assert(headings[5].level == 2 && headings[5].title == "Blockquote Heading" && headings[5].id == "blockquote-heading");
            assert(headings[6].level == 2 && headings[6].title == "Energy E=mc^2" && headings[6].id == "energy-emc2");
            assert(headings[7].level == 2 && headings[7].title == "A b" && headings[7].id == "a-b");
            assert(headings[8].level == 2 && headings[8].title == "Header with logo" && headings[8].id == "header-with-logo");
            assert(headings[9].level == 3 && headings[9].title == "1. அறம் & Code" && headings[9].id == "1-code");
        });
    });

    describe("TOC widget interaction and scroll-spy isolation", [&]() {
        it("isolates scroll-spy programmatic activations from firing outbound jump signals", [&]() {
            auto* tocWidget = win.findChild<QWidget*>("tocSidebar");
            assert(tocWidget != nullptr);
            QSignalSpy jumpSpy(tocWidget, SIGNAL(headingActivated(QString)));
            QMetaObject::invokeMethod(tocWidget, "setActiveHeading", Q_ARG(QString, "whats-new-in-markd-v10"));
            assert(jumpSpy.count() == 0);

            QMetaObject::invokeMethod(tocWidget, "setActiveHeading", Q_ARG(QString, "unknown-heading-id-999"));
            assert(jumpSpy.count() == 0);
        });

        it("displays empty document placeholder label when document contains zero headings", [&]() {
            QTemporaryFile tempEmptyFile(QDir::tempPath() + "/markd_empty_XXXXXX.md");
            tempEmptyFile.setAutoRemove(true);
            assert(tempEmptyFile.open());
            tempEmptyFile.close();
            win.loadFile(tempEmptyFile.fileName());
            app.processEvents();
            auto* placeholder = win.findChild<QLabel*>("tocPlaceholderLabel");
            assert(placeholder != nullptr && placeholder->isVisible());
            assert(placeholder->text().contains("No headings", Qt::CaseInsensitive));
        });
    });
}

inline void testTocSidebarEdgeCases(QApplication& app) {
    using Zinsser::describe;
    using Zinsser::it;

    MainWindow win;
    win.resize(1000, 650);
    win.show();
    win.activateWindow();
    app.processEvents();

    describe("CommonMark parser edge cases and heading boundaries", [&]() {
        it("handles empty buffers and crlf line endings", [&]() {
            auto emptyHeadings = TocParser::extractHeadings("");
            assert(emptyHeadings.empty());

            QString crlfDoc = "# Title\r\n\r\nParagraph\r\n\r\n## Section\r\n";
            auto crlfHeadings = TocParser::extractHeadings(crlfDoc);
            assert(crlfHeadings.size() == 2);
            assert(crlfHeadings[0].level == 1 && crlfHeadings[0].title == "Title" && crlfHeadings[0].lineNumber == 1);
            assert(crlfHeadings[1].level == 2 && crlfHeadings[1].title == "Section" && crlfHeadings[1].lineNumber == 5);
        });

        it("preserves snake_case identifiers and strips inline formatting delimiters", [&]() {
            QString snakeDoc = "## snake_case_identifier_test\n\n### CamelCase_Test_123";
            auto snakeHeadings = TocParser::extractHeadings(snakeDoc);
            assert(snakeHeadings.size() == 2);
            assert(snakeHeadings[0].title == "snake_case_identifier_test");
            assert(snakeHeadings[0].id == "snake_case_identifier_test");
            assert(snakeHeadings[1].title == "CamelCase_Test_123");
            assert(snakeHeadings[1].id == "camelcase_test_123");

            QString formatDoc = "## **Bold** *Italic* ~~Strike~~ `code`";
            auto formatHeadings = TocParser::extractHeadings(formatDoc);
            assert(formatHeadings.size() == 1);
            assert(formatHeadings[0].title == "Bold Italic Strike code");
            assert(formatHeadings[0].id == "bold-italic-strike-code");
        });

        it("discards unclosed comments and rejects headings deeper than six hashes", [&]() {
            QString unclosedComment = "<!--\n# Commented Heading\nStill in comment";
            auto unclosedHeadings = TocParser::extractHeadings(unclosedComment);
            assert(unclosedHeadings.empty());

            QString deepDoc = "####### Too Deep\n###### Six Hashes Ok";
            auto deepHeadings = TocParser::extractHeadings(deepDoc);
            assert(deepHeadings.size() == 1);
            assert(deepHeadings[0].level == 6 && deepHeadings[0].title == "Six Hashes Ok");
        });
    });

    describe("TOC widget selection bounds and cursor navigation", [&]() {
        it("persists active heading on unknown identifier requests", [&]() {
            TocWidget tocWidget;
            std::vector<TocItem> items = {{1, 1, "Section 1", "section-1"}, {2, 5, "Section 2", "section-2"}};
            tocWidget.setHeadings(items);
            assert(tocWidget.headingCount() == 2);
            tocWidget.setActiveHeading("");
            assert(tocWidget.activeHeadingId().isEmpty());
            tocWidget.setActiveHeading("section-2");
            assert(tocWidget.activeHeadingId() == "section-2");
            tocWidget.setActiveHeading("non-existent-id");
            assert(tocWidget.activeHeadingId() == "section-2");
        });

        it("clamps saved sidebar width to minimum two hundred pixels", [&]() {
            win.setSidebarSavedWidth(50);
            assert(win.sidebarSavedWidth() == 200);
        });

        it("suppresses redundant collapse signal emissions", [&]() {
            win.setSidebarCollapsed(true);
            QSignalSpy dupSpy(&win, &MainWindow::sidebarToggled);
            win.setSidebarCollapsed(true);
            assert(dupSpy.count() == 0);
            win.setSidebarCollapsed(false);
            assert(dupSpy.count() == 1 && dupSpy.takeFirst().at(0).toBool() == false);
            win.setSidebarCollapsed(false);
            assert(dupSpy.count() == 0);
        });

        it("navigates editor cursor to target line on heading activation", [&]() {
            win.modeSwitcher()->setMode(ModeSwitcher::DocumentMode::Edit);
            win.editor()->setPlainText("# First Line\n\nLine 3\nLine 4\n\n## Target Section\n\nLine 8");
            win.updateToc();
            assert(win.tocWidget()->headingCount() == 2);
            emit win.tocWidget()->headingActivated("target-section");
            app.processEvents();
            assert(win.editor()->textCursor().blockNumber() == 5);
        });
    });
}

inline void testTocSidebar(QApplication& app) {
    testTocSidebarIntegration(app);
    testTocSidebarEdgeCases(app);
}

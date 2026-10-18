#pragma once

#include <QApplication>
#include <QTest>
#include <QTextBlock>
#include <QScrollBar>
#include <QImage>
#include <QPainter>
#include <cassert>
#include "Zinsser.h"
#include "ui/MainWindow.h"
#include "ui/EditorView.h"
#include "ui/TocWidget.h"
#include "ui/TocHeaderWidget.h"
#include "ui/FindBarOverlay.h"
#include "ui/ModeSwitcher.h"

inline void testEditorViewAndThemeAcuity(QApplication& app) {
    using Zinsser::describe;
    using Zinsser::it;

    describe("EditorView monospace gutter and active line highlight acuity", [&]() {
        it("calculates initial gutter width for single digit documents", [&]() {
            EditorView editor;
            editor.setPlainText(QStringLiteral("Single line document"));
            app.processEvents();

            const int expectedDigits = 1;
            const int expectedWidth = expectedDigits * editor.fontMetrics().horizontalAdvance('9') + 24;
            assert(editor.lineNumberAreaWidth() == expectedWidth);
            assert(editor.viewportMargins().left() == expectedWidth);
            assert(editor.lineNumberArea()->width() == expectedWidth);
        });

        it("expands gutter width dynamically when block count crosses magnitude thresholds", [&]() {
            EditorView editor;
            QString text;
            for (int i = 1; i <= 10; ++i) {
                text += QStringLiteral("Line %1\n").arg(i);
            }
            editor.setPlainText(text);
            app.processEvents();

            const int twoDigitWidth = 2 * editor.fontMetrics().horizontalAdvance('9') + 24;
            assert(editor.lineNumberAreaWidth() == twoDigitWidth);

            for (int i = 11; i <= 100; ++i) {
                text += QStringLiteral("Line %1\n").arg(i);
            }
            editor.setPlainText(text);
            app.processEvents();

            const int threeDigitWidth = 3 * editor.fontMetrics().horizontalAdvance('9') + 24;
            assert(editor.lineNumberAreaWidth() == threeDigitWidth);

            for (int i = 101; i <= 1000; ++i) {
                text += QStringLiteral("Line %1\n").arg(i);
            }
            editor.setPlainText(text);
            app.processEvents();

            const int fourDigitWidth = 4 * editor.fontMetrics().horizontalAdvance('9') + 24;
            assert(editor.lineNumberAreaWidth() == fourDigitWidth);
        });

        it("contracts gutter width synchronously when deleting lines across magnitude boundaries", [&]() {
            EditorView editor;
            QString text;
            for (int i = 1; i <= 1000; ++i) {
                text += QStringLiteral("Line %1\n").arg(i);
            }
            editor.setPlainText(text);
            app.processEvents();
            assert(editor.lineNumberAreaWidth() == 4 * editor.fontMetrics().horizontalAdvance('9') + 24);

            QString fiftyLines;
            for (int i = 1; i <= 50; ++i) {
                fiftyLines += QStringLiteral("Line %1\n").arg(i);
            }
            editor.setPlainText(fiftyLines);
            app.processEvents();
            assert(editor.lineNumberAreaWidth() == 2 * editor.fontMetrics().horizontalAdvance('9') + 24);

            editor.setPlainText(QStringLiteral("One\nTwo\nThree\n"));
            app.processEvents();
            assert(editor.lineNumberAreaWidth() == 1 * editor.fontMetrics().horizontalAdvance('9') + 24);
        });

        it("enforces monospace typography and document margins", [&]() {
            EditorView editor;
            assert(editor.font().fixedPitch() == true || editor.font().styleHint() == QFont::Monospace);
            assert(editor.font().pointSize() == 13);
            assert(editor.document()->documentMargin() == 16);
            assert(editor.palette().color(QPalette::Base) == QColor("#050505"));
            assert(editor.palette().color(QPalette::Text) == QColor("#f0f6fc"));
            assert(editor.isLineHighlightEnabled() == true);
            assert(editor.lineNumberArea() != nullptr);
            assert(editor.lineNumberArea()->sizeHint().width() > 0);
            assert(editor.viewportMargins().left() > 0);

            QFont newFont(QStringLiteral("Courier"));
            newFont.setPointSize(16);
            editor.setFont(newFont);
            app.processEvents();
            assert(editor.font().pointSize() == 16);
        });

        it("applies full-width horizontal highlight band across cursor block", [&]() {
            EditorView editor;
            editor.setPlainText(QStringLiteral("Alpha\nBeta\nGamma"));
            editor.setReadOnly(false);
            app.processEvents();

            editor.highlightCurrentLine();
            const auto selections = editor.allExtraSelections();
            assert(!selections.isEmpty());

            bool foundBand = false;
            for (const auto& sel : selections) {
                if (sel.format.property(QTextFormat::FullWidthSelection).toBool() &&
                    sel.format.background().color() == QColor("#161b22")) {
                    foundBand = true;
                    assert(sel.cursor.blockNumber() == editor.textCursor().blockNumber());
                }
            }
            assert(foundBand == true);
        });

        it("preserves find bar search extra selections alongside active line highlight", [&]() {
            EditorView editor;
            editor.setPlainText(QStringLiteral("Alpha search match\nBeta search match\nGamma plain line"));
            app.processEvents();

            QList<QTextEdit::ExtraSelection> searchSels;
            QTextEdit::ExtraSelection sel1;
            sel1.format.setBackground(QColor("#d2992233"));
            searchSels.append(sel1);
            QTextEdit::ExtraSelection sel2;
            sel2.format.setBackground(QColor("#388bfd66"));
            searchSels.append(sel2);

            editor.setSearchSelections(searchSels);
            assert(editor.searchSelections().size() == 2);
            assert(editor.extraSelections().size() == 2);
            assert(editor.allExtraSelections().size() == 3);

            editor.setExtraSelections(searchSels);
            assert(editor.extraSelections().size() == 2);

            QTextCursor cur = editor.textCursor();
            cur.movePosition(QTextCursor::Down);
            editor.setTextCursor(cur);
            app.processEvents();

            assert(editor.searchSelections().size() == 2);
            assert(editor.extraSelections().size() == 2);
            assert(editor.allExtraSelections().size() == 3);

            editor.clearSearchSelections();
            assert(editor.searchSelections().isEmpty());
            assert(editor.extraSelections().isEmpty());
            assert(editor.allExtraSelections().size() == 1);
        });
    });

    describe("Table of Contents header row and section collapse persistence", [&]() {
        it("mounts table of contents header row with list icon and disclosure caret", [&]() {
            MainWindow win;
            win.resize(900, 600);
            win.show();
            app.processEvents();

            auto* header = win.findChild<QWidget*>("tocHeaderWidget");
            assert(header != nullptr);
            assert(header->height() == 28);

            auto* icon = win.findChild<QLabel*>("tocHeaderIconLabel");
            assert(icon != nullptr);
            assert(icon->text() == QString::fromUtf8("≡"));

            auto* caret = win.findChild<QLabel*>("tocHeaderCaretLabel");
            assert(caret != nullptr);
            assert(caret->text() == QString::fromUtf8("⌵"));

            auto* title = win.findChild<QLabel*>("tocHeaderTitleLabel");
            assert(title != nullptr);
            assert(title->text() == "Table of Contents");
        });

        it("traps mouse press events on header row to isolate window dragging", [&]() {
            TocWidget tocWidget;
            auto* header = tocWidget.headerWidget();
            assert(header != nullptr);

            QMouseEvent pressEv(QEvent::MouseButtonPress, QPointF(10, 10), QPointF(10, 10), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(header, &pressEv);
            assert(pressEv.isAccepted());
        });

        it("toggles outline list visibility and alternates disclosure caret on header click", [&]() {
            TocWidget tocWidget;
            tocWidget.show();
            app.processEvents();
            tocWidget.setHeadings({{1, 1, "Section 1", "section-1"}});
            assert(tocWidget.view()->isVisible() == true);
            assert(tocWidget.isSectionCollapsed() == false);

            auto* header = tocWidget.headerWidget();
            QTest::mouseClick(header, Qt::LeftButton);
            assert(tocWidget.isSectionCollapsed() == true);
            assert(tocWidget.view()->isVisible() == false);
            assert(header->caretLabel()->text() == QString::fromUtf8("›"));

            QTest::mouseClick(header, Qt::LeftButton);
            assert(tocWidget.isSectionCollapsed() == false);
            assert(tocWidget.view()->isVisible() == true);
            assert(header->caretLabel()->text() == QString::fromUtf8("⌵"));
        });

        it("toggles outline collapse via keyboard Return and Space accelerators", [&]() {
            TocWidget tocWidget;
            tocWidget.show();
            app.processEvents();
            tocWidget.setHeadings({{1, 1, "Section 1", "section-1"}});
            auto* header = tocWidget.headerWidget();

            QTest::keyClick(header, Qt::Key_Return);
            assert(tocWidget.isSectionCollapsed() == true);
            assert(tocWidget.view()->isVisible() == false);

            QTest::keyClick(header, Qt::Key_Space);
            assert(tocWidget.isSectionCollapsed() == false);
            assert(tocWidget.view()->isVisible() == true);

            assert(header->iconLabel() != nullptr);
            assert(header->titleLabel() != nullptr);
            assert(header->caretLabel() != nullptr);
            QTest::keyClick(header, Qt::Key_Tab);
            assert(tocWidget.isSectionCollapsed() == false);
        });

        it("preserves collapsed state across heading updates and collapses placeholder when empty", [&]() {
            TocWidget tocWidget;
            tocWidget.show();
            app.processEvents();
            tocWidget.setHeadings({{1, 1, "Initial", "initial"}});
            tocWidget.setSectionCollapsed(true);
            assert(tocWidget.isSectionCollapsed() == true);

            tocWidget.setHeadings({{1, 1, "Updated 1", "updated-1"}, {2, 3, "Updated 2", "updated-2"}});
            assert(tocWidget.isSectionCollapsed() == true);
            assert(tocWidget.view()->isVisible() == false);
            assert(tocWidget.placeholderLabel()->isVisible() == false);

            tocWidget.setHeadings({});
            assert(tocWidget.isSectionCollapsed() == true);
            assert(tocWidget.view()->isVisible() == false);
            assert(tocWidget.placeholderLabel()->isVisible() == false);

            tocWidget.setSectionCollapsed(false);
            assert(tocWidget.isSectionCollapsed() == false);
            assert(tocWidget.view()->isVisible() == false);
            assert(tocWidget.placeholderLabel()->isVisible() == true);
        });
    });

    describe("Headless pixel sampling coordinates and offscreen rendering", [&]() {
        it("samples gutter background and divider hairline pixels offscreen", [&]() {
            EditorView editor;
            editor.setPlainText(QStringLiteral("Line One\nLine Two\nLine Three\nLine Four\nLine Five\n"));
            editor.resize(400, 300);
            editor.show();
            app.processEvents();

            QImage gutterImg(editor.lineNumberArea()->size(), QImage::Format_ARGB32_Premultiplied);
            gutterImg.fill(Qt::transparent);
            QPainter p(&gutterImg);
            editor.lineNumberArea()->render(&p);
            p.end();

            const QColor bgPixel = gutterImg.pixelColor(2, 50);
            assert(bgPixel == QColor("#0d1117"));

            const int hairlineX = editor.lineNumberArea()->width() - 1;
            const QColor hairlinePixel = gutterImg.pixelColor(hairlineX, 50);
            assert(hairlinePixel == QColor("#1e242c"));
        });

        it("samples active line luminance brightening against inactive line numbers", [&]() {
            EditorView editor;
            editor.setPlainText(QStringLiteral("Active block\nInactive block two\nInactive block three\n"));
            editor.resize(400, 300);
            editor.show();
            app.processEvents();

            QTextCursor cur = editor.textCursor();
            cur.movePosition(QTextCursor::Start);
            editor.setTextCursor(cur);
            app.processEvents();

            QImage gutterImg(editor.lineNumberArea()->size(), QImage::Format_ARGB32_Premultiplied);
            gutterImg.fill(Qt::transparent);
            QPainter p(&gutterImg);
            editor.lineNumberArea()->render(&p);
            p.end();

            QTextBlock b1 = editor.document()->findBlockByNumber(0);
            QTextBlock b2 = editor.document()->findBlockByNumber(1);
            QRectF r1 = editor.blockBoundingGeometry(b1).translated(editor.contentOffset());
            QRectF r2 = editor.blockBoundingGeometry(b2).translated(editor.contentOffset());
            int y1 = static_cast<int>(r1.top() + r1.height() / 2);
            int y2 = static_cast<int>(r2.top() + r2.height() / 2);

            int maxLightnessLine1 = 0;
            int maxLightnessLine2 = 0;
            const int scanStart = std::max(0, editor.lineNumberArea()->width() - 24);
            const int scanEnd = editor.lineNumberArea()->width() - 8;

            for (int x = scanStart; x < scanEnd; ++x) {
                maxLightnessLine1 = std::max(maxLightnessLine1, gutterImg.pixelColor(x, y1).lightness());
                maxLightnessLine2 = std::max(maxLightnessLine2, gutterImg.pixelColor(x, y2).lightness());
            }

            assert(maxLightnessLine1 > maxLightnessLine2);
            assert(maxLightnessLine1 >= 200);
        });

        it("samples active line highlight band against inactive canvas background", [&]() {
            EditorView editor;
            editor.setPlainText(QStringLiteral("First line active\nSecond line inactive\nThird line inactive\n"));
            editor.resize(400, 300);
            editor.show();
            app.processEvents();

            QImage vpImg(editor.viewport()->size(), QImage::Format_ARGB32_Premultiplied);
            vpImg.fill(Qt::transparent);
            QPainter p(&vpImg);
            editor.viewport()->render(&p);
            p.end();

            QTextBlock b1 = editor.document()->findBlockByNumber(0);
            QTextBlock b2 = editor.document()->findBlockByNumber(1);
            QRectF r1 = editor.blockBoundingGeometry(b1).translated(editor.contentOffset());
            QRectF r2 = editor.blockBoundingGeometry(b2).translated(editor.contentOffset());
            int y1 = static_cast<int>(r1.top() + r1.height() / 2);
            int y2 = static_cast<int>(r2.top() + r2.height() / 2);

            const QColor activeColor = vpImg.pixelColor(80, y1);
            const QColor inactiveColor = vpImg.pixelColor(80, y2);

            assert(activeColor == QColor("#161b22") || activeColor.lightness() > inactiveColor.lightness());
            assert(inactiveColor == QColor("#050505") || inactiveColor.lightness() < activeColor.lightness());
        });

        it("preserves vertical scroll position across mode toggle cycles with custom margins", [&]() {
            MainWindow win;
            win.resize(900, 600);
            win.show();
            app.processEvents();

            QString longDoc = QStringLiteral("# Long Document Header\n\n");
            for (int i = 0; i < 80; ++i) {
                longDoc += QStringLiteral("Paragraph line %1 for vertical scroll test.\n\n").arg(i);
            }
            win.editor()->setPlainText(longDoc);
            app.processEvents();

            QTextCursor cur = win.editor()->textCursor();
            cur.movePosition(QTextCursor::End);
            win.editor()->setTextCursor(cur);
            win.editor()->verticalScrollBar()->setValue(100);
            const int savedScroll = win.editor()->verticalScrollBar()->value();
            assert(savedScroll > 0);

            win.setMode(MainWindow::Mode::Preview);
            app.processEvents();

            win.setMode(MainWindow::Mode::Edit);
            app.processEvents();

            assert(win.editor()->verticalScrollBar()->value() > 0);
        });
    });
}

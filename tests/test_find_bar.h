#pragma once

#include <QApplication>
#include <QAction>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QScrollBar>
#include <QSignalSpy>
#include <QTemporaryFile>
#include <QDir>
#include <QTest>
#include <cassert>
#include "Zinsser.h"
#include "ui/MainWindow.h"
#include "ui/FindBarOverlay.h"
#include "ui/HeaderBar.h"
#include "ui/StatusBarTelemetry.h"
#include "ui/PreviewView.h"
#include "ui/ModeSwitcher.h"

inline void testInPageFindBarOverlay(QApplication& app) {
    using Zinsser::describe;
    using Zinsser::it;

    describe("In-page FindBarOverlay floating capsule geometry, hit-testing, and dual-mode search", [&]() {
        it("mounts hidden find bar overlay with isolated hit testing and relational top right docking", [&]() {
            MainWindow win;
            win.resize(900, 650);
            win.show();
            win.activateWindow();
            app.processEvents();

            FindBarOverlay* overlay = win.findBarOverlay();
            assert(overlay != nullptr);
            assert(!overlay->isVisible());
            assert(overlay->testAttribute(Qt::WA_StyledBackground));
            assert(overlay->width() == 320);
            assert(overlay->height() == 34);
            assert(overlay->x() == win.width() - 336);
            assert(overlay->y() == 56);
            assert(overlay->y() >= win.headerBar()->geometry().bottom());
            assert(overlay->geometry().bottom() < win.statusBar()->y());

            assert(!overlay->findInput()->accessibleName().isEmpty());
            assert(!overlay->matchCountLabel()->accessibleName().isEmpty());
            assert(!overlay->prevButton()->accessibleName().isEmpty());
            assert(!overlay->nextButton()->accessibleName().isEmpty());
            assert(!overlay->closeButton()->accessibleName().isEmpty());

            win.resize(1200, 800);
            app.processEvents();
            assert(overlay->x() == 1200 - 336);
            assert(overlay->y() == 56);

            const QPoint center = overlay->rect().center();
            QMouseEvent pressEv(QEvent::MouseButtonPress, center, overlay->mapToGlobal(center), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(overlay, &pressEv);
            assert(pressEv.isAccepted());

            QMouseEvent releaseEv(QEvent::MouseButtonRelease, center, overlay->mapToGlobal(center), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(overlay, &releaseEv);
            assert(releaseEv.isAccepted());

            QMouseEvent dblClickEv(QEvent::MouseButtonDblClick, center, overlay->mapToGlobal(center), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(overlay, &dblClickEv);
            assert(dblClickEv.isAccepted());

            QKeyEvent passThroughKey(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, "a");
            QApplication::sendEvent(overlay, &passThroughKey);
        });

        it("summons find bar on shortcut and updates zero bracket match telemetry across modes", [&]() {
            MainWindow win;
            win.resize(900, 650);
            win.show();
            win.activateWindow();
            app.processEvents();

            win.editor()->setPlainText(QString::fromUtf8("alpha beta alpha gamma alpha அறம் delta அறம்"));
            QTextCursor startCur = win.editor()->textCursor();
            startCur.setPosition(6);
            startCur.setPosition(10, QTextCursor::KeepAnchor);
            win.editor()->setTextCursor(startCur);

            QAction* findAct = win.findChild<QAction*>("action_find");
            assert(findAct != nullptr);
            findAct->trigger();
            app.processEvents();

            FindBarOverlay* overlay = win.findBarOverlay();
            assert(overlay->isVisible());
            assert(overlay->findInput()->hasFocus());
            assert(overlay->query() == "beta");
            assert(overlay->matchCountText() == "1 of 1");

            overlay->clearSearch();
            app.processEvents();
            assert(overlay->query().isEmpty());
            assert(overlay->matchCountText().isEmpty());

            startCur.setPosition(0);
            win.editor()->setTextCursor(startCur);

            overlay->findInput()->setText("a");
            app.processEvents();
            assert(overlay->activeMatch() == 1);

            overlay->findInput()->setText("alpha");
            app.processEvents();
            assert(overlay->activeMatch() == 1);
            assert(overlay->totalMatches() == 3);
            assert(overlay->matchCountText() == "1 of 3");
            assert(!overlay->matchCountText().contains('['));
            assert(!overlay->matchCountText().contains(']'));
            assert(win.editor()->extraSelections().size() == 3);
            assert(overlay->prevButton()->isEnabled());
            assert(overlay->nextButton()->isEnabled());

            findAct->trigger();
            app.processEvents();
            assert(overlay->activeMatch() == 1);

            overlay->findInput()->setText(QString::fromUtf8("அறம்"));
            app.processEvents();
            assert(overlay->activeMatch() == 1);
            assert(overlay->totalMatches() == 2);
            assert(overlay->matchCountText() == "1 of 2");

            const int preservedPos = win.editor()->textCursor().selectionStart();
            overlay->findInput()->setText("nonexistent_token");
            app.processEvents();
            assert(overlay->totalMatches() == 0);
            assert(overlay->activeMatch() == 0);
            assert(overlay->matchCountText() == "0 matches");
            assert(!overlay->prevButton()->isEnabled());
            assert(!overlay->nextButton()->isEnabled());
            assert(win.editor()->extraSelections().isEmpty());
            assert(win.editor()->textCursor().position() == preservedPos);
        });

        it("navigates next and previous matches with wrap-around and restores active viewport focus on dismissal", [&]() {
            MainWindow win;
            win.resize(900, 650);
            win.show();
            win.activateWindow();
            app.processEvents();

            win.editor()->setPlainText("alpha one alpha two alpha three");
            QTextCursor cur = win.editor()->textCursor();
            cur.setPosition(0);
            win.editor()->setTextCursor(cur);

            QAction* findAct = win.findChild<QAction*>("action_find");
            findAct->trigger();
            FindBarOverlay* overlay = win.findBarOverlay();
            overlay->findInput()->setText("alpha");
            app.processEvents();
            assert(overlay->matchCountText() == "1 of 3");

            QTest::keyClick(overlay->findInput(), Qt::Key_Return);
            app.processEvents();
            assert(overlay->matchCountText() == "2 of 3");

            QTest::mouseClick(overlay->nextButton(), Qt::LeftButton);
            app.processEvents();
            assert(overlay->matchCountText() == "3 of 3");

            QTest::keyClick(overlay->findInput(), Qt::Key_Return);
            app.processEvents();
            assert(overlay->matchCountText() == "1 of 3");

            QTest::keyClick(overlay->findInput(), Qt::Key_Return, Qt::ShiftModifier);
            app.processEvents();
            assert(overlay->matchCountText() == "3 of 3");

            QTest::mouseClick(overlay->prevButton(), Qt::LeftButton);
            app.processEvents();
            assert(overlay->matchCountText() == "2 of 3");

            QTest::keyClick(overlay->findInput(), Qt::Key_Return, Qt::ShiftModifier);
            app.processEvents();
            assert(overlay->matchCountText() == "1 of 3");

            QTest::keyClick(overlay->findInput(), Qt::Key_Escape);
            app.processEvents();
            assert(!overlay->isVisible());
            assert(win.editor()->extraSelections().isEmpty());
            assert(!win.editor()->textCursor().hasSelection());
            assert(win.editor()->hasFocus());

            findAct->trigger();
            app.processEvents();
            assert(overlay->isVisible());
            QKeyEvent winEsc(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
            QApplication::sendEvent(&win, &winEsc);
            assert(!overlay->isVisible());

            win.setMode(MainWindow::Mode::Preview);
            app.processEvents();
            findAct->trigger();
            app.processEvents();
            assert(overlay->isVisible());

            QTest::mouseClick(overlay->closeButton(), Qt::LeftButton);
            app.processEvents();
            assert(!overlay->isVisible());
            assert(win.previewView()->webView()->hasFocus());
        });

        it("synchronizes active search across mode switches and external disk reloads without losing scroll position", [&]() {
            MainWindow win;
            win.resize(900, 650);
            win.show();
            win.activateWindow();
            app.processEvents();

            QTemporaryFile temp(QDir::tempPath() + "/markd_find_reload_XXXXXX.md");
            temp.setAutoRemove(true);
            assert(temp.open());
            QString doc = "# Find Reload Heading\n\nalpha match one\n\n";
            for (int i = 0; i < 60; ++i) {
                doc += QString("Filler paragraph line %1 for vertical scroll testing.\n\n").arg(i);
            }
            doc += "alpha match two\n\nalpha match three\n";
            temp.write(doc.toUtf8());
            temp.close();

            assert(win.loadFile(temp.fileName()));
            app.processEvents();

            QAction* findAct = win.findChild<QAction*>("action_find");
            findAct->trigger();
            FindBarOverlay* overlay = win.findBarOverlay();
            overlay->findInput()->setText("alpha");
            app.processEvents();
            assert(overlay->matchCountText() == "1 of 3");

            win.editor()->verticalScrollBar()->setValue(80);
            const int targetScroll = win.editor()->verticalScrollBar()->value();
            assert(targetScroll > 0);

            {
                QFile diskFile(temp.fileName());
                assert(diskFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
                QString updatedDoc = doc + "\nalpha match four\n";
                diskFile.write(updatedDoc.toUtf8());
                diskFile.close();
            }

            QMetaObject::invokeMethod(win.session(), "onWatcherTimeout");
            app.processEvents();
            assert(overlay->matchCountText() == "1 of 4");
            assert(win.editor()->verticalScrollBar()->value() == targetScroll);

            win.setMode(MainWindow::Mode::Preview);
            app.processEvents();
            assert(overlay->isVisible());
            assert(win.editor()->extraSelections().isEmpty());

            win.setMode(MainWindow::Mode::Edit);
            app.processEvents();
            assert(overlay->isVisible());
            assert(overlay->matchCountText() == "1 of 4");
            assert(win.editor()->extraSelections().size() == 4);

            win.clearFind();
            QSignalSpy findSpy(&win, &MainWindow::findResult);
            QMetaObject::invokeMethod(&win, "onFindResult", Q_ARG(int, 2), Q_ARG(int, 4));
            assert(findSpy.count() == 0);
        });
    });
}

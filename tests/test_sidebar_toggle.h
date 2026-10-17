#pragma once

#include <QApplication>
#include <cassert>
#include <iostream>
#include <cmath>
#include <QSignalSpy>
#include <QTest>
#include <QMouseEvent>
#include <QPushButton>
#include <QAction>
#include <QSplitter>
#include "Zinsser.h"
#include "ui/platform/PlatformFrame.h"
#include "ui/MainWindow.h"
#include "ui/HeaderBar.h"

inline void testSidebarToggleButton(QApplication& app) {
    using Zinsser::describe;
    using Zinsser::it;

    MainWindow win;
    win.resize(1024, 768);
    win.show();
    app.processEvents();

    auto* header = win.findChild<HeaderBar*>();
    assert(header != nullptr);
    auto* btn = header->findChild<QPushButton*>("sidebarToggleButton");
    assert(btn != nullptr);
    auto* splitter = win.mainSplitter();
    assert(splitter != nullptr);
    auto* toggleAct = win.findChild<QAction*>("action_toggle_sidebar");
    assert(toggleAct != nullptr);

    describe("Top bar sidebar toggle button and global accelerators", [&]() {

        it("paints tactile icon plate with 32x28px bounds", [&]() {
            assert(header->leadingSlot()->layout()->indexOf(btn) != -1);
            assert(btn->isCheckable() == true);
            assert(btn->focusPolicy() == Qt::NoFocus);
            assert(btn->text().isEmpty());
            assert(btn->accessibleName() == "Toggle Table of Contents Outline");

            QPixmap pix(btn->size());
            btn->setChecked(true);
            btn->render(&pix);
            assert(!pix.isNull());

            btn->setChecked(false);
            btn->render(&pix);
            assert(!pix.isNull());

            btn->setEnabled(false);
            btn->render(&pix);
            assert(!pix.isNull());
            btn->setEnabled(true);
            btn->setChecked(true);

            // Relational Containment Invariant: Leading slot must accommodate safe margin + button width without clipping
            const int leftMargin = header->leadingSlot()->contentsMargins().left();
            assert(header->leadingSlot()->width() >= leftMargin + btn->width());
        });

        it("mounts trailing export and find utility actions within 32x28px plate bounds", [&]() {
            auto* exportBtn = header->trailingSlot()->findChild<QPushButton*>("headerExportButton");
            assert(exportBtn != nullptr);
            assert(exportBtn->width() == 32 && exportBtn->height() == 28);
            assert(exportBtn->accessibleName() == "Export Document to HTML");

            auto* findBtn = header->trailingSlot()->findChild<QPushButton*>("headerFindButton");
            assert(findBtn != nullptr);
            assert(findBtn->width() == 32 && findBtn->height() == 28);
            assert(findBtn->accessibleName() == "Find in Document");

            // Relational Containment Invariant: Trailing slot must accommodate right margin + child buttons
            const int rightMargin = header->trailingSlot()->contentsMargins().right();
            assert(header->trailingSlot()->width() >= rightMargin + exportBtn->width() + findBtn->width());
        });

        it("traps mouse press and release events to isolate window dragging", [&]() {
            const QPoint center = btn->rect().center();
            const QPointF centerF(center);
            QMouseEvent pressEvent(QEvent::MouseButtonPress, centerF,
                                   btn->mapToGlobal(center),
                                   Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(btn, &pressEvent);
            assert(pressEvent.isAccepted() == true);

            QMouseEvent releaseEvent(QEvent::MouseButtonRelease, centerF,
                                     btn->mapToGlobal(center),
                                     Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(btn, &releaseEvent);
            assert(releaseEvent.isAccepted() == true);

            QMouseEvent dblClick(QEvent::MouseButtonDblClick, centerF,
                                 btn->mapToGlobal(center),
                                 Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(btn, &dblClick);
            assert(dblClick.isAccepted() == true);

            if (win.isSidebarCollapsed()) {
                win.setSidebarCollapsed(false);
            }
        });

        it("traps mouse double clicks to isolate window titlebar maximization", [&]() {
            const QPoint center = btn->rect().center();
            const QPointF centerF(center);
            QMouseEvent dblClick(QEvent::MouseButtonDblClick, centerF,
                                 btn->mapToGlobal(center),
                                 Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(btn, &dblClick);
            assert(dblClick.isAccepted() == true);
            assert(!win.isMaximized());
        });

        it("balances leading and trailing flanks on cold boot without manual margin calls", [&]() {
            app.processEvents();
            assert(header->leadingSlot()->minimumWidth() == header->trailingSlot()->minimumWidth());
            assert(header->leadingSlot()->width() == header->trailingSlot()->width());
            int centerMid = header->centerSlot()->x() + (header->centerSlot()->width() / 2);
            int windowMid = header->width() / 2;
            assert(std::abs(centerMid - windowMid) <= 1);
        });

        it("preserves mode switcher true centering under odd window widths (1023px)", [&]() {
            win.resize(1023, 768);
            app.processEvents();
            int centerMid = header->centerSlot()->x() + (header->centerSlot()->width() / 2);
            int windowMid = header->width() / 2;
            assert(std::abs(centerMid - windowMid) <= 1);
            win.resize(1024, 768);
            app.processEvents();
        });

        it("toggles sidebar outline collapse on left button click", [&]() {
            QSignalSpy toggleSpy(&win, &MainWindow::sidebarToggled);
            assert(win.isSidebarCollapsed() == false);
            assert(btn->isChecked() == true);

            QTest::mouseClick(btn, Qt::LeftButton);
            assert(win.isSidebarCollapsed() == true);
            assert(btn->isChecked() == false);
            assert(toggleSpy.count() == 1 && toggleSpy.takeFirst().at(0).toBool() == true);
        });

        it("restores sidebar outline expansion on subsequent left button click", [&]() {
            QSignalSpy toggleSpy(&win, &MainWindow::sidebarToggled);
            QTest::mouseClick(btn, Qt::LeftButton);
            assert(win.isSidebarCollapsed() == false);
            assert(btn->isChecked() == true);
            assert(toggleSpy.count() == 1 && toggleSpy.takeFirst().at(0).toBool() == false);
        });

        it("toggles sidebar collapse via global menu shortcut (Cmd+\\ / Ctrl+\\)", [&]() {
            QSignalSpy toggleSpy(&win, &MainWindow::sidebarToggled);
#if defined(Q_OS_MACOS)
            assert(toggleAct->shortcut() == QKeySequence(Qt::META | Qt::Key_Backslash));
#else
            assert(toggleAct->shortcut() == QKeySequence(Qt::CTRL | Qt::Key_Backslash));
#endif
            assert(toggleAct->shortcuts().contains(QKeySequence(Qt::Key_F9)));

            toggleAct->trigger();
            assert(win.isSidebarCollapsed() == true);
            assert(btn->isChecked() == false);
            assert(toggleSpy.count() == 1 && toggleSpy.takeFirst().at(0).toBool() == true);

            toggleAct->trigger();
            assert(win.isSidebarCollapsed() == false);
            assert(btn->isChecked() == true);
            assert(toggleSpy.count() == 1 && toggleSpy.takeFirst().at(0).toBool() == false);
        });

        it("synchronizes button check state on interactive splitter drag-to-zero", [&]() {
            QSignalSpy toggleSpy(&win, &MainWindow::sidebarToggled);

            emit splitter->splitterMoved(0, 1);
            assert(win.isSidebarCollapsed() == true);
            assert(btn->isChecked() == false);
            assert(toggleSpy.count() == 1 && toggleSpy.takeFirst().at(0).toBool() == true);

            emit splitter->splitterMoved(250, 1);
            assert(win.isSidebarCollapsed() == false);
            assert(btn->isChecked() == true);
            assert(toggleSpy.count() == 1 && toggleSpy.takeFirst().at(0).toBool() == false);
        });

        it("synchronizes button check state when restoring persisted session state", [&]() {
            QSignalSpy toggleSpy(&win, &MainWindow::sidebarToggled);

            win.restoreSplitterState(true, 280);
            assert(win.isSidebarCollapsed() == true);
            assert(btn->isChecked() == false);
            assert(toggleSpy.count() == 1 && toggleSpy.takeFirst().at(0).toBool() == true);

            win.restoreSplitterState(false, 280);
            assert(win.isSidebarCollapsed() == false);
            assert(btn->isChecked() == true);
            assert(toggleSpy.count() == 1 && toggleSpy.takeFirst().at(0).toBool() == false);
        });

        it("preserves active scroll-spy heading across collapse cycles without spurious jumps", [&]() {
            QSignalSpy jumpSpy(win.tocWidget(), &TocWidget::headingActivated);
            toggleAct->trigger();
            toggleAct->trigger();
            assert(jumpSpy.count() == 0);
        });
    });
}

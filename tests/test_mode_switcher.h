#pragma once

#include <QApplication>
#include <QMouseEvent>
#include <QPushButton>
#include <QAction>
#include <cassert>
#include <iostream>
#include <cmath>
#include "Zinsser.h"
#include "ui/MainWindow.h"
#include "ui/HeaderBar.h"
#include "ui/ModeSwitcher.h"

inline void testModeSwitcher(QApplication& app) {
    using Zinsser::describe;
    using Zinsser::it;

    Q_UNUSED(app);

    MainWindow win;
    win.resize(900, 650);

    HeaderBar* header = win.headerBar();
    assert(header != nullptr);
    QWidget* centerSlot = header->centerSlot();
    assert(centerSlot != nullptr);

    ModeSwitcher* switcher = win.modeSwitcher();
    assert(switcher != nullptr);

    describe("Mode switcher structural placement and true centering", [&]() {
        it("mounts segmented control inside header center slot", [&]() {
            assert(switcher->parentWidget() == centerSlot);
            assert(switcher->objectName() == "modeSwitcher");
        });

        it("defaults to edit document mode", [&]() {
            assert(switcher->currentMode() == ModeSwitcher::DocumentMode::Edit);
        });

        it("aligns midpoints to window center within single-pixel tolerance", [&]() {
            header->resize(900, 38);
            header->updateMargins();
            header->layout()->activate();
            centerSlot->layout()->activate();

            int switcherMid = switcher->mapTo(header, QPoint(switcher->width() / 2, 0)).x();
            int windowMid = header->width() / 2;
            assert(std::abs(switcherMid - windowMid) <= 1);
        });
    });

    describe("Mode switcher interaction and state synchronization", [&]() {
        it("isolates mouse click events to transition mode to preview", [&]() {
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
        });

        it("emits modeChanged signal on programmatic toggle", [&]() {
            int signalCount = 0;
            ModeSwitcher::DocumentMode lastEmittedMode = ModeSwitcher::DocumentMode::Preview;
            auto conn = QObject::connect(switcher, &ModeSwitcher::modeChanged, [&](ModeSwitcher::DocumentMode mode) {
                signalCount++;
                lastEmittedMode = mode;
            });

            switcher->toggleMode();
            assert(switcher->currentMode() == ModeSwitcher::DocumentMode::Edit);
            assert(signalCount == 1);
            assert(lastEmittedMode == ModeSwitcher::DocumentMode::Edit);

            QObject::disconnect(conn);
        });

        it("synchronizes document mode with global keyboard action", [&]() {
            int signalCount = 0;
            ModeSwitcher::DocumentMode lastEmittedMode = ModeSwitcher::DocumentMode::Edit;
            auto conn = QObject::connect(switcher, &ModeSwitcher::modeChanged, [&](ModeSwitcher::DocumentMode mode) {
                signalCount++;
                lastEmittedMode = mode;
            });

            QAction* toggleAction = win.findChild<QAction*>("action_toggle_mode");
            assert(toggleAction != nullptr);
            toggleAction->trigger();
            assert(switcher->currentMode() == ModeSwitcher::DocumentMode::Preview);
            assert(signalCount == 1);
            assert(lastEmittedMode == ModeSwitcher::DocumentMode::Preview);

            QObject::disconnect(conn);
        });

        it("maintains optical centering across fullscreen transitions", [&]() {
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
        });

        it("enforces bracketless typography and fixed visual geometry", [&]() {
            QList<QPushButton*> buttons = switcher->findChildren<QPushButton*>();
            assert(buttons.size() == 2);
            for (QPushButton* btn : buttons) {
                assert(!btn->text().contains('['));
                assert(!btn->text().contains(']'));
                assert(btn->text() == "Edit" || btn->text() == "Preview");
            }

            assert(switcher->height() == 26);
            assert(switcher->width() == 140);
            assert(switcher->sizeHint().height() == 26);
            assert(switcher->sizeHint().width() == 140);
        });
    });
}

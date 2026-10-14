#pragma once

#include <QApplication>
#include <QMouseEvent>
#include <QPushButton>
#include <QAction>
#include <cassert>
#include <iostream>
#include <cmath>
#include "ui/MainWindow.h"
#include "ui/HeaderBar.h"
#include "ui/ModeSwitcher.h"

inline void testModeSwitcher(QApplication& app) {
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

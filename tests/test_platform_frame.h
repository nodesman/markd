#pragma once

#include <QApplication>
#include <cassert>
#include <iostream>
#include <cmath>
#include "Zinsser.h"
#include "ui/platform/PlatformFrame.h"
#include "ui/MainWindow.h"
#include "ui/HeaderBar.h"

inline void testPlatformFrame(QApplication& app) {
    using Zinsser::describe;
    using Zinsser::it;

    Q_UNUSED(app);

    describe("Platform frame geometry and safe area contracts", [&]() {
        it("configures 50px header height with 4px top resize buffer", [&]() {
            assert(PlatformFrame::headerHeight() == 50);
        });

        it("adjusts safe area margins between windowed and fullscreen states", [&]() {
            MainWindow win;
            win.resize(900, 650);

#if defined(Q_OS_MACOS)
            assert(PlatformFrame::safeArea(&win).leftMargin == 76);
#endif

            win.setWindowState(Qt::WindowFullScreen);
            assert(PlatformFrame::safeArea(&win).leftMargin == 0);

            win.setWindowState(Qt::WindowNoState);
#if defined(Q_OS_MACOS)
            assert(PlatformFrame::safeArea(&win).leftMargin == 76);
#endif
        });

        it("delineates non-draggable window resize buffers from draggable regions", [&]() {
            // 0 <= y <= 4: resize buffer, non-draggable
            assert(PlatformFrame::isDraggableRegion(QPoint(100, 2)) == false);
            assert(PlatformFrame::isDraggableRegion(QPoint(100, 4)) == false);

            // y > 4 && y <= 50: draggable header region
            assert(PlatformFrame::isDraggableRegion(QPoint(100, 5)) == true);
            assert(PlatformFrame::isDraggableRegion(QPoint(100, 10)) == true);
            assert(PlatformFrame::isDraggableRegion(QPoint(100, 50)) == true);

            // y > 50: below header, non-draggable
            assert(PlatformFrame::isDraggableRegion(QPoint(100, 51)) == false);
            assert(PlatformFrame::isDraggableRegion(QPoint(100, 100)) == false);
        });

        it("preserves true centering of center slot by balancing flanking margins", [&]() {
            MainWindow win;
            win.resize(900, 650);
            HeaderBar* header = win.headerBar();
            assert(header != nullptr);
            assert(header->objectName() == "windowHeaderBar");
            assert(header->height() == 50);

            assert(header->leadingSlot() != nullptr);
            assert(header->centerSlot() != nullptr);
            assert(header->trailingSlot() != nullptr);

            header->resize(900, 50);
            header->updateMargins();

#if defined(Q_OS_MACOS)
            assert(header->leadingSlot()->minimumWidth() >= 76);
            assert(header->trailingSlot()->minimumWidth() >= 76);
            assert(header->leadingSlot()->minimumWidth() == header->trailingSlot()->minimumWidth());

            header->layout()->activate();
            int centerSlotMid = header->centerSlot()->x() + (header->centerSlot()->width() / 2);
            int windowMid = header->width() / 2;
            assert(std::abs(centerSlotMid - windowMid) <= 1);
#endif
        });

        it("neutralizes null window pointers in platform frame and safe area queries", [&]() {
            PlatformSafeArea nullArea = PlatformFrame::safeArea(nullptr);
            assert(nullArea.leftMargin == 0);
            assert(nullArea.rightMargin == 0);

            PlatformFrame::setupWindow(nullptr);

            SidebarToggleButton defaultBtn;
            assert(defaultBtn.parent() == nullptr);
        });

        it("handles mouse press and double click gestures across header bar bounds", [&]() {
            MainWindow win;
            HeaderBar* header = win.headerBar();
            assert(header != nullptr);

            QMouseEvent rightPress(QEvent::MouseButtonPress, QPointF(100, 20), QPointF(100, 20), Qt::RightButton, Qt::RightButton, Qt::NoModifier);
            QApplication::sendEvent(header, &rightPress);

            QMouseEvent topResizePress(QEvent::MouseButtonPress, QPointF(100, 2), QPointF(100, 2), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(header, &topResizePress);

            QMouseEvent dragPress(QEvent::MouseButtonPress, QPointF(100, 20), QPointF(100, 20), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(header, &dragPress);

            QMouseEvent dblClick(QEvent::MouseButtonDblClick, QPointF(100, 20), QPointF(100, 20), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(header, &dblClick);
            QApplication::sendEvent(header, &dblClick);

            // Double-click toggle while maximized switches back to normal
            win.setWindowState(Qt::WindowMaximized);
            QApplication::sendEvent(header, &dblClick);

            // Asymmetric margin balancing exercises both left and right flank boundaries
            header->leadingSlot()->setContentsMargins(150, 0, 0, 0);
            header->trailingSlot()->setContentsMargins(0, 0, 160, 0);
            header->balanceFlanks();
            assert(header->leadingSlot()->minimumWidth() >= 160);
            assert(header->trailingSlot()->minimumWidth() >= 160);
        });
    });
}

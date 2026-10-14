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
        it("enforces thirty-eight pixel uniform header height", [&]() {
            assert(PlatformFrame::headerHeight() == 38);
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

            // y > 4 && y <= 38: draggable header region
            assert(PlatformFrame::isDraggableRegion(QPoint(100, 5)) == true);
            assert(PlatformFrame::isDraggableRegion(QPoint(100, 10)) == true);
            assert(PlatformFrame::isDraggableRegion(QPoint(100, 38)) == true);

            // y > 38: below header, non-draggable
            assert(PlatformFrame::isDraggableRegion(QPoint(100, 39)) == false);
            assert(PlatformFrame::isDraggableRegion(QPoint(100, 100)) == false);
        });

        it("preserves true centering of center slot by balancing flanking margins", [&]() {
            MainWindow win;
            win.resize(900, 650);
            HeaderBar* header = win.headerBar();
            assert(header != nullptr);
            assert(header->objectName() == "windowHeaderBar");
            assert(header->height() == 38);

            assert(header->leadingSlot() != nullptr);
            assert(header->centerSlot() != nullptr);
            assert(header->trailingSlot() != nullptr);

            header->resize(900, 38);
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
    });
}

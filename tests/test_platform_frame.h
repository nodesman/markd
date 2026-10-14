#pragma once

#include <QApplication>
#include <cassert>
#include <iostream>
#include <cmath>
#include "ui/platform/PlatformFrame.h"
#include "ui/MainWindow.h"
#include "ui/HeaderBar.h"

inline void testPlatformFrame(QApplication& app) {
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

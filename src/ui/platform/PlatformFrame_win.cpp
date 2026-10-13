#include "ui/platform/PlatformFrame.h"

#if defined(Q_OS_WIN)
#include <windows.h>
#include <dwmapi.h>

PlatformSafeArea PlatformFrame::safeArea(const QWidget* window)
{
    PlatformSafeArea area;
    if (!window) return area;
    // On Windows, caption buttons are in the top-right
    if (window->isFullScreen() || (window->windowState() & Qt::WindowFullScreen)) {
        area.rightMargin = 0;
    } else {
        area.rightMargin = 140; // Default Windows caption controls buffer
    }
    return area;
}

void PlatformFrame::setupWindow(QWidget* window)
{
    if (!window) return;
    HWND hwnd = reinterpret_cast<HWND>(window->winId());
    if (!hwnd) return;

    // Extend frame into client area via DWM
    MARGINS margins = {0, 0, 0, 0};
    DwmExtendFrameIntoClientArea(hwnd, &margins);
}

bool PlatformFrame::isDraggableRegion(const QPoint& pos)
{
    if (pos.y() <= kTopResizeBuffer || pos.y() > kHeaderHeight) {
        return false;
    }
    return true;
}

#endif

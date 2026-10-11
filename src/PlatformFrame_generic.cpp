#include "PlatformFrame.h"

#if !defined(Q_OS_MACOS) && !defined(Q_OS_WIN)

PlatformSafeArea PlatformFrame::safeArea(const QWidget* window)
{
    Q_UNUSED(window);
    PlatformSafeArea area;
    return area;
}

void PlatformFrame::setupWindow(QWidget* window)
{
    Q_UNUSED(window);
}

bool PlatformFrame::isDraggableRegion(const QPoint& pos)
{
    if (pos.y() <= kTopResizeBuffer || pos.y() > kHeaderHeight) {
        return false;
    }
    return true;
}

#endif

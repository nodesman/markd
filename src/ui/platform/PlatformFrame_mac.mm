#include "ui/platform/PlatformFrame.h"
#include <QGuiApplication>
#include <QWindow>

#if defined(Q_OS_MACOS)
#import <Cocoa/Cocoa.h>

PlatformSafeArea PlatformFrame::safeArea(const QWidget* window)
{
    PlatformSafeArea area;
    if (!window) return area;

    // Check if running on Cocoa QPA. In offscreen mode (e.g. tests),
    // we still provide the deterministic platform safeArea based on window state.
    bool isFullScreen = window->isFullScreen();
    if (window->windowState() & Qt::WindowFullScreen) {
        isFullScreen = true;
    }

    if (isFullScreen) {
        area.leftMargin = 0;
    } else {
        // macOS traffic lights standard clearance
        area.leftMargin = 76;
    }
    return area;
}

void PlatformFrame::setupWindow(QWidget* window)
{
    if (!window) return;

    // Safely check if running under Cocoa QPA to prevent crashes in headless/offscreen modes
    if (QGuiApplication::platformName() != "cocoa") {
        return;
    }

    // Ensure native window handle exists
    window->winId();

    QWindow* winHandle = window->windowHandle();
    if (!winHandle) return;

    NSView* nsView = reinterpret_cast<NSView*>(winHandle->winId());
    if (!nsView) return;

    NSWindow* nsWindow = [nsView window];
    if (!nsWindow) return;

    // Apply native macOS chrome configuration
    nsWindow.styleMask |= NSWindowStyleMaskFullSizeContentView;
    nsWindow.titlebarAppearsTransparent = YES;
    nsWindow.titleVisibility = NSWindowTitleHidden;
#if defined(MAC_OS_X_VERSION_11_0) && MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_11_0
    if (@available(macOS 11.0, *)) {
        nsWindow.titlebarSeparatorStyle = NSTitlebarSeparatorStyleNone;
    }
#endif

    // Strictly reject background drag so Qt controls its own header hit-testing
    nsWindow.movableByWindowBackground = NO;
}

bool PlatformFrame::isDraggableRegion(const QPoint& pos)
{
    // Resizing buffer passes through to OS resize hooks
    if (pos.y() <= kTopResizeBuffer || pos.y() > kHeaderHeight) {
        return false;
    }
    return true;
}

#endif

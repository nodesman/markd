#pragma once

#include <QWidget>
#include <QPoint>

struct PlatformSafeArea {
    int leftMargin = 0;
    int topMargin = 0;
    int rightMargin = 0;
    int bottomMargin = 0;
};

class PlatformFrame {
public:
    static constexpr int kHeaderHeight = 38;
    static constexpr int kTopResizeBuffer = 4;

    static PlatformSafeArea safeArea(const QWidget* window);
    static void setupWindow(QWidget* window);
    static bool isDraggableRegion(const QPoint& pos);
    static int headerHeight() { return kHeaderHeight; }
};

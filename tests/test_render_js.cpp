#include <QApplication>
#include "test_render_js.h"

int main(int argc, char* argv[]) {
    // Run QApplication in offscreen platform mode for headless automated test suite
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    testRenderJsEngine(app);

    return 0;
}

#include "app/MarkdApplication.h"

// Modular Domain Test Suites (Zero Task Numbers)
#include "test_cli_options.h"
#include "test_window_menus.h"
#include "test_theme_engine.h"
#include "test_platform_frame.h"
#include "test_mode_switcher.h"
#include "test_converter_pipeline.h"
#include "test_dual_viewports.h"
#include "test_toc_sidebar.h"
#include "test_sidebar_toggle.h"
#include "test_preview_ergonomics.h"
#include "test_document_session.h"
#include "test_application_lifecycle.h"
#include "test_session_observer.h"
#include "test_chromium_engine.h"
#include "test_web_runtime.h"
#include "test_bridge_protocol.h"

int main(int argc, char* argv[]) {
    // Run MarkdApplication in offscreen platform mode for headless automated test suite
    qputenv("QT_QPA_PLATFORM", "offscreen");
    MarkdApplication app(argc, argv);

    testCommandLineOptions();
    testWindowAndMenuIntegrity();
    testThemeEngine(app);
    testPlatformFrame(app);
    testModeSwitcher(app);
    testConverterPipeline();
    testDualViewports(app);
    testTocSidebar(app);
    testSidebarToggleButton(app);
    testPreviewEditorialErgonomics(app);
    testDocumentSession(app);
    testMarkdApplication(app);
    testMainWindowSessionObserver(app);
    testNativeChromiumModernization(app);
    testWorkspaceModularizationAndEngineHierarchy(app);
    testDomainSemanticActionProtocol(app);

    return 0;
}

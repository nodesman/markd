#pragma once

#include <QApplication>
#include <QStackedWidget>
#include <QPlainTextEdit>
#include <QWebEngineView>
#include <QTabBar>
#include <QAction>
#include <cassert>
#include <iostream>
#include "Zinsser.h"
#include "ui/MainWindow.h"
#include "ui/ModeSwitcher.h"
#include "ui/PreviewView.h"

inline void testDualViewports(QApplication& app) {
    using Zinsser::describe;
    using Zinsser::it;

    MainWindow win;
    win.resize(900, 650);
    win.show();
    win.activateWindow();
    app.processEvents();

    auto* stack = win.findChild<QStackedWidget*>("viewportStack");
    assert(stack != nullptr);
    auto* preview = win.findChild<QWebEngineView*>("previewView");
    assert(preview != nullptr);

    describe("Dual viewport stack containment and architecture", [&]() {
        it("enforces zero-tab stacked widget containment with edit and preview viewports", [&]() {
            assert(!win.centralWidget()->inherits("QTabWidget"));
            assert(win.findChild<QTabBar*>() == nullptr);
            assert(stack->count() == 2);
            assert(win.viewportStack() == stack);
            assert(win.editor() != nullptr);
            assert(win.editor() == stack->widget(0));
            assert(win.previewView() != nullptr);
            assert(win.previewView() == stack->widget(1));
        });

        it("pre-clears preview background to dark palette avoiding white flash", [&]() {
            assert(preview->page()->backgroundColor() == QColor("#050505"));
        });

        it("allocates expanding size policies with three-hundred-twenty pixel minimum width", [&]() {
            assert(stack->sizePolicy().horizontalPolicy() == QSizePolicy::Expanding);
            assert(stack->sizePolicy().verticalPolicy() == QSizePolicy::Expanding);
            assert(stack->minimumWidth() >= 320);
            assert(win.editor()->minimumSize() == QSize(0, 0));
            assert(win.previewView()->minimumSize() == QSize(0, 0));
        });

        it("pre-wires web channel bridge into preview view instance", [&]() {
            assert(win.previewView()->bridge() != nullptr);
            assert(win.previewView()->channel() != nullptr);
            assert(win.previewView()->channel()->registeredObjects().contains("markdBridge"));
        });
    });

    describe("Dual viewport mode switching and buffer permanence", [&]() {
        QString testPayload = "# Sovereign Header\n\nPreserved buffer test with formula: $$x^2 + y^2 = z^2$$";

        it("preserves buffer contents across mode transitions", [&]() {
            win.editor()->setPlainText(testPayload);
            assert(win.documentBuffer() == testPayload);
            assert(win.modeSwitcher()->currentMode() == ModeSwitcher::DocumentMode::Edit);
            assert(stack->currentIndex() == 0);

            win.modeSwitcher()->setMode(ModeSwitcher::DocumentMode::Preview);
            app.processEvents();
            assert(stack->currentIndex() == 1);
            assert(win.documentBuffer() == testPayload);
        });

        it("switches active stack index and restores editor focus on edit toggle", [&]() {
            win.modeSwitcher()->setMode(ModeSwitcher::DocumentMode::Edit);
            app.processEvents();
            assert(stack->currentIndex() == 0);
            assert(win.documentBuffer() == testPayload);
            assert(win.editor()->hasFocus() || win.focusWidget() == win.editor());
        });

        it("synchronizes stack visibility with global toggle action triggers", [&]() {
            QAction* toggleAction = win.findChild<QAction*>("action_toggle_mode");
            assert(toggleAction != nullptr);

            toggleAction->trigger();
            app.processEvents();
            assert(stack->currentIndex() == 1);
            assert(win.documentBuffer() == testPayload);

            toggleAction->trigger();
            app.processEvents();
            assert(stack->currentIndex() == 0);
            assert(win.documentBuffer() == testPayload);
            assert(win.editor()->hasFocus() || win.focusWidget() == win.editor());
        });
    });
}

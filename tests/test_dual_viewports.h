#pragma once

#include <QApplication>
#include <QStackedWidget>
#include <QPlainTextEdit>
#include <QWebEngineView>
#include <QTabBar>
#include <QAction>
#include <cassert>
#include <iostream>
#include "ui/MainWindow.h"
#include "ui/ModeSwitcher.h"
#include "ui/PreviewView.h"

inline void testDualViewports(QApplication& app) {
    MainWindow win;
    win.resize(900, 650);
    win.show();
    win.activateWindow();
    app.processEvents();

    // 1. Structural Containment & Zero-Tabs Assertion
    assert(!win.centralWidget()->inherits("QTabWidget"));
    assert(win.findChild<QTabBar*>() == nullptr);

    auto* stack = win.findChild<QStackedWidget*>("viewportStack");
    assert(stack != nullptr);
    assert(stack->count() == 2);
    assert(win.viewportStack() == stack);

    // 2. Public API Non-Regression
    assert(win.editor() != nullptr);
    assert(win.editor() == stack->widget(0)); // Edit mode at index 0

    // PreviewView at index 1
    assert(win.previewView() != nullptr);
    assert(win.previewView() == stack->widget(1));

    // 3. Buffer Permanence Test
    QString testPayload = "# Sovereign Header\n\nPreserved buffer test with formula: $$x^2 + y^2 = z^2$$";
    win.editor()->setPlainText(testPayload);
    assert(win.documentBuffer() == testPayload);

    // 4. Deterministic Stack Mapping
    assert(win.modeSwitcher()->currentMode() == ModeSwitcher::DocumentMode::Edit);
    assert(stack->currentIndex() == 0);

    // 5. Background Color Pre-Clear (White-Flash Guard)
    auto* preview = win.findChild<QWebEngineView*>("previewView");
    assert(preview != nullptr);
    assert(preview->page()->backgroundColor() == QColor("#050505"));

    // 6. Viewport Dynamic Size Hints
    assert(stack->sizePolicy().horizontalPolicy() == QSizePolicy::Expanding);
    assert(stack->sizePolicy().verticalPolicy() == QSizePolicy::Expanding);
    assert(stack->minimumWidth() >= 320);
    assert(win.editor()->minimumSize() == QSize(0, 0));
    assert(win.previewView()->minimumSize() == QSize(0, 0));

    // 7. IPC Architecture Bridge Pre-wiring
    assert(win.previewView()->bridge() != nullptr);
    assert(win.previewView()->channel() != nullptr);
    assert(win.previewView()->channel()->registeredObjects().contains("markdBridge"));

    // Toggle to Preview
    win.modeSwitcher()->setMode(ModeSwitcher::DocumentMode::Preview);
    app.processEvents();
    assert(stack->currentIndex() == 1);
    assert(win.documentBuffer() == testPayload); // Buffer remains completely intact!

    // Toggle back to Edit & Focus Check
    win.modeSwitcher()->setMode(ModeSwitcher::DocumentMode::Edit);
    app.processEvents();
    assert(stack->currentIndex() == 0);
    assert(win.documentBuffer() == testPayload);
    assert(win.editor()->hasFocus() || win.focusWidget() == win.editor());

    // Keyboard shortcut action toggle
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

    std::cout << "PASS: Dual viewports tests passed!\n";
}

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
#include "core/DocumentSession.h"
#include "FakeWebEngineBackend.h"
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryFile>
#include <QTest>
#include <QDir>

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

    describe("Edit-mode typing with a booted preview", [&]() {
        it("sends no render to the web engine while typing in edit mode", [&]() {
            PreviewView::overrideBackendFactory(&createFakeWebEngineBackend);
            MainWindow editWin;
            editWin.show();
            app.processEvents();
            PreviewView* preview = editWin.previewView();
            emit preview->bridge()->bridgeInitialized();
            QSignalSpy renderSpy(preview->bridge(), &WebBridge::renderDocument);
            for (int i = 0; i < 20; ++i) {
                editWin.editor()->insertPlainText(QStringLiteral("x"));
            }
            assert(editWin.session()->markdown().size() == 20);
            assert(renderSpy.count() == 0);
            PreviewView::clearBackendFactoryOverride();
        });

        it("renders the latest buffer once on the first mode switch", [&]() {
            PreviewView::overrideBackendFactory(&createFakeWebEngineBackend);
            MainWindow editWin;
            editWin.show();
            app.processEvents();
            PreviewView* preview = editWin.previewView();
            emit preview->bridge()->bridgeInitialized();
            QSignalSpy renderSpy(preview->bridge(), &WebBridge::renderDocument);
            editWin.editor()->insertPlainText(QStringLiteral("# Typed Title\n\nTyped body."));
            editWin.setMode(MainWindow::Mode::Preview);
            assert(renderSpy.count() == 1);
            assert(renderSpy.at(0).at(0).toString() == editWin.editor()->toPlainText());
            PreviewView::clearBackendFactoryOverride();
        });

        it("forwards disk reloads to the preview in preview mode", [&]() {
            QTemporaryFile tempFile(QDir::tempPath() + "/markd_disk_reload_XXXXXX.md");
            assert(tempFile.open());
            tempFile.write("# Before Reload\n");
            tempFile.close();

            PreviewView::overrideBackendFactory(&createFakeWebEngineBackend);
            MainWindow previewWin(tempFile.fileName(), MainWindow::Mode::Preview, true);
            PreviewView* preview = previewWin.previewView();
            emit preview->bridge()->bridgeInitialized();
            QSignalSpy renderSpy(preview->bridge(), &WebBridge::renderDocument);

            QFile file(tempFile.fileName());
            assert(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
            file.write("# After Reload\n");
            file.close();
            QMetaObject::invokeMethod(previewWin.session(), "onWatcherTimeout");
            assert(renderSpy.count() >= 1);
            assert(renderSpy.last().at(0).toString().contains("After Reload"));

            previewWin.session()->setMarkdown(QStringLiteral("# Dropped In\n"));
            assert(renderSpy.last().at(0).toString().contains("Dropped In"));
            PreviewView::clearBackendFactoryOverride();
        });
    });
}

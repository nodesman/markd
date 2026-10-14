#pragma once

#include <QApplication>
#include <QFile>
#include <QPalette>
#include <QColor>
#include <cassert>
#include <iostream>
#include "Zinsser.h"
#include "ui/Theme.h"

inline void testThemeEngine(QApplication& app) {
    using Zinsser::describe;
    using Zinsser::it;

    describe("Theme engine and visual token application", [&]() {
        it("verifies stylesheet resource existence in virtual filesystem", [&]() {
            assert(QFile::exists(":/theme/theme.qss"));
        });

        it("loads non-empty stylesheet into application instance", [&]() {
            Theme::apply(app);
            QString qss = app.styleSheet();
            assert(!qss.isEmpty());
        });

        it("enforces obsidian palette color tokens in stylesheet", [&]() {
            QString qss = app.styleSheet();
            assert(qss.contains("#050505"));
            assert(qss.contains("#0d1117"));
            assert(qss.contains("#1e242c"));
            assert(qss.contains("#161b22"));
        });

        it("defines comfortable translucent scrollbar metrics in stylesheet", [&]() {
            QString qss = app.styleSheet();
            assert(qss.contains("QScrollBar:vertical"));
            assert(qss.contains("width: 8px"));
            assert(qss.contains("rgba(255, 255, 255, 0.22)"));
            assert(qss.contains("border-radius: 4px"));
        });

        it("configures fallback palette window base text and highlight colors", [&]() {
            assert(app.palette().color(QPalette::Window) == QColor("#050505"));
            assert(app.palette().color(QPalette::Base) == QColor("#050505"));
            assert(app.palette().color(QPalette::Text) == QColor("#f0f6fc"));
            assert(app.palette().color(QPalette::Highlight) == QColor("#161b22"));
        });
    });
}

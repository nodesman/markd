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

        it("configures fallback dark palette with high contrast text luminance", [&]() {
            const QPalette pal = app.palette();
            assert(pal.color(QPalette::Window).lightness() < pal.color(QPalette::Text).lightness());
            assert(pal.color(QPalette::Base).lightness() < pal.color(QPalette::Text).lightness());
            assert(pal.color(QPalette::Window) == QColor("#050505"));
            assert(pal.color(QPalette::Base) == QColor("#050505"));
            assert(pal.color(QPalette::Text) == QColor("#f0f6fc"));
            assert(pal.color(QPalette::Highlight) == QColor("#161b22"));
        });
    });
}


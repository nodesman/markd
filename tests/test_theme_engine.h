#pragma once

#include <QApplication>
#include <QFile>
#include <QPalette>
#include <QColor>
#include <cassert>
#include <iostream>
#include "ui/Theme.h"

inline void testThemeEngine(QApplication& app) {
    // 1. Verify resource exists in Qt VFS
    assert(QFile::exists(":/theme/theme.qss"));

    // 2. Apply theme
    Theme::apply(app);

    // 3. Verify stylesheet is loaded and non-empty
    QString qss = app.styleSheet();
    assert(!qss.isEmpty());

    // 4. Verify Obsidian palette color tokens in stylesheet
    assert(qss.contains("#050505"));
    assert(qss.contains("#0d1117"));
    assert(qss.contains("#1e242c"));
    assert(qss.contains("#161b22"));

    // 5. Verify accessible, comfortable translucent scrollbar styling in stylesheet
    assert(qss.contains("QScrollBar:vertical"));
    assert(qss.contains("width: 8px"));
    assert(qss.contains("rgba(255, 255, 255, 0.22)"));
    assert(qss.contains("border-radius: 4px"));

    // 6. Verify QPalette fallback window color
    assert(app.palette().color(QPalette::Window) == QColor("#050505"));
    assert(app.palette().color(QPalette::Base) == QColor("#050505"));
    assert(app.palette().color(QPalette::Text) == QColor("#f0f6fc"));
    assert(app.palette().color(QPalette::Highlight) == QColor("#161b22"));

    std::cout << "PASS: Theme engine tests passed!\n";
}

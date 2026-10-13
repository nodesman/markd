#include "ui/Theme.h"
#include <QFile>
#include <QPalette>
#include <QColor>
#include <QDebug>

void Theme::apply(QApplication& app)
{
    // Apply dark fallback palette
    QPalette palette = app.palette();
    palette.setColor(QPalette::Window, QColor("#050505"));
    palette.setColor(QPalette::WindowText, QColor("#f0f6fc"));
    palette.setColor(QPalette::Base, QColor("#050505"));
    palette.setColor(QPalette::AlternateBase, QColor("#0d1117"));
    palette.setColor(QPalette::ToolTipBase, QColor("#0d1117"));
    palette.setColor(QPalette::ToolTipText, QColor("#f0f6fc"));
    palette.setColor(QPalette::Text, QColor("#f0f6fc"));
    palette.setColor(QPalette::Button, QColor("#0d1117"));
    palette.setColor(QPalette::ButtonText, QColor("#f0f6fc"));
    palette.setColor(QPalette::BrightText, QColor("#ffffff"));
    palette.setColor(QPalette::Link, QColor("#58a6ff"));
    palette.setColor(QPalette::Highlight, QColor("#161b22"));
    palette.setColor(QPalette::HighlightedText, QColor("#f0f6fc"));
    app.setPalette(palette);

    // Apply global stylesheet
    QFile qssFile(":/theme/theme.qss");
    if (qssFile.open(QFile::ReadOnly | QFile::Text)) {
        QString styleSheet = QString::fromUtf8(qssFile.readAll());
        app.setStyleSheet(styleSheet);
        qssFile.close();
    } else {
        qWarning() << "Failed to load master theme stylesheet from :/theme/theme.qss";
    }
}

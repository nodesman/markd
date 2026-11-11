#include "ui/Theme.h"
#include <QFile>
#include <QFont>
#include <QPalette>
#include <QColor>
#include <QDebug>

void Theme::apply(QApplication& app)
{
#if defined(Q_OS_MACOS)
    QFont::insertSubstitution(QStringLiteral("Sans Serif"), QStringLiteral("Helvetica Neue"));
    QFont::insertSubstitution(QStringLiteral("sans-serif"), QStringLiteral("Helvetica Neue"));
    QFont::insertSubstitution(QStringLiteral("Monospace"), QStringLiteral("Menlo"));
    QFont::insertSubstitution(QStringLiteral("monospace"), QStringLiteral("Menlo"));
    QFont::insertSubstitution(QStringLiteral("SF Mono"), QStringLiteral("Menlo"));
    if (app.platformName() == QLatin1String("offscreen")) {
        QFont defaultFont;
        defaultFont.setFamilies({QStringLiteral("Helvetica Neue"), QStringLiteral("Apple Symbols")});
        defaultFont.setPointSize(13);
        app.setFont(defaultFont);
    }
#endif

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
    if (!qssFile.open(QFile::ReadOnly | QFile::Text)) {
        qWarning() << "Failed to load master theme stylesheet from :/theme/theme.qss";
        return;
    }
    const QString styleSheet = QString::fromUtf8(qssFile.readAll());
    app.setStyleSheet(styleSheet);
    qssFile.close();
}

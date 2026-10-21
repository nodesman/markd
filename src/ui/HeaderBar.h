#pragma once

#include <QWidget>
#include <QPushButton>
#include <QHBoxLayout>
#include <QMouseEvent>
#include "ui/platform/PlatformFrame.h"

class SidebarToggleButton : public QPushButton {
    Q_OBJECT

public:
    explicit SidebarToggleButton(QWidget* parent = nullptr);
    explicit SidebarToggleButton(const QString& text, QWidget* parent = nullptr);
    ~SidebarToggleButton() override = default;

    QSize sizeHint() const override { return QSize(32, 28); }

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
};

class HeaderActionButton : public QPushButton {
    Q_OBJECT

public:
    enum class Glyph { Export, Find };

    explicit HeaderActionButton(Glyph glyph, QWidget* parent = nullptr);
    ~HeaderActionButton() override = default;

    QSize sizeHint() const override { return QSize(32, 28); }
    Glyph glyph() const { return m_glyph; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    Glyph m_glyph;
};

class HeaderBar : public QWidget {
    Q_OBJECT

public:
    explicit HeaderBar(QWidget* parent = nullptr);
    ~HeaderBar() override = default;

    QWidget* leadingSlot() const { return m_leadingSlot; }
    QWidget* centerSlot() const { return m_centerSlot; }
    QWidget* trailingSlot() const { return m_trailingSlot; }

    void updateMargins();
    void balanceFlanks();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void setupUI();

    QWidget* m_leadingSlot = nullptr;
    QWidget* m_centerSlot = nullptr;
    QWidget* m_trailingSlot = nullptr;

    QWidget* m_leftSpacer = nullptr;
    QWidget* m_rightSpacer = nullptr;
};

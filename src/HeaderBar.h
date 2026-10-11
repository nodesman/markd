#pragma once

#include <QWidget>
#include <QHBoxLayout>
#include <QMouseEvent>
#include "PlatformFrame.h"

class HeaderBar : public QWidget {
    Q_OBJECT

public:
    explicit HeaderBar(QWidget* parent = nullptr);
    ~HeaderBar() override = default;

    QWidget* leadingSlot() const { return m_leadingSlot; }
    QWidget* centerSlot() const { return m_centerSlot; }
    QWidget* trailingSlot() const { return m_trailingSlot; }

    void updateMargins();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void setupUI();
    void balanceFlanks();

    QWidget* m_leadingSlot = nullptr;
    QWidget* m_centerSlot = nullptr;
    QWidget* m_trailingSlot = nullptr;

    QWidget* m_leftSpacer = nullptr;
    QWidget* m_rightSpacer = nullptr;
};

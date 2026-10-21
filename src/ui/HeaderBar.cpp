#include "ui/HeaderBar.h"
#include <QHBoxLayout>
#include <QWindow>
#include <QPainter>
#include <QStyleOptionButton>
#include <QStyle>
#include <algorithm>

SidebarToggleButton::SidebarToggleButton(QWidget* parent) : QPushButton(parent)
{
    setAttribute(Qt::WA_Hover, true);
    setFixedSize(32, 28);
}

SidebarToggleButton::SidebarToggleButton(const QString& text, QWidget* parent) : QPushButton(text, parent)
{
    setAttribute(Qt::WA_Hover, true);
    setFixedSize(32, 28);
}

void SidebarToggleButton::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QStyleOptionButton opt;
    initStyleOption(&opt);
    opt.text.clear();
    opt.icon = QIcon();

    QPainter painter(this);
    style()->drawControl(QStyle::CE_PushButtonBevel, &opt, &painter, this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QColor iconColor("#8b949e");
    if (underMouse() || isDown() || isChecked()) iconColor = QColor("#f0f6fc");
    if (!isEnabled()) iconColor = QColor("#484f58");

    const QRectF outerRect(9.0, 8.0, 14.0, 12.0);
    QPen pen(iconColor, 1.25, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(outerRect, 2.0, 2.0);
    const qreal splitX = outerRect.left() + 4.5;
    painter.drawLine(QPointF(splitX, outerRect.top()), QPointF(splitX, outerRect.bottom()));
}

void SidebarToggleButton::mouseDoubleClickEvent(QMouseEvent* event)
{
    event->accept();
    QPushButton::mouseDoubleClickEvent(event);
    event->accept();
}

HeaderActionButton::HeaderActionButton(Glyph glyph, QWidget* parent) : QPushButton(parent), m_glyph(glyph)
{
    setAttribute(Qt::WA_Hover, true);
    setFixedSize(32, 28);
}

void HeaderActionButton::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QStyleOptionButton opt;
    initStyleOption(&opt);
    opt.text.clear();
    opt.icon = QIcon();

    QPainter painter(this);
    style()->drawControl(QStyle::CE_PushButtonBevel, &opt, &painter, this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QColor iconColor("#8b949e");
    if (underMouse() || isDown() || isChecked()) iconColor = QColor("#f0f6fc");
    if (!isEnabled()) iconColor = QColor("#484f58");

    painter.setPen(QPen(iconColor, 1.25, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    if (m_glyph == Glyph::Export) {
        painter.drawLine(QPointF(16.0, 16.0), QPointF(16.0, 8.0));
        painter.drawLine(QPointF(13.0, 11.0), QPointF(16.0, 8.0));
        painter.drawLine(QPointF(19.0, 11.0), QPointF(16.0, 8.0));
        painter.drawLine(QPointF(10.0, 15.0), QPointF(10.0, 19.5));
        painter.drawLine(QPointF(10.0, 19.5), QPointF(22.0, 19.5));
        painter.drawLine(QPointF(22.0, 19.5), QPointF(22.0, 15.0));
        return;
    }
    painter.drawEllipse(QRectF(10.0, 8.0, 9.0, 9.0));
    painter.drawLine(QPointF(17.5, 15.5), QPointF(21.5, 19.5));
}

void HeaderActionButton::mouseDoubleClickEvent(QMouseEvent* event)
{
    event->accept();
    QPushButton::mouseDoubleClickEvent(event);
    event->accept();
}

HeaderBar::HeaderBar(QWidget* parent) : QWidget(parent)
{
    setObjectName("windowHeaderBar");
    setFixedHeight(PlatformFrame::headerHeight());
    setupUI();
}

void HeaderBar::setupUI()
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_leadingSlot = new QWidget(this);
    m_leadingSlot->setObjectName("headerLeadingSlot");
    auto* leadingLayout = new QHBoxLayout(m_leadingSlot);
    leadingLayout->setContentsMargins(0, 0, 0, 0);
    leadingLayout->setSpacing(0);
    leadingLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_leftSpacer = new QWidget(this);
    m_leftSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    m_centerSlot = new QWidget(this);
    m_centerSlot->setObjectName("headerCenterSlot");
    auto* centerLayout = new QHBoxLayout(m_centerSlot);
    centerLayout->setContentsMargins(0, 10, 0, 10);
    centerLayout->setSpacing(0);
    centerLayout->setAlignment(Qt::AlignCenter);

    m_rightSpacer = new QWidget(this);
    m_rightSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    m_trailingSlot = new QWidget(this);
    m_trailingSlot->setObjectName("headerTrailingSlot");
    auto* trailingLayout = new QHBoxLayout(m_trailingSlot);
    trailingLayout->setContentsMargins(0, 0, 0, 0);
    trailingLayout->setSpacing(0);

    layout->addWidget(m_leadingSlot, 0);
    layout->addWidget(m_leftSpacer, 1);
    layout->addWidget(m_centerSlot, 0);
    layout->addWidget(m_rightSpacer, 1);
    layout->addWidget(m_trailingSlot, 0);

    updateMargins();
}

void HeaderBar::updateMargins()
{
    PlatformSafeArea safe = PlatformFrame::safeArea(window());
    m_leadingSlot->setContentsMargins(safe.leftMargin, 0, 0, 0);
    m_trailingSlot->setContentsMargins(0, 0, safe.rightMargin, 0);
    balanceFlanks();
}

void HeaderBar::balanceFlanks()
{
    int leftContent = m_leadingSlot->layout() ? m_leadingSlot->layout()->sizeHint().width() : 0;
    int leftMargins = m_leadingSlot->contentsMargins().left() + m_leadingSlot->contentsMargins().right();
    int rightContent = m_trailingSlot->layout() ? m_trailingSlot->layout()->sizeHint().width() : 0;
    int rightMargins = m_trailingSlot->contentsMargins().left() + m_trailingSlot->contentsMargins().right();
    int flank = std::max(leftMargins + leftContent, rightMargins + rightContent);
    m_leadingSlot->setFixedWidth(flank);
    m_trailingSlot->setFixedWidth(flank);
}

void HeaderBar::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    balanceFlanks();
}

void HeaderBar::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    if (event->position().y() <= PlatformFrame::kTopResizeBuffer) {
        event->ignore();
        return;
    }
    if (PlatformFrame::isDraggableRegion(event->position().toPoint()) && window() && window()->windowHandle()) {
        window()->windowHandle()->startSystemMove();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void HeaderBar::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && event->position().y() > PlatformFrame::kTopResizeBuffer && window()) {
        const bool wasMaximized = window()->isMaximized();
        if (wasMaximized) window()->showNormal();
        if (!wasMaximized) window()->showMaximized();
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

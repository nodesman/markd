#include "ui/HeaderBar.h"
#include <QHBoxLayout>
#include <QWindow>
#include <algorithm>

HeaderBar::HeaderBar(QWidget* parent)
    : QWidget(parent)
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

    m_leftSpacer = new QWidget(this);
    m_leftSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    m_centerSlot = new QWidget(this);
    m_centerSlot->setObjectName("headerCenterSlot");
    auto* centerLayout = new QHBoxLayout(m_centerSlot);
    centerLayout->setContentsMargins(0, 0, 0, 0);
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
    
    // Set padding on leadingSlot and trailingSlot according to safe area
    m_leadingSlot->setContentsMargins(safe.leftMargin, 0, 0, 0);
    m_trailingSlot->setContentsMargins(0, 0, safe.rightMargin, 0);

    balanceFlanks();
}

void HeaderBar::balanceFlanks()
{
    // Dynamic mathematical true-window centering:
    // We counterbalance flanking spacers so that centerSlot is locked precisely to windowWidth / 2.
    // Width consumed on left = leadingSlot width (including safe area margin)
    // Width consumed on right = trailingSlot width (including safe area margin)
    // Flank offset ensures the two expanding sides keep the centerSlot centered.
    // If leading and trailing natural widths differ, we can equalize spacer balance or minimum widths.
    int leftTaken = m_leadingSlot->sizeHint().width();
    if (m_leadingSlot->contentsMargins().left() > leftTaken) {
        leftTaken = m_leadingSlot->contentsMargins().left();
    }

    int rightTaken = m_trailingSlot->sizeHint().width();
    if (m_trailingSlot->contentsMargins().right() > rightTaken) {
        rightTaken = m_trailingSlot->contentsMargins().right();
    }

    int flank = std::max(leftTaken, rightTaken);
    m_leadingSlot->setMinimumWidth(flank);
    m_trailingSlot->setMinimumWidth(flank);
}

void HeaderBar::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    balanceFlanks();
}

void HeaderBar::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        // 0 <= y <= 4: Passes through (event->ignore()) for native OS vertical window resizing
        if (event->position().y() <= PlatformFrame::kTopResizeBuffer) {
            event->ignore();
            return;
        }

        // y > 4: Empty header clicks trigger window drag
        if (PlatformFrame::isDraggableRegion(event->position().toPoint())) {
            if (window() && window()->windowHandle()) {
                window()->windowHandle()->startSystemMove();
                event->accept();
                return;
            }
        }
    }
    QWidget::mousePressEvent(event);
}

void HeaderBar::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        if (event->position().y() > PlatformFrame::kTopResizeBuffer) {
            if (window()) {
                if (window()->isMaximized()) {
                    window()->showNormal();
                } else {
                    window()->showMaximized();
                }
                event->accept();
                return;
            }
        }
    }
    QWidget::mouseDoubleClickEvent(event);
}

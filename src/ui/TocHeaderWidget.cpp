#include "ui/TocHeaderWidget.h"
#include "ui/TocWidget.h"
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QKeyEvent>

TocHeaderWidget::TocHeaderWidget(TocWidget* owner, QWidget* parent)
    : QWidget(parent)
    , m_owner(owner)
{
    setupUI();
}

void TocHeaderWidget::setupUI()
{
    setObjectName(QStringLiteral("tocHeaderWidget"));
    setFixedHeight(28);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(16, 0, 16, 0);
    layout->setSpacing(6);

    m_iconLabel = new QLabel(QString::fromUtf8("≡"), this);
    m_iconLabel->setObjectName(QStringLiteral("tocHeaderIconLabel"));
    m_iconLabel->setStyleSheet(QStringLiteral("color: #8b949e; font-size: 13px;"));

    m_caretLabel = new QLabel(QString::fromUtf8("⌵"), this);
    m_caretLabel->setObjectName(QStringLiteral("tocHeaderCaretLabel"));
    m_caretLabel->setStyleSheet(QStringLiteral("color: #8b949e; font-size: 11px;"));

    m_titleLabel = new QLabel(tr("Table of Contents"), this);
    m_titleLabel->setObjectName(QStringLiteral("tocHeaderTitleLabel"));
    m_titleLabel->setStyleSheet(QStringLiteral("color: #c9d1d9; font-size: 12px; font-weight: 600;"));

    layout->addWidget(m_iconLabel);
    layout->addWidget(m_caretLabel);
    layout->addWidget(m_titleLabel);
    layout->addStretch();
}

void TocHeaderWidget::mousePressEvent(QMouseEvent* event)
{
    event->accept();
    if (m_owner) {
        m_owner->toggleSection();
    }
}

void TocHeaderWidget::keyPressEvent(QKeyEvent* event)
{
    const int key = event->key();
    if (key == Qt::Key_Return || key == Qt::Key_Enter || key == Qt::Key_Space) {
        event->accept();
        if (m_owner) {
            m_owner->toggleSection();
        }
        return;
    }
    QWidget::keyPressEvent(event);
}

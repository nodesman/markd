#include "ui/ModeSwitcher.h"
#include <QMouseEvent>
#include <QStyle>

ModeSwitcher::ModeSwitcher(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("modeSwitcher");
    setFixedHeight(26);
    setFixedWidth(140);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(1, 1, 1, 1);
    layout->setSpacing(0);

    m_editBtn = new QPushButton("Edit", this);
    m_editBtn->setObjectName("modeSegmentEdit");
    m_editBtn->setCheckable(true);
    m_editBtn->setChecked(true);
    m_editBtn->setCursor(Qt::PointingHandCursor);
    m_editBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_previewBtn = new QPushButton("Preview", this);
    m_previewBtn->setObjectName("modeSegmentPreview");
    m_previewBtn->setCheckable(true);
    m_previewBtn->setChecked(false);
    m_previewBtn->setCursor(Qt::PointingHandCursor);
    m_previewBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    layout->addWidget(m_editBtn);
    layout->addWidget(m_previewBtn);

    connect(m_editBtn, &QPushButton::clicked, this, [this]() {
        setMode(DocumentMode::Edit);
    });

    connect(m_previewBtn, &QPushButton::clicked, this, [this]() {
        setMode(DocumentMode::Preview);
    });

    updateSegmentStyles();
}

void ModeSwitcher::setMode(DocumentMode mode)
{
    if (m_currentMode == mode) {
        return;
    }

    m_currentMode = mode;
    m_editBtn->setChecked(m_currentMode == DocumentMode::Edit);
    m_previewBtn->setChecked(m_currentMode == DocumentMode::Preview);
    updateSegmentStyles();

    emit modeChanged(m_currentMode);
}

void ModeSwitcher::toggleMode()
{
    if (m_currentMode == DocumentMode::Edit) {
        setMode(DocumentMode::Preview);
    } else {
        setMode(DocumentMode::Edit);
    }
}

void ModeSwitcher::updateSegmentStyles()
{
    m_editBtn->setProperty("active", m_currentMode == DocumentMode::Edit);
    m_previewBtn->setProperty("active", m_currentMode == DocumentMode::Preview);

    // Refresh dynamic property styling
    m_editBtn->style()->unpolish(m_editBtn);
    m_editBtn->style()->polish(m_editBtn);
    m_previewBtn->style()->unpolish(m_previewBtn);
    m_previewBtn->style()->polish(m_previewBtn);
}

void ModeSwitcher::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        // SWIPE_02: Halts event bubbling to QWindow::startSystemMove()!
        // Prevent click leaking into window drag
        event->accept();
        // If clicking on container directly (outside buttons), toggle mode
        if (event->position().x() < width() / 2) {
            setMode(DocumentMode::Edit);
        } else {
            setMode(DocumentMode::Preview);
        }
        return;
    }
    QWidget::mousePressEvent(event);
}

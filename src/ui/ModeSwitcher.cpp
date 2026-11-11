#include "ui/ModeSwitcher.h"
#include <QMouseEvent>
#include <QStyle>

ModeSwitcher::ModeSwitcher(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("modeSwitcher");
    setFixedHeight(30);
    setFixedWidth(154);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(0);

    m_editBtn = new QPushButton("Edit", this);
    m_editBtn->setObjectName("modeSegmentEdit");
    m_editBtn->setCheckable(true);
    m_editBtn->setChecked(true);
    m_editBtn->setFocusPolicy(Qt::NoFocus);
    m_editBtn->setCursor(Qt::PointingHandCursor);
    m_editBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_previewBtn = new QPushButton("Preview", this);
    m_previewBtn->setObjectName("modeSegmentPreview");
    m_previewBtn->setCheckable(true);
    m_previewBtn->setChecked(false);
    m_previewBtn->setFocusPolicy(Qt::NoFocus);
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
    setMode(m_currentMode == DocumentMode::Edit ? DocumentMode::Preview : DocumentMode::Edit);
}

void ModeSwitcher::updateSegmentStyles()
{
    m_editBtn->setProperty("active", m_currentMode == DocumentMode::Edit);
    m_previewBtn->setProperty("active", m_currentMode == DocumentMode::Preview);

    if (isVisible()) {
        m_editBtn->style()->unpolish(m_editBtn);
        m_editBtn->style()->polish(m_editBtn);
        m_previewBtn->style()->unpolish(m_previewBtn);
        m_previewBtn->style()->polish(m_previewBtn);
    }

    QFont font = m_editBtn->font();
    font.setPixelSize(12);
    font.setWeight(QFont::DemiBold);
    font.setLetterSpacing(QFont::AbsoluteSpacing, 0.2);
    m_editBtn->setFont(font);
    m_previewBtn->setFont(font);
}

void ModeSwitcher::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    event->accept();
    DocumentMode target = (event->position().x() < width() / 2)
        ? DocumentMode::Edit
        : DocumentMode::Preview;
    setMode(target);
}

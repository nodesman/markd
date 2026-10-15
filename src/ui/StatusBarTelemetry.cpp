#include "ui/StatusBarTelemetry.h"
#include <cmath>
#include <QLocale>
#include <QTimer>
#include <QStyle>

TelemetryLabel::TelemetryLabel(QWidget* parent)
    : QLabel(parent)
{
    setObjectName(QStringLiteral("telemetryLabel"));
    setCursor(Qt::PointingHandCursor);
    setToolTip(tr("Click to toggle word and character count"));
}

void TelemetryLabel::setMetrics(const TelemetryMetrics& metrics) {
    m_metrics = metrics;
    updateDisplayText();
}

void TelemetryLabel::setSelectionMetrics(int words, int chars) {
    m_hasSelection = true;
    m_selectionWords = words;
    m_selectionChars = chars;
    updateDisplayText();
}

void TelemetryLabel::clearSelectionMetrics() {
    m_hasSelection = false;
    m_selectionWords = 0;
    m_selectionChars = 0;
    updateDisplayText();
}

void TelemetryLabel::toggleDisplayMode() {
    m_mode = (m_mode == DisplayMode::WordsAndReadingTime)
        ? DisplayMode::CharacterCount
        : DisplayMode::WordsAndReadingTime;
    updateDisplayText();
}

void TelemetryLabel::setDisplayMode(DisplayMode mode) {
    m_mode = mode;
    updateDisplayText();
}

void TelemetryLabel::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        event->accept();
        toggleDisplayMode();
        return;
    }
    QLabel::mousePressEvent(event);
}

void TelemetryLabel::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        event->accept();
        return;
    }
    QLabel::mouseReleaseEvent(event);
}

void TelemetryLabel::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        event->accept();
        toggleDisplayMode();
        return;
    }
    QLabel::mouseDoubleClickEvent(event);
}

void TelemetryLabel::updateDisplayText() {
    if (m_hasSelection) {
        formatSelectionText();
        return;
    }
    formatFullDocumentText();
}

void TelemetryLabel::formatSelectionText() {
    if (m_mode == DisplayMode::CharacterCount) {
        const QString charUnit = (m_metrics.characterCount == 1) ? QStringLiteral("character") : QStringLiteral("characters");
        setText(QStringLiteral("%1 of %2 %3").arg(
            QLocale().toString(m_selectionChars),
            QLocale().toString(m_metrics.characterCount),
            charUnit
        ));
        return;
    }
    const QString wordUnit = (m_metrics.wordCount == 1) ? QStringLiteral("word") : QStringLiteral("words");
    const QString velocity = TelemetryEngine::readingVelocityText(m_selectionWords);
    setText(QStringLiteral("%1 of %2 %3 · %4").arg(
        QLocale().toString(m_selectionWords),
        QLocale().toString(m_metrics.wordCount),
        wordUnit,
        velocity
    ));
}


void TelemetryLabel::formatFullDocumentText() {
    if (m_mode == DisplayMode::CharacterCount) {
        setText(m_metrics.formattedCharacters);
        return;
    }
    setText(m_metrics.formattedTelemetry);
}

StatusBarTelemetry::StatusBarTelemetry(QWidget* parent)
    : QStatusBar(parent)
{
    setObjectName(QStringLiteral("statusBarTelemetry"));
    setFixedHeight(24);
    setSizeGripEnabled(false);
    setContentsMargins(12, 0, 12, 0);

    m_telemetryLabel = new TelemetryLabel(this);

    m_sectionLabel = new QLabel(this);
    m_sectionLabel->setObjectName(QStringLiteral("sectionContextLabel"));
    m_sectionLabel->setVisible(false);

    m_notificationLabel = new QLabel(this);
    m_notificationLabel->setObjectName(QStringLiteral("statusNotificationLabel"));
    m_notificationLabel->setVisible(false);

    m_encodingLabel = new QLabel(QStringLiteral("UTF-8"), this);
    m_encodingLabel->setObjectName(QStringLiteral("encodingLabel"));

    m_statusSeparatorLabel = new QLabel(QStringLiteral("·"), this);
    m_statusSeparatorLabel->setObjectName(QStringLiteral("statusSeparatorLabel"));

    m_zoomLabel = new QLabel(QStringLiteral("100%"), this);
    m_zoomLabel->setObjectName(QStringLiteral("zoomLabel"));

    m_notificationTimer = new QTimer(this);
    m_notificationTimer->setSingleShot(true);
    connect(m_notificationTimer, &QTimer::timeout, this, &StatusBarTelemetry::clearNotification);

    addWidget(m_telemetryLabel);
    addWidget(m_sectionLabel);
    addWidget(m_notificationLabel);

    addPermanentWidget(m_encodingLabel);
    addPermanentWidget(m_statusSeparatorLabel);
    addPermanentWidget(m_zoomLabel);
}

void StatusBarTelemetry::updateMetrics(const TelemetryMetrics& metrics) {
    m_telemetryLabel->setMetrics(metrics);
}

void StatusBarTelemetry::setZoomFactor(qreal factor) {
    const int percent = static_cast<int>(std::round(factor * 100.0));
    m_zoomLabel->setText(QStringLiteral("%1%").arg(percent));
}

void StatusBarTelemetry::setSectionTitle(const QString& title) {
    m_sectionTitle = title.trimmed();
    if (m_sectionTitle.isEmpty()) {
        m_sectionLabel->clear();
        m_sectionLabel->setVisible(false);
        return;
    }
    m_sectionLabel->setText(QStringLiteral("§ %1").arg(m_sectionTitle));
}

void StatusBarTelemetry::setSectionVisible(bool visible) {
    const bool shouldShow = visible && !m_sectionTitle.isEmpty();
    m_sectionLabel->setVisible(shouldShow);
}

void StatusBarTelemetry::showNotification(const QString& message, const QString& level, int timeoutMs) {
    m_notificationTimer->stop();
    m_notificationLabel->setText(message);
    m_notificationLabel->setProperty("level", level);
    m_notificationLabel->style()->unpolish(m_notificationLabel);
    m_notificationLabel->style()->polish(m_notificationLabel);
    m_notificationLabel->setVisible(true);

    if (timeoutMs > 0) {
        m_notificationTimer->start(timeoutMs);
    }
}

void StatusBarTelemetry::clearNotification() {
    m_notificationTimer->stop();
    m_notificationLabel->clear();
    m_notificationLabel->setVisible(false);
}

void StatusBarTelemetry::setSelectionMetrics(int words, int chars) {
    m_telemetryLabel->setSelectionMetrics(words, chars);
}

void StatusBarTelemetry::clearSelectionMetrics() {
    m_telemetryLabel->clearSelectionMetrics();
}


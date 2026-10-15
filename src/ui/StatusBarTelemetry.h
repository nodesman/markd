#pragma once

#include <QStatusBar>
#include <QLabel>
#include <QMouseEvent>
#include "core/TelemetryEngine.h"

class QTimer;

class TelemetryLabel : public QLabel {
    Q_OBJECT

public:
    enum class DisplayMode {
        WordsAndReadingTime,
        CharacterCount
    };

    explicit TelemetryLabel(QWidget* parent = nullptr);

    void setMetrics(const TelemetryMetrics& metrics);
    void setSelectionMetrics(int words, int chars);
    void clearSelectionMetrics();
    void toggleDisplayMode();
    void setDisplayMode(DisplayMode mode);

    DisplayMode currentMode() const { return m_mode; }

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    void updateDisplayText();
    void formatSelectionText();
    void formatFullDocumentText();

    DisplayMode m_mode = DisplayMode::WordsAndReadingTime;
    TelemetryMetrics m_metrics;
    bool m_hasSelection = false;
    int m_selectionWords = 0;
    int m_selectionChars = 0;
};

class StatusBarTelemetry : public QStatusBar {
    Q_OBJECT

public:
    explicit StatusBarTelemetry(QWidget* parent = nullptr);
    ~StatusBarTelemetry() override = default;

    void updateMetrics(const TelemetryMetrics& metrics);
    void setZoomFactor(qreal factor);
    void setSectionTitle(const QString& title);
    void setSectionVisible(bool visible);
    void showNotification(const QString& message, const QString& level = QStringLiteral("info"), int timeoutMs = 0);
    void clearNotification();
    void setSelectionMetrics(int words, int chars);
    void clearSelectionMetrics();

    TelemetryLabel* telemetryLabel() const { return m_telemetryLabel; }
    QLabel* sectionLabel() const { return m_sectionLabel; }
    QLabel* notificationLabel() const { return m_notificationLabel; }
    QLabel* encodingLabel() const { return m_encodingLabel; }
    QLabel* zoomLabel() const { return m_zoomLabel; }

private:
    TelemetryLabel* m_telemetryLabel = nullptr;
    QLabel* m_sectionLabel = nullptr;
    QLabel* m_notificationLabel = nullptr;
    QLabel* m_encodingLabel = nullptr;
    QLabel* m_statusSeparatorLabel = nullptr;
    QLabel* m_zoomLabel = nullptr;
    QTimer* m_notificationTimer = nullptr;
    QString m_sectionTitle;
};


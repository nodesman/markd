#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QFileSystemWatcher>
#include <cstdint>
#include <vector>
#include "parser/TocItem.h"
#include "parser/TocParser.h"
#include "core/TelemetryEngine.h"

// Required for QVariant::value<std::vector<TocItem>>() template extraction in Qt 6 / C++20
Q_DECLARE_METATYPE(std::vector<TocItem>)
Q_DECLARE_METATYPE(TelemetryMetrics)

class DocumentSession : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString markdown READ markdown WRITE setMarkdown NOTIFY markdownChanged)
    Q_PROPERTY(QString selectedHeading READ selectedHeading WRITE setSelectedHeading NOTIFY selectedHeadingChanged)
    Q_PROPERTY(QString activeHeading READ activeHeading WRITE setActiveHeading NOTIFY activeHeadingChanged)
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(qreal zoomFactor READ zoomFactor WRITE setZoomFactor NOTIFY zoomFactorChanged)
    Q_PROPERTY(bool isDirty READ isDirty WRITE setDirty NOTIFY dirtyChanged)
    Q_PROPERTY(QString filePath READ filePath WRITE setFilePath NOTIFY filePathChanged)
    Q_PROPERTY(int mode READ mode WRITE setMode NOTIFY modeChanged)

public:
    explicit DocumentSession(QObject* parent = nullptr);
    ~DocumentSession() override = default;

    static uint64_t allocationCount();
    static void resetAllocationCount();

    // Property Getters
    QString markdown() const { return m_markdown; }
    QString selectedHeading() const { return m_selectedHeading; }
    QString activeHeading() const { return m_activeHeading; }
    QString theme() const { return m_theme; }
    qreal zoomFactor() const { return m_zoomFactor; }
    bool isDirty() const { return m_isDirty; }
    QString filePath() const { return m_filePath; }
    int mode() const { return m_mode; }

    int wordCount() const { return m_wordCount; }
    int readingTimeMinutes() const { return m_readingTimeMinutes; }
    const TelemetryMetrics& telemetryMetrics() const { return m_telemetryMetrics; }

public slots:
    // Property Setters
    void setMarkdown(const QString& md);
    void setSelectedHeading(const QString& id);
    void setActiveHeading(const QString& id);
    void setTheme(const QString& theme);
    void setZoomFactor(qreal factor);
    void setDirty(bool dirty);
    void setFilePath(const QString& path);
    void setMode(int mode);

    // Q_INVOKABLE Domain Methods
    Q_INVOKABLE void reportActiveHeading(const QString& id);
    Q_INVOKABLE void reportMetrics(int words, int minutes);
    Q_INVOKABLE void updateTOC(const QString& tocJson);
    Q_INVOKABLE bool loadFile(const QString& path);
    Q_INVOKABLE bool save();
    Q_INVOKABLE bool saveAs(const QString& path);
    Q_INVOKABLE void zoomIn();
    Q_INVOKABLE void zoomOut();
    Q_INVOKABLE void resetZoom();
    Q_INVOKABLE void flushOutline();
    Q_INVOKABLE void flushTelemetry();
    Q_INVOKABLE void copyToClipboard(const QString& text);

signals:
    void clipboardCopyRequested(const QString& text);
    void markdownChanged(const QString& markdown);
    void selectedHeadingChanged(const QString& selectedHeading);
    void activeHeadingChanged(const QString& activeHeading);
    void themeChanged(const QString& theme);
    void zoomFactorChanged(qreal zoomFactor);
    void dirtyChanged(bool isDirty);
    void filePathChanged(const QString& filePath);
    void modeChanged(int mode);

    void metricsReported(int words, int minutes);
    void telemetryUpdated(const TelemetryMetrics& metrics);
    void tocUpdated(const QString& tocJson);
    void fileLoaded(const QString& path);
    void tocChanged(const std::vector<TocItem>& headings);
    void diskConflictDetected();
    void diskReloaded();

private slots:
    void onTocDebounceTimeout();
    void onWatcherTimeout();
    void onFileChanged(const QString& path);
    void onRearmTimeout();

private:
    void rearmWatcher();
    void updateTelemetry();

    QString m_markdown;
    QString m_selectedHeading;
    QString m_activeHeading;
    QString m_theme = QStringLiteral("obsidian");
    qreal m_zoomFactor = 1.0;
    bool m_isDirty = false;
    QString m_filePath;
    int m_mode = 0;

    int m_wordCount = 0;
    int m_readingTimeMinutes = 0;
    TelemetryMetrics m_telemetryMetrics;
    std::vector<TocItem> m_headings;
    bool m_outlineClean = true;
    bool m_telemetryClean = true;

    // Tracks last content loaded or written to disk to filter self-echoes
    QString m_lastKnownDiskContent;

    QTimer* m_tocDebounceTimer = nullptr;
    QTimer* m_telemetryDebounceTimer = nullptr;
    QTimer* m_watcherDebounceTimer = nullptr;
    QTimer* m_rearmTimer = nullptr;
    QFileSystemWatcher* m_watcher = nullptr;
};


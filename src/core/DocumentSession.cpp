#include "core/DocumentSession.h"
#include <QFile>
#include <QFileInfo>
#include <cmath>
#include <algorithm>

DocumentSession::DocumentSession(QObject* parent)
    : QObject(parent)
{
    qRegisterMetaType<std::vector<TocItem>>("std::vector<TocItem>");
    qRegisterMetaType<TocItem>("TocItem");
    qRegisterMetaType<TelemetryMetrics>("TelemetryMetrics");

    m_tocDebounceTimer = new QTimer(this);
    m_tocDebounceTimer->setSingleShot(true);
    connect(m_tocDebounceTimer, &QTimer::timeout, this, &DocumentSession::onTocDebounceTimeout);

    m_telemetryDebounceTimer = new QTimer(this);
    m_telemetryDebounceTimer->setSingleShot(true);
    connect(m_telemetryDebounceTimer, &QTimer::timeout, this, &DocumentSession::updateTelemetry);

    m_watcher = new QFileSystemWatcher(this);
    m_watcherDebounceTimer = new QTimer(this);
    m_watcherDebounceTimer->setSingleShot(true);
    connect(m_watcherDebounceTimer, &QTimer::timeout, this, &DocumentSession::onWatcherTimeout);
    connect(m_watcher, &QFileSystemWatcher::fileChanged, this, &DocumentSession::onFileChanged);

    m_rearmTimer = new QTimer(this);
    m_rearmTimer->setSingleShot(true);
    connect(m_rearmTimer, &QTimer::timeout, this, &DocumentSession::onRearmTimeout);

    updateTelemetry();
}

void DocumentSession::setMarkdown(const QString& md) {
    if (m_markdown == md) return;

    m_markdown = md;
    setDirty(true);
    emit markdownChanged(m_markdown);
    m_tocDebounceTimer->start(150);
    m_telemetryDebounceTimer->start(150);
}


void DocumentSession::setSelectedHeading(const QString& id) {
    if (m_selectedHeading != id) {
        m_selectedHeading = id;
        emit selectedHeadingChanged(m_selectedHeading);
    }
}

void DocumentSession::setActiveHeading(const QString& id) {
    if (m_activeHeading != id) {
        m_activeHeading = id;
        emit activeHeadingChanged(m_activeHeading);
    }
}

void DocumentSession::setTheme(const QString& theme) {
    if (m_theme != theme) {
        m_theme = theme;
        emit themeChanged(m_theme);
    }
}

void DocumentSession::setZoomFactor(qreal factor) {
    qreal clamped = std::clamp(factor, 0.25, 3.0);
    clamped = std::round(clamped * 100.0) / 100.0;
    if (std::abs(m_zoomFactor - clamped) > 0.001) {
        m_zoomFactor = clamped;
        emit zoomFactorChanged(m_zoomFactor);
    }
}

void DocumentSession::setDirty(bool dirty) {
    if (m_isDirty != dirty) {
        m_isDirty = dirty;
        emit dirtyChanged(m_isDirty);
    }
}

void DocumentSession::setFilePath(const QString& path) {
    if (m_filePath == path) return;

    if (!m_filePath.isEmpty() && m_watcher->files().contains(m_filePath)) {
        m_watcher->removePath(m_filePath);
    }
    m_rearmTimer->stop();

    m_filePath = path;

    if (!m_filePath.isEmpty() && QFile::exists(m_filePath)) {
        m_watcher->addPath(m_filePath);
    }

    emit filePathChanged(m_filePath);
}

void DocumentSession::setMode(int mode) {
    if (m_mode != mode) {
        m_mode = mode;
        emit modeChanged(m_mode);
    }
}

void DocumentSession::reportActiveHeading(const QString& id) {
    if (m_activeHeading != id) {
        m_activeHeading = id;
        emit activeHeadingChanged(m_activeHeading);
    }
}

void DocumentSession::reportMetrics(int words, int minutes) {
    m_wordCount = words;
    m_readingTimeMinutes = minutes;
    m_telemetryMetrics.wordCount = words;
    m_telemetryMetrics.readingTimeMinutes = minutes;
    emit metricsReported(words, minutes);
}

void DocumentSession::updateTOC(const QString& tocJson) {
    emit tocUpdated(tocJson);
}

bool DocumentSession::loadFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QString content = QString::fromUtf8(file.readAll());
    file.close();

    if (m_filePath != path) {
        if (!m_filePath.isEmpty() && m_watcher->files().contains(m_filePath)) {
            m_watcher->removePath(m_filePath);
        }
        m_rearmTimer->stop();
        m_filePath = path;
        emit filePathChanged(m_filePath);
    }

    if (!m_filePath.isEmpty() && QFile::exists(m_filePath) && !m_watcher->files().contains(m_filePath)) {
        m_watcher->addPath(m_filePath);
    }

    m_markdown = content;
    m_lastKnownDiskContent = content;
    m_isDirty = false;

    m_tocDebounceTimer->stop();
    m_telemetryDebounceTimer->stop();
    auto headings = TocParser::extractHeadings(m_markdown);

    updateTelemetry();


    emit fileLoaded(m_filePath);
    emit markdownChanged(m_markdown);
    emit dirtyChanged(false);
    emit tocChanged(headings);

    return true;
}

bool DocumentSession::save() {
    if (m_filePath.isEmpty()) return false;
    return saveAs(m_filePath);
}

bool DocumentSession::saveAs(const QString& path) {
    if (path.isEmpty()) return false;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return false;
    }

    QByteArray bytes = m_markdown.toUtf8();
    qint64 written = file.write(bytes);
    file.flush();
    file.close();

    if (written != bytes.size()) {
        return false;
    }

    if (m_filePath != path) {
        if (!m_filePath.isEmpty() && m_watcher->files().contains(m_filePath)) {
            m_watcher->removePath(m_filePath);
        }
        m_rearmTimer->stop();
        m_filePath = path;
        emit filePathChanged(m_filePath);
    }

    if (!m_filePath.isEmpty() && QFile::exists(m_filePath) && !m_watcher->files().contains(m_filePath)) {
        m_watcher->addPath(m_filePath);
    }

    m_lastKnownDiskContent = m_markdown;
    m_isDirty = false;
    emit dirtyChanged(false);

    return true;
}

void DocumentSession::zoomIn() {
    setZoomFactor(m_zoomFactor + 0.1);
}

void DocumentSession::zoomOut() {
    setZoomFactor(m_zoomFactor - 0.1);
}

void DocumentSession::resetZoom() {
    setZoomFactor(1.0);
}

void DocumentSession::flushOutline() {
    m_tocDebounceTimer->stop();
    auto headings = TocParser::extractHeadings(m_markdown);
    emit tocChanged(headings);
    flushTelemetry();
}

void DocumentSession::flushTelemetry() {
    m_telemetryDebounceTimer->stop();
    updateTelemetry();
}


void DocumentSession::onTocDebounceTimeout() {
    auto headings = TocParser::extractHeadings(m_markdown);
    emit tocChanged(headings);
}

void DocumentSession::onFileChanged(const QString& path) {
    if (path != m_filePath) {
        // Handle canonical path differences (e.g. /var vs /private/var on macOS)
        if (QFileInfo(path).canonicalFilePath() != QFileInfo(m_filePath).canonicalFilePath()) {
            return;
        }
    }

    if (!m_watcher->files().contains(m_filePath) || !QFile::exists(m_filePath)) {
        rearmWatcher();
    }

    m_watcherDebounceTimer->start(150);
}

void DocumentSession::rearmWatcher() {
    if (m_filePath.isEmpty()) return;
    if (m_watcher->files().contains(m_filePath)) {
        m_watcher->removePath(m_filePath);
    }
    if (QFile::exists(m_filePath)) {
        m_watcher->addPath(m_filePath);
    } else {
        m_rearmTimer->start(50);
    }
}

void DocumentSession::onRearmTimeout() {
    if (m_filePath.isEmpty()) return;
    if (QFile::exists(m_filePath)) {
        if (m_watcher->files().contains(m_filePath)) {
            m_watcher->removePath(m_filePath);
        }
        m_watcher->addPath(m_filePath);
        m_watcherDebounceTimer->start(150);
    } else {
        static int retries = 0;
        if (++retries < 10) {
            m_rearmTimer->start(50);
        } else {
            retries = 0;
        }
    }
}

void DocumentSession::onWatcherTimeout() {
    // Re-arm watcher to attach to fresh inode if atomic rename occurred
    if (!m_filePath.isEmpty() && QFile::exists(m_filePath)) {
        if (m_watcher->files().contains(m_filePath)) {
            m_watcher->removePath(m_filePath);
        }
        m_watcher->addPath(m_filePath);
    }

    if (m_filePath.isEmpty()) return;

    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (QFile::exists(m_filePath)) {
            m_watcherDebounceTimer->start(50);
        }
        return;
    }
    QString diskContent = QString::fromUtf8(file.readAll());
    file.close();

    // If disk matches what we loaded or saved, ignore self-echoes
    if (diskContent == m_lastKnownDiskContent || diskContent == m_markdown) {
        return;
    }

    // Disk changed externally
    if (m_isDirty) {
        emit diskConflictDetected();
    } else {
        m_markdown = diskContent;
        m_lastKnownDiskContent = diskContent;
        m_isDirty = false;
        m_tocDebounceTimer->stop();
        m_telemetryDebounceTimer->stop();
        auto headings = TocParser::extractHeadings(m_markdown);

        updateTelemetry();


        emit markdownChanged(m_markdown);
        emit dirtyChanged(false);
        emit tocChanged(headings);
        emit diskReloaded();
    }
}

void DocumentSession::updateTelemetry() {
    m_telemetryMetrics = TelemetryEngine::calculateMetrics(m_markdown);
    m_wordCount = m_telemetryMetrics.wordCount;
    m_readingTimeMinutes = m_telemetryMetrics.readingTimeMinutes;
    emit telemetryUpdated(m_telemetryMetrics);
    emit metricsReported(m_wordCount, m_readingTimeMinutes);
}


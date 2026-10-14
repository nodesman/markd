#pragma once

#include <QApplication>
#include <QSignalSpy>
#include <QTemporaryFile>
#include <QDir>
#include <QFile>
#include <QDateTime>
#include <QTest>
#include <cassert>
#include <iostream>
#include <cmath>
#include <vector>
#include "core/DocumentSession.h"
#include "parser/TocItem.h"

inline void testDocumentSession(QApplication& app) {
    Q_UNUSED(app);

    // 1. initializes default session state
    {
        DocumentSession session;
        assert(session.markdown().isEmpty());
        assert(session.selectedHeading().isEmpty());
        assert(session.activeHeading().isEmpty());
        assert(session.theme() == "obsidian");
        assert(std::abs(session.zoomFactor() - 1.0) < 0.001);
        assert(!session.isDirty());
        assert(session.filePath().isEmpty());
        assert(session.mode() == 0);

        QSignalSpy headingSpy(&session, &DocumentSession::activeHeadingChanged);
        session.reportActiveHeading("sec-1");
        assert(headingSpy.count() == 1);
        assert(session.activeHeading() == "sec-1");

        // Duplicate reportActiveHeading suppressed
        session.reportActiveHeading("sec-1");
        assert(headingSpy.count() == 1);

        QSignalSpy metricsSpy(&session, &DocumentSession::metricsReported);
        session.reportMetrics(450, 3);
        assert(metricsSpy.count() == 1);
        assert(session.wordCount() == 450);
        assert(session.readingTimeMinutes() == 3);

        QSignalSpy tocSpy(&session, &DocumentSession::tocUpdated);
        session.updateTOC("[{\"id\":\"sec-1\"}]");
        assert(tocSpy.count() == 1);
        assert(tocSpy.takeFirst().at(0).toString() == "[{\"id\":\"sec-1\"}]");
    }

    // 2. loads document headlessly from disk
    {
        QTemporaryFile tempFile(QDir::tempPath() + "/markd_session_test_XXXXXX.md");
        tempFile.setAutoRemove(true);
        assert(tempFile.open());
        QString content = "# Architecture Document\n\nOverview of the system.\n\n## Core Invariants\n\nPrinciples.";
        tempFile.write(content.toUtf8());
        tempFile.close();

        DocumentSession session;
        QSignalSpy loadedSpy(&session, &DocumentSession::fileLoaded);
        QSignalSpy mdSpy(&session, &DocumentSession::markdownChanged);
        QSignalSpy dirtySpy(&session, &DocumentSession::dirtyChanged);
        QSignalSpy tocSpy(&session, &DocumentSession::tocChanged);

        bool ok = session.loadFile(tempFile.fileName());
        assert(ok);
        assert(session.filePath() == tempFile.fileName());
        assert(session.markdown() == content);
        assert(!session.isDirty());

        assert(loadedSpy.count() == 1);
        assert(mdSpy.count() == 1);
        assert(dirtySpy.count() == 1);
        assert(dirtySpy.takeFirst().at(0).toBool() == false);
        assert(tocSpy.count() == 1);

        auto headings = tocSpy.takeFirst().at(0).value<std::vector<TocItem>>();
        assert(headings.size() == 2);
        assert(headings[0].id == "architecture-document");
        assert(headings[1].id == "core-invariants");
    }

    // 3. signals buffer mutation and marks dirty flag
    {
        QTemporaryFile tempFile(QDir::tempPath() + "/markd_mutate_test_XXXXXX.md");
        tempFile.setAutoRemove(true);
        assert(tempFile.open());
        tempFile.write("# Initial");
        tempFile.close();

        DocumentSession session;
        session.loadFile(tempFile.fileName());

        QSignalSpy mdSpy(&session, &DocumentSession::markdownChanged);
        QSignalSpy dirtySpy(&session, &DocumentSession::dirtyChanged);

        session.setMarkdown("# Modified Content");
        assert(session.markdown() == "# Modified Content");
        assert(session.isDirty());
        assert(mdSpy.count() == 1);
        assert(dirtySpy.count() == 1);
        assert(dirtySpy.at(0).at(0).toBool() == true);

        // Save clears dirty flag
        bool saved = session.save();
        assert(saved);
        assert(!session.isDirty());
        assert(dirtySpy.count() == 2);
        assert(dirtySpy.at(1).at(0).toBool() == false);

        QFile verifyFile(tempFile.fileName());
        assert(verifyFile.open(QIODevice::ReadOnly));
        assert(verifyFile.readAll() == "# Modified Content");
    }

    // 4. debounces outline extraction on rapid buffer mutations
    {
        DocumentSession session;
        QSignalSpy tocSpy(&session, &DocumentSession::tocChanged);

        // Rapid sequence under 150ms window resets timer
        session.setMarkdown("# First Mutation");
        QTest::qWait(75);
        assert(tocSpy.count() == 0);

        session.setMarkdown("# Second Mutation");
        QTest::qWait(75);
        assert(tocSpy.count() == 0);

        session.setMarkdown("# Final Mutation\n\n## Sub Heading");
        assert(tocSpy.count() == 0);

        // flushOutline forces synchronous extraction
        session.flushOutline();
        assert(tocSpy.count() == 1);
        auto headings = tocSpy.takeFirst().at(0).value<std::vector<TocItem>>();
        assert(headings.size() == 2);
        assert(headings[0].title == "Final Mutation");

        // Quiet period triggers debounced outline extraction
        session.setMarkdown("# Auto Debounced");
        QTest::qWait(200);
        assert(tocSpy.count() == 1);
        auto autoHeadings = tocSpy.takeFirst().at(0).value<std::vector<TocItem>>();
        assert(autoHeadings.size() == 1);
        assert(autoHeadings[0].title == "Auto Debounced");
    }

    // 5. clamps zoom factor within bounds (0.25x - 3.0x)
    {
        DocumentSession session;
        assert(std::abs(session.zoomFactor() - 1.0) < 0.001);

        session.zoomIn();
        assert(std::abs(session.zoomFactor() - 1.1) < 0.001);

        session.zoomOut();
        assert(std::abs(session.zoomFactor() - 1.0) < 0.001);

        for (int i = 0; i < 35; ++i) session.zoomIn();
        assert(std::abs(session.zoomFactor() - 3.0) < 0.001);

        for (int i = 0; i < 40; ++i) session.zoomOut();
        assert(std::abs(session.zoomFactor() - 0.25) < 0.001);

        session.resetZoom();
        assert(std::abs(session.zoomFactor() - 1.0) < 0.001);
    }

    // 6. consolidates rapid filesystem event bursts
    {
        QTemporaryFile tempFile(QDir::tempPath() + "/markd_burst_test_XXXXXX.md");
        tempFile.setAutoRemove(true);
        assert(tempFile.open());
        tempFile.write("# Initial Burst Text");
        tempFile.close();

        DocumentSession session;
        session.loadFile(tempFile.fileName());

        QSignalSpy reloadSpy(&session, &DocumentSession::diskReloaded);

        // Rapid write burst
        for (int i = 1; i <= 3; ++i) {
            QFile f(tempFile.fileName());
            assert(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
            f.write(QString("# Burst Write %1\n").arg(i).toUtf8());
            f.flush();
            f.close();
            QTest::qWait(25);
        }

        assert(reloadSpy.count() == 0); // suppressed during burst
        for (int i = 0; i < 20 && reloadSpy.count() < 1; ++i) {
            QTest::qWait(50);
        }
        assert(reloadSpy.count() == 1);
        assert(session.markdown().contains("# Burst Write 3"));
    }

    // 7. re-arms file watcher across atomic rename operations
    {
        QString dirPath = QDir::tempPath();
        QString targetPath = dirPath + QString("/markd_atomic_%1.md").arg(QDateTime::currentMSecsSinceEpoch());
        {
            QFile f(targetPath);
            assert(f.open(QIODevice::WriteOnly));
            f.write("# Atomic Base");
            f.close();
        }

        DocumentSession session;
        session.loadFile(targetPath);

        QSignalSpy reloadSpy(&session, &DocumentSession::diskReloaded);

        // Simulate Vim atomic write: write swap then rename over target
        QString swapPath = targetPath + ".swp";
        {
            QFile swap(swapPath);
            assert(swap.open(QIODevice::WriteOnly));
            swap.write("# Atomic Replacement 1");
            swap.flush();
            swap.close();
        }
        QFile::remove(targetPath);
        QFile::rename(swapPath, targetPath);
        for (int i = 0; i < 20 && reloadSpy.count() < 1; ++i) {
            QTest::qWait(100);
        }
        assert(reloadSpy.count() >= 1);
        assert(session.markdown() == "# Atomic Replacement 1");

        // Subsequent write to verify re-arming persisted
        {
            QFile f(targetPath);
            assert(f.open(QIODevice::WriteOnly));
            f.write("# Atomic Replacement 2");
            f.flush();
            f.close();
        }
        // Wait for debounce timer (150ms) plus OS event delivery latency
        for (int i = 0; i < 20 && reloadSpy.count() < 2; ++i) {
            QTest::qWait(100);
        }
        assert(reloadSpy.count() >= 2);
        assert(session.markdown() == "# Atomic Replacement 2");

        QFile::remove(targetPath);
    }

    // 8. shields dirty buffer from external disk overwrite
    {
        QTemporaryFile tempFile(QDir::tempPath() + "/markd_conflict_test_XXXXXX.md");
        tempFile.setAutoRemove(true);
        assert(tempFile.open());
        tempFile.write("# Clean Content");
        tempFile.close();

        DocumentSession session;
        session.loadFile(tempFile.fileName());

        // Mutate buffer to dirty state
        session.setMarkdown("# Dirty User Edits");
        assert(session.isDirty());

        QSignalSpy conflictSpy(&session, &DocumentSession::diskConflictDetected);
        QSignalSpy reloadSpy(&session, &DocumentSession::diskReloaded);

        // External overwrite on disk
        {
            QFile f(tempFile.fileName());
            assert(f.open(QIODevice::WriteOnly));
            f.write("# External Conflict Intrusion");
            f.close();
        }

        for (int i = 0; i < 20 && conflictSpy.count() < 1; ++i) {
            QTest::qWait(50);
        }

        assert(conflictSpy.count() == 1);
        assert(reloadSpy.count() == 0);
        assert(session.markdown() == "# Dirty User Edits"); // zero buffer overwrite
        assert(session.isDirty());
    }

    // 9. discriminates local edits from external disk conflicts
    {
        QTemporaryFile tempFile(QDir::tempPath() + "/markd_local_save_XXXXXX.md");
        tempFile.setAutoRemove(true);
        assert(tempFile.open());
        tempFile.write("# Initial Content");
        tempFile.close();

        DocumentSession session;
        session.loadFile(tempFile.fileName());

        session.setMarkdown("# Save Me");
        session.save();

        QSignalSpy conflictSpy(&session, &DocumentSession::diskConflictDetected);

        // User continues typing immediately after save while watcher debounce is pending
        session.setMarkdown("# Save Me - Continuing Edits");
        assert(session.isDirty());

        QTest::qWait(250);

        // Must NOT fire false conflict alert
        assert(conflictSpy.count() == 0);
    }

    std::cout << "PASS: William Zinsser DocumentSession domain tests passed!\n";
}

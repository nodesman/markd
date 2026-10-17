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
#include "Zinsser.h"
#include "core/DocumentSession.h"
#include "parser/TocItem.h"

inline void testDocumentSession(QApplication& app) {
    Q_UNUSED(app);
    using Zinsser::describe;
    using Zinsser::it;

    describe("Document session initial state and reactive metrics", [&]() {
        it("initializes default session state and records active heading and reading metrics", [&]() {
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
        });

        it("mutates document theme, file path, mode, and dirty states directly", [&]() {
            DocumentSession session;
            QSignalSpy themeSpy(&session, &DocumentSession::themeChanged);
            session.setTheme("aurora");
            assert(session.theme() == "aurora");
            assert(themeSpy.count() == 1);
            session.setTheme("aurora");
            assert(themeSpy.count() == 1);

            QSignalSpy dirtySpy(&session, &DocumentSession::dirtyChanged);
            session.setDirty(true);
            assert(session.isDirty());
            assert(dirtySpy.count() == 1);
            session.setDirty(true);
            assert(dirtySpy.count() == 1);

            QSignalSpy modeSpy(&session, &DocumentSession::modeChanged);
            session.setMode(1);
            assert(session.mode() == 1);
            assert(modeSpy.count() == 1);
            session.setMode(1);
            assert(modeSpy.count() == 1);

            QTemporaryFile tempFile(QDir::tempPath() + "/markd_path_test_XXXXXX.md");
            assert(tempFile.open());
            tempFile.close();

            QSignalSpy pathSpy(&session, &DocumentSession::filePathChanged);
            session.setFilePath(tempFile.fileName());
            assert(session.filePath() == tempFile.fileName());
            assert(pathSpy.count() == 1);

            session.setFilePath(tempFile.fileName());
            assert(pathSpy.count() == 1);
        });
    });

    describe("Headless file persistence and buffer mutation tracking", [&]() {
        it("loads markdown document headlessly from disk and initializes document outline", [&]() {
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
        });

        it("tracks dirty state transitions across buffer edits and clears flag on save", [&]() {
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

            bool saved = session.save();
            assert(saved);
            assert(!session.isDirty());
            assert(dirtySpy.count() == 2);
            assert(dirtySpy.at(1).at(0).toBool() == false);

            QFile verifyFile(tempFile.fileName());
            assert(verifyFile.open(QIODevice::ReadOnly));
            assert(verifyFile.readAll() == "# Modified Content");

            // Empty session save returns false
            DocumentSession emptySession;
            assert(!emptySession.save());
            assert(!emptySession.saveAs(""));
            assert(!emptySession.saveAs("/invalid/nonexistent/dir/cannot_write.md"));

            // Loading nonexistent file returns false
            assert(!session.loadFile("/invalid/nonexistent/file_path_12345.md"));

            // saveAs to distinct target path updates filePath and watcher
            QTemporaryFile secondFile(QDir::tempPath() + "/markd_saveas_test_XXXXXX.md");
            secondFile.setAutoRemove(true);
            assert(secondFile.open());
            secondFile.close();

            assert(session.saveAs(secondFile.fileName()));
            assert(session.filePath() == secondFile.fileName());
        });
    });

    describe("Debounced outline extraction and zoom clamping", [&]() {
        it("debounces rapid outline extractions and supports synchronous flush calls", [&]() {
            DocumentSession session;
            QSignalSpy tocSpy(&session, &DocumentSession::tocChanged);

            session.setMarkdown("# First Mutation");
            QTest::qWait(75);
            assert(tocSpy.count() == 0);

            session.setMarkdown("# Second Mutation");
            QTest::qWait(75);
            assert(tocSpy.count() == 0);

            session.setMarkdown("# Final Mutation\n\n## Sub Heading");
            assert(tocSpy.count() == 0);

            session.flushOutline();
            assert(tocSpy.count() == 1);
            auto headings = tocSpy.takeFirst().at(0).value<std::vector<TocItem>>();
            assert(headings.size() == 2);
            assert(headings[0].title == "Final Mutation");

            session.setMarkdown("# Auto Debounced");
            for (int i = 0; i < 8 && tocSpy.count() == 0; ++i) {
                QTest::qWait(50);
            }
            assert(tocSpy.count() == 1);
            auto autoHeadings = tocSpy.takeFirst().at(0).value<std::vector<TocItem>>();
            assert(autoHeadings.size() == 1);
            assert(autoHeadings[0].title == "Auto Debounced");
        });

        it("clamps zoom factor strictly between quarter and three-times scale", [&]() {
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
        });
    });

    describe("Filesystem monitoring and external conflict protection", [&]() {
        it("consolidates rapid filesystem event bursts into single disk reload", [&]() {
            QTemporaryFile tempFile(QDir::tempPath() + "/markd_burst_test_XXXXXX.md");
            tempFile.setAutoRemove(true);
            assert(tempFile.open());
            tempFile.write("# Initial Burst Text");
            tempFile.close();

            DocumentSession session;
            session.loadFile(tempFile.fileName());

            QSignalSpy reloadSpy(&session, &DocumentSession::diskReloaded);

            for (int i = 1; i <= 3; ++i) {
                QFile f(tempFile.fileName());
                assert(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
                f.write(QString("# Burst Write %1\n").arg(i).toUtf8());
                f.flush();
                f.close();
                QTest::qWait(25);
            }

            assert(reloadSpy.count() == 0);
            for (int i = 0; i < 20 && reloadSpy.count() < 1; ++i) {
                QTest::qWait(50);
            }
            assert(reloadSpy.count() == 1);
            assert(session.markdown().contains("# Burst Write 3"));
        });

        it("re-arms filesystem watcher across atomic rename operations", [&]() {
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

            {
                QFile f(targetPath);
                assert(f.open(QIODevice::WriteOnly));
                f.write("# Atomic Replacement 2");
                f.flush();
                f.close();
            }
            for (int i = 0; i < 20 && reloadSpy.count() < 2; ++i) {
                QTest::qWait(100);
            }
            assert(reloadSpy.count() >= 2);
            assert(session.markdown() == "# Atomic Replacement 2");

            QFile::remove(targetPath);
        });

        it("shields dirty buffer from external disk overwrite and raises conflict signal", [&]() {
            QTemporaryFile tempFile(QDir::tempPath() + "/markd_conflict_test_XXXXXX.md");
            tempFile.setAutoRemove(true);
            assert(tempFile.open());
            tempFile.write("# Clean Content");
            tempFile.close();

            DocumentSession session;
            session.loadFile(tempFile.fileName());

            session.setMarkdown("# Dirty User Edits");
            assert(session.isDirty());

            QSignalSpy conflictSpy(&session, &DocumentSession::diskConflictDetected);
            QSignalSpy reloadSpy(&session, &DocumentSession::diskReloaded);

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
            assert(session.markdown() == "# Dirty User Edits");
            assert(session.isDirty());
        });

        it("discriminates immediate local typing from external disk conflicts", [&]() {
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

            session.setMarkdown("# Save Me - Continuing Edits");
            assert(session.isDirty());

            QTest::qWait(250);
            assert(conflictSpy.count() == 0);
        });

        it("handles watcher rearming, canonical path dispatch, and empty file persistence guards", [&]() {
            DocumentSession emptySession;
            assert(!emptySession.save());
            assert(!emptySession.saveAs(""));

            QTemporaryFile tempFile(QDir::tempPath() + "/markd_rearm_XXXXXX.md");
            assert(tempFile.open());
            tempFile.write("# Content");
            tempFile.close();

            DocumentSession session;
            assert(session.loadFile(tempFile.fileName()));

            // Invoking onRearmTimeout when file exists
            QMetaObject::invokeMethod(&session, "onRearmTimeout");

            // Canonical path mismatch in onFileChanged
            QMetaObject::invokeMethod(&session, "onFileChanged", Q_ARG(QString, QString("/nonexistent/arbitrary/path.md")));

            // Empty watcher timeout trigger
            QMetaObject::invokeMethod(&emptySession, "onWatcherTimeout");
        });
    });
}

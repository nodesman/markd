#pragma once

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileOpenEvent>
#include <QLocalServer>
#include <QPalette>
#include <QSignalSpy>
#include <QStackedLayout>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QTest>
#include <QTextDocument>
#include <QUrl>
#include <QUuid>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineView>
#include <cassert>
#include <cmath>
#include "Zinsser.h"
#include "app/CommandLineOptions.h"
#include "app/MarkdApplication.h"
#include "app/SingleInstanceCoordinator.h"
#include "core/DocumentSession.h"
#include "core/TelemetryEngine.h"
#include "parser/TocParser.h"
#include "ui/InstantPreviewSurface.h"
#include "ui/MainWindow.h"
#include "ui/PreviewView.h"

inline void testStartupPerformanceAndHydration(MarkdApplication& app) {
    using Zinsser::describe;
    using Zinsser::it;

    describe("Instant cold-start surface, deferred web engine hydration and startup coalescing", [&]() {
        it("paints obsidian instant preview surface on show while deferring web engine view allocation", [&]() {
            QTemporaryFile tempFile(QDir::tempPath() + "/markd_instant_surface_XXXXXX.md");
            assert(tempFile.open());
            tempFile.write("# Instant Title\n\nBody paragraph with **strong** prose.\n\n## Second Section\n\nDetails.\n");
            tempFile.close();

            MainWindow win(tempFile.fileName(), MainWindow::Mode::Preview, true);
            assert(win.viewportStack() != nullptr);
            assert(win.viewportStack()->count() == 2);
            assert(win.previewView() != nullptr);
            assert(win.previewView()->bridge() != nullptr);
            assert(win.previewView()->channel() != nullptr);
            assert(!win.previewView()->isWebEngineInitialized());
            assert(win.previewView()->isShowingInstantSurface());
            assert(win.findChild<QWebEngineView*>("previewView") == nullptr);

            InstantPreviewSurface* instant = win.previewView()->instantSurface();
            assert(instant != nullptr);
            assert(instant->palette().color(QPalette::Base) == QColor("#050505"));
            assert(instant->palette().color(QPalette::Text) == QColor("#f0f6fc"));
            assert(instant->toPlainText().contains("Instant Title"));

            win.show();
            assert(!win.previewView()->isWebEngineInitialized());
            assert(win.findChild<QWebEngineView*>("previewView") == nullptr);
        });

        it("layers web engine beneath instant surface in stack-all layout and swaps on document rendered", [&]() {
            QTemporaryFile tempFile(QDir::tempPath() + "/markd_stack_all_XXXXXX.md");
            assert(tempFile.open());
            tempFile.write("# Layered Heading\n\nParagraph text.\n");
            tempFile.close();

            MainWindow win(tempFile.fileName(), MainWindow::Mode::Preview, true);
            win.show();
            app.processEvents();

            PreviewView* preview = win.previewView();
            assert(preview->isWebEngineInitialized());
            assert(win.findChild<QWebEngineView*>("previewView") != nullptr);

            auto* stackLayout = qobject_cast<QStackedLayout*>(preview->layout());
            assert(stackLayout != nullptr);
            assert(stackLayout->stackingMode() == QStackedLayout::StackAll);
            assert(preview->isShowingInstantSurface());
            assert(!preview->instantSurface()->isHidden());

            emit preview->bridge()->documentRendered(1);
            app.processEvents();

            assert(!preview->isShowingInstantSurface());
            assert(preview->instantSurface()->isHidden());
        });

        it("preserves scroll ratio zoom factor find query and active focus across web engine hydration", [&]() {
            QTemporaryFile tempFile(QDir::tempPath() + "/markd_hydration_state_XXXXXX.md");
            assert(tempFile.open());
            QString markdown = "# Hydration Root\n\nneedle_target first match\n\n";
            for (int i = 0; i < 80; ++i) {
                markdown += QString("Paragraph filler line %1 for vertical scroll testing.\n\n").arg(i);
            }
            markdown += "## Target Anchor\n\nneedle_target second match\n";
            tempFile.write(markdown.toUtf8());
            tempFile.close();

            MainWindow win(tempFile.fileName(), MainWindow::Mode::Preview, true);
            win.resize(960, 520);

            PreviewView* preview = win.previewView();
            preview->focusPreviewSurface();
            win.session()->setZoomFactor(1.4);
            assert(std::abs(preview->zoomFactor() - 1.4) < 0.01);

            preview->instantSurface()->resize(900, 420);
            preview->instantSurface()->setVerticalScrollRatio(0.5);
            assert(preview->instantSurface()->verticalScrollRatio() >= 0.0);

            int reportedActive = -1;
            int reportedTotal = -1;
            QObject::connect(&win, &MainWindow::findResult, [&](int active, int total) {
                reportedActive = active;
                reportedTotal = total;
            });

            win.findText("needle_target", true, false);
            assert(!preview->isWebEngineInitialized());
            assert(reportedActive == 1);
            assert(reportedTotal == 2);

            preview->selectAllContent();
            preview->copySelection();
            preview->scrollToHeading("target-anchor");
            assert(!preview->isWebEngineInitialized());

            win.show();
            win.activateWindow();
            preview->focusPreviewSurface();
            app.processEvents();

            assert(preview->isWebEngineInitialized());
            assert(std::abs(preview->webView()->zoomFactor() - 1.4) < 0.01);

            emit preview->bridge()->bridgeInitialized();
            emit preview->bridge()->documentRendered(2);
            app.processEvents();

            assert(!preview->isShowingInstantSurface());
            assert(preview->hasPreviewFocus());
            preview->selectAllContent();
            preview->copySelection();
        });

        it("routes external http links to desktop services and internal anchors within instant surface", [&]() {
            QTemporaryDir tempDir;
            assert(tempDir.isValid());
            const QString docPath = tempDir.filePath("links.md");

            InstantPreviewSurface surface;
            surface.resize(800, 400);

            QList<QUrl> openedExternalUrls;
            surface.setExternalUrlHandler([&](const QUrl& url) {
                openedExternalUrls.append(url);
            });

            QSignalSpy externalSpy(&surface, &InstantPreviewSurface::externalLinkActivated);
            QSignalSpy internalSpy(&surface, &InstantPreviewSurface::internalAnchorActivated);

            const QString md =
                "# First Heading\n\n"
                "[Jump](#second-heading)\n\n"
                "[External](https://example.com/docs)\n\n"
                "[Contact](mailto:team@example.com)\n\n"
                "![Diagram](diagram.png)\n\n"
                "## Second Heading\n\n"
                "Target section text.\n";
            surface.renderMarkdown(md, docPath);

            const QString baseUrl = surface.document()->metaInformation(QTextDocument::DocumentUrl);
            assert(baseUrl.startsWith("file://"));

            emit surface.anchorClicked(QUrl(QStringLiteral("#second-heading")));
            assert(internalSpy.count() == 1);
            assert(openedExternalUrls.isEmpty());

            emit surface.anchorClicked(QUrl(QStringLiteral("https://example.com/docs")));
            emit surface.anchorClicked(QUrl(QStringLiteral("mailto:team@example.com")));
            emit surface.anchorClicked(QUrl(QStringLiteral("javascript:alert(1)")));

            assert(externalSpy.count() == 2);
            assert(openedExternalUrls.size() == 2);
            assert(openedExternalUrls.at(0) == QUrl(QStringLiteral("https://example.com/docs")));
            assert(openedExternalUrls.at(1) == QUrl(QStringLiteral("mailto:team@example.com")));
        });

        it("shares off-the-record web engine profile with disk cache and persistent cookies disabled", [&]() {
            QWebEngineProfile* profile = PreviewView::sharedPreviewProfile();
            assert(profile != nullptr);
            assert(profile == PreviewView::sharedPreviewProfile());
            assert(profile->isOffTheRecord());
            assert(profile->httpCacheType() == QWebEngineProfile::NoCache);
            assert(profile->persistentCookiesPolicy() == QWebEngineProfile::NoPersistentCookies);
            assert(!profile->isSpellCheckEnabled());

            PreviewView view;
            view.ensureWebEngineInitialized();
            assert(view.webView() != nullptr);
            assert(view.webView()->page()->profile() == profile);
        });

        it("injects window document session directly into preview view with single session allocation", [&]() {
            QTemporaryFile tempFile(QDir::tempPath() + "/markd_single_session_XXXXXX.md");
            assert(tempFile.open());
            tempFile.write("# Single Session\n");
            tempFile.close();

            DocumentSession::resetAllocationCount();
            MainWindow win(tempFile.fileName(), MainWindow::Mode::Preview, true);
            assert(DocumentSession::allocationCount() == 1);
            assert(win.previewView()->session() == win.session());

            PreviewView standalone;
            assert(standalone.session() != nullptr);
            assert(DocumentSession::allocationCount() == 2);
        });

        it("defers editor plain text layout in preview mode and synchronizes cleanly on edit switch", [&]() {
            QTemporaryFile crlfFile(QDir::tempPath() + "/markd_lazy_editor_XXXXXX.md");
            assert(crlfFile.open());
            crlfFile.write("# Lazy Editor Heading\r\n\r\nBody line with CRLF endings.\r\n");
            crlfFile.close();

            MainWindow win(crlfFile.fileName(), MainWindow::Mode::Preview, true);
            assert(win.editorPlainTextSyncCount() == 0);
            assert(win.documentBuffer().contains("# Lazy Editor Heading"));
            assert(win.editorPlainTextSyncCount() == 0);
            assert(!win.session()->isDirty());

            win.setMode(MainWindow::Mode::Edit);
            assert(win.editorPlainTextSyncCount() == 1);
            assert(!win.session()->isDirty());
            assert(win.editor()->toPlainText().contains("# Lazy Editor Heading"));

            MainWindow winDirectAccessor(crlfFile.fileName(), MainWindow::Mode::Preview, true);
            assert(winDirectAccessor.editorPlainTextSyncCount() == 0);
            assert(winDirectAccessor.editor()->toPlainText().contains("# Lazy Editor Heading"));
            assert(winDirectAccessor.editorPlainTextSyncCount() == 1);
            assert(!winDirectAccessor.session()->isDirty());
        });

        it("emits cached outline and telemetry in single pass when flushing clean document session", [&]() {
            QTemporaryFile tempFile(QDir::tempPath() + "/markd_single_pass_XXXXXX.md");
            assert(tempFile.open());
            tempFile.write("# Pass One\n\nAlpha beta gamma delta.\n\n## Subheading\n\nEpsilon.\n");
            tempFile.close();

            TocParser::resetExtractCallCount();
            TelemetryEngine::resetCalculateCallCount();

            MainWindow win(tempFile.fileName(), MainWindow::Mode::Preview, true);
            assert(TocParser::extractCallCount() == 1);
            assert(TelemetryEngine::calculateCallCount() == 1);

            QSignalSpy tocSpy(win.session(), &DocumentSession::tocChanged);
            QSignalSpy telSpy(win.session(), &DocumentSession::telemetryUpdated);
            win.session()->flushOutline();

            assert(tocSpy.count() == 1);
            assert(telSpy.count() == 1);
            assert(TocParser::extractCallCount() == 1);
            assert(TelemetryEngine::calculateCallCount() == 1);

            win.session()->setMarkdown("# Pass Two\n\nUpdated body tokens.\n");
            win.session()->flushOutline();
            assert(TocParser::extractCallCount() == 2);
            assert(TelemetryEngine::calculateCallCount() == 2);
        });

        it("coalesces macos file-open event including url fallback into single preview window", [&]() {
            app.closeAllWindows();
            app.processEvents();
            assert(app.windows().isEmpty());

            QTemporaryDir tempDir;
            assert(tempDir.isValid());
            const QString targetPath = tempDir.filePath("clicked document.md");
            {
                QFile f(targetPath);
                assert(f.open(QIODevice::WriteOnly));
                f.write("# Clicked From Finder\n\nOpened without untitled flash.\n");
            }

            char arg0[] = "markd";
            char* rawArgv[] = {arg0, nullptr};
            CommandLineOptions emptyOpts = CommandLineOptions::parseFromArgv(1, rawArgv);
            assert(emptyOpts.parseSuccess);
            assert(!emptyOpts.hasFile);

            app.launchInitialWindows(emptyOpts, true);
            assert(app.windows().isEmpty());

            const QString encodedPath = QString::fromUtf8(QUrl::toPercentEncoding(targetPath));
            QFileOpenEvent urlOpenEvent(QUrl(QStringLiteral("markd://open?path=%1").arg(encodedPath)));
            QCoreApplication::sendEvent(&app, &urlOpenEvent);

            QTest::qWait(55);
            assert(app.windows().size() == 1);
            MainWindow* opened = app.windows().first();
            assert(opened->currentMode() == MainWindow::Mode::Preview);
            assert(opened->isSidebarCollapsed());
            assert(QFileInfo(opened->currentFilePath()).canonicalFilePath() == QFileInfo(targetPath).canonicalFilePath());

            const QString testSocket = QStringLiteral("markd-test-ipc-%1")
                .arg(QUuid::createUuid().toString(QUuid::Id128).left(12));
            QLocalServer::removeServer(testSocket);
            assert(!SingleInstanceCoordinator::hasCandidateServerSocket(testSocket));

            SingleInstanceCoordinator primary(testSocket);
            assert(primary.activateOrListen({}, false) == SingleInstanceCoordinator::Role::Primary);
            assert(primary.isPrimary());
            assert(SingleInstanceCoordinator::hasCandidateServerSocket(testSocket));

            QSignalSpy ipcSpy(&primary, &SingleInstanceCoordinator::activationRequested);
            SingleInstanceCoordinator secondary(testSocket);
            assert(secondary.activateOrListen({targetPath}, false) == SingleInstanceCoordinator::Role::SecondaryForwarded);
            app.processEvents();
            assert(ipcSpy.count() == 1);

            app.closeAllWindows();
            app.processEvents();
        });

        it("spawns fallback untitled edit window when startup coalescing expires or file-open path fails", [&]() {
            app.closeAllWindows();
            app.processEvents();

            char arg0[] = "markd";
            char* rawArgv[] = {arg0, nullptr};
            CommandLineOptions emptyOpts = CommandLineOptions::parseFromArgv(1, rawArgv);

            app.launchInitialWindows(emptyOpts, true);
            assert(app.windows().isEmpty());
            QTest::qWait(60);
            assert(app.windows().size() == 1);
            assert(app.windows().first()->currentMode() == MainWindow::Mode::Edit);
            assert(app.windows().first()->currentFilePath().isEmpty());

            app.closeAllWindows();
            app.processEvents();

            app.launchInitialWindows(emptyOpts, true);
            QFileOpenEvent malformedEvent(QUrl(QStringLiteral("markd://open")));
            QCoreApplication::sendEvent(&app, &malformedEvent);
            assert(app.windows().size() == 1);
            assert(app.windows().first()->currentMode() == MainWindow::Mode::Edit);

            app.closeAllWindows();
            app.processEvents();
        });

        it("omits blocking mermaid script from template head and removes duplicate qrc aliases", [&]() {
            QFile templateFile(":/web/template.html");
            assert(templateFile.open(QIODevice::ReadOnly | QIODevice::Text));
            const QString html = QString::fromUtf8(templateFile.readAll());
            templateFile.close();

            assert(!html.contains("vendor/mermaid/mermaid.min.js"));
            assert(QFile::exists(":/web/vendor/mermaid/mermaid.min.js"));
            assert(!QFile::exists(":/web/js/mermaid.min.js"));
            assert(!QFile::exists(":/web/css/style.css"));
            assert(!QFile::exists(":/web/styles/style.css"));
            assert(!QFile::exists(":/web/css/obsidian.css"));
        });

        it("deduplicates canonical file paths across launchInitialWindows and empty IPC requests", [&]() {
            app.closeAllWindows();
            app.processEvents();

            assert(SingleInstanceCoordinator::defaultSocketName().startsWith("markd-ipc-"));

            const QString testSocket = QStringLiteral("markd-empty-ipc-%1")
                .arg(QUuid::createUuid().toString(QUuid::Id128).left(12));
            QLocalServer::removeServer(testSocket);
            SingleInstanceCoordinator primary(testSocket);
            assert(primary.socketName() == testSocket);
            assert(primary.activateOrListen({}, false) == SingleInstanceCoordinator::Role::Primary);

            QSignalSpy emptyIpcSpy(&primary, &SingleInstanceCoordinator::emptyWindowRequested);
            SingleInstanceCoordinator secondaryEmpty(testSocket);
            assert(secondaryEmpty.activateOrListen({}, true) == SingleInstanceCoordinator::Role::SecondaryForwarded);
            app.processEvents();
            assert(emptyIpcSpy.count() == 1);

            QTemporaryDir tempDir;
            assert(tempDir.isValid());
            const QString targetPath = tempDir.filePath("canonical_doc.md");
            {
                QFile f(targetPath);
                assert(f.open(QIODevice::WriteOnly));
                f.write("# Canonical Dedup\n\nBody.\n");
            }
            const QString aliasPath = tempDir.path() + "/./canonical_doc.md";

            QByteArray arg0("markd");
            QByteArray arg1 = targetPath.toUtf8();
            char* fileArgv[] = {arg0.data(), arg1.data(), nullptr};
            CommandLineOptions fileOpts = CommandLineOptions::parseFromArgv(2, fileArgv);
            assert(fileOpts.hasFile);

            app.launchInitialWindows(fileOpts, false);
            assert(app.windows().size() == 1);
            MainWindow* firstWin = app.windows().first();
            assert(app.activeOrLastWindow() == firstWin);

            const QList<MainWindow*> batch = app.openFilesInWindows({targetPath, aliasPath});
            assert(batch.size() == 1);
            assert(batch.first() == firstWin);
            assert(app.windows().size() == 1);

            app.closeAllWindows();
            app.processEvents();
            assert(app.activeOrLastWindow() == nullptr);

            int ipcArgc = 1;
            char* emptyArgv[] = {arg0.data(), nullptr};
            QLocalServer::removeServer(SingleInstanceCoordinator::defaultSocketName());
            assert(!SingleInstanceCoordinator::tryForwardToRunningInstance(ipcArgc, emptyArgv, {}, true));

            CommandLineOptions emptyOpts = CommandLineOptions::parseFromArgv(1, emptyArgv);
            app.launchInitialWindows(emptyOpts, false);
            assert(app.windows().size() == 1);
            assert(app.windows().first()->currentMode() == MainWindow::Mode::Edit);

            DocumentSession preloaded;
            QString tallDoc = QStringLiteral("# Preloaded\n\n");
            for (int i = 0; i < 60; ++i) {
                tallDoc += QStringLiteral("Paragraph %1 with overflowing lines.\n\n").arg(i);
            }
            preloaded.setMarkdown(tallDoc);
            PreviewView preloadedPreview(&preloaded);
            preloadedPreview.resize(420, 220);
            preloadedPreview.show();
            app.processEvents();
            preloadedPreview.instantSurface()->setVerticalScrollRatio(0.5);
            assert(preloadedPreview.instantSurface()->verticalScrollRatio() > 0.2);
            preloadedPreview.instantSurface()->renderMarkdown(QString());
            assert(preloadedPreview.instantSurface()->toPlainText().isEmpty());

            app.closeAllWindows();
            app.processEvents();
        });
    });
}

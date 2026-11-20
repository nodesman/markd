#pragma once

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileOpenEvent>
#include <QLocalServer>
#include <QPalette>
#include <QJsonObject>
#include <QTextBrowser>
#include <QLabel>
#include "core/StartupPhaseTrace.h"
#include <QRegularExpression>
#include <QProcessEnvironment>
#include <QProcess>
#include <QTextCursor>
#include <QScrollBar>
#include <QLineEdit>
#include <QSignalSpy>
#include <QStackedLayout>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QTest>
#include <QTextDocument>
#include <QTextFrame>
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
#include "FakeWebEngineBackend.h"
#include "ui/MainWindow.h"
#include "ui/PreviewView.h"

inline void testStartupPerformanceAndHydration(MarkdApplication& app) {
    using Zinsser::describe;
    using Zinsser::it;

    describe("Single-reveal preview hydration", [&]() {
        it("allocates no draft surface in the preview view", [&]() {
            QTemporaryFile tempFile(QDir::tempPath() + "/markd_single_reveal_XXXXXX.md");
            assert(tempFile.open());
            tempFile.write("# Reveal Title\n\nBody paragraph with **strong** prose.\n");
            tempFile.close();

            MainWindow win(tempFile.fileName(), MainWindow::Mode::Preview, true);
            PreviewView* preview = win.previewView();
            assert(preview != nullptr);
            assert(preview->findChild<QTextBrowser*>() == nullptr);
            assert(preview->palette().color(QPalette::Window) == QColor("#050505"));
            assert(preview->autoFillBackground());
            assert(!preview->isWebEngineInitialized());
            assert(!preview->isRevealed());
        });

        it("starts the web engine when the preview first shows", [&]() {
            QTemporaryFile tempFile(QDir::tempPath() + "/markd_single_reveal_show_XXXXXX.md");
            assert(tempFile.open());
            tempFile.write("# Shown Heading\n\nParagraph text.\n");
            tempFile.close();

            MainWindow win(tempFile.fileName(), MainWindow::Mode::Preview, true);
            PreviewView* preview = win.previewView();
            assert(!preview->isWebEngineInitialized());
            win.show();
            app.processEvents();
            assert(preview->isWebEngineInitialized());
            assert(win.findChild<QWebEngineView*>("previewView") != nullptr);
            auto* stackLayout = qobject_cast<QStackedLayout*>(preview->layout());
            assert(stackLayout != nullptr);
            assert(stackLayout->count() == 1);
        });

        it("renders the loaded document once the bridge initializes", [&]() {
            QTemporaryFile tempFile(QDir::tempPath() + "/markd_single_reveal_render_XXXXXX.md");
            assert(tempFile.open());
            tempFile.write("# Rendered Once\n\nBody.\n");
            tempFile.close();

            MainWindow win(tempFile.fileName(), MainWindow::Mode::Preview, true);
            PreviewView* preview = win.previewView();
            QSignalSpy renderSpy(preview->bridge(), &WebBridge::renderDocument);
            emit preview->bridge()->bridgeInitialized();
            assert(renderSpy.count() == 1);
            assert(renderSpy.at(0).at(0).toString().contains("Rendered Once"));
        });

        it("emits no scroll ratio after the first render of a file", [&]() {
            QTemporaryFile tempFile(QDir::tempPath() + "/markd_single_reveal_ratio_XXXXXX.md");
            assert(tempFile.open());
            QString markdown = "# Tall Document\n\n";
            for (int i = 0; i < 120; ++i) {
                markdown += QString("Paragraph %1 pushes the document past one screen.\n\n").arg(i);
            }
            tempFile.write(markdown.toUtf8());
            tempFile.close();

            MainWindow win(tempFile.fileName(), MainWindow::Mode::Preview, true);
            PreviewView* preview = win.previewView();
            QSignalSpy ratioSpy(preview->bridge(), &WebBridge::scrollToRatio);
            QSignalSpy renderSpy(preview->bridge(), &WebBridge::renderDocument);
            emit preview->bridge()->bridgeInitialized();
            emit preview->bridge()->documentRendered(1);
            app.processEvents();
            assert(ratioSpy.count() == 0);
            assert(renderSpy.count() == 1);
            assert(!renderSpy.at(0).at(1).toJsonObject().contains("scrollRatio"));
            assert(preview->isRevealed());
        });

        it("applies a find query issued before the reveal", [&]() {
            DocumentSession session;
            session.setMarkdown("# Find Me\n\nneedle_target appears here.\n");
            PreviewView::overrideBackendFactory(&createFakeWebEngineBackend);
            PreviewView preview(&session);
            preview.findText("needle_target", true, false);
            assert(!preview.isWebEngineInitialized());

            preview.ensureWebEngineInitialized();
            FakeWebEngineBackend* fake = lastFakeWebEngineBackend();
            assert(fake != nullptr);
            assert(fake->searches.isEmpty());
            emit preview.bridge()->bridgeInitialized();
            emit preview.bridge()->documentRendered(1);
            assert(fake->searches == QStringList{QStringLiteral("needle_target")});

            emit preview.bridge()->documentRendered(1);
            assert(fake->searches.size() == 1);
            PreviewView::clearBackendFactoryOverride();
        });

        it("applies a heading target issued before the reveal", [&]() {
            DocumentSession session;
            session.setMarkdown("# Root\n\n## Target Anchor\n\nText.\n");
            PreviewView preview(&session);
            QSignalSpy navigateSpy(preview.bridge(), &WebBridge::navigateToHeading);
            preview.scrollToHeading("target-anchor");
            assert(navigateSpy.count() == 0);

            emit preview.bridge()->bridgeInitialized();
            assert(navigateSpy.count() == 0);
            emit preview.bridge()->documentRendered(2);
            assert(navigateSpy.count() == 1);
            assert(navigateSpy.at(0).at(0).toString() == QStringLiteral("target-anchor"));
            assert(navigateSpy.at(0).at(1).toString() == QStringLiteral("instant"));

            preview.scrollToHeading("root");
            assert(navigateSpy.count() == 2);
            assert(navigateSpy.at(1).at(1).toString() == QStringLiteral("smooth"));
        });

        it("applies a scroll ratio issued before the reveal", [&]() {
            DocumentSession session;
            session.setMarkdown("# Ratio Root\n\nBody.\n");
            PreviewView preview(&session);
            preview.scrollToRatio(0.4);
            preview.setMarkdown(session.markdown());

            QSignalSpy renderSpy(preview.bridge(), &WebBridge::renderDocument);
            QSignalSpy ratioSpy(preview.bridge(), &WebBridge::scrollToRatio);
            emit preview.bridge()->bridgeInitialized();
            assert(renderSpy.count() == 1);
            const QJsonObject opts = renderSpy.at(0).at(1).toJsonObject();
            assert(std::abs(opts.value("scrollRatio").toDouble() - 0.4) < 1e-9);

            preview.scrollToRatio(0.7);
            assert(ratioSpy.count() == 1);
        });

        it("applies zoom set before the reveal", [&]() {
            DocumentSession session;
            PreviewView::overrideBackendFactory(&createFakeWebEngineBackend);
            PreviewView preview(&session);
            preview.setZoomFactor(1.4);
            preview.ensureWebEngineInitialized();
            FakeWebEngineBackend* fake = lastFakeWebEngineBackend();
            assert(fake != nullptr);
            assert(std::abs(fake->zoom - 1.4) < 0.01);
            assert(fake->openedUrl == QUrl(QStringLiteral("qrc:/web/template.html")));
            PreviewView::clearBackendFactoryOverride();
        });

        it("shows the load failure reason when the web engine plugin is missing", [&]() {
            DocumentSession session;
            PreviewView::overrideBackendFactory(nullptr, QStringLiteral("libmarkd_webengine missing for test"));
            PreviewView preview(&session);
            preview.ensureWebEngineInitialized();
            assert(!preview.isWebEngineInitialized());
            QLabel* failure = preview.loadFailureLabel();
            assert(failure != nullptr);
            assert(failure->objectName() == QStringLiteral("previewLoadFailure"));
            assert(failure->text().contains("libmarkd_webengine missing for test"));
            preview.ensureWebEngineInitialized();
            assert(preview.findChildren<QLabel*>("previewLoadFailure").size() == 1);

            QString exported = QStringLiteral("unset");
            preview.toHtml([&](const QString& html) { exported = html; });
            assert(exported.isEmpty());

            PreviewView::overrideBackendFactory(&createNullWebEngineBackend, QStringLiteral("factory returned no backend"));
            PreviewView nullPreview(&session);
            nullPreview.ensureWebEngineInitialized();
            assert(!nullPreview.isWebEngineInitialized());
            assert(nullPreview.loadFailureLabel()->text().contains("factory returned no backend"));

            PreviewView::overrideBackendFactory(nullptr);
            PreviewView unnamedPreview(&session);
            unnamedPreview.ensureWebEngineInitialized();
            assert(unnamedPreview.loadFailureLabel()->text().contains("libmarkd_webengine could not be found"));
            PreviewView::clearBackendFactoryOverride();
        });

        it("carries the editor scroll ratio into the preview on mode switch", [&]() {
            QTemporaryFile tempFile(QDir::tempPath() + "/markd_mode_switch_ratio_XXXXXX.md");
            assert(tempFile.open());
            QString markdown;
            for (int i = 0; i < 300; ++i) {
                markdown += QString("Plain line %1 with no headings anywhere.\n\n").arg(i);
            }
            tempFile.write(markdown.toUtf8());
            tempFile.close();

            PreviewView::overrideBackendFactory(&createFakeWebEngineBackend);
            MainWindow win(tempFile.fileName(), MainWindow::Mode::Edit, true);
            win.resize(900, 600);
            win.show();
            app.processEvents();
            QScrollBar* bar = win.editor()->verticalScrollBar();
            assert(bar->maximum() > 0);
            bar->setValue(bar->maximum() / 2);

            PreviewView* preview = win.previewView();
            QSignalSpy renderSpy(preview->bridge(), &WebBridge::renderDocument);
            QSignalSpy ratioSpy(preview->bridge(), &WebBridge::scrollToRatio);
            win.setMode(MainWindow::Mode::Preview);
            assert(renderSpy.count() == 0);
            emit preview->bridge()->bridgeInitialized();
            assert(renderSpy.count() == 1);
            const double firstRatio = renderSpy.at(0).at(1).toJsonObject().value("scrollRatio").toDouble();
            assert(firstRatio > 0.3 && firstRatio < 0.7);

            win.setMode(MainWindow::Mode::Edit);
            win.setMode(MainWindow::Mode::Preview);
            assert(ratioSpy.count() == 1);
            assert(std::abs(ratioSpy.at(0).at(0).toDouble() - firstRatio) < 0.05);
            PreviewView::clearBackendFactoryOverride();
        });

        it("jumps instantly to the editor heading on a mode switch without edits", [&]() {
            QTemporaryFile tempFile(QDir::tempPath() + "/markd_instant_heading_XXXXXX.md");
            assert(tempFile.open());
            tempFile.write("# Opening\n\nIntro.\n\n## Late Section\n\nBody.\n");
            tempFile.close();

            PreviewView::overrideBackendFactory(&createFakeWebEngineBackend);
            MainWindow win(tempFile.fileName(), MainWindow::Mode::Edit, true);
            win.show();
            app.processEvents();
            PreviewView* preview = win.previewView();
            emit preview->bridge()->bridgeInitialized();
            assert(preview->currentMarkdown() == win.session()->markdown());

            QTextCursor cursor = win.editor()->textCursor();
            cursor.movePosition(QTextCursor::End);
            win.editor()->setTextCursor(cursor);
            QTest::qWait(300); // outline debounce

            QSignalSpy navigateSpy(preview->bridge(), &WebBridge::navigateToHeading);
            QSignalSpy renderSpy(preview->bridge(), &WebBridge::renderDocument);
            win.setMode(MainWindow::Mode::Preview);
            assert(renderSpy.count() == 0);
            assert(navigateSpy.count() == 1);
            assert(navigateSpy.at(0).at(0).toString() == QStringLiteral("late-section"));
            assert(navigateSpy.at(0).at(1).toString() == QStringLiteral("instant"));
            PreviewView::clearBackendFactoryOverride();
        });

        it("keeps table-of-contents navigation smooth", [&]() {
            QTemporaryFile tempFile(QDir::tempPath() + "/markd_toc_smooth_XXXXXX.md");
            assert(tempFile.open());
            tempFile.write("# Opening\n\n## Chapter Two\n\nBody.\n");
            tempFile.close();

            PreviewView::overrideBackendFactory(&createFakeWebEngineBackend);
            MainWindow win(tempFile.fileName(), MainWindow::Mode::Preview, true);
            PreviewView* preview = win.previewView();
            emit preview->bridge()->bridgeInitialized();
            QTest::qWait(300); // outline debounce
            QSignalSpy navigateSpy(preview->bridge(), &WebBridge::navigateToHeading);
            assert(QMetaObject::invokeMethod(&win, "onHeadingActivated", Q_ARG(QString, QStringLiteral("chapter-two"))));
            assert(navigateSpy.count() == 1);
            assert(navigateSpy.at(0).at(1).toString() == QStringLiteral("smooth"));
            PreviewView::clearBackendFactoryOverride();
        });

        it("hands focus to the web view when it attaches", [&]() {
            DocumentSession session;
            PreviewView::overrideBackendFactory(&createFakeWebEngineBackend);
            QWidget host;
            auto* preview = new PreviewView(&session, &host);
            host.show();
            host.activateWindow();
            app.processEvents();
            preview->focusPreviewSurface();
            assert(preview->hasPreviewFocus());
            preview->ensureWebEngineInitialized();
            FakeWebEngineBackend* fake = lastFakeWebEngineBackend();
            assert(fake != nullptr);
            assert(fake->viewWidget()->hasFocus());
            preview->focusPreviewSurface();
            preview->copySelection();
            preview->selectAllContent();
            assert(fake->copies == 1);
            assert(fake->selectAlls == 1);

            auto* siblingInput = new QLineEdit(preview);
            siblingInput->show();
            siblingInput->setFocus(Qt::OtherFocusReason);
            app.processEvents();
            assert(siblingInput->hasFocus());
            assert(preview->hasPreviewFocus());
            PreviewView::clearBackendFactoryOverride();
        });
    });

    describe("Startup phase tracing", [&]() {
        const QStringList lockedPhases = {
            "main_entry", "window_shown", "plugin_loaded", "profile_created",
            "template_loaded", "bridge_ready", "content_revealed"};
        QTemporaryDir isolation;
        assert(isolation.isValid());
        const QString sampleDoc = isolation.filePath("phases.md");
        {
            QFile doc(sampleDoc);
            assert(doc.open(QIODevice::WriteOnly));
            doc.write("# Phase Trace\n\nBody text.\n\n## Second\n\nMore.\n");
        }
        // Runs the built markd binary in isolation; the child writes its coverage profile elsewhere.
        auto runMarkd = [&](const QString& benchMode, bool tracing) {
            QProcess proc;
            QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
            env.insert("HOME", isolation.filePath("home"));
            env.insert("TMPDIR", isolation.filePath("tmp"));
            env.insert("QT_QPA_PLATFORM", "offscreen");
            env.insert("MARKD_STARTUP_BENCHMARK", benchMode);
            env.insert("LLVM_PROFILE_FILE", isolation.filePath("child-%p.profraw"));
            env.remove("MARKD_STARTUP_PHASES");
            if (tracing) env.insert("MARKD_STARTUP_PHASES", "1");
            QDir().mkpath(isolation.filePath("home"));
            QDir().mkpath(isolation.filePath("tmp"));
            proc.setProcessEnvironment(env);
            proc.start(QCoreApplication::applicationDirPath() + "/markd", {sampleDoc});
            assert(proc.waitForFinished(30000));
            assert(proc.exitCode() == 0);
            return std::make_pair(QString::fromUtf8(proc.readAllStandardOutput()), QString::fromUtf8(proc.readAllStandardError()));
        };
        const auto traced = runMarkd("2", true);
        const QStringList markLines = traced.second.split('\n').filter(QRegularExpression("^MARKD_PHASE "));

        it("prints startup phase marks only when phase tracing is enabled", [&]() {
            assert(!markLines.isEmpty());
            const auto untraced = runMarkd("2", false);
            assert(!untraced.second.contains("MARKD_PHASE"));
        });

        it("prints the seven locked phase names in the locked format", [&]() {
            const QRegularExpression format("^MARKD_PHASE (main_entry|window_shown|plugin_loaded|profile_created|template_loaded|bridge_ready|content_revealed) \\d{13}$");
            for (const QString& line : markLines) {
                assert(format.match(line).hasMatch());
            }
            for (const QString& phase : lockedPhases) {
                assert(!markLines.filter(QRegularExpression("^MARKD_PHASE " + phase + " ")).isEmpty());
            }
        });

        it("records phase marks in launch order", [&]() {
            qint64 previous = 0;
            for (const QString& phase : lockedPhases) {
                const QString first = markLines.filter(QRegularExpression("^MARKD_PHASE " + phase + " ")).first();
                const qint64 at = first.section(' ', 2).toLongLong();
                assert(at >= previous);
                previous = at;
            }
        });

        it("prints the mode switch time in benchmark mode three", [&]() {
            const auto switched = runMarkd("3", false);
            const QRegularExpressionMatch m = QRegularExpression("MARKD_BENCH_MODE_SWITCH_MS=(\\d+)").match(switched.first);
            assert(m.hasMatch());
            assert(m.captured(1).toInt() >= 0);
        });

        it("formats a phase mark as name and epoch milliseconds", [&]() {
            assert(StartupPhaseTrace::formatMark("bridge_ready", 1794482513922) == QByteArray("MARKD_PHASE bridge_ready 1794482513922\n"));
            qputenv("MARKD_STARTUP_PHASES", "1");
            assert(StartupPhaseTrace::enabled());
            StartupPhaseTrace::mark("bridge_ready");
            qunsetenv("MARKD_STARTUP_PHASES");
            assert(!StartupPhaseTrace::enabled());
        });
    });

    describe("Deferred web engine hydration and startup coalescing", [&]() {
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
            assert(preloadedPreview.pendingMarkdown() == tallDoc);

            app.closeAllWindows();
            app.processEvents();
        });
    });
}

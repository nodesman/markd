#pragma once

#include <QApplication>
#include <cassert>
#include <iostream>
#include <cmath>
#include <QSignalSpy>
#include <QTest>
#include <QMouseEvent>
#include <QNativeGestureEvent>
#include <QWheelEvent>
#include <QPointingDevice>
#include <QLabel>
#include <QStatusBar>
#include <QSplitter>
#include "Zinsser.h"
#include "core/TelemetryEngine.h"
#include "core/DocumentSession.h"
#include "ui/MainWindow.h"
#include "ui/StatusBarTelemetry.h"
#include "ui/TocWidget.h"

inline void testStatusBarAndTelemetryEngine(QApplication& app) {
    using Zinsser::describe;
    using Zinsser::it;

    describe("Pure C++ TelemetryEngine text analysis and metric calculations", [&]() {

        it("calculates exact word count and reading velocity for standard prose", [&]() {
            QString prose = QStringLiteral("The quick brown fox jumps over the lazy dog.");
            auto metrics = TelemetryEngine::calculateMetrics(prose);
            assert(metrics.wordCount == 9);
            assert(metrics.readingTimeMinutes == 1);
            assert(metrics.readingVelocityText == QStringLiteral("< 1 min read"));
            assert(metrics.formattedTelemetry == QStringLiteral("9 words · < 1 min read"));
            assert(metrics.characterCount == 44);
            assert(metrics.formattedCharacters == QStringLiteral("44 characters"));
            assert(!metrics.formattedTelemetry.contains('[') && !metrics.formattedTelemetry.contains(']'));
        });

        it("scales reading velocity using strictly 200 words-per-minute formula", [&]() {
            QString emptyDoc;
            auto emptyMetrics = TelemetryEngine::calculateMetrics(emptyDoc);
            assert(emptyMetrics.wordCount == 0);
            assert(emptyMetrics.readingTimeMinutes == 0);
            assert(emptyMetrics.readingVelocityText == QStringLiteral("0 min read"));
            assert(emptyMetrics.formattedCharacters == QStringLiteral("0 characters"));

            // 1 word -> singular unit
            auto singleWord = TelemetryEngine::calculateMetrics(QStringLiteral("Sole"));
            assert(singleWord.wordCount == 1);
            assert(singleWord.readingTimeMinutes == 1);
            assert(singleWord.formattedTelemetry.contains("1 word ·"));
            assert(singleWord.formattedCharacters == QStringLiteral("4 characters"));

            // 1 character -> singular unit
            auto singleChar = TelemetryEngine::calculateMetrics(QStringLiteral("x"));
            assert(singleChar.characterCount == 1);
            assert(singleChar.formattedCharacters == QStringLiteral("1 character"));

            // 150 words -> ceiling(150 / 200) = 1 min read
            QStringList tokens150;
            for (int i = 0; i < 150; ++i) tokens150 << QStringLiteral("word");
            auto metrics150 = TelemetryEngine::calculateMetrics(tokens150.join(' '));
            assert(metrics150.wordCount == 150);
            assert(metrics150.readingTimeMinutes == 1);
            assert(metrics150.readingVelocityText == QStringLiteral("1 min read"));

            // 450 words -> ceiling(450 / 200) = 3 min read
            QStringList tokens450;
            for (int i = 0; i < 450; ++i) tokens450 << QStringLiteral("word");
            auto metrics450 = TelemetryEngine::calculateMetrics(tokens450.join(' '));
            assert(metrics450.wordCount == 450);
            assert(metrics450.readingTimeMinutes == 3);
            assert(metrics450.readingVelocityText == QStringLiteral("3 min read"));
        });

        it("neutralizes markdown formatting syntax from word counting", [&]() {
            QString md = QStringLiteral(
                "# Title of Document\n\n"
                "> A blockquote with emphasis **bold** and *italic* words.\n\n"
                "Visit [our website](https://example.com) for details.\n\n"
                "```cpp\n"
                "void main() {}\n"
                "```\n\n"
                "Formula is $E=mc^2$."
            );
            auto metrics = TelemetryEngine::calculateMetrics(md);
            assert(metrics.wordCount > 0);
            assert(!metrics.formattedTelemetry.contains('[') && !metrics.formattedTelemetry.contains(']'));
        });

        it("strips comments, images, html tags, and code spans", [&]() {
            QString md = QStringLiteral(
                "<!-- Hidden annotation -->\n"
                "![Hero](https://example.com/art.png)\n"
                "<div class=\"panel\">Content</div>\n"
                "~~crossed~~ and `inline_symbol`."
            );
            auto metrics = TelemetryEngine::calculateMetrics(md);
            assert(metrics.wordCount == 4);
        });

        it("shields math blocks with display math precedence over inline math", [&]() {
            // SWIPE_08: Display math $$...$$ stripped BEFORE inline math $...$
            QString mathSample = QStringLiteral("$$\\sum_{i=1}^n x_i = 100$$\nThis is $x$ variable.");
            auto metrics = TelemetryEngine::calculateMetrics(mathSample);
            // "$$\\sum_{i=1}^n x_i = 100$$" stripped to empty; "$x$" stripped to "x"; "This", "is", "x", "variable" -> 4 words
            assert(metrics.wordCount == 4);
        });

        it("discards punctuation-only tokens without alphanumeric content", [&]() {
            // SWIPE_08: Pure punctuation must not inflate word counts
            QString punctSample = QStringLiteral("--- *** > === ...");
            auto metrics = TelemetryEngine::calculateMetrics(punctSample);
            assert(metrics.wordCount == 0);
        });

        it("preserves non-ASCII Unicode words across international scripts", [&]() {
            // SWIPE_08 & Task 28: Tamil words counted with Unicode character properties
            QString tamil = QString::fromUtf8("மேற்கடல் ஆம்ஸ்டர்டாமில் வடித்த அறம்");
            auto metrics = TelemetryEngine::calculateMetrics(tamil);
            assert(metrics.wordCount == 4);
            assert(metrics.characterCount > 0);
        });

        it("operates headlessly with zero QWidget dependencies", [&]() {
            // SWIPE_10 Lens 5: Pure C++ execution
            auto metrics = TelemetryEngine::calculateMetrics(QStringLiteral("Sovereign architecture"));
            assert(metrics.wordCount == 2);
            assert(metrics.characterCount == 22);
        });

        it("computes public velocity metrics for standalone token counts", [&]() {
            assert(TelemetryEngine::readingVelocityText(0) == QStringLiteral("0 min read"));
            assert(TelemetryEngine::readingVelocityText(50) == QStringLiteral("< 1 min read"));
            assert(TelemetryEngine::readingVelocityText(200) == QStringLiteral("1 min read"));
            assert(TelemetryEngine::readingVelocityText(450) == QStringLiteral("3 min read"));
        });

        it("debounces telemetry recalculation across rapid buffer mutations", [&]() {
            DocumentSession session;
            QSignalSpy telemetrySpy(&session, &DocumentSession::telemetryUpdated);

            session.setMarkdown("First rapid draft");
            QTest::qWait(50);
            assert(telemetrySpy.count() == 0);

            session.setMarkdown("Second rapid draft");
            QTest::qWait(50);
            assert(telemetrySpy.count() == 0);

            session.flushTelemetry();
            assert(telemetrySpy.count() == 1);
            assert(session.telemetryMetrics().wordCount == 3);
        });
    });


    describe("Bottom StatusBarTelemetry widget and window frame integration", [&]() {
        MainWindow win;
        win.resize(1024, 768);
        win.show();
        win.layout()->activate();
        app.processEvents();

        auto* statusBar = win.findChild<StatusBarTelemetry*>("statusBarTelemetry");
        assert(statusBar != nullptr);
        auto* telemetryLabel = statusBar->findChild<TelemetryLabel*>("telemetryLabel");
        assert(telemetryLabel != nullptr);

        auto* sectionLabel = statusBar->findChild<QLabel*>("sectionContextLabel");
        assert(sectionLabel != nullptr);
        auto* zoomLabel = statusBar->findChild<QLabel*>("zoomLabel");
        assert(zoomLabel != nullptr);
        auto* encodingLabel = statusBar->findChild<QLabel*>("encodingLabel");
        assert(encodingLabel != nullptr);

        it("mounts 24px status bar with obsidian theme tokens at window base", [&]() {
            // SWIPE_01 & SWIPE_04
            assert(statusBar->height() <= 26);
            assert(statusBar->height() >= 24);
            assert(encodingLabel->text() == QStringLiteral("UTF-8"));
            assert(zoomLabel->text() == QStringLiteral("100%"));
            assert(!telemetryLabel->text().contains('[') && !telemetryLabel->text().contains(']'));
        });

        it("traps mouse click events to isolate window drag interactions", [&]() {
            // SWIPE_02: Mouse event trapping
            QMouseEvent pressEvent(QEvent::MouseButtonPress, QPointF(10, 10),
                                   telemetryLabel->mapToGlobal(QPoint(10, 10)),
                                   Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(telemetryLabel, &pressEvent);
            assert(pressEvent.isAccepted() == true);

            QMouseEvent releaseEvent(QEvent::MouseButtonRelease, QPointF(10, 10),
                                     telemetryLabel->mapToGlobal(QPoint(10, 10)),
                                     Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(telemetryLabel, &releaseEvent);
            assert(releaseEvent.isAccepted() == true);

            QMouseEvent dblClickEvent(QEvent::MouseButtonDblClick, QPointF(10, 10),
                                      telemetryLabel->mapToGlobal(QPoint(10, 10)),
                                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(telemetryLabel, &dblClickEvent);
            assert(dblClickEvent.isAccepted() == true);

            // Right click fallback passes through
            QMouseEvent rightPress(QEvent::MouseButtonPress, QPointF(10, 10),
                                   telemetryLabel->mapToGlobal(QPoint(10, 10)),
                                   Qt::RightButton, Qt::RightButton, Qt::NoModifier);
            QApplication::sendEvent(telemetryLabel, &rightPress);

            QMouseEvent rightRelease(QEvent::MouseButtonRelease, QPointF(10, 10),
                                     telemetryLabel->mapToGlobal(QPoint(10, 10)),
                                     Qt::RightButton, Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(telemetryLabel, &rightRelease);

            telemetryLabel->setDisplayMode(TelemetryLabel::DisplayMode::WordsAndReadingTime);
        });

        it("toggles between word count and character count on label click", [&]() {
            // SWIPE_02 & SWIPE_10
            telemetryLabel->setDisplayMode(TelemetryLabel::DisplayMode::WordsAndReadingTime);
            win.session()->setMarkdown("The quick brown fox jumps over the lazy dog.");
            win.session()->flushTelemetry();
            app.processEvents();

            // Word count mode
            assert(telemetryLabel->text() == QStringLiteral("9 words · < 1 min read"));

            // Click to toggle to character count
            QTest::mouseClick(telemetryLabel, Qt::LeftButton);
            assert(telemetryLabel->text() == QStringLiteral("44 characters"));

            // Buffer mutation preserves user's selected CharacterCount mode
            win.session()->setMarkdown("The quick brown fox jumps over the lazy dog again.");
            win.session()->flushTelemetry();
            app.processEvents();
            assert(telemetryLabel->text().contains("characters"));

            // Click to toggle back to word count
            QTest::mouseClick(telemetryLabel, Qt::LeftButton);
            assert(telemetryLabel->text().contains("words"));
        });

        it("updates zoom telemetry reactively upon viewport zoom factor changes", [&]() {
            // SWIPE_03 & SWIPE_10
            win.session()->setZoomFactor(1.25);
            app.processEvents();
            assert(zoomLabel->text() == QStringLiteral("125%"));

            win.session()->setZoomFactor(1.0);
            app.processEvents();
            assert(zoomLabel->text() == QStringLiteral("100%"));
        });

        it("reveals active section context when outline sidebar is collapsed", [&]() {
            // SWIPE_05: Splitter collapse section context hook
            win.session()->setMarkdown("# Introduction\n\nText here.\n\n## Architecture\n\nDeep text.");
            win.session()->flushOutline();
            app.processEvents();

            // When sidebar is expanded, section context is hidden
            assert(win.isSidebarCollapsed() == false);
            assert(sectionLabel->isVisible() == false);

            // Set active heading
            win.session()->setActiveHeading("introduction");
            app.processEvents();

            // Collapse sidebar
            win.setSidebarCollapsed(true);
            app.processEvents();

            // When sidebar is collapsed, section context is revealed
            assert(win.isSidebarCollapsed() == true);
            assert(sectionLabel->isVisible() == true);
            assert(sectionLabel->text().contains("Introduction"));

            // Expand sidebar back
            win.setSidebarCollapsed(false);
            app.processEvents();
            assert(sectionLabel->isVisible() == false);

            // Empty section title clears and directly hides label
            statusBar->setSectionVisible(true);
            statusBar->setSectionTitle(QString());
            assert(!sectionLabel->isVisible());
        });

        it("displays transient status notifications without displacing reading viewports", [&]() {
            // SWIPE_06 & SWIPE_07
            int preHeight = statusBar->height();
            statusBar->showNotification(QStringLiteral("Copied as HTML"), QStringLiteral("info"), 2000);
            app.processEvents();
            assert(statusBar->height() == preHeight);
            statusBar->clearNotification();
            app.processEvents();
            assert(statusBar->height() == preHeight);

            // Replaces previous timer and prevents early dismissal of persistent alerts
            statusBar->showNotification(QStringLiteral("Brief toast"), QStringLiteral("info"), 50);
            statusBar->showNotification(QStringLiteral("Persistent alert"), QStringLiteral("warning"), 0);
            QTest::qWait(80);
            assert(statusBar->notificationLabel()->isVisible() == true);
            assert(statusBar->notificationLabel()->text() == QStringLiteral("Persistent alert"));
            statusBar->clearNotification();
        });

        it("preserves status bar 24px geometry across interactive splitter collapses", [&]() {
            // SWIPE_03 & SWIPE_05
            int preHeight = statusBar->height();
            win.toggleSidebar();
            app.processEvents();
            assert(statusBar->height() == preHeight);
            win.toggleSidebar();
            app.processEvents();
            assert(statusBar->height() == preHeight);
        });

        it("maintains telemetry metric parity across Edit and Preview mode swaps", [&]() {
            // SWIPE_09: Viewport Mode Swap Parity
            win.session()->setMarkdown("Paragraph for dual viewport verification.");
            win.session()->flushTelemetry();
            app.processEvents();
            QString editTelemetry = telemetryLabel->text();

            win.setMode(MainWindow::Mode::Preview);
            app.processEvents();
            QString previewTelemetry = telemetryLabel->text();
            assert(previewTelemetry == editTelemetry);

            win.setMode(MainWindow::Mode::Edit);
            app.processEvents();
            assert(telemetryLabel->text() == editTelemetry);
        });

        it("supports selection telemetry overrides with fallback to full metrics", [&]() {
            // SWIPE_10 Lens 5: Downstream selection metrics hook
            statusBar->setSelectionMetrics(12, 85);
            assert(telemetryLabel->text().contains("12 of"));
            assert(telemetryLabel->text().contains("< 1 min read"));

            // Character mode selection metrics
            QTest::mouseClick(telemetryLabel, Qt::LeftButton);
            assert(telemetryLabel->text().contains("85 of"));

            // Switch back to words mode
            QTest::mouseClick(telemetryLabel, Qt::LeftButton);
            assert(telemetryLabel->text().contains("12 of"));

            statusBar->clearSelectionMetrics();
            assert(!telemetryLabel->text().contains("of"));
        });

        it("provides access to child telemetry labels and modes", [&]() {
            assert(statusBar->telemetryLabel() != nullptr);
            assert(statusBar->sectionLabel() != nullptr);
            assert(statusBar->notificationLabel() != nullptr);
            assert(statusBar->encodingLabel() != nullptr);
            assert(statusBar->zoomLabel() != nullptr);

            auto* tLabel = statusBar->telemetryLabel();
            tLabel->setDisplayMode(TelemetryLabel::DisplayMode::CharacterCount);
            assert(tLabel->currentMode() == TelemetryLabel::DisplayMode::CharacterCount);
            tLabel->setDisplayMode(TelemetryLabel::DisplayMode::WordsAndReadingTime);
            assert(tLabel->currentMode() == TelemetryLabel::DisplayMode::WordsAndReadingTime);
        });

        it("synchronizes trackpad pinch-to-zoom and modifier wheel magnification with status bar telemetry", [&]() {
            win.session()->resetZoom();
            app.processEvents();
            assert(zoomLabel->text() == QStringLiteral("100%"));
            assert(std::abs(win.previewView()->zoomFactor() - 1.0) < 0.001);
            const int baselineGutterWidth = win.editor()->lineNumberAreaWidth();

            QNativeGestureEvent zeroPinch(Qt::ZoomNativeGesture,
                                          QPointingDevice::primaryPointingDevice(),
                                          QPointF(10, 10), QPointF(10, 10), QPointF(10, 10),
                                          0.0, 0, 0);
            QApplication::sendEvent(win.previewView()->webView(), &zeroPinch);
            QApplication::sendEvent(win.editor(), &zeroPinch);
            QWheelEvent horizontalWheel(QPointF(10, 10), QPointF(10, 10), QPoint(0, 0), QPoint(120, 0),
                                        Qt::NoButton, Qt::ControlModifier, Qt::NoScrollPhase, false);
            QApplication::sendEvent(win.editor(), &horizontalWheel);
            QApplication::sendEvent(win.previewView()->webView(), &horizontalWheel);
            app.processEvents();
            assert(std::abs(win.session()->zoomFactor() - 1.0) < 0.001);

            QNativeGestureEvent pinchIn(Qt::ZoomNativeGesture,
                                        QPointingDevice::primaryPointingDevice(),
                                        QPointF(10, 10), QPointF(10, 10), QPointF(10, 10),
                                        0.25, 0, 0);
            QApplication::sendEvent(win.previewView()->webView(), &pinchIn);
            app.processEvents();
            assert(std::abs(win.session()->zoomFactor() - 1.25) < 0.01);
            assert(zoomLabel->text() == QStringLiteral("125%"));

            QObject* dynamicChild = new QObject(win.previewView()->webView());
            app.processEvents();
            QNativeGestureEvent childPinch(Qt::ZoomNativeGesture,
                                           QPointingDevice::primaryPointingDevice(),
                                           QPointF(10, 10), QPointF(10, 10), QPointF(10, 10),
                                           0.10, 0, 0);
            QApplication::sendEvent(dynamicChild, &childPinch);
            app.processEvents();
            assert(std::abs(win.session()->zoomFactor() - 1.35) < 0.01);
            assert(zoomLabel->text() == QStringLiteral("135%"));

            QWheelEvent wheelOut(QPointF(10, 10), QPointF(10, 10), QPoint(0, 0), QPoint(0, -120),
                                 Qt::NoButton, Qt::ControlModifier, Qt::NoScrollPhase, false);
            QApplication::sendEvent(win.editor(), &wheelOut);
            app.processEvents();
            assert(std::abs(win.session()->zoomFactor() - 1.25) < 0.01);
            assert(zoomLabel->text() == QStringLiteral("125%"));

            QWheelEvent previewWheelIn(QPointF(10, 10), QPointF(10, 10), QPoint(0, 0), QPoint(0, 120),
                                       Qt::NoButton, Qt::ControlModifier, Qt::NoScrollPhase, false);
            QApplication::sendEvent(win.previewView()->webView(), &previewWheelIn);
            app.processEvents();
            assert(std::abs(win.session()->zoomFactor() - 1.35) < 0.01);
            assert(zoomLabel->text() == QStringLiteral("135%"));

            QNativeGestureEvent pinchEditor(Qt::ZoomNativeGesture,
                                            QPointingDevice::primaryPointingDevice(),
                                            QPointF(10, 10), QPointF(10, 10), QPointF(10, 10),
                                            -0.35, 0, 0);
            QApplication::sendEvent(win.editor(), &pinchEditor);
            app.processEvents();
            assert(std::abs(win.session()->zoomFactor() - 1.0) < 0.01);
            assert(zoomLabel->text() == QStringLiteral("100%"));

            QNativeGestureEvent clampMax(Qt::ZoomNativeGesture,
                                         QPointingDevice::primaryPointingDevice(),
                                         QPointF(10, 10), QPointF(10, 10), QPointF(10, 10),
                                         5.0, 0, 0);
            QApplication::sendEvent(win.previewView()->webView(), &clampMax);
            app.processEvents();
            assert(std::abs(win.session()->zoomFactor() - 3.0) < 0.001);
            assert(zoomLabel->text() == QStringLiteral("300%"));
            assert(win.editor()->lineNumberAreaWidth() > baselineGutterWidth);

            QNativeGestureEvent clampMin(Qt::ZoomNativeGesture,
                                         QPointingDevice::primaryPointingDevice(),
                                         QPointF(10, 10), QPointF(10, 10), QPointF(10, 10),
                                         -5.0, 0, 0);
            QApplication::sendEvent(win.previewView()->webView(), &clampMin);
            app.processEvents();
            assert(std::abs(win.session()->zoomFactor() - 0.25) < 0.001);
            assert(zoomLabel->text() == QStringLiteral("25%"));

            QAction* actualSizeAct = win.findChild<QAction*>("action_actual_size");
            assert(actualSizeAct != nullptr);
            actualSizeAct->trigger();
            app.processEvents();
            assert(std::abs(win.session()->zoomFactor() - 1.0) < 0.001);
            assert(win.editor()->lineNumberAreaWidth() == baselineGutterWidth);
        });
    });
}


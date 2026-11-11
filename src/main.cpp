#include "app/MarkdApplication.h"
#include "app/CommandLineOptions.h"
#include "app/SingleInstanceCoordinator.h"
#include "ui/MainWindow.h"
#include "ui/PreviewView.h"
#include "ui/Theme.h"
#include <QElapsedTimer>
#include <QTimer>
#include <iostream>
#if defined(Q_OS_UNIX)
#include <unistd.h>
#endif

namespace {

QString resolveProgramName(int argc, char* argv[])
{
    if (argc > 0 && argv && argv[0] && argv[0][0] != '\0') {
        return QString::fromLocal8Bit(argv[0]);
    }
    return QStringLiteral("markd");
}

bool shouldCoalesceMacosFileOpen(const CommandLineOptions& options)
{
#if defined(Q_OS_MACOS)
    if (options.hasFile) return false;
    if (qEnvironmentVariableIsSet("MARKD_DISABLE_FILEOPEN_COALESCE")) return false;
    return !isatty(STDIN_FILENO);
#else
    Q_UNUSED(options);
    return false;
#endif
}

void attachStartupBenchmarkProbe(MarkdApplication& app, const QElapsedTimer& timer)
{
    const int benchMode = qEnvironmentVariableIntValue("MARKD_STARTUP_BENCHMARK");
    if (benchMode <= 0) return;

    auto bindWindow = [&app, &timer, benchMode](MainWindow* win) {
        if (!win || !win->previewView()) return;
        PreviewView* preview = win->previewView();
        const int webInit = preview->isWebEngineInitialized() ? 1 : 0;
        std::cout << "MARKD_BENCH_INSTANT_PAINT_MS=" << timer.elapsed()
                  << " WEBENGINE_INIT=" << webInit << "\n" << std::flush;
        if (benchMode == 1) {
            QTimer::singleShot(0, &app, &QCoreApplication::quit);
            return;
        }
        QObject::connect(preview, &PreviewView::documentRendered, &app, [&app, &timer]() {
            std::cout << "MARKD_BENCH_HYDRATED_MS=" << timer.elapsed() << "\n" << std::flush;
            QTimer::singleShot(0, &app, &QCoreApplication::quit);
        });
    };

    if (!app.windows().isEmpty()) {
        bindWindow(app.windows().first());
        return;
    }
    QObject::connect(&app, &MarkdApplication::windowCreated, &app, bindWindow);
}

} // namespace

int main(int argc, char* argv[])
{
    QElapsedTimer startupTimer;
    startupTimer.start();

    const CommandLineOptions options = CommandLineOptions::parseFromArgv(argc, argv);
    const QString programName = resolveProgramName(argc, argv);

    if (!options.parseSuccess) {
        std::cerr << qPrintable(options.errorMessage) << "\n\n";
        std::cerr << qPrintable(CommandLineOptions::helpText(programName));
        return 1;
    }
    if (options.showHelp) {
        std::cout << qPrintable(CommandLineOptions::helpText(programName));
        return 0;
    }
    if (options.showVersion) {
        std::cout << qPrintable(CommandLineOptions::versionText());
        return 0;
    }

    const bool benchActive = qEnvironmentVariableIntValue("MARKD_STARTUP_BENCHMARK") > 0;
    if (!benchActive && SingleInstanceCoordinator::tryForwardToRunningInstance(
                            argc, argv, options.filePaths, !options.hasFile)) {
        return 0;
    }

    MarkdApplication app(argc, argv);
    Theme::apply(app);

    SingleInstanceCoordinator coordinator(
        benchActive ? QStringLiteral("markd-bench-ipc-%1").arg(QCoreApplication::applicationPid())
                    : SingleInstanceCoordinator::defaultSocketName());
    if (coordinator.activateOrListen(options.filePaths, !options.hasFile) ==
        SingleInstanceCoordinator::Role::SecondaryForwarded) {
        return 0;
    }

    QObject::connect(&coordinator, &SingleInstanceCoordinator::activationRequested,
                     &app, &MarkdApplication::openFilesInWindows);
    QObject::connect(&coordinator, &SingleInstanceCoordinator::emptyWindowRequested,
                     &app, [&app]() { app.createWindow(QString()); });

    app.launchInitialWindows(options, !benchActive && shouldCoalesceMacosFileOpen(options));
    if (qEnvironmentVariableIntValue("MARKD_STARTUP_BENCHMARK") == 1) {
        app.processEvents();
        MainWindow* win = app.windows().isEmpty() ? nullptr : app.windows().first();
        const int webInit = (win && win->previewView() && win->previewView()->isWebEngineInitialized()) ? 1 : 0;
        std::cout << "MARKD_BENCH_INSTANT_PAINT_MS=" << startupTimer.elapsed()
                  << " WEBENGINE_INIT=" << webInit << "\n" << std::flush;
        return 0;
    }
    attachStartupBenchmarkProbe(app, startupTimer);

    return app.exec();
}

#include <QApplication>
#include <iostream>
#include "MainWindow.h"
#include "CommandLineOptions.h"
#include "Theme.h"

int main(int argc, char* argv[])
{
    // Initialize QApplication
    QApplication app(argc, argv);
    QApplication::setApplicationName("Markd");
    QApplication::setApplicationVersion("1.0.0");
    QApplication::setOrganizationDomain("markd.org");
    QApplication::setOrganizationName("Markd");

    // Apply Master Obsidian Theme
    Theme::apply(app);

    // Parse CLI options
    CommandLineOptions options = CommandLineOptions::parse(app.arguments());

    if (!options.parseSuccess) {
        std::cerr << qPrintable(options.errorMessage) << "\n\n";
        std::cerr << qPrintable(CommandLineOptions::helpText(app.arguments().value(0, "markd")));
        return 1;
    }

    if (options.showHelp) {
        std::cout << qPrintable(CommandLineOptions::helpText(app.arguments().value(0, "markd")));
        return 0;
    }

    if (options.showVersion) {
        std::cout << qPrintable(CommandLineOptions::versionText());
        return 0;
    }

    // Launch standalone window
    MainWindow window(options.hasFile ? options.filePath : QString());
    window.show();

    // Runs main event loop; guarantees clean process exit with zero background IPC daemons
    return app.exec();
}

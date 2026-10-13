#include "MarkdApplication.h"
#include "MainWindow.h"
#include "CommandLineOptions.h"
#include "Theme.h"
#include <iostream>

int main(int argc, char* argv[])
{
    // Initialize MarkdApplication
    MarkdApplication app(argc, argv);

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

    // Launch initial window via application registry
    app.createWindow(options.hasFile ? options.filePath : QString());

    // Runs main event loop; guarantees clean process exit with zero background IPC daemons
    return app.exec();
}

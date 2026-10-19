#include "app/CommandLineOptions.h"
#include <QFileInfo>

CommandLineOptions CommandLineOptions::parse(const QStringList& arguments) {
    CommandLineOptions options;

    // arguments[0] is typically the binary path if passed from QCoreApplication::arguments(),
    // but we handle both full argc/argv lists and raw option lists.
    int startIndex = 1;
    if (arguments.isEmpty()) {
        return options;
    }
    // If the first argument starts with '-' or doesn't look like executable path, check if it's already stripped
    // But standard QCoreApplication::arguments() has argv[0] at index 0.
    // Let's inspect arguments:
    for (int i = 1; i < arguments.size(); ++i) {
        const QString& arg = arguments.at(i);
        if (arg == "-h" || arg == "--help") {
            options.showHelp = true;
        } else if (arg == "-v" || arg == "--version") {
            options.showVersion = true;
        } else if (arg.startsWith("-")) {
            options.errorMessage = QString("Unknown option: %1").arg(arg);
            options.parseSuccess = false;
        } else {
            if (!options.hasFile) {
                options.filePath = arg;
                options.hasFile = true;
            } else {
                // Secondary non-flag argument
                options.errorMessage = QString("Unexpected positional argument: %1").arg(arg);
                options.parseSuccess = false;
            }
        }
    }

    return options;
}

QString CommandLineOptions::helpText(const QString& programName) {
    return QString(
        "Usage: %1 [options] [file.md]\n\n"
        "Markd — High-performance, native Markdown viewer and editor.\n\n"
        "Arguments:\n"
        "  [file.md]          Markdown document to view or edit\n\n"
        "Options:\n"
        "  -h, --help         Show this help message and exit\n"
        "  -v, --version      Show version information and exit\n"
    ).arg(programName);
}

#include "core/Version.h"

QString CommandLineOptions::versionText() {
    return QString("Markd version %1 (Qt 6 C++20)\n").arg(MARKD_VERSION_STR);
}

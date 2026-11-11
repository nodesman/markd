#include "app/CommandLineOptions.h"
#include "core/Version.h"

namespace {

void applySingleArgument(const QString& arg, CommandLineOptions& options)
{
    if (arg == QLatin1String("-h") || arg == QLatin1String("--help")) {
        options.showHelp = true;
        return;
    }
    if (arg == QLatin1String("-v") || arg == QLatin1String("--version")) {
        options.showVersion = true;
        return;
    }
    if (arg == QLatin1String("--register-default")) {
        options.registerDefault = true;
        return;
    }
    if (arg.startsWith(QLatin1Char('-'))) {
        options.errorMessage = QStringLiteral("Unknown option: %1").arg(arg);
        options.parseSuccess = false;
        return;
    }
    options.filePaths.append(arg);
    if (!options.hasFile) {
        options.filePath = arg;
        options.hasFile = true;
    }
}

} // namespace

CommandLineOptions CommandLineOptions::parse(const QStringList& arguments)
{
    CommandLineOptions options;
    for (int i = 1; i < arguments.size(); ++i) {
        applySingleArgument(arguments.at(i), options);
    }
    return options;
}

CommandLineOptions CommandLineOptions::parseFromArgv(int argc, char* argv[])
{
    if (argc <= 0 || argv == nullptr) {
        return CommandLineOptions();
    }
    QStringList arguments;
    arguments.reserve(argc);
    for (int i = 0; i < argc; ++i) {
        arguments.append(QString::fromLocal8Bit(argv[i] ? argv[i] : ""));
    }
    return parse(arguments);
}

QString CommandLineOptions::helpText(const QString& programName)
{
    return QStringLiteral(
        "Usage: %1 [options] [file.md]\n\n"
        "Markd — High-performance, native Markdown viewer and editor.\n\n"
        "Arguments:\n"
        "  [file.md]          Markdown document to view or edit\n\n"
        "Options:\n"
        "  -h, --help              Show this help message and exit\n"
        "  -v, --version           Show version information and exit\n"
        "  --register-default      Register Markd as default handler for .md files\n"
    ).arg(programName);
}

QString CommandLineOptions::versionText()
{
    return QStringLiteral("Markd version %1 (Qt 6 C++20)\n").arg(QString::fromUtf8(MARKD_VERSION_STR));
}

#pragma once

#include <QStringList>
#include <cassert>
#include <iostream>
#include "app/CommandLineOptions.h"

inline void testCommandLineOptions() {
    // Test 1: Empty args
    {
        QStringList args = {"markd"};
        CommandLineOptions opts = CommandLineOptions::parse(args);
        assert(opts.parseSuccess);
        assert(!opts.showHelp);
        assert(!opts.showVersion);
        assert(!opts.hasFile);
    }

    // Test 2: Help flag
    {
        QStringList args = {"markd", "--help"};
        CommandLineOptions opts = CommandLineOptions::parse(args);
        assert(opts.parseSuccess);
        assert(opts.showHelp);
        assert(!opts.showVersion);
        assert(!opts.hasFile);
    }

    // Test 3: Short help flag
    {
        QStringList args = {"markd", "-h"};
        CommandLineOptions opts = CommandLineOptions::parse(args);
        assert(opts.parseSuccess);
        assert(opts.showHelp);
    }

    // Test 4: Version flag
    {
        QStringList args = {"markd", "--version"};
        CommandLineOptions opts = CommandLineOptions::parse(args);
        assert(opts.parseSuccess);
        assert(!opts.showHelp);
        assert(opts.showVersion);
    }

    // Test 5: File argument
    {
        QStringList args = {"markd", "README.md"};
        CommandLineOptions opts = CommandLineOptions::parse(args);
        assert(opts.parseSuccess);
        assert(!opts.showHelp);
        assert(!opts.showVersion);
        assert(opts.hasFile);
        assert(opts.filePath == "README.md");
    }

    // Test 6: Unknown option
    {
        QStringList args = {"markd", "--unknown-flag"};
        CommandLineOptions opts = CommandLineOptions::parse(args);
        assert(!opts.parseSuccess);
        assert(opts.errorMessage.contains("Unknown option"));
    }

    // Test 7: Multiple positional arguments
    {
        QStringList args = {"markd", "file1.md", "file2.md"};
        CommandLineOptions opts = CommandLineOptions::parse(args);
        assert(!opts.parseSuccess);
        assert(opts.errorMessage.contains("Unexpected positional argument"));
    }

    std::cout << "PASS: CommandLineOptions tests passed!\n";
}

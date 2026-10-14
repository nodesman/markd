#pragma once

#include <QStringList>
#include <cassert>
#include <iostream>
#include "Zinsser.h"
#include "app/CommandLineOptions.h"

inline void testCommandLineOptions() {
    using Zinsser::describe;
    using Zinsser::it;

    describe("Command line argument parsing and validation", [&]() {
        it("parses empty arguments into default options", [&]() {
            QStringList args = {"markd"};
            CommandLineOptions opts = CommandLineOptions::parse(args);
            assert(opts.parseSuccess);
            assert(!opts.showHelp);
            assert(!opts.showVersion);
            assert(!opts.hasFile);
        });

        it("flags help option under long flag (--help)", [&]() {
            QStringList args = {"markd", "--help"};
            CommandLineOptions opts = CommandLineOptions::parse(args);
            assert(opts.parseSuccess);
            assert(opts.showHelp);
            assert(!opts.showVersion);
            assert(!opts.hasFile);
        });

        it("flags help option under short flag (-h)", [&]() {
            QStringList args = {"markd", "-h"};
            CommandLineOptions opts = CommandLineOptions::parse(args);
            assert(opts.parseSuccess);
            assert(opts.showHelp);
        });

        it("flags version option under long flag (--version)", [&]() {
            QStringList args = {"markd", "--version"};
            CommandLineOptions opts = CommandLineOptions::parse(args);
            assert(opts.parseSuccess);
            assert(!opts.showHelp);
            assert(opts.showVersion);
        });

        it("captures positional markdown document path", [&]() {
            QStringList args = {"markd", "README.md"};
            CommandLineOptions opts = CommandLineOptions::parse(args);
            assert(opts.parseSuccess);
            assert(!opts.showHelp);
            assert(!opts.showVersion);
            assert(opts.hasFile);
            assert(opts.filePath == "README.md");
        });

        it("rejects unknown options with descriptive error message", [&]() {
            QStringList args = {"markd", "--unknown-flag"};
            CommandLineOptions opts = CommandLineOptions::parse(args);
            assert(!opts.parseSuccess);
            assert(opts.errorMessage.contains("Unknown option"));
        });

        it("rejects superfluous positional arguments", [&]() {
            QStringList args = {"markd", "file1.md", "file2.md"};
            CommandLineOptions opts = CommandLineOptions::parse(args);
            assert(!opts.parseSuccess);
            assert(opts.errorMessage.contains("Unexpected positional argument"));
        });
    });
}

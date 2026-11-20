#pragma once

#include <cassert>
#include "Zinsser.h"
#include "parser/FrontmatterScanner.h"
#include "parser/TocParser.h"
#include "core/TelemetryEngine.h"
#include "core/Converter.h"

inline void testFrontmatterScannerAndSidecarParity() {
    using Zinsser::describe;
    using Zinsser::it;

    describe("Frontmatter scanner 3-invariant contract and sidecar parity", [&]() {
        it("strips standard jekyll yaml frontmatter and returns body line offset", [&]() {
            const QString doc = QStringLiteral(
                "---\n"
                "layout: post\n"
                "title: Native Reader\n"
                "tags: [macos, qt6]\n"
                "---\n"
                "\n"
                "# Native Reader\n"
                "\n"
                "Body paragraph.");
            const QStringList lines = doc.split(QLatin1Char('\n'));
            assert(FrontmatterScanner::bodyStartLine(lines) == 5);
            const QString body = FrontmatterScanner::strip(doc);
            assert(!body.contains(QStringLiteral("layout:")));
            assert(body.contains(QStringLiteral("# Native Reader")));
        });

        it("accepts utf8 bom, crlf, trailing fence spaces, and dot-dot-dot closer", [&]() {
            const QString doc = QString(QChar(0xFEFF)) + QStringLiteral(
                "---   \r\n"
                "title: Windows Post\r\n"
                "...  \r\n"
                "# Windows Post");
            const QString body = FrontmatterScanner::strip(doc);
            assert(!body.contains(QStringLiteral("title:")));
            assert(body.trimmed() == QStringLiteral("# Windows Post"));
        });

        it("accepts empty and comment-only jekyll frontmatter blocks", [&]() {
            const QString emptyDoc = QStringLiteral("---\n---\n# Empty");
            assert(FrontmatterScanner::strip(emptyDoc) == QStringLiteral("# Empty"));

            const QString commentDoc = QStringLiteral("---\n# enable liquid\n# second comment\n\n---\n# Liquid");
            assert(FrontmatterScanner::strip(commentDoc) == QStringLiteral("# Liquid"));

            const QString spacedDoc = QStringLiteral("---\n\n# section comment\n# detail comment\nlayout: post\n---\n# Spaced");
            assert(FrontmatterScanner::strip(spacedDoc) == QStringLiteral("# Spaced"));
        });

        it("ignores indented triple dashes inside multiline yaml scalars", [&]() {
            const QString doc = QStringLiteral(
                "---\n"
                "summary: |\n"
                "  First line\n"
                "  ---\n"
                "  Second line\n"
                "---\n"
                "# Real Body");
            const QString body = FrontmatterScanner::strip(doc);
            assert(!body.contains(QStringLiteral("Second line")));
            assert(body == QStringLiteral("# Real Body"));
        });

        it("rejects broken opening fences such as double dash or bullet list", [&]() {
            const QString doubleDash = QStringLiteral("--\nlayout: post\n---\n# Heading");
            assert(FrontmatterScanner::strip(doubleDash) == doubleDash);

            const QString bulletList = QStringLiteral("- item one\n- item two\n---\n# Heading");
            assert(FrontmatterScanner::strip(bulletList) == bulletList);

            const QString singleLine = QStringLiteral("---");
            assert(FrontmatterScanner::bodyStartLine(QStringList{singleLine}) == 0);
        });

        it("rejects unclosed frontmatter without swallowing document body", [&]() {
            const QString unclosed = QStringLiteral("---\nlayout: post\ntitle: Draft\n\nBody text");
            assert(FrontmatterScanner::strip(unclosed) == unclosed);
        });

        it("rejects unclosed frontmatter followed by heading and later horizontal rule", [&]() {
            const QString swallowedTrap = QStringLiteral(
                "---\n"
                "layout: post\n"
                "title: Missing Closer\n"
                "\n"
                "# Chapter 1\n"
                "\n"
                "Real article prose.\n"
                "\n"
                "---\n"
                "## Chapter 2");
            assert(FrontmatterScanner::strip(swallowedTrap) == swallowedTrap);
        });

        it("preserves decorative top horizontal rule before markdown heading", [&]() {
            const QString hrDoc = QStringLiteral(
                "---\n"
                "\n"
                "# Chapter 1\n"
                "\n"
                "Opening prose.\n"
                "\n"
                "---\n"
                "## Chapter 2");
            assert(FrontmatterScanner::strip(hrDoc) == hrDoc);

            const QString proseBetweenHr = QStringLiteral("---\nJust plain text without keys\n---\n# After");
            assert(FrontmatterScanner::strip(proseBetweenHr) == proseBetweenHr);
        });

        it("excludes yaml comments from toc outline while preserving exact heading line numbers", [&]() {
            const QString doc = QStringLiteral(
                "---\n"
                "# yaml comment that looks like h1\n"
                "layout: post\n"
                "title: Sample\n"
                "---\n"
                "\n"
                "# Real Heading\n"
                "## Second Heading\n");
            const auto items = TocParser::extractHeadings(doc);
            assert(items.size() == 2);
            assert(items[0].title == QStringLiteral("Real Heading"));
            assert(items[0].lineNumber == 7);
            assert(items[1].title == QStringLiteral("Second Heading"));
            assert(items[1].lineNumber == 8);
        });

        it("excludes frontmatter keys and values from telemetry word count", [&]() {
            const QString doc = QStringLiteral(
                "---\n"
                "layout: post\n"
                "title: Ten Extra Frontmatter Words Should Never Count Here\n"
                "tags: [one, two, three]\n"
                "---\n"
                "Alpha beta gamma.");
            const auto metrics = TelemetryEngine::calculateMetrics(doc);
            assert(metrics.wordCount == 3);
        });

        it("omits frontmatter from converted html export fragments", [&]() {
            const QString doc = QStringLiteral(
                "---\n"
                "layout: post\n"
                "secret_meta: hidden_yaml_value\n"
                "---\n"
                "# Exported Title\n\nVisible paragraph.");
            const QString html = Converter::toHtmlFragment(doc);
            assert(!html.contains(QStringLiteral("hidden_yaml_value")));
            assert(html.contains(QStringLiteral("Exported Title")));
            assert(html.contains(QStringLiteral("Visible paragraph.")));
        });
    });
}

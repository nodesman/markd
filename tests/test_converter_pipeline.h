#pragma once

#include <QFile>
#include <cassert>
#include <iostream>
#include "Zinsser.h"
#include "core/Converter.h"

inline void testOfflineAssetSelfContainment() {
    using Zinsser::describe;
    using Zinsser::it;

    describe("Offline presentation asset self-containment", [&]() {
        it("guarantees compiled presentation engine assets in virtual filesystem", [&]() {
            assert(QFile::exists(":/web/template.html"));
            assert(QFile::exists(":/web/css/obsidian.css"));
            assert(QFile::exists(":/web/css/style.css"));
            assert(QFile::exists(":/web/js/qwebchannel.js"));
            assert(QFile::exists(":/web/js/katex.min.css"));
            assert(QFile::exists(":/web/js/katex.min.js"));
            assert(QFile::exists(":/web/js/highlight.min.js"));
            assert(QFile::exists(":/web/js/markdown-it.min.js"));
            assert(QFile::exists(":/web/js/render.js"));
        });
    });
}

inline void testConverterPipeline() {
    using Zinsser::describe;
    using Zinsser::it;

    testOfflineAssetSelfContainment();

    describe("Markdown conversion and HTML generation pipeline", [&]() {
        it("escapes raw html entities into safe xml representations", [&]() {
            assert(Converter::escapeHtml("<div>&\"'</div>") == "&lt;div&gt;&amp;&quot;&#39;&lt;/div&gt;");
        });

        it("transforms markdown tokens into html fragment tags", [&]() {
            QString md = "# Title\n\n**Bold Text** and `code`";
            QString html = Converter::toHtmlFragment(md);
            assert(html.contains("Title"));
            assert(html.contains("Bold Text"));
            assert(html.contains("code"));
        });

        it("wraps rendered markdown into self-contained html document", [&]() {
            QString md = "# Title\n\n**Bold Text** and `code`";
            QString fullDoc = Converter::toFullHtmlDocument(md);
            assert(fullDoc.contains("<!DOCTYPE html>"));
            assert(fullDoc.contains("#050505"));
            assert(fullDoc.contains("qrc:/web/styles/obsidian.css"));
            assert(fullDoc.contains("Title"));
        });
    });
}

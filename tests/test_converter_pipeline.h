#pragma once

#include <QFile>
#include <cassert>
#include <iostream>
#include "core/Converter.h"

inline void testOfflineAssetSelfContainment() {
    // Verify all presentation engine assets exist in compiled Qt VFS (zero network egress)
    assert(QFile::exists(":/web/template.html"));
    assert(QFile::exists(":/web/css/obsidian.css"));
    assert(QFile::exists(":/web/css/style.css"));
    assert(QFile::exists(":/web/js/qwebchannel.js"));
    assert(QFile::exists(":/web/js/katex.min.css"));
    assert(QFile::exists(":/web/js/katex.min.js"));
    assert(QFile::exists(":/web/js/highlight.min.js"));
    assert(QFile::exists(":/web/js/markdown-it.min.js"));
    assert(QFile::exists(":/web/js/render.js"));

    std::cout << "PASS: Offline asset self-containment tests passed!\n";
}

inline void testConverterPipeline() {
    testOfflineAssetSelfContainment();

    // 1. HTML escaping
    assert(Converter::escapeHtml("<div>&\"'</div>") == "&lt;div&gt;&amp;&quot;&#39;&lt;/div&gt;");

    // 2. Markdown to HTML fragment
    QString md = "# Title\n\n**Bold Text** and `code`";
    QString html = Converter::toHtmlFragment(md);
    assert(html.contains("Title"));
    assert(html.contains("Bold Text"));
    assert(html.contains("code"));

    // 3. Full document generation with Obsidian stylesheet
    QString fullDoc = Converter::toFullHtmlDocument(md);
    assert(fullDoc.contains("<!DOCTYPE html>"));
    assert(fullDoc.contains("#050505"));
    assert(fullDoc.contains("qrc:/web/styles/obsidian.css"));
    assert(fullDoc.contains("Title"));

    std::cout << "PASS: Converter pipeline tests passed!\n";
}

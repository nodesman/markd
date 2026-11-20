
// Prevent DOM clobbering of reserved bridge properties
if (typeof DOMPurify !== 'undefined' && typeof DOMPurify.addHook === 'function') {
    DOMPurify.addHook('afterSanitizeAttributes', function (node) {
        if (node.hasAttribute('id')) {
            var id = node.getAttribute('id');
            if (id === 'markdBridge' || id === 'session' || id === 'Markd' || id === 'renderDocument') {
                node.setAttribute('id', 'user-content-' + id);
            }
        }
        if (node.hasAttribute('name')) {
            var name = node.getAttribute('name');
            if (name === 'markdBridge' || name === 'session' || name === 'Markd' || name === 'renderDocument') {
                node.setAttribute('name', 'user-content-' + name);
            }
        }
    });
}

// QWebChannel Bridge setup for Qt C++ IPC
window.markdBridge = null;
window.session = null;

var isBridgeConnected = false;
var pendingRenderSignals = [];

function queueOrRenderDocument(markdown, options) {
    if (!isBridgeConnected && !window.markdBridge) {
        pendingRenderSignals.push({ markdown: markdown, options: options });
        return;
    }
    renderMarkdown(markdown, options);
}
window.renderDocument = queueOrRenderDocument;

function handleSessionMarkdownChanged(newMarkdown) {
    if (window.markdBridge && window.markdBridge.renderDocument) {
        return;
    }
    if (newMarkdown === currentRenderedMarkdown) {
        return;
    }
    renderMarkdown(newMarkdown);
}

function initQWebChannel() {
    if (typeof QWebChannel !== 'undefined' && typeof qt !== 'undefined' && qt.webChannelTransport) {
        new QWebChannel(qt.webChannelTransport, function(channel) {
            window.markdBridge = channel.objects.markdBridge;
            window.session = channel.objects.session;
            isBridgeConnected = true;

            if (window.markdBridge) {
                if (typeof Object.freeze === 'function') {
                    Object.freeze(window.markdBridge);
                }

                window.markdBridge.logMessage("QWebChannel connected to render.js");

                // Downlink typed signal connections from C++ WebBridge
                if (window.markdBridge.renderDocument && typeof window.markdBridge.renderDocument.connect === 'function') {
                    window.markdBridge.renderDocument.connect(function(markdown, options) {
                        if (typeof renderMarkdown === 'function') {
                            renderMarkdown(markdown, options);
                        }
                    });
                }
                if (window.markdBridge.navigateToHeading && typeof window.markdBridge.navigateToHeading.connect === 'function') {
                    window.markdBridge.navigateToHeading.connect(function(headingId, behavior) {
                        if (typeof scrollToHeading === 'function') {
                            scrollToHeading(headingId, behavior);
                        }
                    });
                }
                if (window.markdBridge.setTheme && typeof window.markdBridge.setTheme.connect === 'function') {
                    window.markdBridge.setTheme.connect(function(themeName) {
                        syncMermaidTheme(themeName);
                    });
                }
                if (window.markdBridge.scrollToRatio && typeof window.markdBridge.scrollToRatio.connect === 'function') {
                    window.markdBridge.scrollToRatio.connect(function(ratio) {
                        if (typeof scrollToRatio === 'function') {
                            scrollToRatio(ratio);
                        }
                    });
                }

                // Flush signals buffered before QWebChannel initialization completed
                while (pendingRenderSignals.length > 0) {
                    var queued = pendingRenderSignals.shift();
                    renderMarkdown(queued.markdown, queued.options);
                }

                // Two-way handshake: notify C++ that bridge is initialized and ready
                if (typeof window.markdBridge.bridgeReady === 'function') {
                    window.markdBridge.bridgeReady();
                }
            }
            if (window.session) {
                if (window.session.markdownChanged && typeof window.session.markdownChanged.connect === 'function') {
                    window.session.markdownChanged.connect(function(newMarkdown) {
                        handleSessionMarkdownChanged(newMarkdown);
                    });
                }
                if (window.session.selectedHeadingChanged && typeof window.session.selectedHeadingChanged.connect === 'function') {
                    window.session.selectedHeadingChanged.connect(function(headingId) {
                        if (typeof scrollToHeading === 'function') {
                            scrollToHeading(headingId);
                        }
                    });
                }
            }
        });
    }
}
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', initQWebChannel);
} else {
    initQWebChannel();
}

function dispatchClipboardWrite(rawCode) {
    if (window.session && typeof window.session.copyToClipboard === 'function') {
        window.session.copyToClipboard(rawCode);
        return;
    }
    if (navigator.clipboard && typeof navigator.clipboard.writeText === 'function') {
        navigator.clipboard.writeText(rawCode);
    }
}

function handleCodeCopyClick(copyBtn) {
    var card = copyBtn.closest('.code-block-card');
    var codeEl = card ? card.querySelector('pre > code') : null;
    if (!codeEl) return;
    var rawCode = codeEl.textContent.replace(/\n$/, '');
    dispatchClipboardWrite(rawCode);
    copyBtn.textContent = 'Copied!';
    copyBtn.classList.add('copied');
    if (copyBtn._copyTimeoutId) {
        clearTimeout(copyBtn._copyTimeoutId);
    }
    copyBtn._copyTimeoutId = setTimeout(function () {
        copyBtn.textContent = 'Copy';
        copyBtn.classList.remove('copied');
        copyBtn._copyTimeoutId = null;
    }, 1500);
}

// Global anchor navigation interception & code card copy delegation (Tier 1 WebEngine Guardrail)
document.addEventListener('click', function (e) {
    var target = e.target;
    if (!target || typeof target.closest !== 'function') return;
    var copyBtn = target.closest('.copy-btn');
    if (copyBtn) {
        handleCodeCopyClick(copyBtn);
        return;
    }
    var anchor = target.closest('a');
    if (!anchor) return;
    var href = anchor.getAttribute('href');
    if (!href) return;
    if (href.startsWith('#')) {
        e.preventDefault();
        var targetId = decodeURIComponent(href.slice(1));
        scrollToHeading(targetId);
        return;
    }
    e.preventDefault();
    if (window.markdBridge && typeof window.markdBridge.openExternalUrl === 'function') {
        window.markdBridge.openExternalUrl(href);
        return;
    }
    if (window.session && typeof window.session.openExternal === 'function') {
        window.session.openExternal(href);
        return;
    }
});

// Configure DOMPurify hooks (rel="noopener noreferrer", CSS position neutralization)
if (typeof DOMPurify !== 'undefined' && typeof DOMPurify.addHook === 'function') {
    DOMPurify.addHook('afterSanitizeAttributes', function (node) {
        if (node.tagName === 'A') {
            var target = node.getAttribute('target');
            var href = node.getAttribute('href') || '';
            var isOutbound = target === '_blank' || /^(?:https?:\/\/|\/\/)/i.test(href);
            if (isOutbound) {
                node.setAttribute('rel', 'noopener noreferrer');
            }
        }
        if (node.hasAttribute && node.hasAttribute('style')) {
            var style = node.getAttribute('style');
            if (/position\s*:\s*(?:fixed|absolute)/i.test(style)) {
                var cleanStyle = style.replace(/position\s*:\s*(?:fixed|absolute);?/gi, '').trim();
                if (cleanStyle) {
                    node.setAttribute('style', cleanStyle);
                } else {
                    node.removeAttribute('style');
                }
            }
        }
    });
}

function formatCodeBlock(codeHtml, lang) {
    var safeLang = md.utils.escapeHtml(lang);
    return '<div class="code-block-card" data-lang="' + safeLang + '" role="region" aria-label="Code snippet: ' + safeLang + '">' +
        '<div class="code-block-header">' +
        '<span class="code-lang">' + safeLang + '</span>' +
        '<button class="copy-btn" type="button" aria-label="Copy code to clipboard">Copy</button>' +
        '</div>' +
        '<pre class="hljs"><code>' + codeHtml + '</code></pre>' +
        '</div>';
}

function highlightCodeBlock(str, normalizedLang) {
    if (!hljs.getLanguage(normalizedLang)) {
        return formatCodeBlock(md.utils.escapeHtml(str), normalizedLang);
    }
    try {
        var highlighted = hljs.highlight(str, { language: normalizedLang, ignoreIllegals: true }).value;
        return formatCodeBlock(highlighted, normalizedLang);
    } catch (_) {
        return formatCodeBlock(md.utils.escapeHtml(str), normalizedLang);
    }
}

// Initialize markdown-it with plugins
const md = window.markdownit({
    html: true,
    linkify: true,
    typographer: true
});

md.renderer.rules.fence = function (tokens, idx) {
    var token = tokens[idx];
    var info = md.utils.unescapeAll(token.info).trim();
    var lang = info.split(/\s+/g)[0].toLowerCase();
    if (lang === 'mermaid') {
        return '<pre><code class="language-mermaid">' + md.utils.escapeHtml(token.content) + '</code></pre>\n';
    }
    var normalizedLang = lang || 'text';
    return highlightCodeBlock(token.content, normalizedLang) + '\n';
};

// Add anchor plugin for heading IDs with Unicode property slugification
if (window.markdownItAnchor) {
    md.use(window.markdownItAnchor, {
        permalink: false,
        slugify: function (s) {
            return s.trim()
                .toLowerCase()
                .replace(/\s+/g, '-')
                .replace(/[^\p{L}\p{M}\p{N}_-]/gu, '')
                .replace(/--+/g, '-')
                .replace(/^-|-$/g, '');
        }
    });
}

// Add task list plugin
if (window.markdownitTaskLists) {
    md.use(window.markdownitTaskLists, { enabled: true, label: true });
}

// Add KaTeX math plugin
if (window.texmath) {
    md.use(window.texmath, {
        engine: window.katex,
        delimiters: 'dollars',
        katexOptions: {
            throwOnError: false,
            displayMode: false
        }
    });
}

// Lazy Mermaid configuration & on-demand bundle loader
var activeMermaidTheme = window.matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'default';
var mermaidLoadPromise = null;

function applyMermaidConfig() {
    window.mermaid.initialize({
        startOnLoad: false,
        theme: activeMermaidTheme,
        securityLevel: 'strict',
        fontFamily: '-apple-system, BlinkMacSystemFont, "SF Pro Text", sans-serif'
    });
}

if (typeof window.mermaid !== 'undefined') {
    applyMermaidConfig();
}

function ensureMermaidLoaded() {
    if (typeof window.mermaid !== 'undefined') {
        return Promise.resolve(window.mermaid);
    }
    if (mermaidLoadPromise) {
        return mermaidLoadPromise;
    }
    var pending = new Promise(function (resolve, reject) {
        var script = document.createElement('script');
        script.src = 'vendor/mermaid/mermaid.min.js';
        script.setAttribute('data-mermaid-lazy', '1');
        script.onload = function () {
            applyMermaidConfig();
            resolve(window.mermaid);
        };
        script.onerror = function () {
            script.remove();
            reject(new Error('Failed to load Mermaid bundle'));
        };
        document.head.appendChild(script);
    }).catch(function (err) {
        mermaidLoadPromise = null;
        throw err;
    });
    mermaidLoadPromise = pending;
    return pending;
}
window.ensureMermaidLoaded = ensureMermaidLoaded;

function isDarkMermaidPalette(themeName) {
    var lower = themeName.toLowerCase();
    return lower.indexOf('dark') !== -1 || lower.indexOf('obsidian') !== -1;
}

function syncMermaidTheme(themeName) {
    if (!themeName) return;
    document.documentElement.setAttribute('data-theme', themeName);
    activeMermaidTheme = isDarkMermaidPalette(themeName) ? 'dark' : 'default';
    if (typeof window.mermaid !== 'undefined') {
        applyMermaidConfig();
    }
    if (currentRenderedMarkdown) {
        renderMarkdown(currentRenderedMarkdown, { force: true });
    }
}
window.setTheme = syncMermaidTheme;

// Update mermaid theme on dark mode change
window.matchMedia('(prefers-color-scheme: dark)').addEventListener('change', function (e) {
    syncMermaidTheme(e.matches ? 'dark' : 'default');
});

// ---- Diagram Event Listener Teardown Registry ----
var diagramTeardowns = [];

function cleanupDiagramListeners() {
    stopActiveBlockResize();
    while (diagramTeardowns.length > 0) {
        var teardown = diagramTeardowns.pop();
        teardown();
    }
}
window.cleanupDiagramListeners = cleanupDiagramListeners;

// ---- Async Downlink Mutex Queue ----
var renderDownlinkMutex = null;
var currentRenderedMarkdown = null;

function renderMarkdown(markdownText, options) {
    var scheduledTask = function () {
        return executeRenderMarkdown(markdownText, options);
    };
    if (!renderDownlinkMutex) {
        renderDownlinkMutex = scheduledTask().finally(function () {
            renderDownlinkMutex = null;
        });
        return renderDownlinkMutex;
    }
    renderDownlinkMutex = renderDownlinkMutex.then(scheduledTask, scheduledTask);
    return renderDownlinkMutex;
}

// ---- Frontmatter Extraction & Disclosure Rendering ----

function isFrontmatterOpenFence(rawFirstLine) {
    var line = rawFirstLine.charCodeAt(0) === 0xFEFF ? rawFirstLine.slice(1) : rawFirstLine;
    return /^---[ \t]*$/.test(line);
}

function findFrontmatterCloseIndex(lines) {
    for (var i = 1; i < lines.length; i++) {
        if (/^(---|\.\.\.)[ \t]*$/.test(lines[i])) return i;
    }
    return -1;
}

function isIgnorableYamlLine(line) {
    var trimmed = line.trim();
    return trimmed === '' || trimmed.charAt(0) === '#';
}

function isTopLevelYamlKey(line) {
    return /^[A-Za-z0-9_.-]+[ \t]*:/.test(line);
}

function findNextNonEmptyIndex(lines, startIndex, endIndex) {
    for (var i = startIndex; i < endIndex; i++) {
        if (lines[i].trim() !== '') return i;
    }
    return -1;
}

function hasHeadingFollowedByProse(lines, closeIndex) {
    for (var i = 1; i < closeIndex; i++) {
        if (!/^#{1,6}[ \t]+/.test(lines[i].trim())) continue;
        var nextIdx = findNextNonEmptyIndex(lines, i + 1, closeIndex);
        if (nextIdx >= 0 && !isIgnorableYamlLine(lines[nextIdx]) && !isTopLevelYamlKey(lines[nextIdx])) return true;
    }
    return false;
}

function isPlausibleFrontmatter(lines, closeIndex) {
    if (hasHeadingFollowedByProse(lines, closeIndex)) return false;
    var sawContent = false;
    for (var i = 1; i < closeIndex; i++) {
        if (isIgnorableYamlLine(lines[i])) continue;
        sawContent = true;
        if (isTopLevelYamlKey(lines[i])) return true;
    }
    return !sawContent;
}

function unquoteYamlValue(text) {
    if (text.length < 2) return text;
    var first = text.charAt(0);
    var last = text.charAt(text.length - 1);
    if ((first === '"' && last === '"') || (first === '\'' && last === '\'')) return text.slice(1, -1);
    return text;
}

function hasUnclosedQuote(text) {
    var first = text.charAt(0);
    if (first !== '"' && first !== '\'') return false;
    return text.length === 1 || text.charAt(text.length - 1) !== first;
}

function validateScalarValue(rest, lineNum) {
    if (hasUnclosedQuote(rest)) return { line: lineNum, message: 'Unclosed quoted string (line ' + lineNum + ')' };
    if (rest.charAt(0) === '[' && rest.charAt(rest.length - 1) !== ']') return { line: lineNum, message: 'Unclosed array bracket "[" (line ' + lineNum + ')' };
    if (rest.charAt(0) === '{' && rest.slice(0, 2) !== '{{' && rest.charAt(rest.length - 1) !== '}') return { line: lineNum, message: 'Unclosed inline brace "{" (line ' + lineNum + ')' };
    return null;
}

function collectNestedBlock(yamlLines, startIndex) {
    var items = [];
    var allListItems = true;
    var idx = startIndex;
    while (idx < yamlLines.length) {
        var line = yamlLines[idx];
        if (line.charAt(0) === '\t') return { error: { line: idx + 2, message: 'Tab indentation is not allowed in YAML (line ' + (idx + 2) + ')' } };
        var isIndented = line.charAt(0) === ' ';
        var isZeroList = /^- /.test(line);
        if (!isIndented && !isZeroList && line.trim() !== '') break;
        if (line.trim() !== '') {
            var trimmed = line.trim();
            if (!/^- /.test(trimmed)) allListItems = false;
            items.push(line.replace(/^[ ]{1,2}/, ''));
        }
        idx++;
    }
    if (items.length > 0 && allListItems) {
        return { nextIndex: idx, type: 'array', value: items.map(function (s) { return unquoteYamlValue(s.trim().slice(2).trim()); }) };
    }
    return { nextIndex: idx, type: 'nested', value: items.join('\n') };
}

function parseSingleYamlEntry(key, rest) {
    if (rest.charAt(0) === '[' && rest.charAt(rest.length - 1) === ']') {
        var parts = rest.slice(1, -1).split(',').map(function (s) { return unquoteYamlValue(s.trim()); }).filter(Boolean);
        return { key: key, type: 'array', value: parts };
    }
    if (rest === 'true' || rest === 'false') return { key: key, type: 'bool', value: rest === 'true' };
    var scalar = unquoteYamlValue(rest);
    if (scalar.length > 500) scalar = scalar.slice(0, 500) + '…';
    return { key: key, type: 'scalar', value: scalar };
}

function parseFrontmatterYaml(yamlLines) {
    var entries = [];
    var idx = 0;
    while (idx < yamlLines.length) {
        var line = yamlLines[idx];
        var lineNum = idx + 2;
        if (isIgnorableYamlLine(line)) { idx++; continue; }
        if (line.charAt(0) === '\t') return { entries: [], error: { line: lineNum, message: 'Tab indentation is not allowed in YAML (line ' + lineNum + ')' } };
        var match = line.match(/^([A-Za-z0-9_.-]+)[ \t]*:(.*)$/);
        if (!match) return { entries: [], error: { line: lineNum, message: 'Expected "key: value" pair (line ' + lineNum + ')' } };
        var key = match[1];
        var rest = match[2].trim();
        var valueErr = validateScalarValue(rest, lineNum);
        if (valueErr) return { entries: [], error: valueErr };
        if (rest === '' || rest === '|' || rest === '>' || rest === '|-' || rest === '>-') {
            var block = collectNestedBlock(yamlLines, idx + 1);
            if (block.error) return { entries: [], error: block.error };
            entries.push({ key: key, type: block.type, value: block.value });
            idx = block.nextIndex;
            continue;
        }
        entries.push(parseSingleYamlEntry(key, rest));
        idx++;
    }
    return { entries: entries, error: null };
}

function extractFrontmatter(markdownText) {
    var normalized = markdownText.replace(/\r\n/g, '\n');
    var lines = normalized.split('\n');
    if (lines.length < 2 || !isFrontmatterOpenFence(lines[0])) return { found: false, body: markdownText };
    var closeIndex = findFrontmatterCloseIndex(lines);
    if (closeIndex < 0 || !isPlausibleFrontmatter(lines, closeIndex)) return { found: false, body: markdownText };
    var yamlLines = lines.slice(1, closeIndex);
    var parsed = parseFrontmatterYaml(yamlLines);
    return { found: true, yamlLines: yamlLines, entries: parsed.entries, error: parsed.error, body: lines.slice(closeIndex + 1).join('\n') };
}

function renderFrontmatterValueHtml(entry) {
    if (entry.type === 'array') {
        var chips = entry.value.map(function (v) { return '<span class="markd-frontmatter-chip">' + md.utils.escapeHtml(v) + '</span>'; }).join('');
        return '<div class="markd-frontmatter-chips">' + chips + '</div>';
    }
    if (entry.type === 'bool') {
        var boolCls = entry.value ? 'markd-frontmatter-bool' : 'markd-frontmatter-bool is-false';
        return '<span class="' + boolCls + '">' + String(entry.value) + '</span>';
    }
    if (entry.type === 'nested') return '<div class="markd-frontmatter-nested">' + md.utils.escapeHtml(entry.value) + '</div>';
    return md.utils.escapeHtml(entry.value);
}

function renderFrontmatterErrorHtml(fm) {
    var rawRows = fm.yamlLines.map(function (line, idx) {
        var lineNum = idx + 2;
        var rowCls = lineNum === fm.error.line ? 'markd-frontmatter-raw-line is-error-line' : 'markd-frontmatter-raw-line';
        return '<div class="' + rowCls + '"><span class="markd-frontmatter-ln">' + lineNum + '</span><span class="markd-frontmatter-code">' + md.utils.escapeHtml(line || ' ') + '</span></div>';
    }).join('');
    return [
        '<details class="markd-frontmatter markd-frontmatter-error" open>',
        '<summary><span class="markd-frontmatter-title"><span class="markd-frontmatter-chevron">▶</span>Frontmatter <span class="markd-frontmatter-warn">Syntax issue · line ' + fm.error.line + '</span></span><span class="markd-frontmatter-hint">Switch to Edit (⌘E) to fix</span></summary>',
        '<div class="markd-frontmatter-body"><div class="markd-frontmatter-error-bar">' + md.utils.escapeHtml(fm.error.message) + '</div><div class="markd-frontmatter-raw">' + rawRows + '</div></div>',
        '</details>'
    ].join('');
}

function renderFrontmatterHtml(fm, preserveOpen) {
    if (fm.error) return renderFrontmatterErrorHtml(fm);
    var openAttr = preserveOpen ? ' open' : '';
    var bodyHtml = '<div class="markd-frontmatter-empty">Empty Jekyll frontmatter block (0 fields)</div>';
    var hint = 'empty block';
    if (fm.entries.length > 0) {
        var rows = fm.entries.map(function (entry) {
            return '<tr><td class="markd-frontmatter-key">' + md.utils.escapeHtml(entry.key) + '</td><td class="markd-frontmatter-val">' + renderFrontmatterValueHtml(entry) + '</td></tr>';
        }).join('');
        bodyHtml = '<table class="markd-frontmatter-table"><tbody>' + rows + '</tbody></table>';
        hint = fm.entries.slice(0, 3).map(function (e) { return e.key; }).join(', ') + (fm.entries.length > 3 ? ', …' : '');
    }
    return [
        '<details class="markd-frontmatter"' + openAttr + '>',
        '<summary><span class="markd-frontmatter-title"><span class="markd-frontmatter-chevron">▶</span>Frontmatter <span class="markd-frontmatter-count">' + fm.entries.length + ' fields</span></span><span class="markd-frontmatter-hint">' + md.utils.escapeHtml(hint) + '</span></summary>',
        '<div class="markd-frontmatter-body">' + bodyHtml + '</div>',
        '</details>'
    ].join('');
}

async function executeRenderMarkdown(markdownText, options) {
    // Defensive type guard rejecting non-string payloads
    if (typeof markdownText !== 'string') {
        return;
    }

    if (markdownText === currentRenderedMarkdown && (!options || !options.force)) {
        return;
    }
    currentRenderedMarkdown = markdownText;

    // Synchronously dismiss active modal and unbind lingering diagram listeners
    closeFullView(true);
    cleanupDiagramListeners();

    var preserveBlockSizes = !!(options && (options.preserveScroll === true || options.force === true));
    if (!preserveBlockSizes) {
        blockSizeMemory = Object.create(null);
    }

    const contentEl = document.getElementById('content');
    var prevScrollY = window.scrollY || 0;
    var existingFrontmatter = contentEl.querySelector('.markd-frontmatter');
    var preserveFrontmatterOpen = !!(existingFrontmatter && existingFrontmatter.open);

    // 1. Extract optional YAML frontmatter and parse markdown body to HTML
    var fm = extractFrontmatter(markdownText);
    var frontmatterHtml = fm.found ? renderFrontmatterHtml(fm, preserveFrontmatterOpen) : '';
    var html = frontmatterHtml + md.render(fm.body);

    // 2. Sanitize and insert into DOM
    var clean = DOMPurify.sanitize(html, {
        ADD_TAGS: ['semantics', 'annotation'],
        ADD_ATTR: ['target', 'encoding', 'rel', 'role', 'aria-label', 'data-lang', 'type'],
        FORBID_TAGS: ['style'],
        ALLOW_DATA_ATTR: true,
        SANITIZE_NAMED_PROPS: false,
        RETURN_TRUSTED_TYPE: false
    });
    contentEl.innerHTML = clean;

    // 3. Transform mermaid code blocks into renderable divs
    var mermaidBlocks = contentEl.querySelectorAll('pre > code.language-mermaid');
    for (var i = 0; i < mermaidBlocks.length; i++) {
        var pre = mermaidBlocks[i].parentElement;
        var div = document.createElement('div');
        div.className = 'mermaid';
        var codeText = mermaidBlocks[i].textContent.replace(/\n$/, '');
        div.dataset.rawCode = codeText;
        div.textContent = codeText;
        pre.replaceWith(div);
    }

    // 4. Render mermaid diagrams lazily with per-diagram isolation
    var mermaidDivs = contentEl.querySelectorAll('.mermaid');
    if (mermaidDivs.length > 0) {
        await renderMermaidNodes(mermaidDivs);
    }

    // 5. Wrap mermaid diagrams in expandable containers with zoom/pan
    wrapMermaidDiagrams();

    // 6. Wrap tables in expandable containers and attach code block resize handles
    wrapTables();
    attachCodeBlockResizeHandles();

    // 7. Set up scroll spy
    setupScrollSpy();

    // 8. Resolve the post-render scroll position first (instant), so the reveal lands in place
    resolvePostRenderScroll(options, prevScrollY);

    // 9. Reveal the content in the same frame as the DOM and scroll changes (Task 8.12)
    document.body.classList.remove('is-hydrating');

    // 10. Notify C++ bridge of document render completion
    var headings = contentEl.querySelectorAll('h1[id], h2[id], h3[id], h4[id], h5[id], h6[id]');
    if (window.markdBridge && typeof window.markdBridge.onDocumentRendered === 'function') {
        window.markdBridge.onDocumentRendered(headings.length);
    }
}

function replaceWithMermaidErrorCard(mDiv, rawDiagramCode, err) {
    console.error('Mermaid render error:', err);
    var errorCard = document.createElement('div');
    errorCard.className = 'mermaid-error-card';
    var badge = document.createElement('div');
    badge.className = 'mermaid-error-badge';
    badge.textContent = 'Mermaid Syntax Error: ' + (err && err.message ? err.message : 'Invalid diagram syntax');
    var rawPre = document.createElement('pre');
    rawPre.className = 'mermaid-raw-code hljs';
    var rawCodeNode = document.createElement('code');
    rawCodeNode.textContent = rawDiagramCode;
    rawPre.appendChild(rawCodeNode);
    errorCard.appendChild(badge);
    errorCard.appendChild(rawPre);
    mDiv.replaceWith(errorCard);
}

function scrubMermaidStyleElement(styleNode) {
    var safeCss = styleNode.textContent
        .replace(/position\s*:\s*(?:fixed|absolute)/gi, 'position: static')
        .replace(/@import\b[^;]*;?/gi, '');
    styleNode.textContent = safeCss;
}

function sanitizeCompiledMermaidSvg(mDiv, svgEl) {
    mDiv.innerHTML = DOMPurify.sanitize(svgEl.outerHTML, {
        USE_PROFILES: { html: true, svg: true, svgFilters: true },
        ADD_TAGS: ['foreignObject'],
        ADD_ATTR: ['dominant-baseline', 'xmlns'],
        HTML_INTEGRATION_POINTS: { foreignobject: true }
    });
    var styles = mDiv.querySelectorAll('style');
    for (var i = 0; i < styles.length; i++) {
        scrubMermaidStyleElement(styles[i]);
    }
    mDiv.setAttribute('data-processed', 'true');
}

async function renderSingleMermaidNode(mermaidApi, mDiv) {
    var rawDiagramCode = mDiv.dataset.rawCode;
    try {
        await mermaidApi.run({ nodes: [mDiv] });
        if (!mDiv.isConnected) return;
        var svgEl = mDiv.querySelector('svg');
        if (svgEl) {
            sanitizeCompiledMermaidSvg(mDiv, svgEl);
        }
    } catch (e) {
        replaceWithMermaidErrorCard(mDiv, rawDiagramCode, e);
    }
}

function replaceAllMermaidWithErrors(mermaidDivs, err) {
    for (var k = 0; k < mermaidDivs.length; k++) {
        replaceWithMermaidErrorCard(mermaidDivs[k], mermaidDivs[k].dataset.rawCode, err);
    }
}

async function resolveMermaidApi(mermaidDivs) {
    try {
        return await ensureMermaidLoaded();
    } catch (loadErr) {
        replaceAllMermaidWithErrors(mermaidDivs, loadErr);
        return null;
    }
}

async function renderMermaidNodes(mermaidDivs) {
    var mermaidApi = typeof window.mermaid !== 'undefined'
        ? window.mermaid
        : await resolveMermaidApi(mermaidDivs);
    if (!mermaidApi) return;
    for (var j = 0; j < mermaidDivs.length; j++) {
        var mDiv = mermaidDivs[j];
        if (!mDiv.isConnected) continue;
        await renderSingleMermaidNode(mermaidApi, mDiv);
    }
}

// ---- Scroll spy ----

var scrollSpyObserver = null;

function setupScrollSpy() {
    if (scrollSpyObserver) {
        scrollSpyObserver.disconnect();
    }

    var headings = document.querySelectorAll('#content h1[id], #content h2[id], #content h3[id], #content h4[id], #content h5[id], #content h6[id]');
    if (headings.length === 0) return;

    scrollSpyObserver = new IntersectionObserver(function (entries) {
        var topHeading = null;
        var topY = Infinity;

        entries.forEach(function (entry) {
            if (entry.isIntersecting && entry.boundingClientRect.top < topY) {
                topY = entry.boundingClientRect.top;
                topHeading = entry.target;
            }
        });

        if (topHeading) {
            if (window.session && typeof window.session.reportActiveHeading === 'function') {
                window.session.reportActiveHeading(topHeading.id);
            }
            if (window.markdBridge && typeof window.markdBridge.onActiveHeadingChanged === 'function') {
                window.markdBridge.onActiveHeadingChanged(topHeading.id);
            } else if (window.markdBridge && typeof window.markdBridge.setActiveHeading === 'function') {
                window.markdBridge.setActiveHeading(topHeading.id);
            }
        }
    }, {
        rootMargin: '0px 0px -70% 0px',
        threshold: 0
    });

    headings.forEach(function (h) {
        scrollSpyObserver.observe(h);
    });
}

// ---- Scroll to heading ----

var lastScrolledHeadingId = null;
var lastScrollTimestamp = 0;

function scrollToHeading(id, behavior) {
    var now = Date.now();
    if (id === lastScrolledHeadingId && now - lastScrollTimestamp < 150) {
        return;
    }
    lastScrolledHeadingId = id;
    lastScrollTimestamp = now;

    var el = document.getElementById(id);
    if (el) {
        // Smooth for user navigation (TOC, anchors); callers restoring a position pass 'instant'
        el.scrollIntoView({ behavior: behavior || 'smooth', block: 'start' });
    }
}

function scrollToRatio(ratio) {
    if (typeof ratio !== 'number' || isNaN(ratio)) return;
    var maxScroll = Math.max(0, document.documentElement.scrollHeight - window.innerHeight);
    var clampedRatio = Math.max(0, Math.min(1, ratio));
    window.scrollTo({ top: clampedRatio * maxScroll, behavior: 'instant' });
}

function resolvePostRenderScroll(options, prevScrollY) {
    if (options && options.preserveScroll === true) {
        window.scrollTo(0, prevScrollY);
        return;
    }
    if (options && options.targetHeading) {
        scrollToHeading(options.targetHeading, 'instant');
        return;
    }
    if (options && typeof options.scrollRatio === 'number') {
        scrollToRatio(options.scrollRatio);
        return;
    }
    window.scrollTo(0, 0);
}

// ---- Inline Block Resize (Center-Anchored Breakout & Vertical Aperture) ----

var blockSizeMemory = Object.create(null);
var activeBlockResizeCleanup = null;
var BLOCK_RESIZE_CURSORS = {
    w: 'ew-resize',
    e: 'ew-resize',
    s: 'ns-resize',
    sw: 'nesw-resize',
    se: 'nwse-resize'
};

function stopActiveBlockResize() {
    if (!activeBlockResizeCleanup) return;
    var cleanup = activeBlockResizeCleanup;
    activeBlockResizeCleanup = null;
    cleanup();
}

function clampBlockWidth(rawWidth) {
    var viewportWidth = window.innerWidth || 1024;
    var maxWidth = Math.max(280, viewportWidth - 48);
    var minWidth = Math.min(420, maxWidth);
    return Math.round(Math.max(minWidth, Math.min(maxWidth, rawWidth)));
}

function clampBlockHeight(rawHeight) {
    var viewportHeight = window.innerHeight || 800;
    var maxHeight = Math.max(220, Math.round(viewportHeight * 0.85));
    return Math.round(Math.max(220, Math.min(maxHeight, rawHeight)));
}

function measureBlockWidth(container) {
    var rectWidth = container.getBoundingClientRect().width;
    return rectWidth > 0 ? rectWidth : 664;
}

function measureBlockHeight(target) {
    var rectHeight = target.getBoundingClientRect().height;
    return rectHeight > 0 ? rectHeight : 320;
}

function applyBlockDimensions(container, verticalTarget, blockKey, width, height) {
    var entry = blockSizeMemory[blockKey] || {};
    if (typeof width === 'number') {
        var clampedW = clampBlockWidth(width);
        container.style.width = clampedW + 'px';
        container.classList.add('is-resized-x');
        entry.width = clampedW;
    }
    if (verticalTarget && typeof height === 'number') {
        var clampedH = clampBlockHeight(height);
        verticalTarget.style.height = clampedH + 'px';
        container.classList.add('is-resized-y');
        entry.height = clampedH;
    }
    blockSizeMemory[blockKey] = entry;
}

function resetBlockDimensions(container, verticalTarget, blockKey) {
    container.style.width = '';
    container.classList.remove('is-resized-x');
    container.classList.remove('is-resized-y');
    if (verticalTarget) {
        verticalTarget.style.height = '';
    }
    delete blockSizeMemory[blockKey];
}

function computeNextWidth(axis, startWidth, dx) {
    if (axis.indexOf('e') !== -1) return startWidth + (2 * dx);
    if (axis.indexOf('w') !== -1) return startWidth - (2 * dx);
    return null;
}

function computeNextHeight(axis, startHeight, dy) {
    if (axis.indexOf('s') !== -1) return startHeight + dy;
    return null;
}

function startBlockResizeDrag(event, container, verticalTarget, blockKey, axis) {
    if (event.button !== 0) return;
    event.preventDefault();
    event.stopPropagation();
    stopActiveBlockResize();

    var startX = event.clientX;
    var startY = event.clientY;
    var startWidth = measureBlockWidth(container);
    var startHeight = verticalTarget ? measureBlockHeight(verticalTarget) : 0;

    container.classList.add('is-resizing');
    document.body.classList.add('is-block-resizing');
    document.body.style.cursor = BLOCK_RESIZE_CURSORS[axis];

    var onMove = function (moveEvent) {
        var nextW = computeNextWidth(axis, startWidth, moveEvent.clientX - startX);
        var nextH = computeNextHeight(axis, startHeight, moveEvent.clientY - startY);
        applyBlockDimensions(container, verticalTarget, blockKey, nextW, nextH);
    };

    var finish = function () {
        document.removeEventListener('mousemove', onMove);
        document.removeEventListener('mouseup', finish);
        document.removeEventListener('mouseleave', finish);
        window.removeEventListener('blur', finish);
        container.classList.remove('is-resizing');
        document.body.classList.remove('is-block-resizing');
        document.body.style.cursor = '';
        activeBlockResizeCleanup = null;
    };

    activeBlockResizeCleanup = finish;
    document.addEventListener('mousemove', onMove);
    document.addEventListener('mouseup', finish);
    document.addEventListener('mouseleave', finish);
    window.addEventListener('blur', finish);
}

function createResizeHandle(container, verticalTarget, blockKey, axis) {
    var handle = document.createElement('div');
    handle.className = 'block-resize-handle block-resize-handle-' + axis;
    handle.setAttribute('data-resize-axis', axis);
    handle.setAttribute('role', 'separator');
    handle.setAttribute('aria-label', 'Resize block (' + axis + ')');
    handle.title = 'Drag to resize · Double-click to reset';
    handle.addEventListener('mousedown', function (e) {
        startBlockResizeDrag(e, container, verticalTarget, blockKey, axis);
    });
    handle.addEventListener('dblclick', function (e) {
        e.preventDefault();
        e.stopPropagation();
        resetBlockDimensions(container, verticalTarget, blockKey);
    });
    return handle;
}

function attachBlockResizeHandles(container, verticalTarget, blockKey) {
    if (container.querySelector('.block-resize-handle')) return;
    var axes = verticalTarget ? ['w', 'e', 's', 'sw', 'se'] : ['w', 'e'];
    axes.forEach(function (axis) {
        container.appendChild(createResizeHandle(container, verticalTarget, blockKey, axis));
    });
    var saved = blockSizeMemory[blockKey];
    if (saved) {
        applyBlockDimensions(container, verticalTarget, blockKey, saved.width, saved.height);
    }
}

function attachCodeBlockResizeHandles() {
    var cards = document.querySelectorAll('#content .code-block-card');
    cards.forEach(function (card, index) {
        attachBlockResizeHandles(card, null, 'code:' + index);
    });
}

// ---- Expandable containers ----

function wrapMermaidDiagrams() {
    var diagrams = document.querySelectorAll('#content .mermaid');
    diagrams.forEach(function (diagram, index) {
        if (diagram.closest('.expandable-container')) return;

        var container = document.createElement('div');
        container.className = 'expandable-container';

        // Toolbar with zoom controls + fullview
        var toolbar = document.createElement('div');
        toolbar.className = 'expandable-toolbar';
        toolbar.innerHTML =
            '<button class="zoom-out-btn" title="Zoom out">−</button>' +
            '<span class="zoom-label">100%</span>' +
            '<button class="zoom-in-btn" title="Zoom in">+</button>' +
            '<button class="zoom-reset-btn" title="Reset zoom &amp; size">↺</button>' +
            '<button class="fullview-btn" title="Full view">⛶</button>';

        // Viewport for pan/zoom
        var viewport = document.createElement('div');
        viewport.className = 'mermaid-viewport';
        var inner = document.createElement('div');
        inner.className = 'mermaid-inner';

        diagram.parentNode.insertBefore(container, diagram);
        inner.appendChild(diagram);
        viewport.appendChild(inner);
        container.appendChild(toolbar);
        container.appendChild(viewport);

        var blockKey = 'mermaid:' + index;
        setupDiagramZoomPan(container, viewport, inner, blockKey);
        attachBlockResizeHandles(container, viewport, blockKey);
    });
}

function wrapTables() {
    var tables = document.querySelectorAll('#content > table');
    tables.forEach(function (table, index) {
        if (table.parentElement.classList.contains('table-wrapper')) return;

        var wrapper = document.createElement('div');
        wrapper.className = 'table-wrapper';

        var btn = document.createElement('button');
        btn.className = 'table-expand-btn';
        btn.title = 'Full view';
        btn.textContent = '⛶';

        table.parentNode.insertBefore(wrapper, table);
        wrapper.appendChild(btn);
        wrapper.appendChild(table);

        btn.addEventListener('click', function () {
            openFullView(table.cloneNode(true), false);
        });

        attachBlockResizeHandles(wrapper, null, 'table:' + index);
    });
}

// ---- Diagram zoom/pan ----

function setupDiagramZoomPan(container, viewport, inner, blockKey) {
    var scale = 1;
    var panX = 0;
    var panY = 0;
    var isPanning = false;
    var startX, startY, startPanX, startPanY;
    var label = container.querySelector('.zoom-label');

    function applyTransform(smooth) {
        if (Math.abs(scale - 1) < 0.02) {
            scale = 1;
        }
        inner.style.transition = smooth ? 'transform 0.15s ease-out' : 'none';
        inner.style.transform = 'translate(' + panX + 'px,' + panY + 'px) scale(' + scale + ')';
        label.textContent = Math.round(scale * 100) + '%';
    }

    function clampPan(x, y) {
        var limitX = Math.max(500, (viewport.clientWidth || 600) * scale);
        var limitY = Math.max(500, (viewport.clientHeight || 400) * scale);
        return {
            x: Math.max(-limitX, Math.min(limitX, x)),
            y: Math.max(-limitY, Math.min(limitY, y))
        };
    }

    container.querySelector('.zoom-in-btn').addEventListener('click', function () {
        scale = Math.min(scale * 1.25, 5);
        applyTransform(true);
    });

    container.querySelector('.zoom-out-btn').addEventListener('click', function () {
        scale = Math.max(scale / 1.25, 0.2);
        applyTransform(true);
    });

    container.querySelector('.zoom-reset-btn').addEventListener('click', function () {
        scale = 1; panX = 0; panY = 0;
        applyTransform(true);
        resetBlockDimensions(container, viewport, blockKey);
    });

    container.querySelector('.fullview-btn').addEventListener('click', function () {
        openFullView(inner.cloneNode(true), true);
    });

    // Mouse wheel zoom with zero-delta guard
    viewport.addEventListener('wheel', function (e) {
        e.preventDefault();
        if (Math.abs(e.deltaY) < 1e-3) return;
        var factor = e.deltaY < 0 ? 1.1 : 0.9;
        scale = Math.max(0.2, Math.min(5, scale * factor));
        applyTransform(false);
    }, { passive: false });

    // Pan with mouse drag
    viewport.addEventListener('mousedown', function (e) {
        isPanning = true;
        startX = e.clientX;
        startY = e.clientY;
        startPanX = panX;
        startPanY = panY;
        e.preventDefault();
    });

    var onMouseMove = function (e) {
        if (!isPanning) return;
        var clamped = clampPan(startPanX + (e.clientX - startX), startPanY + (e.clientY - startY));
        panX = clamped.x;
        panY = clamped.y;
        applyTransform(false);
    };

    var onMouseUp = function () {
        isPanning = false;
    };

    var onPointerExit = function () {
        isPanning = false;
    };

    document.addEventListener('mousemove', onMouseMove);
    document.addEventListener('mouseup', onMouseUp);
    document.addEventListener('mouseleave', onPointerExit);
    window.addEventListener('blur', onPointerExit);

    diagramTeardowns.push(function () {
        document.removeEventListener('mousemove', onMouseMove);
        document.removeEventListener('mouseup', onMouseUp);
        document.removeEventListener('mouseleave', onPointerExit);
        window.removeEventListener('blur', onPointerExit);
    });

    applyTransform(false);
}

// ---- Full-view overlay ----

var activeFullViewClose = null;

function closeFullView(immediate) {
    if (activeFullViewClose) {
        activeFullViewClose(immediate);
    }
}

function openFullView(contentNode, isDiagram) {
    // Singleton modal lifecycle: synchronously close and unbind any existing overlay
    if (activeFullViewClose) {
        activeFullViewClose(true);
    }

    var existingOverlays = document.querySelectorAll('.fullview-overlay');
    for (var i = 0; i < existingOverlays.length; i++) {
        existingOverlays[i].remove();
    }

    var previousFocusedElement = document.activeElement;

    // Lock body scroll
    document.body.classList.add('modal-open');
    document.body.style.overflow = 'hidden';

    var overlay = document.createElement('div');
    overlay.className = 'fullview-overlay';

    var toolbar = document.createElement('div');
    toolbar.className = 'fullview-toolbar';

    if (isDiagram) {
        toolbar.innerHTML =
            '<button class="zoom-out-btn" title="Zoom out">−</button>' +
            '<span class="zoom-label" style="font-size:11px;color:var(--text-secondary);min-width:36px;text-align:center">100%</span>' +
            '<button class="zoom-in-btn" title="Zoom in">+</button>' +
            '<button class="zoom-reset-btn" title="Reset zoom">↺</button>' +
            '<span style="flex:1"></span>' +
            '<button class="close-btn">✕ Close</button>';
    } else {
        toolbar.innerHTML =
            '<span style="flex:1"></span>' +
            '<button class="close-btn">✕ Close</button>';
    }

    var body = document.createElement('div');
    body.className = 'fullview-body' + (isDiagram ? ' is-diagram' : '');

    var inner = document.createElement('div');
    inner.className = 'fullview-inner';
    if (isDiagram && contentNode && contentNode.style) {
        contentNode.style.transform = '';
        contentNode.style.transition = '';
    }
    inner.appendChild(contentNode);
    body.appendChild(inner);

    overlay.appendChild(toolbar);
    overlay.appendChild(body);
    document.body.appendChild(overlay);

    // Animate in
    requestAnimationFrame(function () {
        overlay.classList.add('visible');
    });

    var fvMouseMoveHandler = null;
    var fvMouseUpHandler = null;
    var fvExitHandler = null;

    // Close handler
    function closeOverlay(immediate) {
        activeFullViewClose = null;
        document.body.classList.remove('modal-open');
        document.body.style.overflow = '';
        overlay.classList.remove('visible');
        document.removeEventListener('keydown', onKeyDown);
        if (fvMouseMoveHandler) document.removeEventListener('mousemove', fvMouseMoveHandler);
        if (fvMouseUpHandler) document.removeEventListener('mouseup', fvMouseUpHandler);
        if (fvExitHandler) {
            document.removeEventListener('mouseleave', fvExitHandler);
            window.removeEventListener('blur', fvExitHandler);
        }
        if (previousFocusedElement && typeof previousFocusedElement.focus === 'function') {
            previousFocusedElement.focus();
        }
        if (immediate) {
            overlay.remove();
        } else {
            setTimeout(function () { overlay.remove(); }, 200);
        }
    }
    activeFullViewClose = closeOverlay;

    toolbar.querySelector('.close-btn').addEventListener('click', function () {
        closeOverlay(false);
    });

    // Keyboard handlers (Escape to close, Tab focus trapping)
    function onKeyDown(e) {
        if (e.key === 'Escape') {
            closeOverlay(false);
            return;
        }
        if (e.key === 'Tab') {
            var focusable = overlay.querySelectorAll('button, [href], input, select, textarea, [tabindex]:not([tabindex="-1"])');
            if (focusable.length === 0) return;
            var first = focusable[0];
            var last = focusable[focusable.length - 1];
            if (e.shiftKey) {
                if (document.activeElement === first || !overlay.contains(document.activeElement)) {
                    e.preventDefault();
                    last.focus();
                }
                return;
            }
            if (document.activeElement === last || !overlay.contains(document.activeElement)) {
                e.preventDefault();
                first.focus();
                return;
            }
        }
    }
    document.addEventListener('keydown', onKeyDown);

    // Diagram zoom/pan in fullview
    if (isDiagram) {
        var fvScale = 1, fvPanX = 0, fvPanY = 0;
        var fvIsPanning = false, fvStartX, fvStartY, fvStartPanX, fvStartPanY;
        var fvLabel = toolbar.querySelector('.zoom-label');

        function fvApply(smooth) {
            if (Math.abs(fvScale - 1) < 0.02) {
                fvScale = 1;
            }
            inner.style.transition = smooth ? 'transform 0.15s ease-out' : 'none';
            inner.style.transform = 'translate(' + fvPanX + 'px,' + fvPanY + 'px) scale(' + fvScale + ')';
            fvLabel.textContent = Math.round(fvScale * 100) + '%';
        }

        function fvClampPan(x, y) {
            var limitX = Math.max(500, (body.clientWidth || 800) * fvScale);
            var limitY = Math.max(500, (body.clientHeight || 600) * fvScale);
            return {
                x: Math.max(-limitX, Math.min(limitX, x)),
                y: Math.max(-limitY, Math.min(limitY, y))
            };
        }

        toolbar.querySelector('.zoom-in-btn').addEventListener('click', function () {
            fvScale = Math.min(fvScale * 1.25, 5); fvApply(true);
        });
        toolbar.querySelector('.zoom-out-btn').addEventListener('click', function () {
            fvScale = Math.max(fvScale / 1.25, 0.2); fvApply(true);
        });
        toolbar.querySelector('.zoom-reset-btn').addEventListener('click', function () {
            fvScale = 1; fvPanX = 0; fvPanY = 0; fvApply(true);
        });

        body.addEventListener('wheel', function (e) {
            e.preventDefault();
            if (Math.abs(e.deltaY) < 1e-3) return;
            var factor = e.deltaY < 0 ? 1.1 : 0.9;
            fvScale = Math.max(0.2, Math.min(5, fvScale * factor));
            fvApply(false);
        }, { passive: false });

        body.addEventListener('mousedown', function (e) {
            fvIsPanning = true;
            fvStartX = e.clientX; fvStartY = e.clientY;
            fvStartPanX = fvPanX; fvStartPanY = fvPanY;
            e.preventDefault();
        });

        fvMouseMoveHandler = function (e) {
            if (!fvIsPanning) return;
            var clamped = fvClampPan(fvStartPanX + (e.clientX - fvStartX), fvStartPanY + (e.clientY - fvStartY));
            fvPanX = clamped.x;
            fvPanY = clamped.y;
            fvApply(false);
        };
        fvMouseUpHandler = function () {
            fvIsPanning = false;
        };
        fvExitHandler = function () {
            fvIsPanning = false;
        };

        document.addEventListener('mousemove', fvMouseMoveHandler);
        document.addEventListener('mouseup', fvMouseUpHandler);
        document.addEventListener('mouseleave', fvExitHandler);
        window.addEventListener('blur', fvExitHandler);

        fvApply(false);
    }
}

// ---- Structured Engine API (window.Markd) ----

window.Markd = {
    version: "1.0.0",
    isReady: false,
    render: renderMarkdown,
    scrollToHeading: scrollToHeading,
    scrollToRatio: scrollToRatio,
    modal: {
        open: openFullView,
        close: closeFullView
    },
    get bridge() {
        return window.markdBridge;
    },
    get session() {
        return window.session;
    }
};

// Backward-compatibility aliases on global window
window.renderMarkdown = renderMarkdown;
window.scrollToHeading = scrollToHeading;
window.scrollToRatio = scrollToRatio;
window.openFullView = openFullView;
window.closeFullView = closeFullView;
window.setupScrollSpy = setupScrollSpy;
window.wrapMermaidDiagrams = wrapMermaidDiagrams;
window.wrapTables = wrapTables;
window.attachCodeBlockResizeHandles = attachCodeBlockResizeHandles;
window.handleSessionMarkdownChanged = handleSessionMarkdownChanged;

// Signal engine readiness once DOM is fully prepared
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', function () {
        window.Markd.isReady = true;
    });
} else {
    window.Markd.isReady = true;
}
interface PendingRenderSignal {
    markdown: unknown;
    options?: RenderOptions;
}

interface FrontmatterError {
    line: number;
    message: string;
}

type FrontmatterEntry =
    | { key: string; type: 'array'; value: string[] }
    | { key: string; type: 'bool'; value: boolean }
    | { key: string; type: 'nested'; value: string }
    | { key: string; type: 'scalar'; value: string };

type NestedBlockResult =
    | { error: FrontmatterError }
    | { error?: undefined; nextIndex: number; type: 'array'; value: string[] }
    | { error?: undefined; nextIndex: number; type: 'nested'; value: string };

interface ParsedFrontmatterYaml {
    entries: FrontmatterEntry[];
    error: FrontmatterError | null;
}

interface FoundFrontmatter {
    found: true;
    yamlLines: string[];
    entries: FrontmatterEntry[];
    error: FrontmatterError | null;
    body: string;
}

type FrontmatterExtraction =
    | { found: false; body: string }
    | FoundFrontmatter;

type ResizeAxis = 'n' | 'w' | 'e' | 's' | 'nw' | 'ne' | 'sw' | 'se';

interface BlockSizeEntry {
    width?: number;
    height?: number;
}

interface PanCoordinates {
    x: number;
    y: number;
}

// Prevent DOM clobbering of reserved bridge properties
if (typeof DOMPurify !== 'undefined' && typeof DOMPurify.addHook === 'function') {
    DOMPurify.addHook('afterSanitizeAttributes', function (node: Element): void {
        if (node.hasAttribute('id')) {
            const id = node.getAttribute('id');
            if (id === 'markdBridge' || id === 'session' || id === 'Markd' || id === 'renderDocument') {
                node.setAttribute('id', 'user-content-' + id);
            }
        }
        if (node.hasAttribute('name')) {
            const name = node.getAttribute('name');
            if (name === 'markdBridge' || name === 'session' || name === 'Markd' || name === 'renderDocument') {
                node.setAttribute('name', 'user-content-' + name);
            }
        }
    });
}

// QWebChannel Bridge setup for Qt C++ IPC
window.markdBridge = null;
window.session = null;

let isBridgeConnected = false;
const pendingRenderSignals: PendingRenderSignal[] = [];

function queueOrRenderDocument(markdown: unknown, options?: RenderOptions): void {
    if (!isBridgeConnected && !window.markdBridge) {
        pendingRenderSignals.push({ markdown: markdown, options: options });
        return;
    }
    renderMarkdown(markdown, options);
}
window.renderDocument = queueOrRenderDocument;

function handleSessionMarkdownChanged(newMarkdown: string): void {
    if (window.markdBridge && window.markdBridge.renderDocument) {
        return;
    }
    if (newMarkdown === currentRenderedMarkdown) {
        return;
    }
    renderMarkdown(newMarkdown);
}

function initQWebChannel(): void {
    if (typeof QWebChannel !== 'undefined' && typeof qt !== 'undefined' && qt.webChannelTransport) {
        new QWebChannel(qt.webChannelTransport, function (channel: QWebChannelResult): void {
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
                    window.markdBridge.renderDocument.connect(function (markdown: unknown, options?: RenderOptions): void {
                        if (typeof renderMarkdown === 'function') {
                            renderMarkdown(markdown, options);
                        }
                    });
                }
                if (window.markdBridge.navigateToHeading && typeof window.markdBridge.navigateToHeading.connect === 'function') {
                    window.markdBridge.navigateToHeading.connect(function (headingId: string, behavior?: ScrollBehavior): void {
                        if (typeof scrollToHeading === 'function') {
                            scrollToHeading(headingId, behavior);
                        }
                    });
                }
                if (window.markdBridge.setTheme && typeof window.markdBridge.setTheme.connect === 'function') {
                    window.markdBridge.setTheme.connect(function (themeName: string): void {
                        syncMermaidTheme(themeName);
                    });
                }
                if (window.markdBridge.scrollToRatio && typeof window.markdBridge.scrollToRatio.connect === 'function') {
                    window.markdBridge.scrollToRatio.connect(function (ratio: unknown): void {
                        if (typeof scrollToRatio === 'function') {
                            scrollToRatio(ratio);
                        }
                    });
                }

                // Flush signals buffered before QWebChannel initialization completed
                while (pendingRenderSignals.length > 0) {
                    const queued = pendingRenderSignals.shift()!;
                    renderMarkdown(queued.markdown, queued.options);
                }

                // Two-way handshake: notify C++ that bridge is initialized and ready
                if (typeof window.markdBridge.bridgeReady === 'function') {
                    window.markdBridge.bridgeReady();
                }
            }
            if (window.session) {
                if (window.session.markdownChanged && typeof window.session.markdownChanged.connect === 'function') {
                    window.session.markdownChanged.connect(function (newMarkdown: string): void {
                        handleSessionMarkdownChanged(newMarkdown);
                    });
                }
                if (window.session.selectedHeadingChanged && typeof window.session.selectedHeadingChanged.connect === 'function') {
                    window.session.selectedHeadingChanged.connect(function (headingId: string): void {
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

function dispatchClipboardWrite(rawCode: string): void {
    if (window.session && typeof window.session.copyToClipboard === 'function') {
        window.session.copyToClipboard(rawCode);
        return;
    }
    if (navigator.clipboard && typeof navigator.clipboard.writeText === 'function') {
        navigator.clipboard.writeText(rawCode);
    }
}

function handleCodeCopyClick(copyBtn: CopyButtonElement): void {
    const card = copyBtn.closest('.code-block-card');
    const codeEl = card ? card.querySelector('pre > code') : null;
    if (!codeEl) return;
    const rawCode = codeEl.textContent!.replace(/\n$/, '');
    dispatchClipboardWrite(rawCode);
    copyBtn.textContent = 'Copied!';
    copyBtn.classList.add('copied');
    if (copyBtn._copyTimeoutId) {
        clearTimeout(copyBtn._copyTimeoutId);
    }
    copyBtn._copyTimeoutId = setTimeout(function (): void {
        copyBtn.textContent = 'Copy';
        copyBtn.classList.remove('copied');
        copyBtn._copyTimeoutId = null;
    }, 1500);
}

// Global anchor navigation interception & code card copy delegation (Tier 1 WebEngine Guardrail)
document.addEventListener('click', function (e: MouseEvent): void {
    const target = e.target as (EventTarget & { closest?: (selector: string) => Element | null }) | null;
    if (!target || typeof target.closest !== 'function') return;
    const copyBtn = target.closest('.copy-btn') as CopyButtonElement | null;
    if (copyBtn) {
        handleCodeCopyClick(copyBtn);
        return;
    }
    const anchor = target.closest('a');
    if (!anchor) return;
    const href = anchor.getAttribute('href');
    if (!href) return;
    if (href.startsWith('#')) {
        e.preventDefault();
        const targetId = decodeURIComponent(href.slice(1));
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
    DOMPurify.addHook('afterSanitizeAttributes', function (node: Element): void {
        if (node.tagName === 'A') {
            const target = node.getAttribute('target');
            const href = node.getAttribute('href') || '';
            const isOutbound = target === '_blank' || /^(?:https?:\/\/|\/\/)/i.test(href);
            if (isOutbound) {
                node.setAttribute('rel', 'noopener noreferrer');
            }
        }
        if (node.hasAttribute('style')) {
            const style = node.getAttribute('style')!;
            if (/position\s*:\s*(?:fixed|absolute)/i.test(style)) {
                const cleanStyle = style.replace(/position\s*:\s*(?:fixed|absolute);?/gi, '').trim();
                if (cleanStyle) {
                    node.setAttribute('style', cleanStyle);
                } else {
                    node.removeAttribute('style');
                }
            }
        }
    });
}

function formatCodeBlock(codeHtml: string, lang: string): string {
    const safeLang = md.utils.escapeHtml(lang);
    return '<div class="code-block-card" data-lang="' + safeLang + '" role="region" aria-label="Code snippet: ' + safeLang + '">' +
        '<div class="code-block-header">' +
        '<span class="code-lang">' + safeLang + '</span>' +
        '<button class="copy-btn" type="button" aria-label="Copy code to clipboard">Copy</button>' +
        '</div>' +
        '<pre class="hljs"><code>' + codeHtml + '</code></pre>' +
        '</div>';
}

function highlightCodeBlock(str: string, normalizedLang: string): string {
    if (!hljs.getLanguage(normalizedLang)) {
        return formatCodeBlock(md.utils.escapeHtml(str), normalizedLang);
    }
    try {
        const highlighted = hljs.highlight(str, { language: normalizedLang, ignoreIllegals: true }).value;
        return formatCodeBlock(highlighted, normalizedLang);
    } catch (_) {
        return formatCodeBlock(md.utils.escapeHtml(str), normalizedLang);
    }
}

// Initialize markdown-it with plugins
const md: MarkdownItInstance = window.markdownit({
    html: true,
    linkify: true,
    typographer: true
});

md.renderer.rules.fence = function (tokens: MarkdownItToken[], idx: number): string {
    const token = tokens[idx];
    const info = md.utils.unescapeAll(token.info).trim();
    const lang = info.split(/\s+/g)[0].toLowerCase();
    if (lang === 'mermaid') {
        return '<pre><code class="language-mermaid">' + md.utils.escapeHtml(token.content) + '</code></pre>\n';
    }
    const normalizedLang = lang || 'text';
    return highlightCodeBlock(token.content, normalizedLang) + '\n';
};

// Add anchor plugin for heading IDs with Unicode property slugification
if (window.markdownItAnchor) {
    md.use(window.markdownItAnchor, {
        permalink: false,
        slugify: function (s: string): string {
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
let activeMermaidTheme: string = window.matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'default';
let mermaidLoadPromise: Promise<MermaidApi> | null = null;

function applyMermaidConfig(): void {
    window.mermaid!.initialize({
        startOnLoad: false,
        theme: activeMermaidTheme,
        securityLevel: 'strict',
        fontFamily: '-apple-system, BlinkMacSystemFont, "SF Pro Text", sans-serif'
    });
}

if (typeof window.mermaid !== 'undefined') {
    applyMermaidConfig();
}

function ensureMermaidLoaded(): Promise<MermaidApi> {
    if (typeof window.mermaid !== 'undefined') {
        return Promise.resolve(window.mermaid);
    }
    if (mermaidLoadPromise) {
        return mermaidLoadPromise;
    }
    const pending: Promise<MermaidApi> = new Promise<MermaidApi>(function (resolve, reject): void {
        const script = document.createElement('script');
        script.src = 'vendor/mermaid/mermaid.min.js';
        script.setAttribute('data-mermaid-lazy', '1');
        script.onload = function (): void {
            applyMermaidConfig();
            resolve(window.mermaid!);
        };
        script.onerror = function (): void {
            script.remove();
            reject(new Error('Failed to load Mermaid bundle'));
        };
        document.head.appendChild(script);
    }).catch(function (err: unknown): never {
        mermaidLoadPromise = null;
        throw err;
    });
    mermaidLoadPromise = pending;
    return pending;
}
window.ensureMermaidLoaded = ensureMermaidLoaded;

function isDarkMermaidPalette(themeName: string): boolean {
    const lower = themeName.toLowerCase();
    return lower.indexOf('dark') !== -1 || lower.indexOf('obsidian') !== -1;
}

function syncMermaidTheme(themeName?: string): void {
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
window.matchMedia('(prefers-color-scheme: dark)').addEventListener('change', function (e: MediaQueryListEvent): void {
    syncMermaidTheme(e.matches ? 'dark' : 'default');
});

// ---- Diagram Event Listener Teardown Registry ----
const diagramTeardowns: Array<() => void> = [];

function cleanupDiagramListeners(): void {
    stopActiveBlockResize();
    while (diagramTeardowns.length > 0) {
        const teardown = diagramTeardowns.pop()!;
        teardown();
    }
}
window.cleanupDiagramListeners = cleanupDiagramListeners;

// ---- Async Downlink Mutex Queue ----
let renderDownlinkMutex: Promise<void> | null = null;
let currentRenderedMarkdown: string | null = null;

function renderMarkdown(markdownText: unknown, options?: RenderOptions): Promise<void> {
    const scheduledTask = function (): Promise<void> {
        return executeRenderMarkdown(markdownText, options);
    };
    if (!renderDownlinkMutex) {
        renderDownlinkMutex = scheduledTask().finally(function (): void {
            renderDownlinkMutex = null;
        });
        return renderDownlinkMutex;
    }
    renderDownlinkMutex = renderDownlinkMutex.then(scheduledTask, scheduledTask);
    return renderDownlinkMutex;
}

// ---- Frontmatter Extraction & Disclosure Rendering ----

function isFrontmatterOpenFence(rawFirstLine: string): boolean {
    const line = rawFirstLine.charCodeAt(0) === 0xFEFF ? rawFirstLine.slice(1) : rawFirstLine;
    return /^---[ \t]*$/.test(line);
}

function findFrontmatterCloseIndex(lines: string[]): number {
    for (let i = 1; i < lines.length; i++) {
        if (/^(---|\.\.\.)[ \t]*$/.test(lines[i])) return i;
    }
    return -1;
}

function isIgnorableYamlLine(line: string): boolean {
    const trimmed = line.trim();
    return trimmed === '' || trimmed.charAt(0) === '#';
}

function isTopLevelYamlKey(line: string): boolean {
    return /^[A-Za-z0-9_.-]+[ \t]*:/.test(line);
}

function findNextNonEmptyIndex(lines: string[], startIndex: number, endIndex: number): number {
    for (let i = startIndex; i < endIndex; i++) {
        if (lines[i].trim() !== '') return i;
    }
    return -1;
}

function hasHeadingFollowedByProse(lines: string[], closeIndex: number): boolean {
    for (let i = 1; i < closeIndex; i++) {
        if (!/^#{1,6}[ \t]+/.test(lines[i].trim())) continue;
        const nextIdx = findNextNonEmptyIndex(lines, i + 1, closeIndex);
        if (nextIdx >= 0 && !isIgnorableYamlLine(lines[nextIdx]) && !isTopLevelYamlKey(lines[nextIdx])) return true;
    }
    return false;
}

function isPlausibleFrontmatter(lines: string[], closeIndex: number): boolean {
    if (hasHeadingFollowedByProse(lines, closeIndex)) return false;
    let sawContent = false;
    for (let i = 1; i < closeIndex; i++) {
        if (isIgnorableYamlLine(lines[i])) continue;
        sawContent = true;
        if (isTopLevelYamlKey(lines[i])) return true;
    }
    return !sawContent;
}

function unquoteYamlValue(text: string): string {
    if (text.length < 2) return text;
    const first = text.charAt(0);
    const last = text.charAt(text.length - 1);
    if ((first === '"' && last === '"') || (first === '\'' && last === '\'')) return text.slice(1, -1);
    return text;
}

function hasUnclosedQuote(text: string): boolean {
    const first = text.charAt(0);
    if (first !== '"' && first !== '\'') return false;
    return text.length === 1 || text.charAt(text.length - 1) !== first;
}

function validateScalarValue(rest: string, lineNum: number): FrontmatterError | null {
    if (hasUnclosedQuote(rest)) return { line: lineNum, message: 'Unclosed quoted string (line ' + lineNum + ')' };
    if (rest.charAt(0) === '[' && rest.charAt(rest.length - 1) !== ']') return { line: lineNum, message: 'Unclosed array bracket "[" (line ' + lineNum + ')' };
    if (rest.charAt(0) === '{' && rest.slice(0, 2) !== '{{' && rest.charAt(rest.length - 1) !== '}') return { line: lineNum, message: 'Unclosed inline brace "{" (line ' + lineNum + ')' };
    return null;
}

function collectNestedBlock(yamlLines: string[], startIndex: number): NestedBlockResult {
    const items: string[] = [];
    let allListItems = true;
    let idx = startIndex;
    while (idx < yamlLines.length) {
        const line = yamlLines[idx];
        if (line.charAt(0) === '\t') return { error: { line: idx + 2, message: 'Tab indentation is not allowed in YAML (line ' + (idx + 2) + ')' } };
        const isIndented = line.charAt(0) === ' ';
        const isZeroList = /^- /.test(line);
        if (!isIndented && !isZeroList && line.trim() !== '') break;
        if (line.trim() !== '') {
            const trimmed = line.trim();
            if (!/^- /.test(trimmed)) allListItems = false;
            items.push(line.replace(/^[ ]{1,2}/, ''));
        }
        idx++;
    }
    if (items.length > 0 && allListItems) {
        return { nextIndex: idx, type: 'array', value: items.map(function (s: string): string { return unquoteYamlValue(s.trim().slice(2).trim()); }) };
    }
    return { nextIndex: idx, type: 'nested', value: items.join('\n') };
}

function parseSingleYamlEntry(key: string, rest: string): FrontmatterEntry {
    if (rest.charAt(0) === '[' && rest.charAt(rest.length - 1) === ']') {
        const parts = rest.slice(1, -1).split(',').map(function (s: string): string { return unquoteYamlValue(s.trim()); }).filter(Boolean);
        return { key: key, type: 'array', value: parts };
    }
    if (rest === 'true' || rest === 'false') return { key: key, type: 'bool', value: rest === 'true' };
    let scalar = unquoteYamlValue(rest);
    if (scalar.length > 500) scalar = scalar.slice(0, 500) + '…';
    return { key: key, type: 'scalar', value: scalar };
}

function parseFrontmatterYaml(yamlLines: string[]): ParsedFrontmatterYaml {
    const entries: FrontmatterEntry[] = [];
    let idx = 0;
    while (idx < yamlLines.length) {
        const line = yamlLines[idx];
        const lineNum = idx + 2;
        if (isIgnorableYamlLine(line)) { idx++; continue; }
        if (line.charAt(0) === '\t') return { entries: [], error: { line: lineNum, message: 'Tab indentation is not allowed in YAML (line ' + lineNum + ')' } };
        const match = line.match(/^([A-Za-z0-9_.-]+)[ \t]*:(.*)$/);
        if (!match) return { entries: [], error: { line: lineNum, message: 'Expected "key: value" pair (line ' + lineNum + ')' } };
        const key = match[1];
        const rest = match[2].trim();
        const valueErr = validateScalarValue(rest, lineNum);
        if (valueErr) return { entries: [], error: valueErr };
        if (rest === '' || rest === '|' || rest === '>' || rest === '|-' || rest === '>-') {
            const block = collectNestedBlock(yamlLines, idx + 1);
            if (block.error) return { entries: [], error: block.error };
            entries.push(block.type === 'array' ? { key: key, type: 'array', value: block.value } : { key: key, type: 'nested', value: block.value });
            idx = block.nextIndex;
            continue;
        }
        entries.push(parseSingleYamlEntry(key, rest));
        idx++;
    }
    return { entries: entries, error: null };
}

function extractFrontmatter(markdownText: string): FrontmatterExtraction {
    const normalized = markdownText.replace(/\r\n/g, '\n');
    const lines = normalized.split('\n');
    if (lines.length < 2 || !isFrontmatterOpenFence(lines[0])) return { found: false, body: markdownText };
    const closeIndex = findFrontmatterCloseIndex(lines);
    if (closeIndex < 0 || !isPlausibleFrontmatter(lines, closeIndex)) return { found: false, body: markdownText };
    const yamlLines = lines.slice(1, closeIndex);
    const parsed = parseFrontmatterYaml(yamlLines);
    return { found: true, yamlLines: yamlLines, entries: parsed.entries, error: parsed.error, body: lines.slice(closeIndex + 1).join('\n') };
}

function renderFrontmatterValueHtml(entry: FrontmatterEntry): string {
    if (entry.type === 'array') {
        const chips = entry.value.map(function (v: string): string { return '<span class="markd-frontmatter-chip">' + md.utils.escapeHtml(v) + '</span>'; }).join('');
        return '<div class="markd-frontmatter-chips">' + chips + '</div>';
    }
    if (entry.type === 'bool') {
        const boolCls = entry.value ? 'markd-frontmatter-bool' : 'markd-frontmatter-bool is-false';
        return '<span class="' + boolCls + '">' + String(entry.value) + '</span>';
    }
    if (entry.type === 'nested') return '<div class="markd-frontmatter-nested">' + md.utils.escapeHtml(entry.value) + '</div>';
    return md.utils.escapeHtml(entry.value);
}

function renderFrontmatterErrorHtml(fm: FoundFrontmatter, error: FrontmatterError): string {
    const rawRows = fm.yamlLines.map(function (line: string, idx: number): string {
        const lineNum = idx + 2;
        const rowCls = lineNum === error.line ? 'markd-frontmatter-raw-line is-error-line' : 'markd-frontmatter-raw-line';
        return '<div class="' + rowCls + '"><span class="markd-frontmatter-ln">' + lineNum + '</span><span class="markd-frontmatter-code">' + md.utils.escapeHtml(line || ' ') + '</span></div>';
    }).join('');
    return [
        '<details class="markd-frontmatter markd-frontmatter-error" open>',
        '<summary><span class="markd-frontmatter-title"><span class="markd-frontmatter-chevron">▶</span>Frontmatter <span class="markd-frontmatter-warn">Syntax issue · line ' + error.line + '</span></span><span class="markd-frontmatter-hint">Switch to Edit (⌘E) to fix</span></summary>',
        '<div class="markd-frontmatter-body"><div class="markd-frontmatter-error-bar">' + md.utils.escapeHtml(error.message) + '</div><div class="markd-frontmatter-raw">' + rawRows + '</div></div>',
        '</details>'
    ].join('');
}

function renderFrontmatterHtml(fm: FoundFrontmatter, preserveOpen: boolean): string {
    if (fm.error) return renderFrontmatterErrorHtml(fm, fm.error);
    const openAttr = preserveOpen ? ' open' : '';
    let bodyHtml = '<div class="markd-frontmatter-empty">Empty Jekyll frontmatter block (0 fields)</div>';
    let hint = 'empty block';
    if (fm.entries.length > 0) {
        const rows = fm.entries.map(function (entry: FrontmatterEntry): string {
            return '<tr><td class="markd-frontmatter-key">' + md.utils.escapeHtml(entry.key) + '</td><td class="markd-frontmatter-val">' + renderFrontmatterValueHtml(entry) + '</td></tr>';
        }).join('');
        bodyHtml = '<table class="markd-frontmatter-table"><tbody>' + rows + '</tbody></table>';
        hint = fm.entries.slice(0, 3).map(function (e: FrontmatterEntry): string { return e.key; }).join(', ') + (fm.entries.length > 3 ? ', …' : '');
    }
    return [
        '<details class="markd-frontmatter"' + openAttr + '>',
        '<summary><span class="markd-frontmatter-title"><span class="markd-frontmatter-chevron">▶</span>Frontmatter <span class="markd-frontmatter-count">' + fm.entries.length + ' fields</span></span><span class="markd-frontmatter-hint">' + md.utils.escapeHtml(hint) + '</span></summary>',
        '<div class="markd-frontmatter-body">' + bodyHtml + '</div>',
        '</details>'
    ].join('');
}

async function executeRenderMarkdown(markdownText: unknown, options?: RenderOptions): Promise<void> {
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

    const preserveBlockSizes = !!(options && (options.preserveScroll === true || options.force === true));
    if (!preserveBlockSizes) {
        blockSizeMemory = Object.create(null);
    }

    const contentEl = document.getElementById('content')!;
    const prevScrollY = window.scrollY || 0;
    const existingFrontmatter = contentEl.querySelector<HTMLDetailsElement>('.markd-frontmatter');
    const preserveFrontmatterOpen = !!(existingFrontmatter && existingFrontmatter.open);

    // 1. Extract optional YAML frontmatter and parse markdown body to HTML
    const fm = extractFrontmatter(markdownText);
    const frontmatterHtml = fm.found ? renderFrontmatterHtml(fm, preserveFrontmatterOpen) : '';
    const html = frontmatterHtml + md.render(fm.body);

    // 2. Sanitize and insert into DOM
    const clean = DOMPurify!.sanitize(html, {
        ADD_TAGS: ['semantics', 'annotation'],
        ADD_ATTR: ['target', 'encoding', 'rel', 'role', 'aria-label', 'data-lang', 'type'],
        FORBID_TAGS: ['style'],
        ALLOW_DATA_ATTR: true,
        SANITIZE_NAMED_PROPS: false,
        RETURN_TRUSTED_TYPE: false
    });
    contentEl.innerHTML = clean;

    // 3. Transform mermaid code blocks into renderable divs
    const mermaidBlocks = contentEl.querySelectorAll<HTMLElement>('pre > code.language-mermaid');
    for (let i = 0; i < mermaidBlocks.length; i++) {
        const pre = mermaidBlocks[i].parentElement!;
        const div = document.createElement('div');
        div.className = 'mermaid';
        const codeText = mermaidBlocks[i].textContent!.replace(/\n$/, '');
        div.dataset.rawCode = codeText;
        div.textContent = codeText;
        pre.replaceWith(div);
    }

    // 4. Render mermaid diagrams lazily with per-diagram isolation
    const mermaidDivs = contentEl.querySelectorAll<HTMLElement>('.mermaid');
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
    const headings = contentEl.querySelectorAll('h1[id], h2[id], h3[id], h4[id], h5[id], h6[id]');
    if (window.markdBridge && typeof window.markdBridge.onDocumentRendered === 'function') {
        window.markdBridge.onDocumentRendered(headings.length);
    }
}

function replaceWithMermaidErrorCard(mDiv: HTMLElement, rawDiagramCode: string, err: unknown): void {
    console.error('Mermaid render error:', err);
    const errorCard = document.createElement('div');
    errorCard.className = 'mermaid-error-card';
    const badge = document.createElement('div');
    badge.className = 'mermaid-error-badge';
    badge.textContent = 'Mermaid Syntax Error: ' + (err && (err as Error).message ? (err as Error).message : 'Invalid diagram syntax');
    const rawPre = document.createElement('pre');
    rawPre.className = 'mermaid-raw-code hljs';
    const rawCodeNode = document.createElement('code');
    rawCodeNode.textContent = rawDiagramCode;
    rawPre.appendChild(rawCodeNode);
    errorCard.appendChild(badge);
    errorCard.appendChild(rawPre);
    mDiv.replaceWith(errorCard);
}

function scrubMermaidStyleElement(styleNode: HTMLStyleElement): void {
    const safeCss = styleNode.textContent!
        .replace(/position\s*:\s*(?:fixed|absolute)/gi, 'position: static')
        .replace(/@import\b[^;]*;?/gi, '');
    styleNode.textContent = safeCss;
}

function sanitizeCompiledMermaidSvg(mDiv: HTMLElement, svgEl: SVGSVGElement): void {
    mDiv.innerHTML = DOMPurify!.sanitize(svgEl.outerHTML, {
        USE_PROFILES: { html: true, svg: true, svgFilters: true },
        ADD_TAGS: ['foreignObject'],
        ADD_ATTR: ['dominant-baseline', 'xmlns'],
        HTML_INTEGRATION_POINTS: { foreignobject: true }
    });
    const styles = mDiv.querySelectorAll('style');
    for (let i = 0; i < styles.length; i++) {
        scrubMermaidStyleElement(styles[i]);
    }
    mDiv.setAttribute('data-processed', 'true');
}

async function renderSingleMermaidNode(mermaidApi: MermaidApi, mDiv: HTMLElement): Promise<void> {
    const rawDiagramCode = mDiv.dataset.rawCode!;
    try {
        await mermaidApi.run({ nodes: [mDiv] });
        if (!mDiv.isConnected) return;
        const svgEl = mDiv.querySelector('svg');
        if (svgEl) {
            sanitizeCompiledMermaidSvg(mDiv, svgEl);
        }
    } catch (e) {
        replaceWithMermaidErrorCard(mDiv, rawDiagramCode, e);
    }
}

function replaceAllMermaidWithErrors(mermaidDivs: NodeListOf<HTMLElement>, err: unknown): void {
    for (let k = 0; k < mermaidDivs.length; k++) {
        replaceWithMermaidErrorCard(mermaidDivs[k], mermaidDivs[k].dataset.rawCode!, err);
    }
}

async function resolveMermaidApi(mermaidDivs: NodeListOf<HTMLElement>): Promise<MermaidApi | null> {
    try {
        return await ensureMermaidLoaded();
    } catch (loadErr) {
        replaceAllMermaidWithErrors(mermaidDivs, loadErr);
        return null;
    }
}

async function renderMermaidNodes(mermaidDivs: NodeListOf<HTMLElement>): Promise<void> {
    const mermaidApi = typeof window.mermaid !== 'undefined'
        ? window.mermaid
        : await resolveMermaidApi(mermaidDivs);
    if (!mermaidApi) return;
    for (let j = 0; j < mermaidDivs.length; j++) {
        const mDiv = mermaidDivs[j];
        if (!mDiv.isConnected) continue;
        await renderSingleMermaidNode(mermaidApi, mDiv);
    }
}

// ---- Scroll spy ----

let scrollSpyObserver: IntersectionObserver | null = null;

function setupScrollSpy(): void {
    if (scrollSpyObserver) {
        scrollSpyObserver.disconnect();
    }

    const headings = document.querySelectorAll('#content h1[id], #content h2[id], #content h3[id], #content h4[id], #content h5[id], #content h6[id]');
    if (headings.length === 0) return;

    scrollSpyObserver = new IntersectionObserver(function (entries: IntersectionObserverEntry[]): void {
        let topHeading: Element | null = null;
        let topY = Infinity;

        for (let i = 0; i < entries.length; i++) {
            const entry = entries[i];
            if (entry.isIntersecting && entry.boundingClientRect.top < topY) {
                topY = entry.boundingClientRect.top;
                topHeading = entry.target;
            }
        }

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

    headings.forEach(function (h: Element): void {
        scrollSpyObserver!.observe(h);
    });
}

// ---- Scroll to heading ----

let lastScrolledHeadingId: string | null = null;
let lastScrollTimestamp = 0;

function scrollToHeading(id: string, behavior?: ScrollBehavior): void {
    const now = Date.now();
    if (id === lastScrolledHeadingId && now - lastScrollTimestamp < 150) {
        return;
    }
    lastScrolledHeadingId = id;
    lastScrollTimestamp = now;

    const el = document.getElementById(id);
    if (el) {
        // Smooth for user navigation (TOC, anchors); callers restoring a position pass 'instant'
        el.scrollIntoView({ behavior: behavior || 'smooth', block: 'start' });
    }
}

function scrollToRatio(ratio: unknown): void {
    if (typeof ratio !== 'number' || isNaN(ratio)) return;
    const maxScroll = Math.max(0, document.documentElement.scrollHeight - window.innerHeight);
    const clampedRatio = Math.max(0, Math.min(1, ratio));
    window.scrollTo({ top: clampedRatio * maxScroll, behavior: 'instant' });
}

function resolvePostRenderScroll(options: RenderOptions | undefined, prevScrollY: number): void {
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

let blockSizeMemory: Record<string, BlockSizeEntry> = Object.create(null);
let activeBlockResizeCleanup: (() => void) | null = null;
const BLOCK_RESIZE_CURSORS: Record<ResizeAxis, string> = {
    n: 'ns-resize',
    w: 'ew-resize',
    e: 'ew-resize',
    s: 'ns-resize',
    nw: 'nwse-resize',
    ne: 'nesw-resize',
    sw: 'nesw-resize',
    se: 'nwse-resize'
};

function stopActiveBlockResize(): void {
    if (!activeBlockResizeCleanup) return;
    const cleanup = activeBlockResizeCleanup;
    activeBlockResizeCleanup = null;
    cleanup();
}

function clampBlockWidth(rawWidth: number): number {
    const viewportWidth = window.innerWidth || 1024;
    const maxWidth = Math.min(viewportWidth, Math.max(280, viewportWidth - 48));
    const minWidth = Math.min(420, maxWidth);
    return Math.round(Math.max(minWidth, Math.min(maxWidth, rawWidth)));
}

function clampBlockHeight(rawHeight: number): number {
    const viewportHeight = window.innerHeight || 800;
    const maxHeight = Math.max(220, Math.round(viewportHeight * 0.85));
    return Math.round(Math.max(220, Math.min(maxHeight, rawHeight)));
}

function measureBlockWidth(container: HTMLElement): number {
    const rectWidth = container.getBoundingClientRect().width;
    return rectWidth > 0 ? rectWidth : 664;
}

function measureBlockHeight(target: HTMLElement): number {
    const rectHeight = target.getBoundingClientRect().height;
    return rectHeight > 0 ? rectHeight : 320;
}

function applyBlockDimensions(
    container: HTMLElement,
    verticalTarget: HTMLElement | null,
    blockKey: string,
    width: number | null | undefined,
    height: number | null | undefined
): void {
    const entry = blockSizeMemory[blockKey] || {};
    if (typeof width === 'number') {
        const clampedW = clampBlockWidth(width);
        container.style.width = clampedW + 'px';
        container.classList.add('is-resized-x');
        entry.width = clampedW;
    }
    if (verticalTarget && typeof height === 'number') {
        const clampedH = clampBlockHeight(height);
        verticalTarget.style.height = clampedH + 'px';
        container.classList.add('is-resized-y');
        entry.height = clampedH;
    }
    blockSizeMemory[blockKey] = entry;
}

function resetBlockDimensions(container: HTMLElement, verticalTarget: HTMLElement | null, blockKey: string): void {
    container.style.width = '';
    container.classList.remove('is-resized-x');
    container.classList.remove('is-resized-y');
    if (verticalTarget) {
        verticalTarget.style.height = '';
    }
    delete blockSizeMemory[blockKey];
}

function computeNextWidth(axis: ResizeAxis, startWidth: number, dx: number): number | null {
    if (axis.indexOf('e') !== -1) return startWidth + (2 * dx);
    if (axis.indexOf('w') !== -1) return startWidth - (2 * dx);
    return null;
}

function computeNextHeight(axis: ResizeAxis, startHeight: number, dy: number): number | null {
    if (axis.indexOf('s') !== -1) return startHeight + dy;
    if (axis.indexOf('n') !== -1) return startHeight - dy;
    return null;
}

function startBlockResizeDrag(
    event: MouseEvent,
    container: HTMLElement,
    verticalTarget: HTMLElement | null,
    blockKey: string,
    axis: ResizeAxis
): void {
    if (event.button !== 0) return;
    event.preventDefault();
    event.stopPropagation();
    stopActiveBlockResize();

    const startX = event.clientX;
    const startY = event.clientY;
    const startWidth = measureBlockWidth(container);
    const startHeight = verticalTarget ? measureBlockHeight(verticalTarget) : 0;

    container.classList.add('is-resizing');
    document.body.classList.add('is-block-resizing');
    document.body.style.cursor = BLOCK_RESIZE_CURSORS[axis];

    const onMove = function (moveEvent: MouseEvent): void {
        const nextW = computeNextWidth(axis, startWidth, moveEvent.clientX - startX);
        const nextH = computeNextHeight(axis, startHeight, moveEvent.clientY - startY);
        applyBlockDimensions(container, verticalTarget, blockKey, nextW, nextH);
    };

    const finish = function (): void {
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

function createResizeHandle(
    container: HTMLElement,
    verticalTarget: HTMLElement | null,
    blockKey: string,
    axis: ResizeAxis
): HTMLDivElement {
    const handle = document.createElement('div');
    handle.className = 'block-resize-handle block-resize-handle-' + axis;
    handle.setAttribute('data-resize-axis', axis);
    handle.setAttribute('role', 'separator');
    handle.setAttribute('aria-label', 'Resize block (' + axis + ')');
    handle.title = 'Drag to resize · Double-click to reset';
    handle.addEventListener('mousedown', function (e: MouseEvent): void {
        startBlockResizeDrag(e, container, verticalTarget, blockKey, axis);
    });
    handle.addEventListener('dblclick', function (e: MouseEvent): void {
        e.preventDefault();
        e.stopPropagation();
        resetBlockDimensions(container, verticalTarget, blockKey);
    });
    return handle;
}

function attachBlockResizeHandles(container: HTMLElement, verticalTarget: HTMLElement | null, blockKey: string): void {
    if (container.querySelector('.block-resize-handle')) return;
    const axes: ResizeAxis[] = verticalTarget ? ['n', 'w', 'e', 's', 'nw', 'ne', 'sw', 'se'] : ['w', 'e'];
    axes.forEach(function (axis: ResizeAxis): void {
        container.appendChild(createResizeHandle(container, verticalTarget, blockKey, axis));
    });
    const saved = blockSizeMemory[blockKey];
    if (saved) {
        applyBlockDimensions(container, verticalTarget, blockKey, saved.width, saved.height);
    }
}

function attachCodeBlockResizeHandles(): void {
    const cards = document.querySelectorAll<HTMLElement>('#content .code-block-card');
    cards.forEach(function (card: HTMLElement, index: number): void {
        attachBlockResizeHandles(card, null, 'code:' + index);
    });
}

// ---- Expandable containers ----

function wrapMermaidDiagrams(): void {
    const diagrams = document.querySelectorAll<HTMLElement>('#content .mermaid');
    diagrams.forEach(function (diagram: HTMLElement, index: number): void {
        if (diagram.closest('.expandable-container')) return;

        const container = document.createElement('div');
        container.className = 'expandable-container';

        // Toolbar with zoom controls + fullview
        const toolbar = document.createElement('div');
        toolbar.className = 'expandable-toolbar';
        toolbar.innerHTML =
            '<button class="zoom-out-btn" title="Zoom out">−</button>' +
            '<span class="zoom-label">100%</span>' +
            '<button class="zoom-in-btn" title="Zoom in">+</button>' +
            '<button class="zoom-reset-btn" title="Reset zoom &amp; size">↺</button>' +
            '<button class="fullview-btn" title="Full view">⛶</button>';

        // Viewport for pan/zoom
        const viewport = document.createElement('div');
        viewport.className = 'mermaid-viewport';
        const inner = document.createElement('div');
        inner.className = 'mermaid-inner';

        diagram.parentNode!.insertBefore(container, diagram);
        inner.appendChild(diagram);
        viewport.appendChild(inner);
        container.appendChild(toolbar);
        container.appendChild(viewport);

        const blockKey = 'mermaid:' + index;
        setupDiagramZoomPan(container, viewport, inner, blockKey);
        attachBlockResizeHandles(container, viewport, blockKey);
    });
}

function wrapTables(): void {
    const tables = document.querySelectorAll<HTMLTableElement>('#content > table');
    tables.forEach(function (table: HTMLTableElement, index: number): void {
        if (table.parentElement!.classList.contains('table-wrapper')) return;

        const wrapper = document.createElement('div');
        wrapper.className = 'table-wrapper';

        const btn = document.createElement('button');
        btn.className = 'table-expand-btn';
        btn.title = 'Full view';
        btn.textContent = '⛶';

        table.parentNode!.insertBefore(wrapper, table);
        wrapper.appendChild(btn);
        wrapper.appendChild(table);

        btn.addEventListener('click', function (): void {
            openFullView(table.cloneNode(true) as HTMLElement, false);
        });

        attachBlockResizeHandles(wrapper, null, 'table:' + index);
    });
}

// ---- Diagram zoom/pan ----

function setupDiagramZoomPan(container: HTMLElement, viewport: HTMLElement, inner: HTMLElement, blockKey: string): void {
    let scale = 1;
    let panX = 0;
    let panY = 0;
    let isPanning = false;
    let startX = 0;
    let startY = 0;
    let startPanX = 0;
    let startPanY = 0;
    const label = container.querySelector<HTMLElement>('.zoom-label')!;

    function applyTransform(smooth: boolean): void {
        if (Math.abs(scale - 1) < 0.02) {
            scale = 1;
        }
        inner.style.transition = smooth ? 'transform 0.15s ease-out' : 'none';
        inner.style.transform = 'translate(' + panX + 'px,' + panY + 'px) scale(' + scale + ')';
        label.textContent = Math.round(scale * 100) + '%';
    }

    function clampPan(x: number, y: number): PanCoordinates {
        const limitX = Math.max(500, (viewport.clientWidth || 600) * scale);
        const limitY = Math.max(500, (viewport.clientHeight || 400) * scale);
        return {
            x: Math.max(-limitX, Math.min(limitX, x)),
            y: Math.max(-limitY, Math.min(limitY, y))
        };
    }

    container.querySelector<HTMLButtonElement>('.zoom-in-btn')!.addEventListener('click', function (): void {
        scale = Math.min(scale * 1.25, 5);
        applyTransform(true);
    });

    container.querySelector<HTMLButtonElement>('.zoom-out-btn')!.addEventListener('click', function (): void {
        scale = Math.max(scale / 1.25, 0.2);
        applyTransform(true);
    });

    container.querySelector<HTMLButtonElement>('.zoom-reset-btn')!.addEventListener('click', function (): void {
        scale = 1; panX = 0; panY = 0;
        applyTransform(true);
        resetBlockDimensions(container, viewport, blockKey);
    });

    container.querySelector<HTMLButtonElement>('.fullview-btn')!.addEventListener('click', function (): void {
        openFullView(inner.cloneNode(true) as HTMLElement, true);
    });

    // Mouse wheel zoom with zero-delta guard
    viewport.addEventListener('wheel', function (e: WheelEvent): void {
        e.preventDefault();
        if (Math.abs(e.deltaY) < 1e-3) return;
        const factor = e.deltaY < 0 ? 1.1 : 0.9;
        scale = Math.max(0.2, Math.min(5, scale * factor));
        applyTransform(false);
    }, { passive: false });

    // Pan with mouse drag
    viewport.addEventListener('mousedown', function (e: MouseEvent): void {
        isPanning = true;
        startX = e.clientX;
        startY = e.clientY;
        startPanX = panX;
        startPanY = panY;
        e.preventDefault();
    });

    const onMouseMove = function (e: MouseEvent): void {
        if (!isPanning) return;
        const clamped = clampPan(startPanX + (e.clientX - startX), startPanY + (e.clientY - startY));
        panX = clamped.x;
        panY = clamped.y;
        applyTransform(false);
    };

    const onMouseUp = function (): void {
        isPanning = false;
    };

    const onPointerExit = function (): void {
        isPanning = false;
    };

    document.addEventListener('mousemove', onMouseMove);
    document.addEventListener('mouseup', onMouseUp);
    document.addEventListener('mouseleave', onPointerExit);
    window.addEventListener('blur', onPointerExit);

    diagramTeardowns.push(function (): void {
        document.removeEventListener('mousemove', onMouseMove);
        document.removeEventListener('mouseup', onMouseUp);
        document.removeEventListener('mouseleave', onPointerExit);
        window.removeEventListener('blur', onPointerExit);
    });

    applyTransform(false);
}

// ---- Full-view overlay ----

let activeFullViewClose: ((immediate?: boolean) => void) | null = null;

function closeFullView(immediate?: boolean): void {
    if (activeFullViewClose) {
        activeFullViewClose(immediate);
    }
}

function openFullView(contentNode: HTMLElement, isDiagram: boolean): void {
    // Singleton modal lifecycle: synchronously close and unbind any existing overlay
    if (activeFullViewClose) {
        activeFullViewClose(true);
    }

    const existingOverlays = document.querySelectorAll('.fullview-overlay');
    for (let i = 0; i < existingOverlays.length; i++) {
        existingOverlays[i].remove();
    }

    const previousFocusedElement = document.activeElement as HTMLElement | null;

    // Lock body scroll
    document.body.classList.add('modal-open');
    document.body.style.overflow = 'hidden';

    const overlay = document.createElement('div');
    overlay.className = 'fullview-overlay';

    const toolbar = document.createElement('div');
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

    const body = document.createElement('div');
    body.className = 'fullview-body' + (isDiagram ? ' is-diagram' : '');

    const inner = document.createElement('div');
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
    requestAnimationFrame(function (): void {
        overlay.classList.add('visible');
    });

    let fvMouseMoveHandler: ((e: MouseEvent) => void) | null = null;
    let fvMouseUpHandler: (() => void) | null = null;
    let fvExitHandler: (() => void) | null = null;

    // Close handler
    function closeOverlay(immediate?: boolean): void {
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
            setTimeout(function (): void { overlay.remove(); }, 200);
        }
    }
    activeFullViewClose = closeOverlay;

    toolbar.querySelector<HTMLButtonElement>('.close-btn')!.addEventListener('click', function (): void {
        closeOverlay(false);
    });

    // Keyboard handlers (Escape to close, Tab focus trapping)
    function onKeyDown(e: KeyboardEvent): void {
        if (e.key === 'Escape') {
            closeOverlay(false);
            return;
        }
        if (e.key === 'Tab') {
            const focusable = overlay.querySelectorAll<HTMLElement>('button, [href], input, select, textarea, [tabindex]:not([tabindex="-1"])');
            if (focusable.length === 0) return;
            const first = focusable[0];
            const last = focusable[focusable.length - 1];
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
        let fvScale = 1;
        let fvPanX = 0;
        let fvPanY = 0;
        let fvIsPanning = false;
        let fvStartX = 0;
        let fvStartY = 0;
        let fvStartPanX = 0;
        let fvStartPanY = 0;
        const fvLabel = toolbar.querySelector<HTMLElement>('.zoom-label')!;

        function fvApply(smooth: boolean): void {
            if (Math.abs(fvScale - 1) < 0.02) {
                fvScale = 1;
            }
            inner.style.transition = smooth ? 'transform 0.15s ease-out' : 'none';
            inner.style.transform = 'translate(' + fvPanX + 'px,' + fvPanY + 'px) scale(' + fvScale + ')';
            fvLabel.textContent = Math.round(fvScale * 100) + '%';
        }

        function fvClampPan(x: number, y: number): PanCoordinates {
            const limitX = Math.max(500, (body.clientWidth || 800) * fvScale);
            const limitY = Math.max(500, (body.clientHeight || 600) * fvScale);
            return {
                x: Math.max(-limitX, Math.min(limitX, x)),
                y: Math.max(-limitY, Math.min(limitY, y))
            };
        }

        toolbar.querySelector<HTMLButtonElement>('.zoom-in-btn')!.addEventListener('click', function (): void {
            fvScale = Math.min(fvScale * 1.25, 5); fvApply(true);
        });
        toolbar.querySelector<HTMLButtonElement>('.zoom-out-btn')!.addEventListener('click', function (): void {
            fvScale = Math.max(fvScale / 1.25, 0.2); fvApply(true);
        });
        toolbar.querySelector<HTMLButtonElement>('.zoom-reset-btn')!.addEventListener('click', function (): void {
            fvScale = 1; fvPanX = 0; fvPanY = 0; fvApply(true);
        });

        body.addEventListener('wheel', function (e: WheelEvent): void {
            e.preventDefault();
            if (Math.abs(e.deltaY) < 1e-3) return;
            const factor = e.deltaY < 0 ? 1.1 : 0.9;
            fvScale = Math.max(0.2, Math.min(5, fvScale * factor));
            fvApply(false);
        }, { passive: false });

        body.addEventListener('mousedown', function (e: MouseEvent): void {
            fvIsPanning = true;
            fvStartX = e.clientX; fvStartY = e.clientY;
            fvStartPanX = fvPanX; fvStartPanY = fvPanY;
            e.preventDefault();
        });

        fvMouseMoveHandler = function (e: MouseEvent): void {
            if (!fvIsPanning) return;
            const clamped = fvClampPan(fvStartPanX + (e.clientX - fvStartX), fvStartPanY + (e.clientY - fvStartY));
            fvPanX = clamped.x;
            fvPanY = clamped.y;
            fvApply(false);
        };
        fvMouseUpHandler = function (): void {
            fvIsPanning = false;
        };
        fvExitHandler = function (): void {
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
    get bridge(): MarkdBridge | null {
        return window.markdBridge;
    },
    get session(): MarkdSession | null {
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
    document.addEventListener('DOMContentLoaded', function (): void {
        window.Markd.isReady = true;
    });
} else {
    window.Markd.isReady = true;
}

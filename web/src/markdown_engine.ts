// Markdown-it engine setup, syntax highlighting fence rule, and plugin registration

function formatCodeBlock(codeHtml: string, lang: string): string {
    const safeLang = md.utils.escapeHtml(lang);
    return [
        '<div class="code-block-card" data-lang="' + safeLang + '" role="region" aria-label="Code snippet: ' + safeLang + '">',
        '<div class="code-block-header">',
        '<span class="code-lang">' + safeLang + '</span>',
        '<button class="copy-btn" type="button" aria-label="Copy code to clipboard">Copy</button>',
        '</div>',
        '<pre class="hljs"><code>' + codeHtml + '</code></pre>',
        '</div>'
    ].join('');
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

function slugifyHeading(text: string): string {
    return text.trim()
        .toLowerCase()
        .replace(/\s+/g, '-')
        .replace(/[^\p{L}\p{M}\p{N}_-]/gu, '')
        .replace(/--+/g, '-')
        .replace(/^-|-$/g, '');
}

function renderFenceToken(tokens: MarkdownItToken[], idx: number): string {
    const token = tokens[idx];
    const info = md.utils.unescapeAll(token.info).trim();
    const lang = info.split(/\s+/g)[0].toLowerCase();
    if (lang === 'mermaid') {
        return '<pre><code class="language-mermaid">' + md.utils.escapeHtml(token.content) + '</code></pre>\n';
    }
    const normalizedLang = lang || 'text';
    return highlightCodeBlock(token.content, normalizedLang) + '\n';
}

const md: MarkdownItInstance = window.markdownit({
    html: true,
    linkify: true,
    typographer: true
});

md.renderer.rules.fence = renderFenceToken;

function registerTexmathPlugin(): void {
    const options = {
        engine: window.katex,
        delimiters: 'dollars',
        katexOptions: { throwOnError: false, displayMode: false }
    };
    md.use(window.texmath, options);
}

function configureMarkdownPlugins(): void {
    if (window.markdownItAnchor) {
        md.use(window.markdownItAnchor, { permalink: false, slugify: slugifyHeading });
    }
    if (window.markdownitTaskLists) {
        md.use(window.markdownitTaskLists, { enabled: true, label: true });
    }
    if (window.texmath) {
        registerTexmathPlugin();
    }
}

configureMarkdownPlugins();

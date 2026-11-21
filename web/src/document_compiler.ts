// Pure markdown text -> sanitized HTML compiler composing Frontmatter, markdown-it, and DomSanitizer

class DocumentCompiler {
  private readonly engine: MarkdownItInstance;
  private readonly sanitizer: DomSanitizer;

  constructor(sanitizer: DomSanitizer) {
    this.sanitizer = sanitizer;
    this.engine = window.markdownit({
      html: true,
      linkify: true,
      typographer: true
    });
    this.engine.renderer.rules.fence = (tokens: MarkdownItToken[], idx: number): string => {
      return this.renderFenceToken(tokens, idx);
    };
    this.configurePlugins();
  }

  private formatCodeBlock(codeHtml: string, lang: string): string {
    const safeLang = this.engine.utils.escapeHtml(lang);
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

  private highlightCodeBlock(str: string, normalizedLang: string): string {
    if (!hljs.getLanguage(normalizedLang)) {
      return this.formatCodeBlock(this.engine.utils.escapeHtml(str), normalizedLang);
    }
    try {
      const highlighted = hljs.highlight(str, { language: normalizedLang, ignoreIllegals: true }).value;
      return this.formatCodeBlock(highlighted, normalizedLang);
    } catch (_) {
      return this.formatCodeBlock(this.engine.utils.escapeHtml(str), normalizedLang);
    }
  }

  private slugifyHeading(text: string): string {
    return text.trim()
      .toLowerCase()
      .replace(/\s+/g, '-')
      .replace(/[^\p{L}\p{M}\p{N}_-]/gu, '')
      .replace(/--+/g, '-')
      .replace(/^-|-$/g, '');
  }

  private renderFenceToken(tokens: MarkdownItToken[], idx: number): string {
    const token = tokens[idx];
    const info = this.engine.utils.unescapeAll(token.info).trim();
    const lang = info.split(/\s+/g)[0].toLowerCase();
    if (lang === 'mermaid') {
      return '<pre><code class="language-mermaid">' + this.engine.utils.escapeHtml(token.content) + '</code></pre>\n';
    }
    const normalizedLang = lang || 'text';
    return this.highlightCodeBlock(token.content, normalizedLang) + '\n';
  }

  private registerTexmathPlugin(): void {
    const options = {
      engine: window.katex,
      delimiters: 'dollars',
      katexOptions: { throwOnError: false, displayMode: false }
    };
    this.engine.use(window.texmath, options);
  }

  private configurePlugins(): void {
    if (window.markdownItAnchor) {
      this.engine.use(window.markdownItAnchor, {
        permalink: false,
        slugify: (text: string): string => this.slugifyHeading(text)
      });
    }
    if (window.markdownitTaskLists) {
      this.engine.use(window.markdownitTaskLists, { enabled: true, label: true });
    }
    if (window.texmath) {
      this.registerTexmathPlugin();
    }
  }

  compile(markdownText: string, preserveFrontmatterOpen: boolean): string {
    const fm = FrontmatterParser.extract(markdownText);
    const escapeHtml = (s: string): string => this.engine.utils.escapeHtml(s);
    const frontmatterHtml = fm.found ? FrontmatterRenderer.render(fm, preserveFrontmatterOpen, escapeHtml) : '';
    const html = frontmatterHtml + this.engine.render(fm.body);
    return this.sanitizer.sanitizeDocument(html);
  }
}

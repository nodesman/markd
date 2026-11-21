// YAML frontmatter disclosure panel and fail-open error HTML rendering

class FrontmatterRenderer {
  private static renderValue(entry: FrontmatterEntry, escapeHtml: (s: string) => string): string {
    if (entry.type === 'array') {
      const chips = entry.value.map((v: string): string => '<span class="markd-frontmatter-chip">' + escapeHtml(v) + '</span>').join('');
      return '<div class="markd-frontmatter-chips">' + chips + '</div>';
    }
    if (entry.type === 'bool') {
      const boolCls = entry.value ? 'markd-frontmatter-bool' : 'markd-frontmatter-bool is-false';
      return '<span class="' + boolCls + '">' + String(entry.value) + '</span>';
    }
    if (entry.type === 'nested') return '<div class="markd-frontmatter-nested">' + escapeHtml(entry.value) + '</div>';
    return escapeHtml(entry.value);
  }

  private static renderErrorRow(line: string, idx: number, errorLine: number, escapeHtml: (s: string) => string): string {
    const lineNum = idx + 2;
    const rowCls = lineNum === errorLine ? 'markd-frontmatter-raw-line is-error-line' : 'markd-frontmatter-raw-line';
    return '<div class="' + rowCls + '"><span class="markd-frontmatter-ln">' + lineNum + '</span><span class="markd-frontmatter-code">' + escapeHtml(line || ' ') + '</span></div>';
  }

  private static renderError(fm: FoundFrontmatter, error: FrontmatterError, escapeHtml: (s: string) => string): string {
    const rawRows = fm.yamlLines.map((line: string, idx: number): string => {
      return FrontmatterRenderer.renderErrorRow(line, idx, error.line, escapeHtml);
    }).join('');
    return [
      '<details class="markd-frontmatter markd-frontmatter-error" open>',
      '<summary><span class="markd-frontmatter-title"><span class="markd-frontmatter-chevron">▶</span>Frontmatter <span class="markd-frontmatter-warn">Syntax issue · line ' + error.line + '</span></span><span class="markd-frontmatter-hint">Switch to Edit (⌘E) to fix</span></summary>',
      '<div class="markd-frontmatter-body"><div class="markd-frontmatter-error-bar">' + escapeHtml(error.message) + '</div><div class="markd-frontmatter-raw">' + rawRows + '</div></div>',
      '</details>'
    ].join('');
  }

  private static renderEntryRow(entry: FrontmatterEntry, escapeHtml: (s: string) => string): string {
    return '<tr><td class="markd-frontmatter-key">' + escapeHtml(entry.key) + '</td><td class="markd-frontmatter-val">' + FrontmatterRenderer.renderValue(entry, escapeHtml) + '</td></tr>';
  }

  private static renderSummaryHint(entries: FrontmatterEntry[]): string {
    const previewKeys = entries.slice(0, 3).map((e: FrontmatterEntry): string => e.key).join(', ');
    return previewKeys + (entries.length > 3 ? ', …' : '');
  }

  static render(fm: FoundFrontmatter, preserveOpen: boolean, escapeHtml: (s: string) => string): string {
    if (fm.error) return FrontmatterRenderer.renderError(fm, fm.error, escapeHtml);
    const openAttr = preserveOpen ? ' open' : '';
    let bodyHtml = '<div class="markd-frontmatter-empty">Empty Jekyll frontmatter block (0 fields)</div>';
    let hint = 'empty block';
    if (fm.entries.length > 0) {
      const rows = fm.entries.map((entry: FrontmatterEntry): string => FrontmatterRenderer.renderEntryRow(entry, escapeHtml)).join('');
      bodyHtml = '<table class="markd-frontmatter-table"><tbody>' + rows + '</tbody></table>';
      hint = FrontmatterRenderer.renderSummaryHint(fm.entries);
    }
    return [
      '<details class="markd-frontmatter"' + openAttr + '>',
      '<summary><span class="markd-frontmatter-title"><span class="markd-frontmatter-chevron">▶</span>Frontmatter <span class="markd-frontmatter-count">' + fm.entries.length + ' fields</span></span><span class="markd-frontmatter-hint">' + escapeHtml(hint) + '</span></summary>',
      '<div class="markd-frontmatter-body">' + bodyHtml + '</div>',
      '</details>'
    ].join('');
  }
}

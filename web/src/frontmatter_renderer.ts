// YAML frontmatter disclosure panel and fail-open error HTML rendering

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

function renderFrontmatterErrorRow(line: string, idx: number, errorLine: number): string {
    const lineNum = idx + 2;
    const rowCls = lineNum === errorLine ? 'markd-frontmatter-raw-line is-error-line' : 'markd-frontmatter-raw-line';
    return '<div class="' + rowCls + '"><span class="markd-frontmatter-ln">' + lineNum + '</span><span class="markd-frontmatter-code">' + md.utils.escapeHtml(line || ' ') + '</span></div>';
}

function renderFrontmatterErrorHtml(fm: FoundFrontmatter, error: FrontmatterError): string {
    const rawRows = fm.yamlLines.map(function (line: string, idx: number): string {
        return renderFrontmatterErrorRow(line, idx, error.line);
    }).join('');
    return [
        '<details class="markd-frontmatter markd-frontmatter-error" open>',
        '<summary><span class="markd-frontmatter-title"><span class="markd-frontmatter-chevron">▶</span>Frontmatter <span class="markd-frontmatter-warn">Syntax issue · line ' + error.line + '</span></span><span class="markd-frontmatter-hint">Switch to Edit (⌘E) to fix</span></summary>',
        '<div class="markd-frontmatter-body"><div class="markd-frontmatter-error-bar">' + md.utils.escapeHtml(error.message) + '</div><div class="markd-frontmatter-raw">' + rawRows + '</div></div>',
        '</details>'
    ].join('');
}

function renderFrontmatterEntryRow(entry: FrontmatterEntry): string {
    return '<tr><td class="markd-frontmatter-key">' + md.utils.escapeHtml(entry.key) + '</td><td class="markd-frontmatter-val">' + renderFrontmatterValueHtml(entry) + '</td></tr>';
}

function renderFrontmatterSummaryHint(entries: FrontmatterEntry[]): string {
    const previewKeys = entries.slice(0, 3).map(function (e: FrontmatterEntry): string { return e.key; }).join(', ');
    return previewKeys + (entries.length > 3 ? ', …' : '');
}

function renderFrontmatterHtml(fm: FoundFrontmatter, preserveOpen: boolean): string {
    if (fm.error) return renderFrontmatterErrorHtml(fm, fm.error);
    const openAttr = preserveOpen ? ' open' : '';
    let bodyHtml = '<div class="markd-frontmatter-empty">Empty Jekyll frontmatter block (0 fields)</div>';
    let hint = 'empty block';
    if (fm.entries.length > 0) {
        const rows = fm.entries.map(renderFrontmatterEntryRow).join('');
        bodyHtml = '<table class="markd-frontmatter-table"><tbody>' + rows + '</tbody></table>';
        hint = renderFrontmatterSummaryHint(fm.entries);
    }
    return [
        '<details class="markd-frontmatter"' + openAttr + '>',
        '<summary><span class="markd-frontmatter-title"><span class="markd-frontmatter-chevron">▶</span>Frontmatter <span class="markd-frontmatter-count">' + fm.entries.length + ' fields</span></span><span class="markd-frontmatter-hint">' + md.utils.escapeHtml(hint) + '</span></summary>',
        '<div class="markd-frontmatter-body">' + bodyHtml + '</div>',
        '</details>'
    ].join('');
}

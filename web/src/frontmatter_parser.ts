// YAML frontmatter extraction, plausibility gating, and fail-open syntax parsing

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
    if ((first === "\"" && last === "\"") || (first === "'" && last === "'")) return text.slice(1, -1);
    return text;
}

function hasUnclosedQuote(text: string): boolean {
    const first = text.charAt(0);
    if (first !== "\"" && first !== "'") return false;
    return text.length === 1 || text.charAt(text.length - 1) !== first;
}

function validateScalarValue(rest: string, lineNum: number): FrontmatterError | null {
    if (hasUnclosedQuote(rest)) return { line: lineNum, message: 'Unclosed quoted string (line ' + lineNum + ')' };
    if (rest.charAt(0) === '[' && rest.charAt(rest.length - 1) !== ']') return { line: lineNum, message: 'Unclosed array bracket "[" (line ' + lineNum + ')' };
    if (rest.charAt(0) === '{' && rest.slice(0, 2) !== '{{' && rest.charAt(rest.length - 1) !== '}') return { line: lineNum, message: 'Unclosed inline brace "{" (line ' + lineNum + ')' };
    return null;
}

function recordNestedItem(line: string, items: string[], allListItems: boolean): boolean {
    const trimmed = line.trim();
    const stillList = /^- /.test(trimmed) ? allListItems : false;
    items.push(line.replace(/^[ ]{1,2}/, ''));
    return stillList;
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
        if (line.trim() !== '') allListItems = recordNestedItem(line, items, allListItems);
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

function isNestedBlockIndicator(rest: string): boolean {
    return rest === '' || rest === '|' || rest === '>' || rest === '|-' || rest === '>-';
}

function consumeNestedEntry(yamlLines: string[], idx: number, key: string, entries: FrontmatterEntry[]): { nextIndex: number; error: FrontmatterError | null } {
    const block = collectNestedBlock(yamlLines, idx + 1);
    if (block.error) return { nextIndex: idx, error: block.error };
    entries.push(block.type === 'array' ? { key: key, type: 'array', value: block.value } : { key: key, type: 'nested', value: block.value });
    return { nextIndex: block.nextIndex, error: null };
}

function parseFrontmatterLine(yamlLines: string[], idx: number, entries: FrontmatterEntry[]): { nextIndex: number; error: FrontmatterError | null } {
    const line = yamlLines[idx];
    const lineNum = idx + 2;
    if (isIgnorableYamlLine(line)) return { nextIndex: idx + 1, error: null };
    if (line.charAt(0) === '\t') return { nextIndex: idx, error: { line: lineNum, message: 'Tab indentation is not allowed in YAML (line ' + lineNum + ')' } };
    const match = line.match(/^([A-Za-z0-9_.-]+)[ \t]*:(.*)$/);
    if (!match) return { nextIndex: idx, error: { line: lineNum, message: 'Expected "key: value" pair (line ' + lineNum + ')' } };
    const key = match[1];
    const rest = match[2].trim();
    const valueErr = validateScalarValue(rest, lineNum);
    if (valueErr) return { nextIndex: idx, error: valueErr };
    if (isNestedBlockIndicator(rest)) return consumeNestedEntry(yamlLines, idx, key, entries);
    entries.push(parseSingleYamlEntry(key, rest));
    return { nextIndex: idx + 1, error: null };
}

function parseFrontmatterYaml(yamlLines: string[]): ParsedFrontmatterYaml {
    const entries: FrontmatterEntry[] = [];
    let idx = 0;
    while (idx < yamlLines.length) {
        const step = parseFrontmatterLine(yamlLines, idx, entries);
        if (step.error) return { entries: [], error: step.error };
        idx = step.nextIndex;
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

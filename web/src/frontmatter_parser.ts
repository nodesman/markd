// YAML frontmatter extraction, plausibility gating, and fail-open syntax parsing

class FrontmatterParser {
  private static isOpenFence(rawFirstLine: string): boolean {
    const line = rawFirstLine.charCodeAt(0) === 0xFEFF ? rawFirstLine.slice(1) : rawFirstLine;
    return /^---[ \t]*$/.test(line);
  }

  private static findCloseIndex(lines: string[]): number {
    for (let i = 1; i < lines.length; i++) {
      if (/^(---|\.\.\.)[ \t]*$/.test(lines[i])) return i;
    }
    return -1;
  }

  private static isIgnorableLine(line: string): boolean {
    const trimmed = line.trim();
    return trimmed === '' || trimmed.charAt(0) === '#';
  }

  private static isTopLevelKey(line: string): boolean {
    return /^[A-Za-z0-9_.-]+[ \t]*:/.test(line);
  }

  private static findNextNonEmptyIndex(lines: string[], startIndex: number, endIndex: number): number {
    for (let i = startIndex; i < endIndex; i++) {
      if (lines[i].trim() !== '') return i;
    }
    return -1;
  }

  private static hasHeadingFollowedByProse(lines: string[], closeIndex: number): boolean {
    for (let i = 1; i < closeIndex; i++) {
      if (!/^#{1,6}[ \t]+/.test(lines[i].trim())) continue;
      const nextIdx = FrontmatterParser.findNextNonEmptyIndex(lines, i + 1, closeIndex);
      if (nextIdx >= 0 && !FrontmatterParser.isIgnorableLine(lines[nextIdx]) && !FrontmatterParser.isTopLevelKey(lines[nextIdx])) {
        return true;
      }
    }
    return false;
  }

  private static isPlausible(lines: string[], closeIndex: number): boolean {
    if (FrontmatterParser.hasHeadingFollowedByProse(lines, closeIndex)) return false;
    let sawContent = false;
    for (let i = 1; i < closeIndex; i++) {
      if (FrontmatterParser.isIgnorableLine(lines[i])) continue;
      sawContent = true;
      if (FrontmatterParser.isTopLevelKey(lines[i])) return true;
    }
    return !sawContent;
  }

  private static unquoteValue(text: string): string {
    if (text.length < 2) return text;
    const first = text.charAt(0);
    const last = text.charAt(text.length - 1);
    if ((first === '"' && last === '"') || (first === "'" && last === "'")) return text.slice(1, -1);
    return text;
  }

  private static hasUnclosedQuote(text: string): boolean {
    const first = text.charAt(0);
    if (first !== '"' && first !== "'") return false;
    return text.length === 1 || text.charAt(text.length - 1) !== first;
  }

  private static validateScalar(rest: string, lineNum: number): FrontmatterError | null {
    if (FrontmatterParser.hasUnclosedQuote(rest)) return { line: lineNum, message: 'Unclosed quoted string (line ' + lineNum + ')' };
    if (rest.charAt(0) === '[' && rest.charAt(rest.length - 1) !== ']') return { line: lineNum, message: 'Unclosed array bracket "[" (line ' + lineNum + ')' };
    if (rest.charAt(0) === '{' && rest.slice(0, 2) !== '{{' && rest.charAt(rest.length - 1) !== '}') return { line: lineNum, message: 'Unclosed inline brace "{" (line ' + lineNum + ')' };
    return null;
  }

  private static recordNestedItem(line: string, items: string[], allListItems: boolean): boolean {
    const trimmed = line.trim();
    const stillList = /^- /.test(trimmed) ? allListItems : false;
    items.push(line.replace(/^[ ]{1,2}/, ''));
    return stillList;
  }

  private static collectNestedBlock(yamlLines: string[], startIndex: number): NestedBlockResult {
    const items: string[] = [];
    let allListItems = true;
    let idx = startIndex;
    while (idx < yamlLines.length) {
      const line = yamlLines[idx];
      if (line.charAt(0) === '\t') return { error: { line: idx + 2, message: 'Tab indentation is not allowed in YAML (line ' + (idx + 2) + ')' } };
      const isIndented = line.charAt(0) === ' ';
      const isZeroList = /^- /.test(line);
      if (!isIndented && !isZeroList && line.trim() !== '') break;
      if (line.trim() !== '') allListItems = FrontmatterParser.recordNestedItem(line, items, allListItems);
      idx++;
    }
    if (items.length > 0 && allListItems) {
      return { nextIndex: idx, type: 'array', value: items.map((s: string): string => FrontmatterParser.unquoteValue(s.trim().slice(2).trim())) };
    }
    return { nextIndex: idx, type: 'nested', value: items.join('\n') };
  }

  private static parseSingleEntry(key: string, rest: string): FrontmatterEntry {
    if (rest.charAt(0) === '[' && rest.charAt(rest.length - 1) === ']') {
      const parts = rest.slice(1, -1).split(',').map((s: string): string => FrontmatterParser.unquoteValue(s.trim())).filter(Boolean);
      return { key: key, type: 'array', value: parts };
    }
    if (rest === 'true' || rest === 'false') return { key: key, type: 'bool', value: rest === 'true' };
    let scalar = FrontmatterParser.unquoteValue(rest);
    if (scalar.length > 500) scalar = scalar.slice(0, 500) + '…';
    return { key: key, type: 'scalar', value: scalar };
  }

  private static isNestedIndicator(rest: string): boolean {
    return rest === '' || rest === '|' || rest === '>' || rest === '|-' || rest === '>-';
  }

  private static consumeNestedEntry(yamlLines: string[], idx: number, key: string, entries: FrontmatterEntry[]): { nextIndex: number; error: FrontmatterError | null } {
    const block = FrontmatterParser.collectNestedBlock(yamlLines, idx + 1);
    if (block.error) return { nextIndex: idx, error: block.error };
    entries.push(block.type === 'array' ? { key: key, type: 'array', value: block.value } : { key: key, type: 'nested', value: block.value });
    return { nextIndex: block.nextIndex, error: null };
  }

  private static parseLine(yamlLines: string[], idx: number, entries: FrontmatterEntry[]): { nextIndex: number; error: FrontmatterError | null } {
    const line = yamlLines[idx];
    const lineNum = idx + 2;
    if (FrontmatterParser.isIgnorableLine(line)) return { nextIndex: idx + 1, error: null };
    if (line.charAt(0) === '\t') return { nextIndex: idx, error: { line: lineNum, message: 'Tab indentation is not allowed in YAML (line ' + lineNum + ')' } };
    const match = line.match(/^([A-Za-z0-9_.-]+)[ \t]*:(.*)$/);
    if (!match) return { nextIndex: idx, error: { line: lineNum, message: 'Expected "key: value" pair (line ' + lineNum + ')' } };
    const key = match[1];
    const rest = match[2].trim();
    const valueErr = FrontmatterParser.validateScalar(rest, lineNum);
    if (valueErr) return { nextIndex: idx, error: valueErr };
    if (FrontmatterParser.isNestedIndicator(rest)) return FrontmatterParser.consumeNestedEntry(yamlLines, idx, key, entries);
    entries.push(FrontmatterParser.parseSingleEntry(key, rest));
    return { nextIndex: idx + 1, error: null };
  }

  private static parseYaml(yamlLines: string[]): ParsedFrontmatterYaml {
    const entries: FrontmatterEntry[] = [];
    let idx = 0;
    while (idx < yamlLines.length) {
      const step = FrontmatterParser.parseLine(yamlLines, idx, entries);
      if (step.error) return { entries: [], error: step.error };
      idx = step.nextIndex;
    }
    return { entries: entries, error: null };
  }

  static extract(markdownText: string): FrontmatterExtraction {
    const normalized = markdownText.replace(/\r\n/g, '\n');
    const lines = normalized.split('\n');
    if (lines.length < 2 || !FrontmatterParser.isOpenFence(lines[0])) return { found: false, body: markdownText };
    const closeIndex = FrontmatterParser.findCloseIndex(lines);
    if (closeIndex < 0 || !FrontmatterParser.isPlausible(lines, closeIndex)) return { found: false, body: markdownText };
    const yamlLines = lines.slice(1, closeIndex);
    const parsed = FrontmatterParser.parseYaml(yamlLines);
    return { found: true, yamlLines: yamlLines, entries: parsed.entries, error: parsed.error, body: lines.slice(closeIndex + 1).join('\n') };
  }
}

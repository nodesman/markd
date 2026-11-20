'use strict';

const fs = require('node:fs');
const path = require('node:path');
const { execFileSync } = require('node:child_process');

const BASE64_CHARS = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
const BASE64_INDEX = Object.create(null);
for (let i = 0; i < BASE64_CHARS.length; i++) {
    BASE64_INDEX[BASE64_CHARS[i]] = i;
}

const RUNTIME_MODULES = [
    'dom_security',
    'bridge_channel',
    'dom_interactions',
    'markdown_engine',
    'mermaid_loader',
    'mermaid_renderer',
    'frontmatter_parser',
    'frontmatter_renderer',
    'scroll_navigation',
    'block_resize',
    'diagram_containers',
    'fullview_zoom',
    'fullview_modal',
    'render'
];

function parseTscPath(argv, webDir) {
    const flagIndex = argv.indexOf('--tsc');
    if (flagIndex !== -1) {
        const explicit = argv[flagIndex + 1];
        if (!explicit) throw new Error('Missing value for --tsc flag');
        return explicit;
    }
    const binName = process.platform === 'win32' ? 'tsc.cmd' : 'tsc';
    const localTsc = path.join(webDir, 'node_modules', '.bin', binName);
    if (fs.existsSync(localTsc)) return localTsc;
    return 'tsc';
}

function encodeVlqInt(value) {
    let vlq = value < 0 ? ((-value) << 1) + 1 : (value << 1);
    let encoded = '';
    do {
        let digit = vlq & 31;
        vlq >>>= 5;
        if (vlq > 0) digit |= 32;
        encoded += BASE64_CHARS[digit];
    } while (vlq > 0);
    return encoded;
}

function decodeVlqSegment(segment) {
    const values = [];
    let result = 0;
    let shift = 0;
    for (let i = 0; i < segment.length; i++) {
        const digit = BASE64_INDEX[segment[i]];
        if (digit === undefined) throw new Error('Invalid VLQ character: ' + segment[i]);
        result |= (digit & 31) << shift;
        shift += 5;
        if ((digit & 32) !== 0) continue;
        const isNegative = (result & 1) === 1;
        const magnitude = result >>> 1;
        values.push(isNegative ? -magnitude : magnitude);
        result = 0;
        shift = 0;
    }
    if (shift > 0) throw new Error('Truncated VLQ segment: ' + segment);
    return values;
}

function remapSegment(segment, state, sourceOffset, nameOffset) {
    const fields = decodeVlqSegment(segment);
    if (fields.length === 0) return '';
    state.inGenCol += fields[0];
    const parts = [encodeVlqInt(state.inGenCol - state.outGenCol)];
    state.outGenCol = state.inGenCol;
    if (fields.length >= 4) {
        state.inSrc += fields[1];
        state.inOrigLine += fields[2];
        state.inOrigCol += fields[3];
        const absSrc = sourceOffset + state.inSrc;
        parts.push(encodeVlqInt(absSrc - state.outSrc));
        parts.push(encodeVlqInt(state.inOrigLine - state.outOrigLine));
        parts.push(encodeVlqInt(state.inOrigCol - state.outOrigCol));
        state.outSrc = absSrc;
        state.outOrigLine = state.inOrigLine;
        state.outOrigCol = state.inOrigCol;
    }
    if (fields.length === 5) {
        state.inName += fields[4];
        const absName = nameOffset + state.inName;
        parts.push(encodeVlqInt(absName - state.outName));
        state.outName = absName;
    }
    return parts.join('');
}

function remapMappingLine(line, state, sourceOffset, nameOffset) {
    state.inGenCol = 0;
    state.outGenCol = 0;
    if (!line) return '';
    return line.split(',').map(function (seg) {
        return remapSegment(seg, state, sourceOffset, nameOffset);
    }).join(',');
}

function stripSourceMapTrailer(rawJs) {
    const lines = rawJs.replace(/\r\n/g, '\n').split('\n');
    while (lines.length > 0 && lines[lines.length - 1] === '') lines.pop();
    if (lines.length > 0 && lines[lines.length - 1].startsWith('//# sourceMappingURL=')) lines.pop();
    return lines;
}

function collectEmittedChunks(dir, prefix, out) {
    if (!fs.existsSync(dir)) return out;
    const entries = fs.readdirSync(dir, { withFileTypes: true });
    for (let i = 0; i < entries.length; i++) {
        const entry = entries[i];
        const rel = prefix ? prefix + '/' + entry.name : entry.name;
        if (entry.isDirectory()) collectEmittedChunks(path.join(dir, entry.name), rel, out);
        if (!entry.isDirectory() && entry.name.endsWith('.js')) out.push(rel.slice(0, -3));
    }
    return out;
}

function assertChunkManifestParity(chunksDir) {
    const emitted = collectEmittedChunks(chunksDir, '', []).sort();
    const expected = RUNTIME_MODULES.slice().sort();
    if (JSON.stringify(emitted) !== JSON.stringify(expected)) {
        throw new Error('Runtime chunk mismatch. Emitted: [' + emitted.join(', ') + '] vs Expected: [' + expected.join(', ') + ']');
    }
}

function appendChunk(modName, dirs, bundle) {
    const jsPath = path.join(dirs.chunksDir, modName + '.js');
    const mapPath = path.join(dirs.chunksDir, modName + '.js.map');
    const codeLines = stripSourceMapTrailer(fs.readFileSync(jsPath, 'utf8'));
    const rawMap = JSON.parse(fs.readFileSync(mapPath, 'utf8'));
    const sourceOffset = bundle.sources.length;
    const nameOffset = bundle.names.length;
    rawMap.sources.forEach(function (src) {
        const resolved = path.resolve(dirs.chunksDir, src);
        bundle.sources.push(path.relative(dirs.distDir, resolved).split(path.sep).join('/'));
    });
    rawMap.names.forEach(function (name) { bundle.names.push(name); });
    bundle.state.inSrc = 0;
    bundle.state.inOrigLine = 0;
    bundle.state.inOrigCol = 0;
    bundle.state.inName = 0;
    const mapLines = rawMap.mappings.split(';');
    for (let i = 0; i < codeLines.length; i++) {
        bundle.codeLines.push(codeLines[i]);
        bundle.mappingLines.push(remapMappingLine(mapLines[i] || '', bundle.state, sourceOffset, nameOffset));
    }
}

function createBundleState() {
    return {
        codeLines: [], mappingLines: [], sources: [], names: [],
        state: { inGenCol: 0, inSrc: 0, inOrigLine: 0, inOrigCol: 0, inName: 0, outGenCol: 0, outSrc: 0, outOrigLine: 0, outOrigCol: 0, outName: 0 }
    };
}

function main() {
    const webDir = path.resolve(__dirname, '..');
    const distDir = path.join(webDir, 'dist');
    const chunksDir = path.join(distDir, 'chunks');
    const tscBin = parseTscPath(process.argv.slice(2), webDir);
    const useShell = process.platform === 'win32' && tscBin.toLowerCase().endsWith('.cmd');

    fs.rmSync(chunksDir, { recursive: true, force: true });
    execFileSync(tscBin, ['-p', path.join(webDir, 'tsconfig.json')], { cwd: webDir, stdio: 'inherit', shell: useShell });
    assertChunkManifestParity(chunksDir);

    const bundle = createBundleState();
    const dirs = { distDir: distDir, chunksDir: chunksDir };
    RUNTIME_MODULES.forEach(function (modName) { appendChunk(modName, dirs, bundle); });

    bundle.codeLines.push('//# sourceMappingURL=render.js.map');
    fs.writeFileSync(path.join(distDir, 'render.js'), bundle.codeLines.join('\n') + '\n', 'utf8');
    fs.writeFileSync(path.join(distDir, 'render.js.map'), JSON.stringify({
        version: 3, file: 'render.js', sourceRoot: '',
        sources: bundle.sources, names: bundle.names, mappings: bundle.mappingLines.join(';')
    }), 'utf8');
}

if (require.main === module) main();

module.exports = {
    RUNTIME_MODULES: RUNTIME_MODULES, parseTscPath: parseTscPath, encodeVlqInt: encodeVlqInt,
    decodeVlqSegment: decodeVlqSegment, remapSegment: remapSegment, remapMappingLine: remapMappingLine,
    stripSourceMapTrailer: stripSourceMapTrailer, collectEmittedChunks: collectEmittedChunks,
    assertChunkManifestParity: assertChunkManifestParity, appendChunk: appendChunk, createBundleState: createBundleState, main: main
};

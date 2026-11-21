'use strict';

const { describe, it } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const vm = require('node:vm');
const { JSDOM } = require('jsdom');
const builder = require('../scripts/build_runtime.js');

function encodeMappingLine(entries, prev) {
    let genCol = 0;
    return entries.map(function (entry) {
        const parts = [builder.encodeVlqInt(entry.gCol - genCol)];
        genCol = entry.gCol;
        if (entry.src !== undefined) {
            parts.push(builder.encodeVlqInt(entry.src - prev.src));
            parts.push(builder.encodeVlqInt(entry.oLine - prev.oLine));
            parts.push(builder.encodeVlqInt(entry.oCol - prev.oCol));
            prev.src = entry.src;
            prev.oLine = entry.oLine;
            prev.oCol = entry.oCol;
        }
        if (entry.name !== undefined) {
            parts.push(builder.encodeVlqInt(entry.name - prev.name));
            prev.name = entry.name;
        }
        return parts.join('');
    }).join(',');
}

function expandMappings(sources, names, mappingLines) {
    let src = 0;
    let oLine = 0;
    let oCol = 0;
    let name = 0;
    return mappingLines.map(function (line) {
        let gCol = 0;
        if (!line) return [];
        return line.split(',').map(function (seg) {
            const fields = builder.decodeVlqSegment(seg);
            gCol += fields[0];
            const item = { gCol: gCol };
            if (fields.length >= 4) {
                src += fields[1];
                oLine += fields[2];
                oCol += fields[3];
                item.src = sources[src];
                item.oLine = oLine;
                item.oCol = oCol;
            }
            if (fields.length === 5) {
                name += fields[4];
                item.name = names[name];
            }
            return item;
        });
    });
}

describe('build_runtime VLQ source-map merger and manifest guard', function () {
    it('encodes and decodes positive, negative, zero, and multi-byte VLQ integers', function () {
        const samples = [0, 1, -1, 15, -15, 16, -16, 1234, -1234, 65535, -65535];
        const encoded = samples.map(builder.encodeVlqInt).join('');
        assert.deepEqual(builder.decodeVlqSegment(encoded), samples);
    });

    it('rejects invalid Base64 characters and truncated VLQ continuations', function () {
        assert.throws(function () { builder.decodeVlqSegment('A!B'); }, /Invalid VLQ character/);
        assert.throws(function () { builder.decodeVlqSegment('g'); }, /Truncated VLQ segment/);
    });

    it('merges multi-chunk 5-field names mappings and negative deltas across chunks', function () {
        const tmpRoot = fs.mkdtempSync(path.join(os.tmpdir(), 'markd-vlq-'));
        const distDir = path.join(tmpRoot, 'dist');
        const chunksDir = path.join(distDir, 'chunks');
        fs.mkdirSync(chunksDir, { recursive: true });
        try {
            const prevA = { src: 0, oLine: 0, oCol: 0, name: 0 };
            const mapA = {
                version: 3,
                file: 'a.js',
                sources: ['../../src/a.ts'],
                names: ['alpha', 'beta'],
                mappings: [
                    encodeMappingLine([{ gCol: 0, src: 0, oLine: 10, oCol: 12, name: 1 }], prevA),
                    encodeMappingLine([{ gCol: 4, src: 0, oLine: 3, oCol: 2, name: 0 }], prevA)
                ].join(';')
            };
            const prevB = { src: 0, oLine: 0, oCol: 0, name: 0 };
            const mapB = {
                version: 3,
                file: 'b.js',
                sources: ['../../src/b.ts'],
                names: ['gamma'],
                mappings: [
                    encodeMappingLine([{ gCol: 2, src: 0, oLine: 1, oCol: 0, name: 0 }], prevB)
                ].join(';')
            };

            fs.writeFileSync(path.join(chunksDir, 'a.js'), 'const a1 = 1;\nconst a2 = 2;\n//# sourceMappingURL=a.js.map\n', 'utf8');
            fs.writeFileSync(path.join(chunksDir, 'a.js.map'), JSON.stringify(mapA), 'utf8');
            fs.writeFileSync(path.join(chunksDir, 'b.js'), 'const b1 = 3;\n//# sourceMappingURL=b.js.map\n', 'utf8');
            fs.writeFileSync(path.join(chunksDir, 'b.js.map'), JSON.stringify(mapB), 'utf8');

            const bundle = builder.createBundleState();
            const dirs = { distDir: distDir, chunksDir: chunksDir };
            builder.appendChunk('a', dirs, bundle);
            builder.appendChunk('b', dirs, bundle);

            assert.deepEqual(bundle.codeLines, ['const a1 = 1;', 'const a2 = 2;', 'const b1 = 3;']);
            assert.deepEqual(bundle.sources, ['../src/a.ts', '../src/b.ts']);
            assert.deepEqual(bundle.names, ['alpha', 'beta', 'gamma']);

            const expanded = expandMappings(bundle.sources, bundle.names, bundle.mappingLines);
            assert.deepEqual(expanded, [
                [{ gCol: 0, src: '../src/a.ts', oLine: 10, oCol: 12, name: 'beta' }],
                [{ gCol: 4, src: '../src/a.ts', oLine: 3, oCol: 2, name: 'alpha' }],
                [{ gCol: 2, src: '../src/b.ts', oLine: 1, oCol: 0, name: 'gamma' }]
            ]);
        } finally {
            fs.rmSync(tmpRoot, { recursive: true, force: true });
        }
    });

    it('rejects nested subdirectory chunks and missing chunks directory in manifest parity check', function () {
        const tmpRoot = fs.mkdtempSync(path.join(os.tmpdir(), 'markd-parity-'));
        const missingDir = path.join(tmpRoot, 'does_not_exist');
        const chunksDir = path.join(tmpRoot, 'chunks');
        fs.mkdirSync(path.join(chunksDir, 'nested'), { recursive: true });
        try {
            assert.throws(function () { builder.assertChunkManifestParity(missingDir); }, /Runtime chunk mismatch/);
            builder.RUNTIME_MODULES.forEach(function (modName) {
                fs.writeFileSync(path.join(chunksDir, modName + '.js'), '', 'utf8');
            });
            fs.writeFileSync(path.join(chunksDir, 'nested', 'unlisted.js'), '', 'utf8');
            assert.throws(function () { builder.assertChunkManifestParity(chunksDir); }, /nested\/unlisted/);
        } finally {
            fs.rmSync(tmpRoot, { recursive: true, force: true });
        }
    });

    it('resolves tsc binary from explicit flag, local node_modules, or PATH fallback', function () {
        const tmpWeb = fs.mkdtempSync(path.join(os.tmpdir(), 'markd-tsc-'));
        try {
            assert.equal(builder.parseTscPath(['--tsc', '/custom/tsc'], tmpWeb), '/custom/tsc');
            assert.throws(function () { builder.parseTscPath(['--tsc'], tmpWeb); }, /Missing value for --tsc/);
            assert.equal(builder.parseTscPath([], tmpWeb), 'tsc');

            const binDir = path.join(tmpWeb, 'node_modules', '.bin');
            fs.mkdirSync(binDir, { recursive: true });
            const localBin = path.join(binDir, process.platform === 'win32' ? 'tsc.cmd' : 'tsc');
            fs.writeFileSync(localBin, '', 'utf8');
            assert.equal(builder.parseTscPath([], tmpWeb), localBin);
        } finally {
            fs.rmSync(tmpWeb, { recursive: true, force: true });
        }
    });
});

describe('Compiled runtime bootstrap and async mutex invariants', function () {
    it('initializes synchronous QWebChannel on a complete document without temporal dead zone errors', async function () {
        const webDir = path.resolve(__dirname, '..');
        const dom = new JSDOM('<!DOCTYPE html><html><body class="is-hydrating"><div id="content"></div></body></html>', {
            runScripts: 'dangerously',
            url: 'http://localhost/'
        });
        const win = dom.window;
        Object.defineProperty(win.document, 'readyState', { value: 'complete', configurable: true });
        win.matchMedia = function () { return { matches: false, addEventListener: function () {} }; };
        win.markdownit = function () {
            return {
                utils: { escapeHtml: function (s) { return s; }, unescapeAll: function (s) { return s; } },
                renderer: { rules: {} },
                use: function () { return this; },
                render: function (s) { return '<p>' + s + '</p>'; }
            };
        };
        win.DOMPurify = { addHook: function () {}, sanitize: function (s) { return s; } };
        win.hljs = { getLanguage: function () { return false; }, highlight: function (s) { return { value: s }; } };
        win.scrollTo = function () {};
        win.IntersectionObserver = class { observe() {} disconnect() {} };

        let renderHandler = null;
        let themeHandler = null;
        win.qt = { webChannelTransport: {} };
        win.QWebChannel = function (_transport, callback) {
            callback({
                objects: {
                    markdBridge: {
                        logMessage: function () {},
                        renderDocument: { connect: function (fn) { renderHandler = fn; } },
                        setTheme: { connect: function (fn) { themeHandler = fn; } },
                        bridgeReady: function () {
                            themeHandler('obsidian');
                            renderHandler('# Ready Document');
                        }
                    },
                    session: null
                }
            });
        };

        const renderPath = path.join(webDir, 'dist', 'render.js');
        new vm.Script(fs.readFileSync(renderPath, 'utf8'), { filename: renderPath }).runInContext(dom.getInternalVMContext());
        await new Promise(function (resolve) { setTimeout(resolve, 0); });

        assert.equal(win.document.documentElement.getAttribute('data-theme'), 'obsidian');
        assert.ok(win.document.getElementById('content').innerHTML.includes('# Ready Document'));
        assert.equal(win.Markd.isReady, true);
    });

    it('serializes three staggered overlapping async renders through renderDownlinkMutex', async function () {
        const webDir = path.resolve(__dirname, '..');
        const dom = new JSDOM('<!DOCTYPE html><html><body class="is-hydrating"><div id="content"></div></body></html>', {
            runScripts: 'dangerously',
            url: 'http://localhost/'
        });
        const win = dom.window;
        win.matchMedia = function () { return { matches: false, addEventListener: function () {} }; };
        win.markdownit = function () {
            return {
                utils: { escapeHtml: function (s) { return s; }, unescapeAll: function (s) { return s; } },
                renderer: { rules: {} },
                use: function () { return this; },
                render: function (s) { return '<pre><code class="language-mermaid">' + s + '</code></pre>'; }
            };
        };
        win.DOMPurify = { addHook: function () {}, sanitize: function (s) { return s; } };
        win.hljs = { getLanguage: function () { return false; }, highlight: function (s) { return { value: s }; } };
        win.scrollTo = function () {};
        win.IntersectionObserver = class { observe() {} disconnect() {} };

        let inFlight = 0;
        let maxInFlight = 0;
        const resolvers = [];
        win.mermaid = {
            initialize: function () {},
            run: function () {
                inFlight++;
                maxInFlight = Math.max(maxInFlight, inFlight);
                return new Promise(function (resolve) {
                    resolvers.push(function () {
                        inFlight--;
                        resolve();
                    });
                });
            }
        };

        const renderPath = path.join(webDir, 'dist', 'render.js');
        new vm.Script(fs.readFileSync(renderPath, 'utf8'), { filename: renderPath }).runInContext(dom.getInternalVMContext());

        const first = win.renderMarkdown('graph TD; A-->1');
        const second = win.renderMarkdown('graph TD; A-->2');
        await new Promise(function (resolve) { setTimeout(resolve, 10); });
        resolvers.shift()();
        await new Promise(function (resolve) { setTimeout(resolve, 10); });
        const third = win.renderMarkdown('graph TD; A-->3');
        await new Promise(function (resolve) { setTimeout(resolve, 10); });

        assert.equal(maxInFlight, 1);
        assert.equal(inFlight, 1);

        while (resolvers.length > 0) {
            resolvers.shift()();
            await new Promise(function (resolve) { setTimeout(resolve, 10); });
        }
        await Promise.all([first, second, third]);
        assert.equal(maxInFlight, 1);
    });

    it('rerenders the latest queued document on setTheme without clobbering it with stale markdown', async function () {
        const webDir = path.resolve(__dirname, '..');
        const dom = new JSDOM('<!DOCTYPE html><html><body class="is-hydrating"><div id="content"></div></body></html>', {
            runScripts: 'dangerously',
            url: 'http://localhost/'
        });
        const win = dom.window;
        win.matchMedia = function () { return { matches: false, addEventListener: function () {} }; };
        win.markdownit = function () {
            return {
                utils: { escapeHtml: function (s) { return s; }, unescapeAll: function (s) { return s; } },
                renderer: { rules: {} },
                use: function () { return this; },
                render: function (s) {
                    if (s.indexOf('graph TD') !== -1) return '<pre><code class="language-mermaid">' + s + '</code></pre>';
                    return '<h1>' + s + '</h1>';
                }
            };
        };
        win.DOMPurify = { addHook: function () {}, sanitize: function (s) { return s; } };
        win.hljs = { getLanguage: function () { return false; }, highlight: function (s) { return { value: s }; } };
        win.scrollTo = function () {};
        win.IntersectionObserver = class { observe() {} disconnect() {} };

        let releaseFirst = null;
        win.mermaid = {
            initialize: function () {},
            run: function (opts) {
                return new Promise(function (resolve) {
                    releaseFirst = function () {
                        opts.nodes[0].innerHTML = '<svg><text>Stale Diagram</text></svg>';
                        resolve();
                    };
                });
            }
        };

        const renderPath = path.join(webDir, 'dist', 'render.js');
        new vm.Script(fs.readFileSync(renderPath, 'utf8'), { filename: renderPath }).runInContext(dom.getInternalVMContext());

        const firstRender = win.renderMarkdown('graph TD; A-->Stale');
        const secondRender = win.renderMarkdown('# Latest Document');
        win.setTheme('dark');

        releaseFirst();
        await Promise.all([firstRender, secondRender]);
        await new Promise(function (resolve) { setTimeout(resolve, 10); });

        assert.equal(win.document.documentElement.getAttribute('data-theme'), 'dark');
        assert.equal(win.document.getElementById('content').innerHTML, '<h1># Latest Document</h1>');
    });
});

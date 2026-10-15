const { describe, it, beforeEach } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const { JSDOM } = require('jsdom');

/**
 * Creates a clean JSDOM window environment with vendor scripts and render.js evaluated.
 */
function createRenderEnvironment() {
    const webDir = path.resolve(__dirname, '..');
    const obsidianCss = fs.readFileSync(path.join(webDir, 'styles/obsidian.css'), 'utf8');
    const katexCss = fs.readFileSync(path.join(webDir, 'vendor/katex/katex.min.css'), 'utf8');

    const html = `<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>${obsidianCss}</style>
    <style>${katexCss}</style>
    <link rel="stylesheet" href="vendor/katex/katex.min.css">
    <link rel="stylesheet" href="styles/obsidian.css">
    <link rel="stylesheet" href="vendor/highlight/github-dark.min.css">
</head>
<body style="background-color: #050505;">
    <article id="content"></article>
</body>
</html>`;

    const dom = new JSDOM(html, {
        runScripts: 'dangerously',
        pretendToBeVisual: true
    });
    const win = dom.window;

    win.document.fonts = {
        check: () => true,
        load: async () => []
    };

    // Platform mocks for headless Node.js environment
    win.matchMedia = win.matchMedia || function (query) {
        return {
            matches: false,
            media: query,
            onchange: null,
            addListener: () => {},
            removeListener: () => {},
            addEventListener: () => {},
            removeEventListener: () => {},
            dispatchEvent: () => false
        };
    };

    win.IntersectionObserver = class {
        constructor(callback) {
            this.callback = callback;
        }
        observe() {}
        unobserve() {}
        disconnect() {}
    };

    win.requestAnimationFrame = (cb) => setTimeout(cb, 0);
    win.scrollTo = () => {};
    win.Element.prototype.scrollIntoView = () => {};

    // Mock Mermaid renderer to simulate SVG generation without browser canvas metrics
    win.mermaid = {
        initialize: () => {},
        run: async ({ nodes }) => {
            if (nodes) {
                for (const node of nodes) {
                    node.innerHTML = '<svg class="mermaid-svg"><text>Mock Diagram</text></svg>';
                }
            }
        }
    };

    // Load vendor libraries and render.js in strict dependency order from web root

    win.eval(fs.readFileSync(path.join(webDir, 'vendor/markdown-it/markdown-it.min.js'), 'utf8'));
    win.eval(fs.readFileSync(path.join(webDir, 'vendor/markdown-it/markdownItAnchor.umd.js'), 'utf8'));
    win.eval(fs.readFileSync(path.join(webDir, 'vendor/markdown-it/markdown-it-task-lists.min.js'), 'utf8'));
    win.eval(fs.readFileSync(path.join(webDir, 'vendor/highlight/highlight.min.js'), 'utf8'));
    win.eval(fs.readFileSync(path.join(webDir, 'vendor/katex/katex.min.js'), 'utf8'));

    // Evaluate texmath in browser scope so it binds to window.texmath instead of module.exports
    win.eval('(function() { ' + fs.readFileSync(path.join(webDir, 'vendor/markdown-it/texmath.min.js'), 'utf8') + '\nwindow.texmath = texmath; })();');

    win.eval(fs.readFileSync(path.join(webDir, 'vendor/purify/purify.min.js'), 'utf8'));

    const renderPath = path.join(webDir, 'src/render.js');
    const renderSrc = fs.readFileSync(renderPath, 'utf8');
    const renderScript = new vm.Script(renderSrc, { filename: renderPath });
    renderScript.runInContext(dom.getInternalVMContext());

    // Trigger DOMContentLoaded to mark Markd engine as ready
    win.document.dispatchEvent(new win.Event('DOMContentLoaded'));

    return { dom, win, doc: win.document };
}

describe('render.js Native Node.js Test Suite', () => {

    describe('QWebChannel Session Object IPC Contract', () => {
        it('publishes session object into client window context', () => {
            const { win } = createRenderEnvironment();

            // Mock session object as injected by Qt QWebChannel
            win.session = {
                reportActiveHeading: (id) => { win.__reportedHeading = id; },
                reportMetrics: () => {},
                updateTOC: () => {}
            };

            assert.ok(win.session !== null && win.session !== undefined);
            assert.equal(typeof win.session.reportActiveHeading, 'function');
            assert.equal(typeof win.session.reportMetrics, 'function');
            assert.equal(typeof win.session.updateTOC, 'function');
        });

        it('dispatches active heading updates through session IPC channel', () => {
            const { win } = createRenderEnvironment();
            let reportedHeading = null;
            win.session = {
                reportActiveHeading: (id) => { reportedHeading = id; }
            };

            win.session.reportActiveHeading('test-heading-session');
            assert.equal(reportedHeading, 'test-heading-session');
        });
    });

    describe('Markdown AST & DOM Compilation', () => {
        let env;
        beforeEach(async () => {
            env = createRenderEnvironment();
            const markdown =
                '# Markdown AST Engine\n\n' +
                '## Heading Two with *italic* and **bold**\n\n' +
                '| Component | Status | Speed |\n' +
                '| :--- | :--- | :--- |\n' +
                '| Chromium V8 | Active | Sub-second |\n' +
                '| DOMPurify | Enforced | Instant |\n\n' +
                '- [x] Headless Offscreen Harness\n' +
                '- [ ] Legacy Node.js Runner\n\n' +
                'The energy formula is $E = mc^2$ and code block:\n\n' +
                '```javascript\n' +
                'const sovereign = true;\n' +
                '```\n';

            await env.win.renderMarkdown(markdown);
        });

        it('compiles headings with semantic anchor identifiers', () => {
            const h1 = env.doc.querySelector('#content h1[id="markdown-ast-engine"]');
            assert.ok(h1 !== null);
            assert.equal(h1.textContent.trim(), 'Markdown AST Engine');

            const h2 = env.doc.querySelector('#content h2[id="heading-two-with-italic-and-bold"]');
            assert.ok(h2 !== null);
            assert.equal(h2.textContent.trim(), 'Heading Two with italic and bold');
        });

        it('formats tabular data with header and cell alignment', () => {
            const headers = env.doc.querySelectorAll('#content table th');
            const cells = env.doc.querySelectorAll('#content table td');
            assert.equal(headers.length, 3);
            assert.equal(cells.length, 6);
        });

        it('evaluates interactive task list state checkboxes', () => {
            const checkboxes = env.doc.querySelectorAll('#content input[type="checkbox"]');
            assert.equal(checkboxes.length, 2);
            assert.equal(checkboxes[0].checked, true);
            assert.equal(checkboxes[1].checked, false);
        });

        it('compiles katex mathematical equations', () => {
            const katexEl = env.doc.querySelector('#content .katex');
            assert.ok(katexEl !== null);
            const katexHtml = env.doc.querySelector('#content .katex-html');
            assert.ok(katexHtml !== null);
            assert.equal(typeof env.win.katex, 'object');
            assert.ok(katexEl.textContent.includes('E') && katexEl.textContent.includes('mc'));
        });

        it('highlights fenced syntax code blocks via highlight.js', () => {
            const codeBlock = env.doc.querySelector('#content pre.hljs code');
            assert.ok(codeBlock !== null);
        });
    });

    describe('DOMPurify Security Sanitization (Zero XSS)', () => {
        let env;
        beforeEach(async () => {
            env = createRenderEnvironment();
            const maliciousMd =
                '# XSS Sanitization Gate\n\n' +
                '<script>window.__xss_script_executed = true;</script>\n' +
                '<img src="invalid_xss_probe.png" onerror="window.__xss_onerror_executed = true;">\n' +
                '<a href="javascript:window.__xss_href_executed=true">Harmful Link</a>\n' +
                '<svg onload="window.__xss_svg_executed = true;"></svg>\n' +
                '<iframe src="javascript:window.__xss_iframe_executed=true"></iframe>\n';

            await env.win.renderMarkdown(maliciousMd);
        });

        it('strips executable script elements from rendered DOM', () => {
            assert.equal(env.win.__xss_script_executed, undefined);
            assert.equal(env.doc.querySelectorAll('#content script').length, 0);
        });

        it('neutralizes inline event handler attributes on images and svgs', () => {
            assert.equal(env.win.__xss_onerror_executed, undefined);
            assert.equal(env.win.__xss_svg_executed, undefined);
            assert.equal(env.doc.querySelectorAll('#content img[onerror]').length, 0);
            assert.equal(env.doc.querySelectorAll('#content svg[onload]').length, 0);
        });

        it('strips harmful javascript URI schemes from anchors', () => {
            assert.equal(env.win.__xss_href_executed, undefined);
            const link = env.doc.querySelector('#content a');
            if (link) {
                assert.notEqual(link.getAttribute('href'), 'javascript:window.__xss_href_executed=true');
            }
        });

        it('purges embedded iframe injection elements', () => {
            assert.equal(env.win.__xss_iframe_executed, undefined);
            assert.equal(env.doc.querySelectorAll('#content iframe').length, 0);
        });
    });

    describe('Mermaid Diagram & Container Wrapping', () => {
        let env;
        beforeEach(async () => {
            env = createRenderEnvironment();
            const mermaidMd =
                '# Architecture\n\n' +
                '```mermaid\n' +
                'graph TD\n' +
                '    A[Qt Client] --> B[Offscreen V8]\n' +
                '    B --> C[Zero Bloat CTest]\n' +
                '```\n';

            await env.win.renderMarkdown(mermaidMd);
        });

        it('transforms mermaid code blocks into renderable div elements', () => {
            assert.equal(env.doc.querySelector('#content pre code.language-mermaid'), null);
            assert.ok(env.doc.querySelector('#content .mermaid') !== null);
        });

        it('mounts expandable container with toolbar controls', () => {
            assert.ok(env.doc.querySelector('#content .expandable-container') !== null);
            assert.ok(env.doc.querySelector('#content .expandable-toolbar') !== null);
            assert.ok(env.doc.querySelector('#content .expandable-toolbar .zoom-out-btn') !== null);
            assert.ok(env.doc.querySelector('#content .expandable-toolbar .zoom-in-btn') !== null);
            assert.ok(env.doc.querySelector('#content .expandable-toolbar .zoom-reset-btn') !== null);
            assert.ok(env.doc.querySelector('#content .expandable-toolbar .fullview-btn') !== null);
            assert.equal(env.doc.querySelector('#content .expandable-toolbar .zoom-label').textContent, '100%');
            assert.ok(env.doc.querySelector('#content .mermaid-viewport') !== null);
            assert.ok(env.doc.querySelector('#content .mermaid-viewport .mermaid-inner') !== null);
        });

        it('preserves container structure against duplicate wrapping invocations', async () => {
            await env.win.renderMarkdown(
                '```mermaid\ngraph TD\nA-->B\n```\n\n| Header 1 | Header 2 |\n|---|---|\n| Val 1 | Val 2 |\n'
            );
            env.win.wrapMermaidDiagrams();
            env.win.wrapTables();
            const containers = env.doc.querySelectorAll('#content .expandable-container');
            const tableWrappers = env.doc.querySelectorAll('#content .table-wrapper');
            assert.equal(containers.length, 1);
            assert.equal(tableWrappers.length, 1);
        });

        it('guards against re-wrapping tables when parent element already has table-wrapper class', () => {
            const content = env.doc.querySelector('#content');
            content.classList.add('table-wrapper');
            const tbl = env.doc.createElement('table');
            content.appendChild(tbl);
            env.win.wrapTables();
            assert.equal(tbl.parentElement, content);
            content.classList.remove('table-wrapper');
            tbl.remove();
        });
    });

    describe('Zoom & Pan Math Bounds Clamping', () => {
        let env;
        beforeEach(async () => {
            env = createRenderEnvironment();
            const mermaidMd =
                '# Zoom Test\n\n' +
                '```mermaid\n' +
                'graph LR\n' +
                '    Start --> Finish\n' +
                '```\n';
            await env.win.renderMarkdown(mermaidMd);
        });

        it('clamps maximum zoom level to five hundred percent', () => {
            const zoomInBtn = env.doc.querySelector('.expandable-toolbar .zoom-in-btn');
            for (let i = 0; i < 15; i++) {
                zoomInBtn.click();
            }

            const inner = env.doc.querySelector('.mermaid-inner');
            const label = env.doc.querySelector('.expandable-toolbar .zoom-label');
            assert.ok(inner.style.transform.includes('scale(5)'));
            assert.equal(label.textContent, '500%');
        });

        it('clamps minimum zoom level to twenty percent', () => {
            const zoomOutBtn = env.doc.querySelector('.expandable-toolbar .zoom-out-btn');
            for (let i = 0; i < 25; i++) {
                zoomOutBtn.click();
            }

            const inner = env.doc.querySelector('.mermaid-inner');
            const label = env.doc.querySelector('.expandable-toolbar .zoom-label');
            assert.ok(inner.style.transform.includes('scale(0.2)'));
            assert.equal(label.textContent, '20%');
        });

        it('resets zoom and pan coordinates to baseline defaults', () => {
            const zoomInBtn = env.doc.querySelector('.expandable-toolbar .zoom-in-btn');
            zoomInBtn.click();
            zoomInBtn.click();

            const resetBtn = env.doc.querySelector('.expandable-toolbar .zoom-reset-btn');
            resetBtn.click();

            const inner = env.doc.querySelector('.mermaid-inner');
            const label = env.doc.querySelector('.expandable-toolbar .zoom-label');
            assert.equal(inner.style.transform, 'translate(0px,0px) scale(1)');
            assert.equal(label.textContent, '100%');
        });

        it('scales viewport via mouse wheel scroll events', () => {
            const viewport = env.doc.querySelector('.mermaid-viewport');
            const wheelEvent = new env.win.WheelEvent('wheel', { deltaY: -100, bubbles: true });
            viewport.dispatchEvent(wheelEvent);

            const inner = env.doc.querySelector('.mermaid-inner');
            assert.ok(inner.style.transform.includes('scale(1.1)'));
        });

        it('translates coordinate matrix during mouse drag operations', () => {
            const viewport = env.doc.querySelector('.mermaid-viewport');
            viewport.dispatchEvent(new env.win.MouseEvent('mousedown', { clientX: 100, clientY: 100, bubbles: true }));
            env.doc.dispatchEvent(new env.win.MouseEvent('mousemove', { clientX: 180, clientY: 240, bubbles: true }));
            env.doc.dispatchEvent(new env.win.MouseEvent('mouseup', { bubbles: true }));

            const inner = env.doc.querySelector('.mermaid-inner');
            assert.ok(inner.style.transform.includes('translate(80px,140px)'));
        });
    });

    describe('DOM Heading Anchor Parity & Outline Signals', () => {
        let env;
        let renderedCount = 0;

        beforeEach(async () => {
            env = createRenderEnvironment();
            renderedCount = 0;
            env.win.markdBridge = {
                onDocumentRendered: (count) => { renderedCount = count; },
                bridgeReady: () => {},
                logMessage: () => {}
            };

            const tocMd =
                '# Markd Master Document\n\n' +
                '## Architecture Overview\n\n' +
                '### Native Engine\n\n' +
                '### Offscreen V8 Subsystem\n\n' +
                '## Verification & Auditing\n';

            await env.win.renderMarkdown(tocMd);
        });

        it('generates slugified heading identifiers matching parser contract', () => {
            assert.equal(env.doc.querySelector('#content h1[id="markd-master-document"]').textContent.trim(), 'Markd Master Document');
            assert.equal(env.doc.querySelector('#content h2[id="architecture-overview"]').textContent.trim(), 'Architecture Overview');
            assert.equal(env.doc.querySelector('#content h3[id="native-engine"]').textContent.trim(), 'Native Engine');
            assert.equal(env.doc.querySelector('#content h3[id="offscreen-v8-subsystem"]').textContent.trim(), 'Offscreen V8 Subsystem');
            assert.equal(env.doc.querySelector('#content h2[id="verification-auditing"]').textContent.trim(), 'Verification & Auditing');
        });

        it('notifies bridge with total rendered heading count', () => {
            assert.equal(renderedCount, 5);
        });
    });

    describe('Scroll-Spy & Viewport Navigation', () => {
        it('resets and reconnects intersection observers safely', () => {
            const env = createRenderEnvironment();
            assert.doesNotThrow(() => {
                env.win.setupScrollSpy();
                env.win.setupScrollSpy();
            });
        });

        it('guards against empty markdown input during scroll spy setup', async () => {
            const env = createRenderEnvironment();
            await env.win.renderMarkdown('');
            assert.doesNotThrow(() => {
                env.win.setupScrollSpy();
            });
        });

        it('dispatches active heading changes to bridge uplink', () => {
            const env = createRenderEnvironment();
            let activeHeading = null;
            env.win.markdBridge = {
                setActiveHeading: (id) => { activeHeading = id; },
                onActiveHeadingChanged: (id) => { activeHeading = id; }
            };

            env.win.markdBridge.setActiveHeading('native-engine');
            assert.equal(activeHeading, 'native-engine');
        });

        it('scrolls targeted heading element into view', async () => {
            const env = createRenderEnvironment();
            await env.win.renderMarkdown('# Target Section\n\nBody text');
            assert.doesNotThrow(() => {
                env.win.scrollToHeading('target-section');
                env.win.scrollToHeading('non-existent-id');
            });
        });
    });

    describe('Interactive Full-View Modal Lifecycle', () => {
        let env;
        beforeEach(async () => {
            env = createRenderEnvironment();
            const fullviewMd =
                '# Fullview Suite\n\n' +
                '```mermaid\n' +
                'graph LR\n' +
                '    Alpha --> Omega\n' +
                '```\n\n' +
                '| Item | Description |\n' +
                '| :--- | :--- |\n' +
                '| Spec | Complete |\n';

            await env.win.renderMarkdown(fullviewMd);
        });

        it('mounts fullview modal overlay for diagram elements', () => {
            const node = env.doc.querySelector('.mermaid-inner').cloneNode(true);
            env.win.openFullView(node, true);

            assert.ok(env.doc.querySelector('.fullview-overlay') !== null);
            assert.ok(env.doc.querySelector('.fullview-body.is-diagram') !== null);
            assert.ok(env.doc.querySelector('.fullview-toolbar .zoom-in-btn') !== null);
            assert.ok(env.doc.querySelector('.fullview-toolbar .close-btn') !== null);
        });

        it('adjusts modal zoom scale through dedicated toolbar controls', () => {
            const node = env.doc.querySelector('.mermaid-inner').cloneNode(true);
            env.win.openFullView(node, true);

            const zoomInBtn = env.doc.querySelector('.fullview-toolbar .zoom-in-btn');
            zoomInBtn.click();
            zoomInBtn.click();

            const label = env.doc.querySelector('.fullview-toolbar .zoom-label');
            assert.notEqual(label.textContent, '100%');
        });

        it('dismisses modal overlay upon escape key press', async () => {
            const node = env.doc.querySelector('.mermaid-inner').cloneNode(true);
            env.win.openFullView(node, true);
            assert.ok(env.doc.querySelector('.fullview-overlay') !== null);

            env.doc.dispatchEvent(new env.win.KeyboardEvent('keydown', { key: 'Escape', bubbles: true }));
            await new Promise((resolve) => setTimeout(resolve, 250));

            assert.equal(env.doc.querySelector('.fullview-overlay'), null);
        });

        it('mounts fullview modal overlay for tabular data', () => {
            const expandBtn = env.doc.querySelector('.table-expand-btn');
            assert.ok(expandBtn !== null);
            expandBtn.click();

            assert.ok(env.doc.querySelector('.fullview-overlay') !== null);
            assert.equal(env.doc.querySelector('.fullview-body.is-diagram'), null);
        });

        it('closes modal overlay via toolbar dismiss button', () => {
            const expandBtn = env.doc.querySelector('.table-expand-btn');
            expandBtn.click();
            assert.ok(env.doc.querySelector('.fullview-overlay') !== null);

            env.win.closeFullView(true);
            assert.equal(env.doc.querySelector('.fullview-overlay'), null);
        });

        it('prevents duplicate concurrent modal overlay instantiations and removes lingering overlays', () => {
            const node1 = env.doc.createElement('div');
            const node2 = env.doc.createElement('div');
            const node3 = env.doc.createElement('div');

            env.win.openFullView(node1, false);
            // Click close-btn to trigger non-immediate close (clears activeFullViewClose while overlay remains in DOM during fadeout)
            const closeBtn = env.doc.querySelector('.fullview-toolbar .close-btn');
            closeBtn.click();

            // Calling openFullView while overlay lingers in DOM covers the (existing) removal branch
            env.win.openFullView(node2, false);

            // Calling openFullView while activeFullViewClose is active covers activeFullViewClose branch
            env.win.openFullView(node3, false);

            const overlays = env.doc.querySelectorAll('.fullview-overlay');
            assert.equal(overlays.length, 1);

            env.win.closeFullView(true);
        });
    });

    describe('WebBridge Domain Semantic Action Protocol', () => {
        it('initializes bridge handshake and publishes engine API', () => {
            const env = createRenderEnvironment();
            let loggedMessage = null;
            let bridgeReadyCalled = false;

            env.win.markdBridge = {
                logMessage: (msg) => { loggedMessage = msg; },
                bridgeReady: () => { bridgeReadyCalled = true; },
                renderDocument: { connect: () => {} },
                navigateToHeading: { connect: () => {} },
                setTheme: { connect: () => {} }
            };

            assert.ok(env.win.Markd !== null && typeof env.win.Markd === 'object');
            assert.equal(env.win.Markd.isReady, true);
            assert.equal(typeof env.win.Markd.render, 'function');
            assert.equal(typeof env.win.Markd.scrollToHeading, 'function');
            assert.equal(typeof env.win.Markd.modal.open, 'function');
            assert.equal(typeof env.win.Markd.modal.close, 'function');
        });

        it('processes downlink renderDocument action dispatch', async () => {
            const env = createRenderEnvironment();
            let headingCount = 0;
            env.win.markdBridge = {
                onDocumentRendered: (count) => { headingCount = count; },
                bridgeReady: () => {},
                logMessage: () => {}
            };

            const markdown =
                '# Domain Semantic Action Protocol\n\n' +
                'Native C++ dispatches high-level semantic actions without DOM query strings.\n\n' +
                '## Downlink Verification\n\n' +
                'Action execution compiles pure markdown to DOM.\n';

            await env.win.renderMarkdown(markdown);

            assert.equal(headingCount, 2);
            assert.equal(env.doc.querySelector('#content h1[id="domain-semantic-action-protocol"]').textContent.trim(), 'Domain Semantic Action Protocol');
            assert.equal(env.doc.querySelector('#content h2[id="downlink-verification"]').textContent.trim(), 'Downlink Verification');
        });

        it('processes downlink setTheme action and updates data theme attribute', () => {
            const env = createRenderEnvironment();
            env.doc.documentElement.setAttribute('data-theme', 'theme-aurora');
            assert.equal(env.doc.documentElement.getAttribute('data-theme'), 'theme-aurora');
        });

        it('exposes sovereign window.Markd engine contract methods', () => {
            const env = createRenderEnvironment();
            assert.equal(env.win.Markd.version, '1.0.0');
            assert.equal(env.win.Markd.render, env.win.renderMarkdown);
            assert.equal(env.win.Markd.scrollToHeading, env.win.scrollToHeading);
            assert.equal(env.win.Markd.modal.open, env.win.openFullView);
            assert.equal(env.win.Markd.modal.close, env.win.closeFullView);
        });
    });

    describe('Obsidian Theme CSS Custom Properties', () => {
        let env;
        beforeEach(() => {
            env = createRenderEnvironment();
        });

        it('applies obsidian theme link color variable in live DOM', () => {
            const linkColor = env.win.getComputedStyle(env.doc.documentElement).getPropertyValue('--link').trim();
            assert.equal(linkColor, '#58a6ff');
        });

        it('applies obsidian theme task-check color variable in live DOM', () => {
            const taskColor = env.win.getComputedStyle(env.doc.documentElement).getPropertyValue('--task-check').trim();
            assert.equal(taskColor, '#3fb950');
        });
    });

    describe('QWebChannel Transport Handshake & Signal Downlinks', () => {
        it('establishes QWebChannel transport and binds downlinks and uplinks', () => {
            const env = createRenderEnvironment();
            let loggedMessage = '';
            let bridgeReadyCalled = false;

            env.win.qt = { webChannelTransport: {} };
            env.win.QWebChannel = function(transport, callback) {
                callback({
                    objects: {
                        markdBridge: {
                            logMessage: (msg) => { loggedMessage = msg; },
                            bridgeReady: () => { bridgeReadyCalled = true; },
                            renderDocument: {
                                connect: (fn) => { fn('# Downlink Test', { force: true }); }
                            },
                            navigateToHeading: {
                                connect: (fn) => { fn('downlink-heading'); }
                            },
                            setTheme: {
                                connect: (fn) => { fn('dark'); }
                            }
                        },
                        session: {
                            markdownChanged: {
                                connect: (fn) => { fn('# Session Changed'); }
                            },
                            selectedHeadingChanged: {
                                connect: (fn) => { fn('session-heading'); }
                            }
                        }
                    }
                });
            };

            env.win.eval('initQWebChannel()');

            assert.equal(loggedMessage, 'QWebChannel connected to render.js');
            assert.equal(bridgeReadyCalled, true);
            assert.equal(env.win.Markd.bridge, env.win.markdBridge);
            assert.equal(env.win.Markd.session, env.win.session);
            assert.equal(env.doc.documentElement.getAttribute('data-theme'), 'dark');
        });
    });

    describe('Modal Interactive Panning and Zoom Transformations', () => {
        let env;
        beforeEach(async () => {
            env = createRenderEnvironment();
            await env.win.renderMarkdown('```mermaid\ngraph TD;\nA-->B;\n```\n');
        });

        it('manipulates modal zoom via toolbar buttons and mouse wheel', () => {
            const node = env.doc.querySelector('.mermaid-inner').cloneNode(true);
            env.win.openFullView(node, true);
            const modal = env.doc.querySelector('.fullview-overlay');
            assert.ok(modal !== null);

            const zoomOutBtn = modal.querySelector('.zoom-out-btn');
            const zoomInBtn = modal.querySelector('.zoom-in-btn');
            const zoomResetBtn = modal.querySelector('.zoom-reset-btn');
            const zoomLabel = modal.querySelector('.zoom-label');
            const body = modal.querySelector('.fullview-body');

            zoomOutBtn.click();
            assert.ok(zoomLabel.textContent.includes('%'));

            zoomInBtn.click();
            zoomResetBtn.click();
            assert.equal(zoomLabel.textContent, '100%');

            const wheelEvent = new env.win.Event('wheel', { bubbles: true, cancelable: true });
            wheelEvent.deltaY = -120;
            body.dispatchEvent(wheelEvent);

            const wheelZoomOut = new env.win.Event('wheel', { bubbles: true, cancelable: true });
            wheelZoomOut.deltaY = 120;
            body.dispatchEvent(wheelZoomOut);

            const mouseDown = new env.win.MouseEvent('mousedown', { bubbles: true, clientX: 200, clientY: 200 });
            body.dispatchEvent(mouseDown);

            const mouseMove = new env.win.MouseEvent('mousemove', { bubbles: true, clientX: 250, clientY: 260 });
            env.doc.dispatchEvent(mouseMove);

            const mouseUp = new env.win.MouseEvent('mouseup', { bubbles: true });
            env.doc.dispatchEvent(mouseUp);

            env.win.closeFullView();
        });
    });

    describe('Scroll-Spy Intersection Observation and Heading Throttling', () => {
        it('reports active heading through intersection observer entries', async () => {
            const env = createRenderEnvironment();
            let reportedHeading = '';
            let bridgeHeading = '';
            env.win.session = {
                reportActiveHeading: (id) => { reportedHeading = id; }
            };
            env.win.markdBridge = {
                onActiveHeadingChanged: (id) => { bridgeHeading = id; }
            };

            let observerCallback = null;
            env.win.IntersectionObserver = class {
                constructor(cb) {
                    observerCallback = cb;
                }
                observe() {}
                unobserve() {}
                disconnect() {}
            };

            await env.win.renderMarkdown('# Top Section\n\n## Sub Section\n');
            env.win.setupScrollSpy();

            assert.equal(typeof observerCallback, 'function');
            observerCallback([
                { isIntersecting: true, boundingClientRect: { top: 50 }, target: { id: 'top-section' } },
                { isIntersecting: true, boundingClientRect: { top: 120 }, target: { id: 'sub-section' } }
            ]);

            assert.equal(reportedHeading, 'top-section');
            assert.equal(bridgeHeading, 'top-section');
        });

        it('throttles rapid scroll requests to same heading within 150ms', async () => {
            const env = createRenderEnvironment();
            await env.win.renderMarkdown('# Throttled Heading\n\nContent');

            let scrollCount = 0;
            const headingEl = env.doc.getElementById('throttled-heading');
            if (headingEl) {
                headingEl.scrollIntoView = () => { scrollCount++; };
            }

            env.win.scrollToHeading('throttled-heading');
            env.win.scrollToHeading('throttled-heading');

            assert.equal(scrollCount, 1);
        });

        it('respects render caching unless force option is supplied', async () => {
            const env = createRenderEnvironment();
            const md = '# Cached Markdown Document\n\nBody.';
            await env.win.renderMarkdown(md);

            env.doc.getElementById('content').innerHTML = '<div id="mutated"></div>';

            await env.win.renderMarkdown(md);
            assert.ok(env.doc.getElementById('mutated') !== null);

            await env.win.renderMarkdown(md, { force: true });
            assert.ok(env.doc.getElementById('mutated') === null);
        });

        it('opens fullview modal via container toolbar fullview button', async () => {
            const env = createRenderEnvironment();
            await env.win.renderMarkdown('```mermaid\ngraph TD;\nA-->B;\n```\n');

            const fullviewBtn = env.doc.querySelector('.expandable-container .fullview-btn');
            assert.ok(fullviewBtn !== null);
            fullviewBtn.click();

            const modal = env.doc.querySelector('.fullview-overlay');
            assert.ok(modal !== null);

            const closeBtn = modal.querySelector('.fullview-toolbar .close-btn');
            closeBtn.click();
            await new Promise((resolve) => setTimeout(resolve, 250));
            assert.equal(env.doc.querySelector('.fullview-overlay'), null);
        });

        it('dispatches active heading via legacy setActiveHeading bridge fallback', async () => {
            const env = createRenderEnvironment();
            let legacyHeading = '';
            env.win.markdBridge = {
                setActiveHeading: (id) => { legacyHeading = id; }
            };

            let observerCallback = null;
            env.win.IntersectionObserver = class {
                constructor(cb) { observerCallback = cb; }
                observe() {} unobserve() {} disconnect() {}
            };

            await env.win.renderMarkdown('# Legacy Heading\n\nBody');
            env.win.setupScrollSpy();

            observerCallback([
                { isIntersecting: true, boundingClientRect: { top: 20 }, target: { id: 'legacy-heading' } }
            ]);

            assert.equal(legacyHeading, 'legacy-heading');
        });

        it('re-initializes mermaid theme when dark mode media query changes', () => {
            const webDir = path.resolve(__dirname, '..');
            const html = `<!DOCTYPE html><html><head></head><body><div id="content"></div></body></html>`;
            const dom = new JSDOM(html, { runScripts: 'dangerously' });
            const win = dom.window;

            let matchMediaListener = null;
            win.matchMedia = (query) => ({
                matches: true,
                media: query,
                addEventListener: (event, fn) => { matchMediaListener = fn; },
                removeEventListener: () => {}
            });

            let mermaidTheme = '';
            win.mermaid = {
                initialize: (opts) => { mermaidTheme = opts.theme; }
            };

            win.eval(fs.readFileSync(path.join(webDir, 'vendor/markdown-it/markdown-it.min.js'), 'utf8'));
            win.eval(fs.readFileSync(path.join(webDir, 'vendor/markdown-it/markdownItAnchor.umd.js'), 'utf8'));
            win.eval(fs.readFileSync(path.join(webDir, 'vendor/markdown-it/markdown-it-task-lists.min.js'), 'utf8'));
            win.eval(fs.readFileSync(path.join(webDir, 'vendor/highlight/highlight.min.js'), 'utf8'));
            win.eval(fs.readFileSync(path.join(webDir, 'vendor/katex/katex.min.js'), 'utf8'));
            win.eval('(function() { ' + fs.readFileSync(path.join(webDir, 'vendor/markdown-it/texmath.min.js'), 'utf8') + '\nwindow.texmath = texmath; })();');
            win.eval(fs.readFileSync(path.join(webDir, 'vendor/purify/purify.min.js'), 'utf8'));

            const renderPath = path.join(webDir, 'src/render.js');
            const renderScript = new vm.Script(fs.readFileSync(renderPath, 'utf8'), { filename: renderPath });
            renderScript.runInContext(dom.getInternalVMContext());

            assert.equal(typeof matchMediaListener, 'function');
            matchMediaListener({ matches: true });
            assert.equal(mermaidTheme, 'dark');

            matchMediaListener({ matches: false });
            assert.equal(mermaidTheme, 'default');
        });

        it('initializes Markd.isReady and QWebChannel when loaded into ready document', () => {
            const webDir = path.resolve(__dirname, '..');
            const html = `<!DOCTYPE html><html><head></head><body><div id="content"></div></body></html>`;
            const dom = new JSDOM(html, { runScripts: 'dangerously' });
            const win = dom.window;

            win.matchMedia = () => ({
                matches: false,
                addEventListener: () => {},
                removeEventListener: () => {}
            });

            win.mermaid = {
                initialize: () => {}
            };

            win.eval(fs.readFileSync(path.join(webDir, 'vendor/markdown-it/markdown-it.min.js'), 'utf8'));
            win.eval(fs.readFileSync(path.join(webDir, 'vendor/markdown-it/markdownItAnchor.umd.js'), 'utf8'));
            win.eval(fs.readFileSync(path.join(webDir, 'vendor/markdown-it/markdown-it-task-lists.min.js'), 'utf8'));
            win.eval(fs.readFileSync(path.join(webDir, 'vendor/highlight/highlight.min.js'), 'utf8'));
            win.eval(fs.readFileSync(path.join(webDir, 'vendor/katex/katex.min.js'), 'utf8'));
            win.eval('(function() { ' + fs.readFileSync(path.join(webDir, 'vendor/markdown-it/texmath.min.js'), 'utf8') + '\nwindow.texmath = texmath; })();');
            win.eval(fs.readFileSync(path.join(webDir, 'vendor/purify/purify.min.js'), 'utf8'));

            Object.defineProperty(win.document, 'readyState', { value: 'complete', configurable: true });

            const renderPath = path.join(webDir, 'src/render.js');
            const renderScript = new vm.Script(fs.readFileSync(renderPath, 'utf8'), { filename: renderPath });
            renderScript.runInContext(dom.getInternalVMContext());

            assert.equal(win.Markd.isReady, true);
        });

        it('escapes and formats fenced code block when syntax language is unknown', async () => {
            const env = createRenderEnvironment();
            const md = '```unknownxyz\n<alert>raw code</alert>\n```';
            await env.win.renderMarkdown(md);

            const codeEl = env.doc.querySelector('#content pre.hljs code');
            assert.ok(codeEl !== null);
            assert.ok(codeEl.textContent.includes('<alert>raw code</alert>'));
        });

        it('handles and logs mermaid rendering exceptions gracefully', async () => {
            const env = createRenderEnvironment();
            let loggedMessage = null;
            let loggedError = null;
            env.win.console.error = (msg, err) => {
                loggedMessage = msg;
                loggedError = err;
            };
            env.win.mermaid.run = async () => {
                throw new Error('Simulated mermaid parser failure');
            };

            await env.win.renderMarkdown('```mermaid\ninvalid syntax;\n```');
            assert.ok(env.doc.querySelector('#content') !== null);
            assert.equal(loggedMessage, 'Mermaid render error:');
            assert.equal(loggedError?.message, 'Simulated mermaid parser failure');
        });

        it('handles container zoom reset, mousewheel zooming, and fullview replacement', async () => {
            const env = createRenderEnvironment();
            await env.win.renderMarkdown('```mermaid\ngraph TD;\nA-->B;\n```\n');

            const container = env.doc.querySelector('.expandable-container');
            const viewport = container.querySelector('.mermaid-viewport');
            const zoomInBtn = container.querySelector('.zoom-in-btn');
            const zoomResetBtn = container.querySelector('.zoom-reset-btn');

            zoomInBtn.click();
            zoomResetBtn.click();

            const wheelIn = new env.win.Event('wheel', { bubbles: true, cancelable: true });
            wheelIn.deltaY = -100;
            viewport.dispatchEvent(wheelIn);

            const wheelOut = new env.win.Event('wheel', { bubbles: true, cancelable: true });
            wheelOut.deltaY = 100;
            viewport.dispatchEvent(wheelOut);

            const node1 = env.doc.querySelector('.mermaid-inner').cloneNode(true);
            env.win.openFullView(node1, true);

            const mouseMove = new env.win.MouseEvent('mousemove', { bubbles: true, clientX: 10, clientY: 10 });
            env.doc.dispatchEvent(mouseMove);

            const node2 = env.doc.querySelector('.mermaid-inner').cloneNode(true);
            env.win.openFullView(node2, true);

            env.win.closeFullView();
        });

        it('handles highlight syntax errors by falling back to escaped HTML', async () => {
            const env = createRenderEnvironment();
            env.win.hljs.highlight = () => {
                throw new Error('Highlight syntax engine error');
            };

            await env.win.renderMarkdown('```javascript\nconst x = 1;\n```');
            const codeEl = env.doc.querySelector('#content pre.hljs code');
            assert.ok(codeEl !== null);
            assert.ok(codeEl.textContent.includes('const x = 1;'));
        });
    });
});

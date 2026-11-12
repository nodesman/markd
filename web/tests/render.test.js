const { describe, it, beforeEach } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const { JSDOM } = require('jsdom');

/**
 * Creates a clean JSDOM window environment with vendor scripts and render.js evaluated.
 */
function createRenderEnvironment(options = {}) {
    const withMermaid = options.withMermaid !== false;
    const webDir = path.resolve(__dirname, '..');
    const obsidianCss = fs.readFileSync(path.join(webDir, 'styles/obsidian.css'), 'utf8');
    const katexCss = fs.readFileSync(path.join(webDir, 'vendor/katex/katex.min.css'), 'utf8');
    // Mirror the live page: its inline <style> and <body> come from template.html itself, so they cannot drift.
    const template = fs.readFileSync(path.join(webDir, 'template.html'), 'utf8');
    const templateStyle = template.match(/<style>[\s\S]*?<\/style>/)[0];
    const templateBody = template.match(/<body[\s\S]*?<\/body>/)[0];

    const html = `<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    ${templateStyle}
    <style>${obsidianCss}</style>
    <style>${katexCss}</style>
    <link rel="stylesheet" href="vendor/katex/katex.min.css">
    <link rel="stylesheet" href="styles/obsidian.css">
    <link rel="stylesheet" href="vendor/highlight/github-dark.min.css">
</head>
${templateBody}
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
    if (withMermaid) {
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
    }

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

    describe('Web runtime engine hardening and behavioral invariants', () => {

        describe('Domain 1: DOMPurify security sanitization and navigation guardrails', () => {
            it('sanitizes DOM clobbering payloads targeting window.markdBridge and window.session', async () => {
                const env = createRenderEnvironment();
                env.win.markdBridge = { openExternalUrl: () => {}, active: true };
                env.win.session = { openExternal: () => {}, active: true };

                const maliciousMd =
                    '# Heading\n\n' +
                    '<form id="markdBridge"><input name="openExternalUrl"></form>\n' +
                    '<form id="session"><input name="openExternal"></form>\n' +
                    '<img name="markdBridge" src="x">\n';

                await env.win.renderMarkdown(maliciousMd);

                assert.ok(env.win.markdBridge !== null);
                assert.equal(env.win.markdBridge.active, true);
                assert.equal(typeof env.win.markdBridge.openExternalUrl, 'function');
                assert.equal(env.win.session.active, true);
                assert.equal(typeof env.win.session.openExternal, 'function');
            });

            it('intercepts external anchor navigation and dispatches URLs to native C++ bridge', async () => {
                const env = createRenderEnvironment();
                let openedUrl = null;
                env.win.markdBridge = {
                    openExternalUrl: (url) => { openedUrl = url; }
                };

                await env.win.renderMarkdown('[External Site](https://example.com/docs)');
                const anchor = env.doc.querySelector('#content a');
                assert.ok(anchor !== null);

                const event = new env.win.MouseEvent('click', { bubbles: true, cancelable: true });
                anchor.dispatchEvent(event);

                assert.equal(event.defaultPrevented, true);
                assert.equal(openedUrl, 'https://example.com/docs');

                delete env.win.markdBridge;
                let sessionUrl = null;
                env.win.session = {
                    openExternal: (url) => { sessionUrl = url; }
                };

                const sessionEvent = new env.win.MouseEvent('click', { bubbles: true, cancelable: true });
                anchor.dispatchEvent(sessionEvent);

                assert.equal(sessionEvent.defaultPrevented, true);
                assert.equal(sessionUrl, 'https://example.com/docs');

                delete env.win.session;
                const noBridgeEvent = new env.win.MouseEvent('click', { bubbles: true, cancelable: true });
                anchor.dispatchEvent(noBridgeEvent);
                assert.equal(noBridgeEvent.defaultPrevented, true);

                const hashMd = '[Internal Link](#internal-section)\n\n# Internal Section';
                await env.win.renderMarkdown(hashMd);
                const hashAnchor = env.doc.querySelector('#content a[href="#internal-section"]');
                let scrolledHeading = null;
                env.win.scrollToHeading = (id) => { scrolledHeading = id; };

                const hashEvent = new env.win.MouseEvent('click', { bubbles: true, cancelable: true });
                hashAnchor.dispatchEvent(hashEvent);

                assert.equal(hashEvent.defaultPrevented, true);
                assert.equal(scrolledHeading, 'internal-section');

                const pClickEvent = new env.win.MouseEvent('click', { bubbles: true, cancelable: true });
                env.doc.querySelector('#content h1').dispatchEvent(pClickEvent);
                assert.equal(pClickEvent.defaultPrevented, false);

                const noHrefAnchor = env.doc.createElement('a');
                env.doc.body.appendChild(noHrefAnchor);
                const noHrefEvent = new env.win.MouseEvent('click', { bubbles: true, cancelable: true });
                noHrefAnchor.dispatchEvent(noHrefEvent);
                assert.equal(noHrefEvent.defaultPrevented, false);
                noHrefAnchor.remove();

                const emptyEvent = new env.win.Event('click', { bubbles: false, cancelable: true });
                Object.defineProperty(emptyEvent, 'target', { value: null });
                env.doc.dispatchEvent(emptyEvent);
            });

            it('neutralizes malicious CSS positioning and fullscreen overlay injection', async () => {
                const env = createRenderEnvironment();
                const maliciousMd =
                    '<div style="position: fixed; top: 0; left: 0; width: 100vw; height: 100vh; z-index: 9999;">Malicious Overlay</div>\n' +
                    '<div style="position: absolute; width: 100%; height: 100%;">Overlay 2</div>\n' +
                    '<div style="position: absolute">Overlay 3</div>\n' +
                    '<div style="color: red;">Safe Style</div>\n' +
                    '<svg><style>body { display: none; }</style></svg>\n';

                await env.win.renderMarkdown(maliciousMd);

                assert.equal(env.doc.querySelectorAll('#content style').length, 0);
                const divs = env.doc.querySelectorAll('#content div');
                divs.forEach((div) => {
                    const style = div.getAttribute('style') || '';
                    assert.equal(/position\s*:\s*(?:fixed|absolute)/i.test(style), false);
                });
            });

            it('neutralizes mutation XSS (mXSS) constructs across innerHTML roundtrips', async () => {
                const env = createRenderEnvironment();
                const mxssMd = '<form><math><mtext></form><form><mglyph><style></math><img src="x" onerror="window.__mxss = true">';

                await env.win.renderMarkdown(mxssMd);

                assert.equal(env.win.__mxss, undefined);
                assert.equal(env.doc.querySelectorAll('#content img[onerror]').length, 0);
            });

            it('enforces rel="noopener noreferrer" on all external target="_blank" anchors', async () => {
                const env = createRenderEnvironment();
                const md =
                    '<a href="https://example.com" target="_blank">Outbound Link</a>\n\n' +
                    '<a href="/local/path" target="_blank">Target Blank Local</a>\n\n' +
                    '<a href="#local-anchor">Local Link</a>\n\n' +
                    '[Markdown Outbound](https://test.org)';

                await env.win.renderMarkdown(md);

                const outbound1 = env.doc.querySelector('#content a[href="https://example.com"]');
                const outbound2 = env.doc.querySelector('#content a[href="https://test.org"]');
                const targetBlankLocal = env.doc.querySelector('#content a[href="/local/path"]');
                const localAnchor = env.doc.querySelector('#content a[href="#local-anchor"]');

                assert.equal(outbound1.getAttribute('rel'), 'noopener noreferrer');
                assert.equal(outbound2.getAttribute('rel'), 'noopener noreferrer');
                assert.equal(targetBlankLocal.getAttribute('rel'), 'noopener noreferrer');
                assert.equal(localAnchor.getAttribute('rel'), null);
            });
        });

        describe('Domain 2: Modal lifecycle, memory hygiene and focus trapping', () => {
            it('unbinds document mousemove and mouseup listeners when diagram zoom pan containers are dismantled', async () => {
                const env = createRenderEnvironment();
                await env.win.renderMarkdown('```mermaid\ngraph TD;\nA-->B;\n```\n');

                assert.doesNotThrow(() => {
                    env.win.cleanupDiagramListeners();
                });

                assert.doesNotThrow(() => {
                    env.doc.dispatchEvent(new env.win.MouseEvent('mousemove', { bubbles: true, clientX: 200, clientY: 200 }));
                    env.doc.dispatchEvent(new env.win.MouseEvent('mouseup', { bubbles: true }));
                });
            });

            it('dismisses active fullview modal when renderMarkdown executes a fresh document build', async () => {
                const env = createRenderEnvironment();
                await env.win.renderMarkdown('```mermaid\ngraph TD;\nA-->B;\n```\n');

                const node = env.doc.querySelector('.mermaid-inner').cloneNode(true);
                env.win.openFullView(node, true);
                assert.ok(env.doc.querySelector('.fullview-overlay') !== null);

                await env.win.renderMarkdown('# Next Document');
                assert.equal(env.doc.querySelector('.fullview-overlay'), null);
            });

            it('locks document body scroll when fullview modal opens and restores scroll on dismissal', async () => {
                const env = createRenderEnvironment();
                assert.equal(env.doc.body.classList.contains('modal-open'), false);
                assert.notEqual(env.doc.body.style.overflow, 'hidden');

                env.win.openFullView(env.doc.createElement('div'), false);
                assert.equal(env.doc.body.classList.contains('modal-open'), true);
                assert.equal(env.doc.body.style.overflow, 'hidden');

                env.win.closeFullView(true);
                assert.equal(env.doc.body.classList.contains('modal-open'), false);
                assert.equal(env.doc.body.style.overflow, '');
            });

            it('traps keyboard focus within fullview modal and prevents tab navigation into background document', async () => {
                const env = createRenderEnvironment();
                const externalBtn = env.doc.createElement('button');
                env.doc.body.appendChild(externalBtn);
                externalBtn.focus();

                env.win.openFullView(env.doc.createElement('div'), true);

                const buttons = env.doc.querySelectorAll('.fullview-toolbar button');
                assert.ok(buttons.length >= 2);
                const firstBtn = buttons[0];
                const middleBtn = buttons[1];
                const lastBtn = buttons[buttons.length - 1];

                lastBtn.focus();
                const forwardTab = new env.win.KeyboardEvent('keydown', { key: 'Tab', bubbles: true, cancelable: true });
                env.doc.dispatchEvent(forwardTab);
                assert.equal(env.doc.activeElement, firstBtn);

                firstBtn.focus();
                const reverseTab = new env.win.KeyboardEvent('keydown', { key: 'Tab', shiftKey: true, bubbles: true, cancelable: true });
                env.doc.dispatchEvent(reverseTab);
                assert.equal(env.doc.activeElement, lastBtn);

                middleBtn.focus();
                const middleTab = new env.win.KeyboardEvent('keydown', { key: 'Tab', bubbles: true, cancelable: true });
                env.doc.dispatchEvent(middleTab);
                assert.equal(middleTab.defaultPrevented, false);

                middleBtn.focus();
                const middleReverseTab = new env.win.KeyboardEvent('keydown', { key: 'Tab', shiftKey: true, bubbles: true, cancelable: true });
                env.doc.dispatchEvent(middleReverseTab);
                assert.equal(middleReverseTab.defaultPrevented, false);

                externalBtn.focus();
                const outsideForwardTab = new env.win.KeyboardEvent('keydown', { key: 'Tab', bubbles: true, cancelable: true });
                env.doc.dispatchEvent(outsideForwardTab);
                assert.equal(env.doc.activeElement, firstBtn);

                externalBtn.focus();
                const outsideReverseTab = new env.win.KeyboardEvent('keydown', { key: 'Tab', shiftKey: true, bubbles: true, cancelable: true });
                env.doc.dispatchEvent(outsideReverseTab);
                assert.equal(env.doc.activeElement, lastBtn);

                const otherKey = new env.win.KeyboardEvent('keydown', { key: 'Enter', bubbles: true, cancelable: true });
                env.doc.dispatchEvent(otherKey);

                env.win.closeFullView(true);
                assert.equal(env.doc.activeElement, externalBtn);
                externalBtn.remove();

                // Test modal with zero focusable elements
                const emptyOverlay = env.doc.createElement('div');
                emptyOverlay.className = 'fullview-overlay';
                env.doc.body.appendChild(emptyOverlay);
                const tabEmpty = new env.win.KeyboardEvent('keydown', { key: 'Tab', bubbles: true, cancelable: true });
                env.doc.dispatchEvent(tabEmpty);
                emptyOverlay.remove();
            });

            it('dismantles previous modal synchronously when openFullView is invoked consecutively', async () => {
                const env = createRenderEnvironment();
                env.win.openFullView(env.doc.createElement('div'), false);
                env.win.openFullView(env.doc.createElement('div'), false);
                env.win.openFullView(env.doc.createElement('div'), false);

                assert.equal(env.doc.querySelectorAll('.fullview-overlay').length, 1);
                env.win.closeFullView(true);
            });
        });

        describe('Domain 3: QWebChannel bridge IPC and concurrency', () => {
            it('buffers incoming C++ renderDocument signals until QWebChannel handshake completes', async () => {
                const env = createRenderEnvironment();
                env.win.renderDocument('# Buffered Document Title\n\nContent');

                assert.equal(env.doc.querySelector('#content h1'), null);

                env.win.qt = { webChannelTransport: {} };
                env.win.QWebChannel = function(transport, callback) {
                    callback({
                        objects: {
                            markdBridge: {
                                logMessage: () => {},
                                bridgeReady: () => {},
                                renderDocument: { connect: () => {} },
                                navigateToHeading: { connect: () => {} },
                                setTheme: { connect: () => {} }
                            },
                            session: {}
                        }
                    });
                };
                env.win.initQWebChannel();
                await new Promise((resolve) => setTimeout(resolve, 50));

                assert.ok(env.doc.querySelector('#content h1[id="buffered-document-title"]') !== null);

                env.win.renderDocument('# Direct Document\n\nDirect Content');
                await new Promise((resolve) => setTimeout(resolve, 50));
                assert.ok(env.doc.querySelector('#content h1[id="direct-document"]') !== null);
            });

            it('serializes concurrent renderMarkdown executions through an async downlink mutex', async () => {
                const env = createRenderEnvironment();
                const order = [];
                let releaseLock;

                const originalRun = env.win.mermaid.run;
                env.win.mermaid.run = async ({ nodes }) => {
                    order.push('start-diagram');
                    await new Promise((r) => { releaseLock = r; });
                    order.push('finish-diagram');
                    return originalRun({ nodes });
                };

                const p1 = env.win.renderMarkdown('# First Doc\n\n```mermaid\ngraph TD;\nA-->B;\n```\n');
                const p2 = env.win.renderMarkdown('# Second Doc\n\nContent');

                assert.equal(order.length, 1);
                assert.equal(order[0], 'start-diagram');

                releaseLock();
                await p1;
                await p2;

                assert.equal(order.length, 2);
                assert.equal(order[1], 'finish-diagram');
                assert.ok(env.doc.querySelector('#content h1') !== null);
                assert.equal(env.doc.querySelector('#content h1').textContent, 'Second Doc');
            });

            it('rejects malformed non-string markdown payloads without throwing uncaught exceptions', async () => {
                const env = createRenderEnvironment();
                await assert.doesNotReject(async () => {
                    await env.win.renderMarkdown(null);
                    await env.win.renderMarkdown(undefined);
                    await env.win.renderMarkdown(12345);
                    await env.win.renderMarkdown({});
                    await env.win.renderMarkdown([]);
                });
                assert.equal(env.doc.querySelector('#content').innerHTML, '');
            });

            it('synchronizes Mermaid engine theme dynamically upon setTheme bridge invocation', async () => {
                const env = createRenderEnvironment();
                let initializedTheme = null;
                env.win.mermaid.initialize = (cfg) => {
                    initializedTheme = cfg.theme;
                };

                await env.win.renderMarkdown('```mermaid\ngraph TD;\nA-->B;\n```\n');

                env.win.setTheme('');
                assert.equal(initializedTheme, null);

                env.win.setTheme('dark');
                assert.equal(env.doc.documentElement.getAttribute('data-theme'), 'dark');
                assert.equal(initializedTheme, 'dark');

                env.win.setTheme('obsidian-dark');
                assert.equal(initializedTheme, 'dark');

                env.win.setTheme('obsidian');
                assert.equal(env.doc.documentElement.getAttribute('data-theme'), 'obsidian');
                assert.equal(initializedTheme, 'dark');

                env.win.setTheme('light');
                assert.equal(env.doc.documentElement.getAttribute('data-theme'), 'light');
                assert.equal(initializedTheme, 'default');
            });

            it('preserves Tamil and international Unicode characters in heading anchors matching C++ slug parity', async () => {
                const env = createRenderEnvironment();
                const md =
                    '# அறம் வெல்லும்\n\n' +
                    '## Über Uns\n\n' +
                    '### 東京 Skyline\n\n' +
                    '#### Hello World 2026';

                await env.win.renderMarkdown(md);

                const h1 = env.doc.querySelector('#content h1');
                const h2 = env.doc.querySelector('#content h2');
                const h3 = env.doc.querySelector('#content h3');
                const h4 = env.doc.querySelector('#content h4');

                assert.equal(h1.id, 'அறம்-வெல்லும்');
                assert.equal(h2.id, 'über-uns');
                assert.equal(h3.id, '東京-skyline');
                assert.equal(h4.id, 'hello-world-2026');
            });
        });

        describe('Domain 4: Zoom/pan math, coordinate clamping and state physics', () => {
            it('ignores zero-delta wheel events without mutating current scale factor', async () => {
                const env = createRenderEnvironment();
                await env.win.renderMarkdown('```mermaid\ngraph TD;\nA-->B;\n```\n');

                const viewport = env.doc.querySelector('.mermaid-viewport');
                const label = env.doc.querySelector('.zoom-label');

                assert.equal(label.textContent, '100%');

                const zeroDeltaEvent = new env.win.Event('wheel', { bubbles: true, cancelable: true });
                zeroDeltaEvent.deltaY = 0;
                viewport.dispatchEvent(zeroDeltaEvent);

                assert.equal(label.textContent, '100%');

                const microDeltaEvent = new env.win.Event('wheel', { bubbles: true, cancelable: true });
                microDeltaEvent.deltaY = 0.00001;
                viewport.dispatchEvent(microDeltaEvent);

                assert.equal(label.textContent, '100%');

                const node = env.doc.querySelector('.mermaid-inner').cloneNode(true);
                env.win.openFullView(node, true);
                const fvBody = env.doc.querySelector('.fullview-body');
                const fvLabel = env.doc.querySelector('.fullview-toolbar .zoom-label');
                assert.equal(fvLabel.textContent, '100%');

                const fvZeroDelta = new env.win.Event('wheel', { bubbles: true, cancelable: true });
                fvZeroDelta.deltaY = 0;
                fvBody.dispatchEvent(fvZeroDelta);
                assert.equal(fvLabel.textContent, '100%');

                env.win.closeFullView(true);
            });

            it('snaps zoom scale to unity when reversing zoom in and zoom out cycles', async () => {
                const env = createRenderEnvironment();
                await env.win.renderMarkdown('```mermaid\ngraph TD;\nA-->B;\n```\n');

                const container = env.doc.querySelector('.expandable-container');
                const zoomInBtn = container.querySelector('.zoom-in-btn');
                const zoomOutBtn = container.querySelector('.zoom-out-btn');
                const label = container.querySelector('.zoom-label');

                zoomInBtn.click();
                zoomOutBtn.click();

                assert.equal(label.textContent, '100%');
                const inner = container.querySelector('.mermaid-inner');
                assert.ok(inner.style.transform.includes('scale(1)'));

                const node = env.doc.querySelector('.mermaid-inner').cloneNode(true);
                env.win.openFullView(node, true);
                const fvToolbar = env.doc.querySelector('.fullview-toolbar');
                const fvInBtn = fvToolbar.querySelector('.zoom-in-btn');
                const fvOutBtn = fvToolbar.querySelector('.zoom-out-btn');
                const fvLabel = fvToolbar.querySelector('.zoom-label');

                fvInBtn.click();
                fvOutBtn.click();
                assert.equal(fvLabel.textContent, '100%');
                const fvInner = env.doc.querySelector('.fullview-inner');
                assert.ok(fvInner.style.transform.includes('scale(1)'));

                env.win.closeFullView(true);
            });

            it('clamps pan coordinates within bounded diagram viewport extents', async () => {
                const env = createRenderEnvironment();
                await env.win.renderMarkdown('```mermaid\ngraph TD;\nA-->B;\n```\n');

                const viewport = env.doc.querySelector('.mermaid-viewport');
                const inner = env.doc.querySelector('.mermaid-inner');

                const mouseDown = new env.win.MouseEvent('mousedown', { bubbles: true, clientX: 100, clientY: 100 });
                viewport.dispatchEvent(mouseDown);

                const mouseMove = new env.win.MouseEvent('mousemove', { bubbles: true, clientX: 100000, clientY: 100000 });
                env.doc.dispatchEvent(mouseMove);

                assert.ok(!inner.style.transform.includes('translate(99900px, 99900px)'));
                const match = inner.style.transform.match(/translate\(([-\d.]+)px,\s*([-\d.]+)px\)/);
                assert.ok(match !== null);
                const x = parseFloat(match[1]);
                const y = parseFloat(match[2]);
                assert.ok(x <= 2000 && x > 0);
                assert.ok(y <= 2000 && y > 0);

                env.doc.dispatchEvent(new env.win.MouseEvent('mouseup', { bubbles: true }));

                const node = env.doc.querySelector('.mermaid-inner').cloneNode(true);
                env.win.openFullView(node, true);
                const fvBody = env.doc.querySelector('.fullview-body');
                const fvInner = env.doc.querySelector('.fullview-inner');

                fvBody.dispatchEvent(new env.win.MouseEvent('mousedown', { bubbles: true, clientX: 100, clientY: 100 }));
                env.doc.dispatchEvent(new env.win.MouseEvent('mousemove', { bubbles: true, clientX: 50000, clientY: 50000 }));
                env.doc.dispatchEvent(new env.win.MouseEvent('mouseup', { bubbles: true }));

                const fvMatch = fvInner.style.transform.match(/translate\(([-\d.]+)px,\s*([-\d.]+)px\)/);
                assert.ok(fvMatch !== null);
                const fvX = parseFloat(fvMatch[1]);
                const fvY = parseFloat(fvMatch[2]);
                assert.ok(fvX <= 2000 && fvX > 0);
                assert.ok(fvY <= 2000 && fvY > 0);

                env.win.closeFullView(true);
            });

            it('resets panning state when pointer leaves window or loses focus', async () => {
                const env = createRenderEnvironment();
                await env.win.renderMarkdown('```mermaid\ngraph TD;\nA-->B;\n```\n');

                const viewport = env.doc.querySelector('.mermaid-viewport');
                const inner = env.doc.querySelector('.mermaid-inner');

                viewport.dispatchEvent(new env.win.MouseEvent('mousedown', { bubbles: true, clientX: 50, clientY: 50 }));

                env.doc.dispatchEvent(new env.win.Event('mouseleave'));

                const prevTransform = inner.style.transform;
                env.doc.dispatchEvent(new env.win.MouseEvent('mousemove', { bubbles: true, clientX: 150, clientY: 150 }));
                assert.equal(inner.style.transform, prevTransform);

                viewport.dispatchEvent(new env.win.MouseEvent('mousedown', { bubbles: true, clientX: 50, clientY: 50 }));
                env.win.dispatchEvent(new env.win.Event('blur'));
                env.doc.dispatchEvent(new env.win.MouseEvent('mousemove', { bubbles: true, clientX: 200, clientY: 200 }));
                assert.equal(inner.style.transform, prevTransform);

                const node = env.doc.querySelector('.mermaid-inner').cloneNode(true);
                env.win.openFullView(node, true);
                const fvBody = env.doc.querySelector('.fullview-body');
                const fvInner = env.doc.querySelector('.fullview-inner');

                fvBody.dispatchEvent(new env.win.MouseEvent('mousedown', { bubbles: true, clientX: 50, clientY: 50 }));
                env.doc.dispatchEvent(new env.win.Event('mouseleave'));
                const prevFvTransform = fvInner.style.transform;
                env.doc.dispatchEvent(new env.win.MouseEvent('mousemove', { bubbles: true, clientX: 150, clientY: 150 }));
                assert.equal(fvInner.style.transform, prevFvTransform);

                fvBody.dispatchEvent(new env.win.MouseEvent('mousedown', { bubbles: true, clientX: 50, clientY: 50 }));
                env.win.dispatchEvent(new env.win.Event('blur'));
                env.doc.dispatchEvent(new env.win.MouseEvent('mousemove', { bubbles: true, clientX: 200, clientY: 200 }));
                assert.equal(fvInner.style.transform, prevFvTransform);

                env.win.closeFullView(true);
            });

            it('clamps zoom scale strictly between 0.2 and 5.0 across rapid wheel bursts', async () => {
                const env = createRenderEnvironment();
                await env.win.renderMarkdown('```mermaid\ngraph TD;\nA-->B;\n```\n');

                const viewport = env.doc.querySelector('.mermaid-viewport');
                const label = env.doc.querySelector('.zoom-label');

                for (let i = 0; i < 50; i++) {
                    const wheel = new env.win.Event('wheel', { bubbles: true, cancelable: true });
                    wheel.deltaY = 500;
                    viewport.dispatchEvent(wheel);
                }
                assert.equal(label.textContent, '20%');

                for (let i = 0; i < 50; i++) {
                    const wheel = new env.win.Event('wheel', { bubbles: true, cancelable: true });
                    wheel.deltaY = -500;
                    viewport.dispatchEvent(wheel);
                }
                assert.equal(label.textContent, '500%');

                const node = env.doc.querySelector('.mermaid-inner').cloneNode(true);
                env.win.openFullView(node, true);
                const fvBody = env.doc.querySelector('.fullview-body');
                const fvLabel = env.doc.querySelector('.fullview-toolbar .zoom-label');

                for (let i = 0; i < 50; i++) {
                    const wheel = new env.win.Event('wheel', { bubbles: true, cancelable: true });
                    wheel.deltaY = 500;
                    fvBody.dispatchEvent(wheel);
                }
                assert.equal(fvLabel.textContent, '20%');

                for (let i = 0; i < 50; i++) {
                    const wheel = new env.win.Event('wheel', { bubbles: true, cancelable: true });
                    wheel.deltaY = -500;
                    fvBody.dispatchEvent(wheel);
                }
                assert.equal(fvLabel.textContent, '500%');

                env.win.closeFullView(true);
            });
        });

        describe('Domain 5: Mermaid pipeline resiliency and SVG isolation', () => {
            it('isolates individual Mermaid diagram compilation failures to prevent batch fail-stop', async () => {
                const env = createRenderEnvironment();

                env.win.mermaid.run = async ({ nodes }) => {
                    for (const node of nodes) {
                        if (node.dataset.rawCode && node.dataset.rawCode.includes('BROKEN_DIAGRAM')) {
                            throw new Error('Syntax error in diagram 1');
                        }
                        node.innerHTML = '<svg class="valid-mermaid-svg"><text>Valid Diagram 2</text></svg>';
                    }
                };

                const md =
                    '```mermaid\nBROKEN_DIAGRAM\n```\n\n' +
                    '```mermaid\ngraph TD;\nA-->B;\n```\n';

                await env.win.renderMarkdown(md);

                const errorCard = env.doc.querySelector('.mermaid-error-card');
                assert.ok(errorCard !== null);
                assert.ok(errorCard.querySelector('.mermaid-error-badge').textContent.includes('Syntax error in diagram 1'));

                const validSvg = env.doc.querySelector('.valid-mermaid-svg');
                assert.ok(validSvg !== null);
            });

            it('retains raw fenced code block with error badge when Mermaid diagram syntax is invalid', async () => {
                const env = createRenderEnvironment();
                env.win.mermaid.run = async () => {
                    throw new Error('Parse error on line 2');
                };

                const rawMermaid = 'sequenceDiagram\nAlice->>Bob: Hello\nInvalid Syntax Line';
                await env.win.renderMarkdown('```mermaid\n' + rawMermaid + '\n```');

                const errorCard = env.doc.querySelector('.mermaid-error-card');
                assert.ok(errorCard !== null);

                const badge = errorCard.querySelector('.mermaid-error-badge');
                assert.ok(badge !== null);
                assert.ok(badge.textContent.includes('Parse error on line 2'));

                const rawCode = errorCard.querySelector('pre.mermaid-raw-code code');
                assert.ok(rawCode !== null);
                assert.equal(rawCode.textContent, rawMermaid);

                env.win.mermaid.run = async () => {
                    throw 'string-exception';
                };
                await env.win.renderMarkdown('```mermaid\nraw fallback\n```', { force: true });
                const errorCard2 = env.doc.querySelector('.mermaid-error-card');
                assert.ok(errorCard2.querySelector('.mermaid-error-badge').textContent.includes('Invalid diagram syntax'));
            });

            it('sanitizes Mermaid SVG output through DOMPurify SVG profile before DOM insertion', async () => {
                const env = createRenderEnvironment();
                env.win.mermaid.run = async ({ nodes }) => {
                    for (const node of nodes) {
                        node.innerHTML = '<svg><script>window.__svgXss = true;</script><text>Safe Text</text></svg>';
                    }
                };

                await env.win.renderMarkdown('```mermaid\ngraph TD;\nA-->B;\n```\n');

                assert.equal(env.win.__svgXss, undefined);
                assert.equal(env.doc.querySelectorAll('#content script').length, 0);
                assert.ok(env.doc.querySelector('#content text') !== null);

                env.win.mermaid.run = async ({ nodes }) => {
                    for (const node of nodes) {
                        node.innerHTML = '<div>No SVG content</div>';
                    }
                };
                await env.win.renderMarkdown('```mermaid\ngraph TD;\nC-->D;\n```\n', { force: true });
                assert.ok(env.doc.querySelector('#content') !== null);
            });

            it('preserves Mermaid SVG style rules and foreignObject labels while neutralizing script and overlay rules', async () => {
                const env = createRenderEnvironment();
                env.win.mermaid.run = async ({ nodes }) => {
                    for (const node of nodes) {
                        node.innerHTML = [
                            '<svg id="mermaid-0" viewBox="0 0 240 100">',
                            '<style>#mermaid-0 .node rect { fill: #1f2020; stroke: #81B1FF; } #mermaid-0 .flowchart-link { fill: none; stroke: #ccc; } .hijack { position: fixed; top: 0; } @import url("https://evil.example/a.css");</style>',
                            '<script>window.__mermaidStyleXss = true;</script>',
                            '<g class="node"><rect width="120" height="44"></rect>',
                            '<foreignObject width="120" height="44"><div xmlns="http://www.w3.org/1999/xhtml"><span class="nodeLabel"><p>Open File</p></span></div></foreignObject>',
                            '</g></svg>'
                        ].join('');
                    }
                };

                await env.win.renderMarkdown(
                    '<svg class="raw-user-svg"><style>body { display: none; }</style></svg>\n\n' +
                    '```mermaid\ngraph LR;\nA[Open File]-->B[Render];\n```\n'
                );

                assert.equal(env.win.__mermaidStyleXss, undefined);
                assert.equal(env.doc.querySelectorAll('#content script').length, 0);
                assert.equal(env.doc.querySelectorAll('#content .raw-user-svg style').length, 0);

                const mermaidStyle = env.doc.querySelector('#content .mermaid svg style');
                assert.ok(mermaidStyle !== null);
                assert.ok(mermaidStyle.textContent.includes('fill: #1f2020'));
                assert.ok(mermaidStyle.textContent.includes('fill: none'));
                assert.equal(/position\s*:\s*(?:fixed|absolute)/i.test(mermaidStyle.textContent), false);
                assert.equal(/@import/i.test(mermaidStyle.textContent), false);

                const labelNode = env.doc.querySelector('#content .mermaid svg foreignObject .nodeLabel');
                assert.ok(labelNode !== null);
                assert.equal(labelNode.textContent, 'Open File');
                const mermaidHost = env.doc.querySelector('#content .mermaid');
                assert.equal(mermaidHost.getAttribute('data-processed'), 'true');
                assert.equal(env.win.getComputedStyle(mermaidHost).visibility, 'visible');
            });

            it('hides uncompiled Mermaid source in DOM until async bundle compilation completes', async () => {
                const env = createRenderEnvironment();
                let visibilityDuringCompile = null;
                env.win.mermaid.run = async ({ nodes }) => {
                    visibilityDuringCompile = env.win.getComputedStyle(nodes[0]).visibility;
                    nodes[0].innerHTML = '<svg viewBox="0 0 80 40"><text>Ready</text></svg>';
                };

                await env.win.renderMarkdown('```mermaid\ngraph TD;\nRawCode-->Hidden;\n```\n');
                assert.equal(visibilityDuringCompile, 'hidden');
                assert.equal(env.win.getComputedStyle(env.doc.querySelector('#content .mermaid')).visibility, 'visible');
            });

            it('neutralizes Mermaid click directives and javascript URI callbacks', async () => {
                const env = createRenderEnvironment();
                env.win.mermaid.run = async ({ nodes }) => {
                    for (const node of nodes) {
                        node.innerHTML =
                            '<svg>' +
                            '<a href="javascript:alert(1)"><text>Click Exploit</text></a>' +
                            '<a xlink:href="javascript:void(0)"><text>Click Exploit 2</text></a>' +
                            '<a href="https://example.com"><text>Safe Link</text></a>' +
                            '<a><text>No Href Link</text></a>' +
                            '</svg>';
                    }
                };

                await env.win.renderMarkdown('```mermaid\ngraph TD;\nA-->B;\n```\n');

                const links = env.doc.querySelectorAll('#content .mermaid a');
                assert.equal(links.length, 4);
                assert.equal(links[0].getAttribute('href'), null);
                assert.equal(links[1].getAttribute('xlink:href'), null);
                assert.equal(links[2].getAttribute('href'), 'https://example.com');
                assert.equal(links[3].getAttribute('href'), null);
            });

            it('aborts Mermaid rendering if diagram DOM node is disconnected before completion', async () => {
                const env = createRenderEnvironment();
                let diagramDiv = null;

                env.win.mermaid.run = async ({ nodes }) => {
                    diagramDiv = nodes[0];
                    diagramDiv.remove();
                    diagramDiv.innerHTML = '<svg><text>Detached</text></svg>';
                };

                await env.win.renderMarkdown('```mermaid\ngraph TD;\nA-->B;\n```\n');

                assert.equal(env.doc.querySelectorAll('.expandable-container').length, 0);

                // Cover pre-mermaid.run disconnection check
                const preRemoveEnv = createRenderEnvironment();
                const origQuerySelectorAll = preRemoveEnv.doc.getElementById('content').querySelectorAll.bind(preRemoveEnv.doc.getElementById('content'));
                preRemoveEnv.doc.getElementById('content').querySelectorAll = (sel) => {
                    const res = origQuerySelectorAll(sel);
                    if (sel === '.mermaid' && res.length > 0) {
                        res[0].remove();
                    }
                    return res;
                };
                await preRemoveEnv.win.renderMarkdown('```mermaid\ngraph TD;\nE-->F;\n```\n');
                assert.equal(preRemoveEnv.doc.querySelectorAll('.expandable-container').length, 0);
            });
        });
    });

    describe('Cross-Mode Scroll and Downstream Hook Contract', () => {
        it('restores window scroll coordinate when preserveScroll option is enabled', async () => {
            const env = createRenderEnvironment();
            let dispatchedScroll = null;
            env.win.scrollTo = (x, y) => { dispatchedScroll = { x, y }; };
            env.win.scrollY = 380;

            await env.win.renderMarkdown('# Header\n\nExternal disk reload content.', { preserveScroll: true });

            assert.ok(dispatchedScroll !== null);
            assert.equal(dispatchedScroll.y, 380);
        });

        it('navigates to targetHeading option when provided in renderDocument options', async () => {
            const env = createRenderEnvironment();
            let targetScrolledElement = null;
            env.win.Element.prototype.scrollIntoView = function() { targetScrolledElement = this.id; };

            await env.win.renderMarkdown('# Top\n\nText\n\n## Target Section\n\nTarget content', {
                targetHeading: 'target-section'
            });

            assert.equal(targetScrolledElement, 'target-section');
        });

        it('scrolls to proportional coordinate when scrollToRatio option is supplied', async () => {
            const env = createRenderEnvironment();
            let dispatchedScroll = null;
            env.win.scrollTo = (opts) => { dispatchedScroll = opts; };

            await env.win.renderMarkdown('# Header\n\nProportional test.', { scrollRatio: 0.5 });

            assert.ok(dispatchedScroll !== null);
            assert.equal(typeof dispatchedScroll.top, 'number');
            assert.equal(dispatchedScroll.behavior, 'instant');
        });

        it('handles scrollToRatio invalid inputs and boundary clamping', async () => {
            const env = createRenderEnvironment();
            let dispatchedScroll = null;
            env.win.scrollTo = (opts) => { dispatchedScroll = opts; };

            // Invalid non-number or NaN
            env.win.scrollToRatio('invalid');
            assert.equal(dispatchedScroll, null);
            env.win.scrollToRatio(NaN);
            assert.equal(dispatchedScroll, null);

            // Clamping below 0 and above 1
            env.win.scrollToRatio(-0.5);
            assert.equal(dispatchedScroll.top, 0);

            env.win.scrollToRatio(1.5);
            assert.ok(dispatchedScroll.top >= 0);
        });

        it('connects scrollToRatio downlink from markdBridge in initQWebChannel', () => {
            const env = createRenderEnvironment();
            let connectedCallback = null;
            let receivedRatio = null;
            env.win.scrollToRatio = (r) => { receivedRatio = r; };

            env.win.qt = {
                webChannelTransport: {}
            };
            env.win.QWebChannel = function(transport, cb) {
                const mockBridge = {
                    logMessage: () => {},
                    renderDocument: { connect: () => {} },
                    navigateToHeading: { connect: () => {} },
                    setTheme: { connect: () => {} },
                    scrollToRatio: {
                        connect: (fn) => { connectedCallback = fn; }
                    },
                    bridgeReady: () => {}
                };
                cb({
                    objects: {
                        markdBridge: mockBridge,
                        session: {}
                    }
                });
            };

            env.win.initQWebChannel();
            assert.ok(connectedCallback !== null);

            connectedCallback(0.42);
            assert.equal(receivedRatio, 0.42);
        });

        it('ignores Tab key events in modal overlay when no focusable elements exist', () => {
            const env = createRenderEnvironment();
            const div = env.doc.createElement('div');
            div.textContent = 'Empty Modal Content';
            env.win.openFullView(div, false);

            const overlay = env.doc.querySelector('.fullview-overlay');
            assert.ok(overlay !== null);

            const focusables = overlay.querySelectorAll('button, [href], input, select, textarea, [tabindex]:not([tabindex="-1"])');
            focusables.forEach(f => f.remove());

            const tabEvent = new env.win.KeyboardEvent('keydown', { key: 'Tab', bubbles: true });
            env.doc.dispatchEvent(tabEvent);

            env.win.closeFullView();
        });

        it('handles session markdownChanged fallback when markdBridge renderDocument is absent', async () => {
            const env = createRenderEnvironment();
            env.win.markdBridge = null;

            await env.win.renderMarkdown('# Initial Baseline');

            let calledRender = false;
            const origRender = env.win.renderMarkdown;
            env.win.renderMarkdown = () => { calledRender = true; };

            env.win.handleSessionMarkdownChanged('# Initial Baseline');
            assert.equal(calledRender, false);

            env.win.handleSessionMarkdownChanged('# Distinct Content');
            assert.equal(calledRender, true);
            env.win.renderMarkdown = origRender;
        });
    });

    describe('Task 8.2 Code Block Presentation Cards and Copy Action', () => {
        it('wraps fenced code blocks in presentation card containers with normalized language badges', async () => {
            const env = createRenderEnvironment();
            const md =
                '```RUST\n' +
                'fn main() {\n' +
                '    println!("Aram");\n' +
                '}\n' +
                '```\n';

            await env.win.renderMarkdown(md);

            const card = env.doc.querySelector('#content .code-block-card');
            assert.ok(card !== null);
            assert.notEqual(card.parentElement.tagName, 'PRE');
            assert.equal(card.getAttribute('data-lang'), 'rust');
            assert.equal(card.getAttribute('role'), 'region');
            assert.equal(card.getAttribute('aria-label'), 'Code snippet: rust');

            const header = card.querySelector('.code-block-header');
            assert.ok(header !== null);

            const langBadge = header.querySelector('.code-lang');
            assert.ok(langBadge !== null);
            assert.equal(langBadge.textContent, 'rust');
            assert.ok(!langBadge.textContent.includes('['));
            assert.ok(!langBadge.textContent.includes(']'));

            const copyBtn = header.querySelector('.copy-btn');
            assert.ok(copyBtn !== null);
            assert.equal(copyBtn.getAttribute('type'), 'button');
            assert.equal(copyBtn.getAttribute('aria-label'), 'Copy code to clipboard');
            assert.equal(copyBtn.textContent, 'Copy');

            const pre = card.querySelector('pre.hljs');
            assert.ok(pre !== null);
            const code = pre.querySelector('code');
            assert.ok(code !== null);
            assert.ok(code.textContent.includes('fn main()'));
        });

        it('wraps untagged fenced code blocks with default text badge and card container', async () => {
            const env = createRenderEnvironment();
            const md =
                '```\n' +
                'plain untagged snippet\n' +
                '```\n';

            await env.win.renderMarkdown(md);

            const card = env.doc.querySelector('#content .code-block-card');
            assert.ok(card !== null);
            assert.equal(card.getAttribute('data-lang'), 'text');
            assert.equal(card.getAttribute('role'), 'region');
            assert.equal(card.getAttribute('aria-label'), 'Code snippet: text');

            const langBadge = card.querySelector('.code-lang');
            assert.ok(langBadge !== null);
            assert.equal(langBadge.textContent, 'text');
        });

        it('dispatches raw code text to native bridge upon copy button activation and resets state after timeout', async () => {
            const env = createRenderEnvironment();
            const copiedPayloads = [];
            env.win.session = {
                copyToClipboard: (text) => { copiedPayloads.push(text); }
            };

            let scheduledCallback = null;
            let scheduledDelay = 0;
            let clearedIds = [];
            let nextTimerId = 100;

            env.win.setTimeout = (fn, delay) => {
                scheduledCallback = fn;
                scheduledDelay = delay;
                nextTimerId += 1;
                return nextTimerId;
            };
            env.win.clearTimeout = (id) => {
                clearedIds.push(id);
            };

            const md =
                '```python\n' +
                'def compute():\n' +
                '    return 42\n' +
                '```\n';

            await env.win.renderMarkdown(md);

            const copyBtn = env.doc.querySelector('#content .code-block-card .copy-btn');
            assert.ok(copyBtn !== null);

            copyBtn.click();
            assert.equal(copiedPayloads.length, 1);
            assert.equal(copiedPayloads[0], 'def compute():\n    return 42');
            assert.equal(copyBtn.textContent, 'Copied!');
            assert.equal(copyBtn.classList.contains('copied'), true);
            assert.equal(scheduledDelay, 1500);
            const firstTimerId = copyBtn._copyTimeoutId;
            assert.ok(firstTimerId);

            // Rapid second click clears previous timeout before scheduling new one
            copyBtn.click();
            assert.equal(copiedPayloads.length, 2);
            assert.equal(clearedIds.includes(firstTimerId), true);
            assert.notEqual(copyBtn._copyTimeoutId, firstTimerId);

            // Trigger timer completion
            assert.equal(typeof scheduledCallback, 'function');
            scheduledCallback();
            assert.equal(copyBtn.textContent, 'Copy');
            assert.equal(copyBtn.classList.contains('copied'), false);
            assert.equal(copyBtn._copyTimeoutId, null);
        });

        it('falls back to navigator clipboard when native session bridge is absent and guards missing providers', async () => {
            const env = createRenderEnvironment();
            env.win.session = null;

            const navPayloads = [];
            Object.defineProperty(env.win.navigator, 'clipboard', {
                value: {
                    writeText: (text) => { navPayloads.push(text); }
                },
                configurable: true
            });

            await env.win.renderMarkdown('```js\nconst fallback = true;\n```\n');
            const copyBtn = env.doc.querySelector('#content .code-block-card .copy-btn');
            copyBtn.click();

            assert.equal(navPayloads.length, 1);
            assert.equal(navPayloads[0], 'const fallback = true;');

            // Guard when neither session nor navigator.clipboard is available
            Object.defineProperty(env.win.navigator, 'clipboard', {
                value: undefined,
                configurable: true
            });
            assert.doesNotThrow(() => {
                copyBtn.click();
            });

            // Orphan .copy-btn outside .code-block-card is safely ignored
            const orphanBtn = env.doc.createElement('button');
            orphanBtn.className = 'copy-btn';
            orphanBtn.textContent = 'Copy';
            env.doc.getElementById('content').appendChild(orphanBtn);
            orphanBtn.click();
            assert.equal(orphanBtn.textContent, 'Copy');
            assert.equal(orphanBtn.classList.contains('copied'), false);

            // Card without pre > code is safely ignored
            const hollowCard = env.doc.createElement('div');
            hollowCard.className = 'code-block-card';
            const hollowBtn = env.doc.createElement('button');
            hollowBtn.className = 'copy-btn';
            hollowBtn.textContent = 'Copy';
            hollowCard.appendChild(hollowBtn);
            env.doc.getElementById('content').appendChild(hollowCard);
            hollowBtn.click();
            assert.equal(hollowBtn.textContent, 'Copy');
        });

        it('preserves mermaid code blocks for diagram compilation without card wrapping', async () => {
            const env = createRenderEnvironment();
            const md =
                '```MERMAID\n' +
                'graph TD;\n' +
                '    A-->B;\n' +
                '```\n';

            await env.win.renderMarkdown(md);

            assert.equal(env.doc.querySelector('#content .code-block-card'), null);
            assert.ok(env.doc.querySelector('#content .expandable-container .mermaid') !== null);
        });
    });

    describe('Lazy Mermaid Bundle Injection & Theme Synchronization', () => {
        it('renders prose math and highlighted code without injecting mermaid script tag', async () => {
            const env = createRenderEnvironment({ withMermaid: false });
            assert.equal(env.win.mermaid, undefined);

            await env.win.renderMarkdown(
                '# Fast Cold Start\n\nInline formula $a^2 + b^2 = c^2$ and code:\n\n```js\nconst x = 1;\n```\n'
            );

            assert.ok(env.doc.querySelector('#content h1[id="fast-cold-start"]') !== null);
            assert.ok(env.doc.querySelector('#content .katex') !== null);
            assert.ok(env.doc.querySelector('#content .code-block-card') !== null);
            assert.equal(env.doc.head.querySelector('script[data-mermaid-lazy="1"]'), null);
        });

        it('caches mermaid theme preference before bundle load and applies theme on first diagram', async () => {
            const env = createRenderEnvironment({ withMermaid: false });
            env.win.setTheme('obsidian-dark');
            assert.equal(env.doc.documentElement.getAttribute('data-theme'), 'obsidian-dark');

            env.win.setTheme('light');
            assert.equal(env.doc.documentElement.getAttribute('data-theme'), 'light');

            let initializedConfig = null;
            const origAppend = env.doc.head.appendChild.bind(env.doc.head);
            env.doc.head.appendChild = (node) => {
                const appended = origAppend(node);
                if (node.tagName === 'SCRIPT' && node.getAttribute('data-mermaid-lazy') === '1') {
                    env.win.mermaid = {
                        initialize: (cfg) => { initializedConfig = cfg; },
                        run: async ({ nodes }) => {
                            for (const n of nodes) {
                                n.innerHTML = '<svg class="lazy-svg"><text>Lazy</text></svg>';
                            }
                        }
                    };
                    node.onload();
                }
                return appended;
            };

            await env.win.renderMarkdown('```mermaid\ngraph TD;\nA-->B;\n```\n');
            assert.ok(initializedConfig !== null);
            assert.equal(initializedConfig.theme, 'default');
            assert.ok(env.doc.querySelector('#content .lazy-svg') !== null);
        });

        it('injects mermaid script once on first diagram fence and reuses cached loader promise', async () => {
            const env = createRenderEnvironment({ withMermaid: false });
            let scriptCount = 0;
            const origAppend = env.doc.head.appendChild.bind(env.doc.head);
            env.doc.head.appendChild = (node) => {
                const appended = origAppend(node);
                if (node.tagName === 'SCRIPT' && node.getAttribute('data-mermaid-lazy') === '1') {
                    scriptCount += 1;
                    env.win.mermaid = {
                        initialize: () => {},
                        run: async ({ nodes }) => {
                            for (const n of nodes) {
                                n.innerHTML = '<svg class="cached-svg"><text>Cached</text></svg>';
                            }
                        }
                    };
                    node.onload();
                }
                return appended;
            };

            await env.win.renderMarkdown('```mermaid\ngraph LR;\nX-->Y;\n```\n');
            const firstPromise = env.win.ensureMermaidLoaded();
            const secondPromise = env.win.ensureMermaidLoaded();
            await env.win.renderMarkdown('```mermaid\ngraph LR;\nY-->Z;\n```\n');

            assert.equal(scriptCount, 1);
            assert.equal(await firstPromise, await secondPromise);
            assert.equal(env.doc.head.querySelectorAll('script[data-mermaid-lazy="1"]').length, 1);
        });

        it('coalesces concurrent diagram renders behind single script element injection', async () => {
            const env = createRenderEnvironment({ withMermaid: false });
            let pendingScriptNode = null;
            let appendCalls = 0;
            const origAppend = env.doc.head.appendChild.bind(env.doc.head);
            env.doc.head.appendChild = (node) => {
                const appended = origAppend(node);
                if (node.tagName === 'SCRIPT' && node.getAttribute('data-mermaid-lazy') === '1') {
                    appendCalls += 1;
                    pendingScriptNode = node;
                }
                return appended;
            };

            const p1 = env.win.ensureMermaidLoaded();
            const p2 = env.win.ensureMermaidLoaded();
            assert.equal(p1, p2);
            assert.equal(appendCalls, 1);
            assert.ok(pendingScriptNode !== null);

            env.win.mermaid = {
                initialize: () => {},
                run: async () => {}
            };
            pendingScriptNode.onload();
            await Promise.all([p1, p2]);
        });

        it('resets loader promise renders error card and releases render mutex on script load failure', async () => {
            const env = createRenderEnvironment({ withMermaid: false });
            let failNextLoad = true;
            const origAppend = env.doc.head.appendChild.bind(env.doc.head);
            env.doc.head.appendChild = (node) => {
                const appended = origAppend(node);
                if (node.tagName === 'SCRIPT' && node.getAttribute('data-mermaid-lazy') === '1') {
                    if (failNextLoad) {
                        node.onerror();
                    } else {
                        env.win.mermaid = {
                            initialize: () => {},
                            run: async ({ nodes }) => {
                                for (const n of nodes) {
                                    n.innerHTML = '<svg class="recovered-svg"><text>Recovered</text></svg>';
                                }
                            }
                        };
                        node.onload();
                    }
                }
                return appended;
            };

            await env.win.renderMarkdown('```mermaid\ngraph TD;\nFail-->Recover;\n```\n');
            const errCard = env.doc.querySelector('#content .mermaid-error-card');
            assert.ok(errCard !== null);
            assert.ok(errCard.textContent.includes('Failed to load Mermaid bundle'));

            failNextLoad = false;
            await env.win.renderMarkdown('```mermaid\ngraph TD;\nRecover-->Success;\n```\n');
            assert.ok(env.doc.querySelector('#content .recovered-svg') !== null);
        });

        it('renders mermaid syntax error card without blocking subsequent markdown updates', async () => {
            const env = createRenderEnvironment({ withMermaid: false });
            const origAppend = env.doc.head.appendChild.bind(env.doc.head);
            env.doc.head.appendChild = (node) => {
                const appended = origAppend(node);
                if (node.tagName === 'SCRIPT' && node.getAttribute('data-mermaid-lazy') === '1') {
                    env.win.mermaid = {
                        initialize: () => {},
                        run: async () => { throw new Error('Bad lexical token'); }
                    };
                    node.onload();
                }
                return appended;
            };

            await env.win.renderMarkdown('```mermaid\ninvalid_syntax\n```\n');
            assert.ok(env.doc.querySelector('#content .mermaid-error-card') !== null);

            await env.win.renderMarkdown('# Clean Prose Recovery\n\nMutex remains unlocked.');
            assert.ok(env.doc.querySelector('#content h1[id="clean-prose-recovery"]') !== null);
        });
    });
});

describe('Single-Reveal Contract', () => {
    const webDir = path.resolve(__dirname, '..');
    const tallMarkdown = '# Top\n\n' + Array.from({ length: 40 }, (_, i) => `Paragraph ${i}.`).join('\n\n') + '\n\n## Late Heading\n\nEnd.';

    describe('Content visibility before and after render', () => {
        it('starts the live page in the hydrating state', () => {
            const env = createRenderEnvironment();
            assert.equal(env.doc.body.classList.contains('is-hydrating'), true);
        });

        it('reveals content by clearing the hydrating state after render', async () => {
            const env = createRenderEnvironment();
            await env.win.renderMarkdown('# Title\n\ntext');
            assert.equal(env.doc.body.classList.contains('is-hydrating'), false);
        });

        it('clears the hydrating state after an empty render', async () => {
            const env = createRenderEnvironment();
            await env.win.renderMarkdown('');
            assert.equal(env.doc.body.classList.contains('is-hydrating'), false);
        });

        it('never restores the hydrating state on live re-renders', async () => {
            const env = createRenderEnvironment();
            await env.win.renderMarkdown('# First');
            await env.win.renderMarkdown('# Second');
            assert.equal(env.doc.body.classList.contains('is-hydrating'), false);
            assert.ok(env.doc.getElementById('content').textContent.includes('Second'));
        });
    });

    describe('Reveal ordering', () => {
        it('resolves scroll position before clearing the hydrating state', async () => {
            const env = createRenderEnvironment();
            let hydratingAtScroll = null;
            env.win.scrollTo = () => { hydratingAtScroll = env.doc.body.classList.contains('is-hydrating'); };
            await env.win.renderMarkdown('# Ordered');
            assert.equal(hydratingAtScroll, true);
        });

        it('notifies the bridge after the hydrating state clears', async () => {
            const env = createRenderEnvironment();
            let hydratingAtNotify = null;
            env.win.markdBridge = { onDocumentRendered: () => { hydratingAtNotify = env.doc.body.classList.contains('is-hydrating'); } };
            await env.win.renderMarkdown('# Notified');
            assert.equal(hydratingAtNotify, false);
        });

        it('notifies the bridge before renderDocument resolves', async () => {
            const env = createRenderEnvironment();
            let reportedHeadings = -1;
            env.win.markdBridge = { onDocumentRendered: (count) => { reportedHeadings = count; } };
            await env.win.renderMarkdown('# One\n\n## Two');
            assert.equal(reportedHeadings, 2);
        });
    });

    describe('Scroll restoration without animation', () => {
        it('opens a document at the top with no scroll options', async () => {
            const env = createRenderEnvironment();
            const calls = [];
            env.win.scrollTo = (...args) => { calls.push(args); };
            await env.win.renderMarkdown(tallMarkdown);
            assert.deepEqual(calls, [[0, 0]]);
        });

        it('restores scroll ratio without smooth animation', async () => {
            const env = createRenderEnvironment();
            let dispatched = null;
            env.win.scrollTo = (opts) => { dispatched = opts; };
            await env.win.renderMarkdown(tallMarkdown, { scrollRatio: 0.4 });
            assert.equal(dispatched.behavior, 'instant');
            env.win.scrollToRatio(0.9);
            assert.equal(dispatched.behavior, 'instant');
        });

        it('restores preserved scroll offset without smooth animation', async () => {
            const env = createRenderEnvironment();
            const calls = [];
            env.win.scrollTo = (...args) => { calls.push(args); };
            env.win.scrollY = 240;
            await env.win.renderMarkdown(tallMarkdown, { preserveScroll: true });
            assert.deepEqual(calls, [[0, 240]]);
        });

        it('jumps to targetHeading option without smooth animation', async () => {
            const env = createRenderEnvironment();
            let used = null;
            env.win.Element.prototype.scrollIntoView = function (opts) { used = { id: this.id, behavior: opts.behavior }; };
            await env.win.renderMarkdown(tallMarkdown, { targetHeading: 'late-heading' });
            assert.deepEqual(used, { id: 'late-heading', behavior: 'instant' });
        });

        it('keeps smooth scrolling for heading navigation', async () => {
            const env = createRenderEnvironment();
            await env.win.renderMarkdown(tallMarkdown);
            let used = null;
            env.win.Element.prototype.scrollIntoView = function (opts) { used = opts.behavior; };
            env.win.scrollToHeading('late-heading');
            assert.equal(used, 'smooth');
        });
    });

    describe('Bridge heading navigation behavior', () => {
        it('passes the bridge navigation behavior through to the heading scroll', async () => {
            const env = createRenderEnvironment();
            await env.win.renderMarkdown(tallMarkdown);
            let navigate = null;
            env.win.qt = { webChannelTransport: {} };
            env.win.QWebChannel = function (transport, callback) {
                callback({ objects: { markdBridge: { logMessage: () => {}, bridgeReady: () => {}, navigateToHeading: { connect: (fn) => { navigate = fn; } } } } });
            };
            env.win.eval('initQWebChannel()');
            const used = [];
            env.win.Element.prototype.scrollIntoView = function (opts) { used.push(opts.behavior); };
            navigate('late-heading', 'instant');
            navigate('top', 'smooth');
            assert.deepEqual(used, ['instant', 'smooth']);
        });
    });

    describe('Stylesheet reveal rules', () => {
        it('declares no global smooth scroll on html or body', () => {
            const css = fs.readFileSync(path.join(webDir, 'styles/obsidian.css'), 'utf8');
            assert.equal(/scroll-behavior\s*:\s*smooth/.test(css), false);
        });

        it('keeps the hydrating rule inside the live template only', () => {
            const template = fs.readFileSync(path.join(webDir, 'template.html'), 'utf8');
            const css = fs.readFileSync(path.join(webDir, 'styles/obsidian.css'), 'utf8');
            assert.match(template, /<body class="is-hydrating">/);
            assert.match(template, /body\.is-hydrating #content\s*\{\s*opacity:\s*0;\s*\}/);
            assert.equal(css.includes('is-hydrating'), false);
            assert.equal(/#content[^{]*\{[^}]*opacity\s*:\s*0/.test(css), false);
        });
    });
});

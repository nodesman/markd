// Async render downlink mutex, document pipeline orchestration, and public window.Markd API

let renderDownlinkMutex: Promise<void> | null = null;
let currentRenderedMarkdown: string | null = null;

function releaseRenderDownlinkMutex(completedRun: Promise<void>): void {
    if (renderDownlinkMutex === completedRun) {
        renderDownlinkMutex = null;
    }
}

function renderMarkdown(markdownText: unknown, options?: RenderOptions): Promise<void> {
    const scheduledTask = function (): Promise<void> {
        return executeRenderMarkdown(markdownText, options);
    };
    const nextRun = renderDownlinkMutex
        ? renderDownlinkMutex.then(scheduledTask, scheduledTask)
        : scheduledTask();
    const trackedRun = nextRun.finally(function (): void {
        releaseRenderDownlinkMutex(trackedRun);
    });
    renderDownlinkMutex = trackedRun;
    return trackedRun;
}

function sanitizeDocumentHtml(html: string): string {
    return DOMPurify!.sanitize(html, {
        ADD_TAGS: ['semantics', 'annotation'],
        ADD_ATTR: ['target', 'encoding', 'rel', 'role', 'aria-label', 'data-lang', 'type'],
        FORBID_TAGS: ['style'],
        ALLOW_DATA_ATTR: true,
        SANITIZE_NAMED_PROPS: false,
        RETURN_TRUSTED_TYPE: false
    });
}

function notifyBridgeDocumentRendered(contentEl: HTMLElement): void {
    const headings = contentEl.querySelectorAll('h1[id], h2[id], h3[id], h4[id], h5[id], h6[id]');
    if (window.markdBridge && typeof window.markdBridge.onDocumentRendered === 'function') {
        window.markdBridge.onDocumentRendered(headings.length);
    }
}

async function executeRenderMarkdown(markdownText: unknown, options?: RenderOptions): Promise<void> {
    if (typeof markdownText !== 'string') {
        return;
    }
    if (markdownText === currentRenderedMarkdown && (!options || !options.force)) {
        return;
    }
    currentRenderedMarkdown = markdownText;

    closeFullView(true);
    cleanupDiagramListeners();

    const preserveBlockSizes = !!(options && (options.preserveScroll === true || options.force === true));
    if (!preserveBlockSizes) {
        blockSizeMemory = Object.create(null);
    }

    const contentEl = document.getElementById('content')!;
    const prevScrollY = window.scrollY || 0;
    const existingFrontmatter = contentEl.querySelector<HTMLDetailsElement>('.markd-frontmatter');
    const preserveFrontmatterOpen = !!(existingFrontmatter && existingFrontmatter.open);

    const fm = extractFrontmatter(markdownText);
    const frontmatterHtml = fm.found ? renderFrontmatterHtml(fm, preserveFrontmatterOpen) : '';
    const html = frontmatterHtml + md.render(fm.body);

    contentEl.innerHTML = sanitizeDocumentHtml(html);

    transformMermaidCodeBlocks(contentEl);

    const mermaidDivs = contentEl.querySelectorAll<HTMLElement>('.mermaid');
    if (mermaidDivs.length > 0) {
        await renderMermaidNodes(mermaidDivs);
    }

    wrapMermaidDiagrams();
    wrapTables();
    attachCodeBlockResizeHandles();
    setupScrollSpy();
    resolvePostRenderScroll(options, prevScrollY);

    document.body.classList.remove('is-hydrating');
    notifyBridgeDocumentRendered(contentEl);
}

window.Markd = {
    version: '1.0.0',
    isReady: false,
    render: renderMarkdown,
    scrollToHeading: scrollToHeading,
    scrollToRatio: scrollToRatio,
    modal: {
        open: openFullView,
        close: closeFullView
    },
    get bridge(): MarkdBridge | null {
        return window.markdBridge;
    },
    get session(): MarkdSession | null {
        return window.session;
    }
};

window.renderMarkdown = renderMarkdown;
window.scrollToHeading = scrollToHeading;
window.scrollToRatio = scrollToRatio;
window.openFullView = openFullView;
window.closeFullView = closeFullView;
window.setupScrollSpy = setupScrollSpy;
window.wrapMermaidDiagrams = wrapMermaidDiagrams;
window.wrapTables = wrapTables;
window.attachCodeBlockResizeHandles = attachCodeBlockResizeHandles;
window.handleSessionMarkdownChanged = handleSessionMarkdownChanged;

function markEngineReady(): void {
    window.Markd.isReady = true;
}

function bootstrapMarkdReady(): void {
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', markEngineReady);
        return;
    }
    markEngineReady();
}

bootstrapQWebChannel();
bootstrapMarkdReady();

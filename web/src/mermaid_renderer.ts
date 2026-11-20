// Mermaid code-block transformation, isolated diagram rendering, SVG sanitization, and error cards

function transformMermaidCodeBlocks(contentEl: HTMLElement): void {
    const mermaidBlocks = contentEl.querySelectorAll<HTMLElement>('pre > code.language-mermaid');
    for (let i = 0; i < mermaidBlocks.length; i++) {
        const pre = mermaidBlocks[i].parentElement!;
        const div = document.createElement('div');
        div.className = 'mermaid';
        const codeText = mermaidBlocks[i].textContent!.replace(/\n$/, '');
        div.dataset.rawCode = codeText;
        div.textContent = codeText;
        pre.replaceWith(div);
    }
}

function replaceWithMermaidErrorCard(mDiv: HTMLElement, rawDiagramCode: string, err: unknown): void {
    console.error('Mermaid render error:', err);
    const errorCard = document.createElement('div');
    errorCard.className = 'mermaid-error-card';
    const badge = document.createElement('div');
    badge.className = 'mermaid-error-badge';
    badge.textContent = 'Mermaid Syntax Error: ' + (err && (err as Error).message ? (err as Error).message : 'Invalid diagram syntax');
    const rawPre = document.createElement('pre');
    rawPre.className = 'mermaid-raw-code hljs';
    const rawCodeNode = document.createElement('code');
    rawCodeNode.textContent = rawDiagramCode;
    rawPre.appendChild(rawCodeNode);
    errorCard.appendChild(badge);
    errorCard.appendChild(rawPre);
    mDiv.replaceWith(errorCard);
}

function scrubMermaidStyleElement(styleNode: HTMLStyleElement): void {
    const safeCss = styleNode.textContent!
        .replace(/position\s*:\s*(?:fixed|absolute)/gi, 'position: static')
        .replace(/@import\b[^;]*;?/gi, '');
    styleNode.textContent = safeCss;
}

function sanitizeCompiledMermaidSvg(mDiv: HTMLElement, svgEl: SVGSVGElement): void {
    mDiv.innerHTML = DOMPurify!.sanitize(svgEl.outerHTML, {
        USE_PROFILES: { html: true, svg: true, svgFilters: true },
        ADD_TAGS: ['foreignObject'],
        ADD_ATTR: ['dominant-baseline', 'xmlns'],
        HTML_INTEGRATION_POINTS: { foreignobject: true }
    });
    const styles = mDiv.querySelectorAll('style');
    for (let i = 0; i < styles.length; i++) {
        scrubMermaidStyleElement(styles[i]);
    }
    mDiv.setAttribute('data-processed', 'true');
}

async function renderSingleMermaidNode(mermaidApi: MermaidApi, mDiv: HTMLElement): Promise<void> {
    const rawDiagramCode = mDiv.dataset.rawCode!;
    try {
        await mermaidApi.run({ nodes: [mDiv] });
        if (!mDiv.isConnected) return;
        const svgEl = mDiv.querySelector('svg');
        if (svgEl) sanitizeCompiledMermaidSvg(mDiv, svgEl);
    } catch (e) {
        replaceWithMermaidErrorCard(mDiv, rawDiagramCode, e);
    }
}

function replaceAllMermaidWithErrors(mermaidDivs: NodeListOf<HTMLElement>, err: unknown): void {
    for (let k = 0; k < mermaidDivs.length; k++) {
        replaceWithMermaidErrorCard(mermaidDivs[k], mermaidDivs[k].dataset.rawCode!, err);
    }
}

async function resolveMermaidApi(mermaidDivs: NodeListOf<HTMLElement>): Promise<MermaidApi | null> {
    try {
        return await ensureMermaidLoaded();
    } catch (loadErr) {
        replaceAllMermaidWithErrors(mermaidDivs, loadErr);
        return null;
    }
}

async function renderMermaidNodes(mermaidDivs: NodeListOf<HTMLElement>): Promise<void> {
    const mermaidApi = typeof window.mermaid !== 'undefined'
        ? window.mermaid
        : await resolveMermaidApi(mermaidDivs);
    if (!mermaidApi) return;
    for (let j = 0; j < mermaidDivs.length; j++) {
        const mDiv = mermaidDivs[j];
        if (!mDiv.isConnected) continue;
        await renderSingleMermaidNode(mermaidApi, mDiv);
    }
}

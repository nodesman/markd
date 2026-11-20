// Expandable diagram/table containers and inline diagram zoom/pan controls

const DIAGRAM_TOOLBAR_HTML = [
    '<button class="zoom-out-btn" title="Zoom out">−</button>',
    '<span class="zoom-label">100%</span>',
    '<button class="zoom-in-btn" title="Zoom in">+</button>',
    '<button class="zoom-reset-btn" title="Reset zoom &amp; size">↺</button>',
    '<button class="fullview-btn" title="Full view">⛶</button>'
].join('');

function wrapSingleMermaidDiagram(diagram: HTMLElement, index: number): void {
    if (diagram.closest('.expandable-container')) return;

    const container = document.createElement('div');
    container.className = 'expandable-container';

    const toolbar = document.createElement('div');
    toolbar.className = 'expandable-toolbar';
    toolbar.innerHTML = DIAGRAM_TOOLBAR_HTML;

    const viewport = document.createElement('div');
    viewport.className = 'mermaid-viewport';
    const inner = document.createElement('div');
    inner.className = 'mermaid-inner';

    diagram.parentNode!.insertBefore(container, diagram);
    inner.appendChild(diagram);
    viewport.appendChild(inner);
    container.appendChild(toolbar);
    container.appendChild(viewport);

    const blockKey = 'mermaid:' + index;
    setupDiagramZoomPan(container, viewport, inner, blockKey);
    attachBlockResizeHandles(container, viewport, blockKey);
}

function wrapMermaidDiagrams(): void {
    const diagrams = document.querySelectorAll<HTMLElement>('#content .mermaid');
    diagrams.forEach(wrapSingleMermaidDiagram);
}

function wrapSingleTable(table: HTMLTableElement, index: number): void {
    if (table.parentElement!.classList.contains('table-wrapper')) return;

    const wrapper = document.createElement('div');
    wrapper.className = 'table-wrapper';

    const btn = document.createElement('button');
    btn.className = 'table-expand-btn';
    btn.title = 'Full view';
    btn.textContent = '⛶';

    table.parentNode!.insertBefore(wrapper, table);
    wrapper.appendChild(btn);
    wrapper.appendChild(table);

    btn.addEventListener('click', function (): void {
        openFullView(table.cloneNode(true) as HTMLElement, false);
    });

    attachBlockResizeHandles(wrapper, null, 'table:' + index);
}

function wrapTables(): void {
    const tables = document.querySelectorAll<HTMLTableElement>('#content > table');
    tables.forEach(wrapSingleTable);
}

function setupDiagramZoomPan(container: HTMLElement, viewport: HTMLElement, inner: HTMLElement, blockKey: string): void {
    let scale = 1;
    let panX = 0;
    let panY = 0;
    let isPanning = false;
    let startX = 0;
    let startY = 0;
    let startPanX = 0;
    let startPanY = 0;
    const label = container.querySelector<HTMLElement>('.zoom-label')!;

    function applyTransform(smooth: boolean): void {
        if (Math.abs(scale - 1) < 0.02) scale = 1;
        inner.style.transition = smooth ? 'transform 0.15s ease-out' : 'none';
        inner.style.transform = 'translate(' + panX + 'px,' + panY + 'px) scale(' + scale + ')';
        label.textContent = Math.round(scale * 100) + '%';
    }

    function clampPan(x: number, y: number): PanCoordinates {
        const limitX = Math.max(500, (viewport.clientWidth || 600) * scale);
        const limitY = Math.max(500, (viewport.clientHeight || 400) * scale);
        return { x: Math.max(-limitX, Math.min(limitX, x)), y: Math.max(-limitY, Math.min(limitY, y)) };
    }

    container.querySelector<HTMLButtonElement>('.zoom-in-btn')!.addEventListener('click', function (): void {
        scale = Math.min(scale * 1.25, 5);
        applyTransform(true);
    });

    container.querySelector<HTMLButtonElement>('.zoom-out-btn')!.addEventListener('click', function (): void {
        scale = Math.max(scale / 1.25, 0.2);
        applyTransform(true);
    });

    container.querySelector<HTMLButtonElement>('.zoom-reset-btn')!.addEventListener('click', function (): void {
        scale = 1;
        panX = 0;
        panY = 0;
        applyTransform(true);
        resetBlockDimensions(container, viewport, blockKey);
    });

    container.querySelector<HTMLButtonElement>('.fullview-btn')!.addEventListener('click', function (): void {
        openFullView(inner.cloneNode(true) as HTMLElement, true);
    });

    viewport.addEventListener('wheel', function (e: WheelEvent): void {
        e.preventDefault();
        if (Math.abs(e.deltaY) < 1e-3) return;
        const factor = e.deltaY < 0 ? 1.1 : 0.9;
        scale = Math.max(0.2, Math.min(5, scale * factor));
        applyTransform(false);
    }, { passive: false });

    viewport.addEventListener('mousedown', function (e: MouseEvent): void {
        isPanning = true;
        startX = e.clientX;
        startY = e.clientY;
        startPanX = panX;
        startPanY = panY;
        e.preventDefault();
    });

    const onMouseMove = function (e: MouseEvent): void {
        if (!isPanning) return;
        const clamped = clampPan(startPanX + (e.clientX - startX), startPanY + (e.clientY - startY));
        panX = clamped.x;
        panY = clamped.y;
        applyTransform(false);
    };

    const onMouseUp = function (): void {
        isPanning = false;
    };

    const onPointerExit = function (): void {
        isPanning = false;
    };

    document.addEventListener('mousemove', onMouseMove);
    document.addEventListener('mouseup', onMouseUp);
    document.addEventListener('mouseleave', onPointerExit);
    window.addEventListener('blur', onPointerExit);

    diagramTeardowns.push(function (): void {
        document.removeEventListener('mousemove', onMouseMove);
        document.removeEventListener('mouseup', onMouseUp);
        document.removeEventListener('mouseleave', onPointerExit);
        window.removeEventListener('blur', onPointerExit);
    });

    applyTransform(false);
}

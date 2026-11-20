// Inline Block Resize (Center-Anchored Breakout & Vertical Aperture)

let blockSizeMemory: Record<string, BlockSizeEntry> = Object.create(null);
let activeBlockResizeCleanup: (() => void) | null = null;
const BLOCK_RESIZE_CURSORS: Record<ResizeAxis, string> = {
    n: 'ns-resize',
    w: 'ew-resize',
    e: 'ew-resize',
    s: 'ns-resize',
    nw: 'nwse-resize',
    ne: 'nesw-resize',
    sw: 'nesw-resize',
    se: 'nwse-resize'
};

function stopActiveBlockResize(): void {
    if (!activeBlockResizeCleanup) return;
    const cleanup = activeBlockResizeCleanup;
    activeBlockResizeCleanup = null;
    cleanup();
}

function clampBlockWidth(rawWidth: number): number {
    const viewportWidth = window.innerWidth || 1024;
    const maxWidth = Math.min(viewportWidth, Math.max(280, viewportWidth - 48));
    const minWidth = Math.min(420, maxWidth);
    return Math.round(Math.max(minWidth, Math.min(maxWidth, rawWidth)));
}

function clampBlockHeight(rawHeight: number): number {
    const viewportHeight = window.innerHeight || 800;
    const maxHeight = Math.max(220, Math.round(viewportHeight * 0.85));
    return Math.round(Math.max(220, Math.min(maxHeight, rawHeight)));
}

function measureBlockWidth(container: HTMLElement): number {
    const rectWidth = container.getBoundingClientRect().width;
    return rectWidth > 0 ? rectWidth : 664;
}

function measureBlockHeight(target: HTMLElement): number {
    const rectHeight = target.getBoundingClientRect().height;
    return rectHeight > 0 ? rectHeight : 320;
}

function applyBlockDimensions(
    container: HTMLElement,
    verticalTarget: HTMLElement | null,
    blockKey: string,
    width: number | null | undefined,
    height: number | null | undefined
): void {
    const entry = blockSizeMemory[blockKey] || {};
    if (typeof width === 'number') {
        const clampedW = clampBlockWidth(width);
        container.style.width = clampedW + 'px';
        container.classList.add('is-resized-x');
        entry.width = clampedW;
    }
    if (verticalTarget && typeof height === 'number') {
        const clampedH = clampBlockHeight(height);
        verticalTarget.style.height = clampedH + 'px';
        container.classList.add('is-resized-y');
        entry.height = clampedH;
    }
    blockSizeMemory[blockKey] = entry;
}

function resetBlockDimensions(container: HTMLElement, verticalTarget: HTMLElement | null, blockKey: string): void {
    container.style.width = '';
    container.classList.remove('is-resized-x');
    container.classList.remove('is-resized-y');
    if (verticalTarget) {
        verticalTarget.style.height = '';
    }
    delete blockSizeMemory[blockKey];
}

function computeNextWidth(axis: ResizeAxis, startWidth: number, dx: number): number | null {
    if (axis.indexOf('e') !== -1) return startWidth + (2 * dx);
    if (axis.indexOf('w') !== -1) return startWidth - (2 * dx);
    return null;
}

function computeNextHeight(axis: ResizeAxis, startHeight: number, dy: number): number | null {
    if (axis.indexOf('s') !== -1) return startHeight + dy;
    if (axis.indexOf('n') !== -1) return startHeight - dy;
    return null;
}

function startBlockResizeDrag(
    event: MouseEvent,
    container: HTMLElement,
    verticalTarget: HTMLElement | null,
    blockKey: string,
    axis: ResizeAxis
): void {
    if (event.button !== 0) return;
    event.preventDefault();
    event.stopPropagation();
    stopActiveBlockResize();

    const startX = event.clientX;
    const startY = event.clientY;
    const startWidth = measureBlockWidth(container);
    const startHeight = verticalTarget ? measureBlockHeight(verticalTarget) : 0;

    container.classList.add('is-resizing');
    document.body.classList.add('is-block-resizing');
    document.body.style.cursor = BLOCK_RESIZE_CURSORS[axis];

    const onMove = function (moveEvent: MouseEvent): void {
        const nextW = computeNextWidth(axis, startWidth, moveEvent.clientX - startX);
        const nextH = computeNextHeight(axis, startHeight, moveEvent.clientY - startY);
        applyBlockDimensions(container, verticalTarget, blockKey, nextW, nextH);
    };

    const finish = function (): void {
        document.removeEventListener('mousemove', onMove);
        document.removeEventListener('mouseup', finish);
        document.removeEventListener('mouseleave', finish);
        window.removeEventListener('blur', finish);
        container.classList.remove('is-resizing');
        document.body.classList.remove('is-block-resizing');
        document.body.style.cursor = '';
        activeBlockResizeCleanup = null;
    };

    activeBlockResizeCleanup = finish;
    document.addEventListener('mousemove', onMove);
    document.addEventListener('mouseup', finish);
    document.addEventListener('mouseleave', finish);
    window.addEventListener('blur', finish);
}

function createResizeHandle(
    container: HTMLElement,
    verticalTarget: HTMLElement | null,
    blockKey: string,
    axis: ResizeAxis
): HTMLDivElement {
    const handle = document.createElement('div');
    handle.className = 'block-resize-handle block-resize-handle-' + axis;
    handle.setAttribute('data-resize-axis', axis);
    handle.setAttribute('role', 'separator');
    handle.setAttribute('aria-label', 'Resize block (' + axis + ')');
    handle.title = 'Drag to resize · Double-click to reset';
    handle.addEventListener('mousedown', function (e: MouseEvent): void {
        startBlockResizeDrag(e, container, verticalTarget, blockKey, axis);
    });
    handle.addEventListener('dblclick', function (e: MouseEvent): void {
        e.preventDefault();
        e.stopPropagation();
        resetBlockDimensions(container, verticalTarget, blockKey);
    });
    return handle;
}

function attachBlockResizeHandles(container: HTMLElement, verticalTarget: HTMLElement | null, blockKey: string): void {
    if (container.querySelector('.block-resize-handle')) return;
    const axes: ResizeAxis[] = verticalTarget ? ['n', 'w', 'e', 's', 'nw', 'ne', 'sw', 'se'] : ['w', 'e'];
    axes.forEach(function (axis: ResizeAxis): void {
        container.appendChild(createResizeHandle(container, verticalTarget, blockKey, axis));
    });
    const saved = blockSizeMemory[blockKey];
    if (saved) {
        applyBlockDimensions(container, verticalTarget, blockKey, saved.width, saved.height);
    }
}

function attachCodeBlockResizeHandles(): void {
    const cards = document.querySelectorAll<HTMLElement>('#content .code-block-card');
    cards.forEach(function (card: HTMLElement, index: number): void {
        attachBlockResizeHandles(card, null, 'code:' + index);
    });
}

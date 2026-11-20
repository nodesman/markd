// Singleton full-view modal overlay lifecycle, keyboard focus trap, and listener teardown

const FULLVIEW_DIAGRAM_TOOLBAR_HTML = [
    '<button class="zoom-out-btn" title="Zoom out">−</button>',
    '<span class="zoom-label" style="font-size:11px;color:var(--text-secondary);min-width:36px;text-align:center">100%</span>',
    '<button class="zoom-in-btn" title="Zoom in">+</button>',
    '<button class="zoom-reset-btn" title="Reset zoom">↺</button>',
    '<span style="flex:1"></span>',
    '<button class="close-btn">✕ Close</button>'
].join('');

const FULLVIEW_TABLE_TOOLBAR_HTML = '<span style="flex:1"></span><button class="close-btn">✕ Close</button>';

let activeFullViewClose: ((immediate?: boolean) => void) | null = null;

function closeFullView(immediate?: boolean): void {
    if (activeFullViewClose) {
        activeFullViewClose(immediate);
    }
}

function purgeExistingFullViewOverlays(): void {
    const existingOverlays = document.querySelectorAll('.fullview-overlay');
    for (let i = 0; i < existingOverlays.length; i++) {
        existingOverlays[i].remove();
    }
}

function buildFullViewToolbarHtml(isDiagram: boolean): string {
    if (isDiagram) return FULLVIEW_DIAGRAM_TOOLBAR_HTML;
    return FULLVIEW_TABLE_TOOLBAR_HTML;
}

function detachFullViewExitHandler(exitHandler: (() => void) | null): void {
    if (!exitHandler) return;
    document.removeEventListener('mouseleave', exitHandler);
    window.removeEventListener('blur', exitHandler);
}

function restoreFullViewFocus(previousFocusedElement: HTMLElement | null): void {
    if (previousFocusedElement && typeof previousFocusedElement.focus === 'function') {
        previousFocusedElement.focus();
    }
}

function removeFullViewOverlay(overlay: HTMLElement, immediate?: boolean): void {
    if (immediate) {
        overlay.remove();
        return;
    }
    setTimeout(function (): void { overlay.remove(); }, 200);
}

function wrapFocusToBoundary(e: KeyboardEvent, target: HTMLElement, shouldWrap: boolean): void {
    if (!shouldWrap) return;
    e.preventDefault();
    target.focus();
}

function trapFullViewTabFocus(overlay: HTMLElement, e: KeyboardEvent): void {
    const focusable = overlay.querySelectorAll<HTMLElement>('button, [href], input, select, textarea, [tabindex]:not([tabindex="-1"])');
    if (focusable.length === 0) return;
    const first = focusable[0];
    const last = focusable[focusable.length - 1];
    const outsideOverlay = !overlay.contains(document.activeElement);
    if (e.shiftKey) {
        wrapFocusToBoundary(e, last, document.activeElement === first || outsideOverlay);
        return;
    }
    wrapFocusToBoundary(e, first, document.activeElement === last || outsideOverlay);
}

function handleFullViewKeyDown(overlay: HTMLElement, onEscape: () => void, e: KeyboardEvent): void {
    if (e.key === 'Escape') {
        onEscape();
        return;
    }
    if (e.key === 'Tab') trapFullViewTabFocus(overlay, e);
}

function createFullViewKeyHandler(overlay: HTMLElement, onEscape: () => void): (e: KeyboardEvent) => void {
    return function (e: KeyboardEvent): void {
        handleFullViewKeyDown(overlay, onEscape, e);
    };
}

function openFullView(contentNode: HTMLElement, isDiagram: boolean): void {
    if (activeFullViewClose) {
        activeFullViewClose(true);
    }
    purgeExistingFullViewOverlays();

    const previousFocusedElement = document.activeElement as HTMLElement | null;
    document.body.classList.add('modal-open');
    document.body.style.overflow = 'hidden';

    const overlay = document.createElement('div');
    overlay.className = 'fullview-overlay';

    const toolbar = document.createElement('div');
    toolbar.className = 'fullview-toolbar';
    toolbar.innerHTML = buildFullViewToolbarHtml(isDiagram);

    const body = document.createElement('div');
    body.className = 'fullview-body' + (isDiagram ? ' is-diagram' : '');

    const inner = document.createElement('div');
    inner.className = 'fullview-inner';
    if (isDiagram && contentNode && contentNode.style) {
        contentNode.style.transform = '';
        contentNode.style.transition = '';
    }
    inner.appendChild(contentNode);
    body.appendChild(inner);

    overlay.appendChild(toolbar);
    overlay.appendChild(body);
    document.body.appendChild(overlay);

    requestAnimationFrame(function (): void {
        overlay.classList.add('visible');
    });

    let fvMouseMoveHandler: ((e: MouseEvent) => void) | null = null;
    let fvMouseUpHandler: (() => void) | null = null;
    let fvExitHandler: (() => void) | null = null;

    function closeOverlay(immediate?: boolean): void {
        activeFullViewClose = null;
        document.body.classList.remove('modal-open');
        document.body.style.overflow = '';
        overlay.classList.remove('visible');
        document.removeEventListener('keydown', onKeyDown);
        if (fvMouseMoveHandler) document.removeEventListener('mousemove', fvMouseMoveHandler);
        if (fvMouseUpHandler) document.removeEventListener('mouseup', fvMouseUpHandler);
        detachFullViewExitHandler(fvExitHandler);
        restoreFullViewFocus(previousFocusedElement);
        removeFullViewOverlay(overlay, immediate);
    }
    activeFullViewClose = closeOverlay;

    toolbar.querySelector<HTMLButtonElement>('.close-btn')!.addEventListener('click', function (): void {
        closeOverlay(false);
    });

    const onKeyDown = createFullViewKeyHandler(overlay, function (): void { closeOverlay(false); });
    document.addEventListener('keydown', onKeyDown);

    if (isDiagram) {
        const handlers = attachFullViewDiagramZoom(toolbar, body, inner);
        fvMouseMoveHandler = handlers.mouseMove;
        fvMouseUpHandler = handlers.mouseUp;
        fvExitHandler = handlers.exit;
    }
}

// Singleton full-view modal overlay lifecycle, keyboard focus trap, and ZoomPanViewport delegation

const FULLVIEW_DIAGRAM_TOOLBAR_HTML = [
  '<button class="zoom-out-btn" title="Zoom out">−</button>',
  '<span class="zoom-label" style="font-size:11px;color:var(--text-secondary);min-width:36px;text-align:center">100%</span>',
  '<button class="zoom-in-btn" title="Zoom in">+</button>',
  '<button class="zoom-reset-btn" title="Reset zoom">↺</button>',
  '<span style="flex:1"></span>',
  '<button class="close-btn">✕ Close</button>'
].join('');

const FULLVIEW_TABLE_TOOLBAR_HTML = '<span style="flex:1"></span><button class="close-btn">✕ Close</button>';

class FullViewModal {
  private activeClose: ((immediate?: boolean) => void) | null = null;

  close(immediate?: boolean): void {
    if (this.activeClose) {
      this.activeClose(immediate);
    }
  }

  private purgeExistingOverlays(): void {
    const existingOverlays = document.querySelectorAll('.fullview-overlay');
    for (let i = 0; i < existingOverlays.length; i++) {
      existingOverlays[i].remove();
    }
  }

  private buildToolbarHtml(isDiagram: boolean): string {
    if (isDiagram) return FULLVIEW_DIAGRAM_TOOLBAR_HTML;
    return FULLVIEW_TABLE_TOOLBAR_HTML;
  }

  private restoreFocus(previousFocusedElement: HTMLElement | null): void {
    if (previousFocusedElement && typeof previousFocusedElement.focus === 'function') {
      previousFocusedElement.focus();
    }
  }

  private removeOverlay(overlay: HTMLElement, immediate?: boolean): void {
    if (immediate) {
      overlay.remove();
      return;
    }
    setTimeout((): void => { overlay.remove(); }, 200);
  }

  private wrapFocusToBoundary(e: KeyboardEvent, target: HTMLElement, shouldWrap: boolean): void {
    if (!shouldWrap) return;
    e.preventDefault();
    target.focus();
  }

  private trapTabFocus(overlay: HTMLElement, e: KeyboardEvent): void {
    const focusable = overlay.querySelectorAll<HTMLElement>('button, [href], input, select, textarea, [tabindex]:not([tabindex="-1"])');
    if (focusable.length === 0) return;
    const first = focusable[0];
    const last = focusable[focusable.length - 1];
    const outsideOverlay = !overlay.contains(document.activeElement);
    if (e.shiftKey) {
      this.wrapFocusToBoundary(e, last, document.activeElement === first || outsideOverlay);
      return;
    }
    this.wrapFocusToBoundary(e, first, document.activeElement === last || outsideOverlay);
  }

  private handleKeyDown(overlay: HTMLElement, onEscape: () => void, e: KeyboardEvent): void {
    if (e.key === 'Escape') {
      onEscape();
      return;
    }
    if (e.key === 'Tab') this.trapTabFocus(overlay, e);
  }

  open(contentNode: HTMLElement, isDiagram: boolean): void {
    if (this.activeClose) {
      this.activeClose(true);
    }
    this.purgeExistingOverlays();

    const previousFocusedElement = document.activeElement as HTMLElement | null;
    document.body.classList.add('modal-open');
    document.body.style.overflow = 'hidden';

    const overlay = document.createElement('div');
    overlay.className = 'fullview-overlay';

    const toolbar = document.createElement('div');
    toolbar.className = 'fullview-toolbar';
    toolbar.innerHTML = this.buildToolbarHtml(isDiagram);

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

    requestAnimationFrame((): void => {
      overlay.classList.add('visible');
    });

    const zoomViewport = isDiagram
      ? new ZoomPanViewport({ toolbar: toolbar, viewport: body, inner: inner, fallbackWidth: 800, fallbackHeight: 600 })
      : null;

    const closeOverlay = (immediate?: boolean): void => {
      this.activeClose = null;
      document.body.classList.remove('modal-open');
      document.body.style.overflow = '';
      overlay.classList.remove('visible');
      document.removeEventListener('keydown', onKeyDown);
      if (zoomViewport) zoomViewport.dispose();
      this.restoreFocus(previousFocusedElement);
      this.removeOverlay(overlay, immediate);
    };
    this.activeClose = closeOverlay;

    toolbar.querySelector<HTMLButtonElement>('.close-btn')!.addEventListener('click', (): void => {
      closeOverlay(false);
    });

    const onKeyDown = (e: KeyboardEvent): void => {
      this.handleKeyDown(overlay, (): void => { closeOverlay(false); }, e);
    };
    document.addEventListener('keydown', onKeyDown);
  }
}

// Inline Block Resize (Center-Anchored Breakout & Vertical Aperture)

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

class BlockResizer {
  private readonly memory: BlockSizeMemory;
  private activeCleanup: (() => void) | null = null;

  constructor(memory: BlockSizeMemory) {
    this.memory = memory;
  }

  stopActiveDrag(): void {
    if (!this.activeCleanup) return;
    const cleanup = this.activeCleanup;
    this.activeCleanup = null;
    cleanup();
  }

  clearMemory(): void {
    this.memory.clear();
  }

  teardown(preserveSizes?: boolean): void {
    this.stopActiveDrag();
    if (!preserveSizes) {
      this.clearMemory();
    }
  }

  private clampWidth(rawWidth: number): number {
    const viewportWidth = window.innerWidth || 1024;
    const maxWidth = Math.min(viewportWidth, Math.max(280, viewportWidth - 48));
    const minWidth = Math.min(420, maxWidth);
    return Math.round(Math.max(minWidth, Math.min(maxWidth, rawWidth)));
  }

  private clampHeight(rawHeight: number): number {
    const viewportHeight = window.innerHeight || 800;
    const maxHeight = Math.max(220, Math.round(viewportHeight * 0.85));
    return Math.round(Math.max(220, Math.min(maxHeight, rawHeight)));
  }

  private measureWidth(container: HTMLElement): number {
    const rectWidth = container.getBoundingClientRect().width;
    return rectWidth > 0 ? rectWidth : 664;
  }

  private measureHeight(target: HTMLElement): number {
    const rectHeight = target.getBoundingClientRect().height;
    return rectHeight > 0 ? rectHeight : 320;
  }

  private applyDimensions(
    container: HTMLElement,
    verticalTarget: HTMLElement | null,
    blockKey: string,
    width: number | null | undefined,
    height: number | null | undefined
  ): void {
    let savedWidth: number | undefined;
    let savedHeight: number | undefined;
    if (typeof width === 'number') {
      const clampedW = this.clampWidth(width);
      container.style.width = clampedW + 'px';
      container.classList.add('is-resized-x');
      savedWidth = clampedW;
    }
    if (verticalTarget && typeof height === 'number') {
      const clampedH = this.clampHeight(height);
      verticalTarget.style.height = clampedH + 'px';
      container.classList.add('is-resized-y');
      savedHeight = clampedH;
    }
    this.memory.record(blockKey, savedWidth, savedHeight);
  }

  reset(container: HTMLElement, verticalTarget: HTMLElement | null, blockKey: string): void {
    container.style.width = '';
    container.classList.remove('is-resized-x');
    container.classList.remove('is-resized-y');
    if (verticalTarget) {
      verticalTarget.style.height = '';
    }
    this.memory.forget(blockKey);
  }

  private computeNextWidth(axis: ResizeAxis, startWidth: number, dx: number): number | null {
    if (axis.indexOf('e') !== -1) return startWidth + (2 * dx);
    if (axis.indexOf('w') !== -1) return startWidth - (2 * dx);
    return null;
  }

  private computeNextHeight(axis: ResizeAxis, startHeight: number, dy: number): number | null {
    if (axis.indexOf('s') !== -1) return startHeight + dy;
    if (axis.indexOf('n') !== -1) return startHeight - dy;
    return null;
  }

  private startDrag(
    event: MouseEvent,
    container: HTMLElement,
    verticalTarget: HTMLElement | null,
    blockKey: string,
    axis: ResizeAxis
  ): void {
    if (event.button !== 0) return;
    event.preventDefault();
    event.stopPropagation();
    this.stopActiveDrag();

    const startX = event.clientX;
    const startY = event.clientY;
    const startWidth = this.measureWidth(container);
    const startHeight = verticalTarget ? this.measureHeight(verticalTarget) : 0;

    container.classList.add('is-resizing');
    document.body.classList.add('is-block-resizing');
    document.body.style.cursor = BLOCK_RESIZE_CURSORS[axis];

    const onMove = (moveEvent: MouseEvent): void => {
      const nextW = this.computeNextWidth(axis, startWidth, moveEvent.clientX - startX);
      const nextH = this.computeNextHeight(axis, startHeight, moveEvent.clientY - startY);
      this.applyDimensions(container, verticalTarget, blockKey, nextW, nextH);
    };

    const finish = (): void => {
      document.removeEventListener('mousemove', onMove);
      document.removeEventListener('mouseup', finish);
      document.removeEventListener('mouseleave', finish);
      window.removeEventListener('blur', finish);
      container.classList.remove('is-resizing');
      document.body.classList.remove('is-block-resizing');
      document.body.style.cursor = '';
      this.activeCleanup = null;
    };

    this.activeCleanup = finish;
    document.addEventListener('mousemove', onMove);
    document.addEventListener('mouseup', finish);
    document.addEventListener('mouseleave', finish);
    window.addEventListener('blur', finish);
  }

  private createHandle(
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
    handle.addEventListener('mousedown', (e: MouseEvent): void => {
      this.startDrag(e, container, verticalTarget, blockKey, axis);
    });
    handle.addEventListener('dblclick', (e: MouseEvent): void => {
      e.preventDefault();
      e.stopPropagation();
      this.reset(container, verticalTarget, blockKey);
    });
    return handle;
  }

  attach(container: HTMLElement, verticalTarget: HTMLElement | null, blockKey: string): void {
    if (container.querySelector('.block-resize-handle')) return;
    const axes: ResizeAxis[] = verticalTarget ? ['n', 'w', 'e', 's', 'nw', 'ne', 'sw', 'se'] : ['w', 'e'];
    axes.forEach((axis: ResizeAxis): void => {
      container.appendChild(this.createHandle(container, verticalTarget, blockKey, axis));
    });
    this.memory.restore(blockKey, (saved: BlockSizeEntry): void => {
      this.applyDimensions(container, verticalTarget, blockKey, saved.width, saved.height);
    });
  }
}

// Unified zoom, wheel, and drag-pan viewport controller shared by inline Mermaid diagrams and FullViewModal

interface ZoomPanConfig {
  toolbar: HTMLElement;
  viewport: HTMLElement;
  inner: HTMLElement;
  label: HTMLElement;
  fallbackWidth: number;
  fallbackHeight: number;
  onReset?: () => void;
}

interface ZoomPanState {
  scale: number;
  panX: number;
  panY: number;
  isPanning: boolean;
  startX: number;
  startY: number;
  startPanX: number;
  startPanY: number;
  onMouseMove: (e: MouseEvent) => void;
  onMouseUp: () => void;
  onPointerExit: () => void;
}

class ZoomPanViewport {
  private readonly config: ZoomPanConfig;
  private readonly state: ZoomPanState;

  constructor(options: ZoomPanOptions) {
    this.config = {
      toolbar: options.toolbar,
      viewport: options.viewport,
      inner: options.inner,
      label: options.toolbar.querySelector<HTMLElement>('.zoom-label')!,
      fallbackWidth: options.fallbackWidth,
      fallbackHeight: options.fallbackHeight,
      onReset: options.onReset
    };
    this.state = {
      scale: 1,
      panX: 0,
      panY: 0,
      isPanning: false,
      startX: 0,
      startY: 0,
      startPanX: 0,
      startPanY: 0,
      onMouseMove: (e: MouseEvent): void => this.handleMouseMove(e),
      onMouseUp: (): void => this.stopPanning(),
      onPointerExit: (): void => this.stopPanning()
    };
    this.bindToolbarControls();
    this.bindViewportEvents();
    this.applyTransform(false);
  }

  private applyTransform(smooth: boolean): void {
    if (Math.abs(this.state.scale - 1) < 0.02) this.state.scale = 1;
    this.config.inner.style.transition = smooth ? 'transform 0.15s ease-out' : 'none';
    this.config.inner.style.transform = 'translate(' + this.state.panX + 'px,' + this.state.panY + 'px) scale(' + this.state.scale + ')';
    this.config.label.textContent = Math.round(this.state.scale * 100) + '%';
  }

  private clampPan(x: number, y: number): PanCoordinates {
    const limitX = Math.max(500, (this.config.viewport.clientWidth || this.config.fallbackWidth) * this.state.scale);
    const limitY = Math.max(500, (this.config.viewport.clientHeight || this.config.fallbackHeight) * this.state.scale);
    return {
      x: Math.max(-limitX, Math.min(limitX, x)),
      y: Math.max(-limitY, Math.min(limitY, y))
    };
  }

  private handleMouseMove(e: MouseEvent): void {
    if (!this.state.isPanning) return;
    const dx = e.clientX - this.state.startX;
    const dy = e.clientY - this.state.startY;
    const clamped = this.clampPan(this.state.startPanX + dx, this.state.startPanY + dy);
    this.state.panX = clamped.x;
    this.state.panY = clamped.y;
    this.applyTransform(false);
  }

  private stopPanning(): void {
    this.state.isPanning = false;
  }

  private bindToolbarControls(): void {
    this.config.toolbar.querySelector<HTMLButtonElement>('.zoom-in-btn')!.addEventListener('click', (): void => {
      this.state.scale = Math.min(this.state.scale * 1.25, 5);
      this.applyTransform(true);
    });
    this.config.toolbar.querySelector<HTMLButtonElement>('.zoom-out-btn')!.addEventListener('click', (): void => {
      this.state.scale = Math.max(this.state.scale / 1.25, 0.2);
      this.applyTransform(true);
    });
    this.config.toolbar.querySelector<HTMLButtonElement>('.zoom-reset-btn')!.addEventListener('click', (): void => {
      this.state.scale = 1;
      this.state.panX = 0;
      this.state.panY = 0;
      this.applyTransform(true);
      if (this.config.onReset) this.config.onReset();
    });
  }

  private bindViewportEvents(): void {
    this.config.viewport.addEventListener('wheel', (e: WheelEvent): void => {
      e.preventDefault();
      if (Math.abs(e.deltaY) < 1e-3) return;
      const factor = e.deltaY < 0 ? 1.1 : 0.9;
      this.state.scale = Math.max(0.2, Math.min(5, this.state.scale * factor));
      this.applyTransform(false);
    }, { passive: false });

    this.config.viewport.addEventListener('mousedown', (e: MouseEvent): void => {
      this.state.isPanning = true;
      this.state.startX = e.clientX;
      this.state.startY = e.clientY;
      this.state.startPanX = this.state.panX;
      this.state.startPanY = this.state.panY;
      e.preventDefault();
    });

    document.addEventListener('mousemove', this.state.onMouseMove);
    document.addEventListener('mouseup', this.state.onMouseUp);
    document.addEventListener('mouseleave', this.state.onPointerExit);
    window.addEventListener('blur', this.state.onPointerExit);
  }

  dispose(): void {
    document.removeEventListener('mousemove', this.state.onMouseMove);
    document.removeEventListener('mouseup', this.state.onMouseUp);
    document.removeEventListener('mouseleave', this.state.onPointerExit);
    window.removeEventListener('blur', this.state.onPointerExit);
  }
}

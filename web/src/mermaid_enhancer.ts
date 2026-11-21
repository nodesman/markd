// Mermaid diagram DOM enhancer: code block transformation, isolated rendering, and expandable containers

const DIAGRAM_TOOLBAR_HTML = [
  '<button class="zoom-out-btn" title="Zoom out">−</button>',
  '<span class="zoom-label">100%</span>',
  '<button class="zoom-in-btn" title="Zoom in">+</button>',
  '<button class="zoom-reset-btn" title="Reset zoom &amp; size">↺</button>',
  '<button class="fullview-btn" title="Full view">⛶</button>'
].join('');

interface MermaidEnhancerDeps {
  loader: MermaidLoader;
  sanitizer: DomSanitizer;
  resizer: BlockResizer;
  modal: FullViewModal;
}

class MermaidEnhancer implements IDomEnhancer {
  private readonly deps: MermaidEnhancerDeps;
  private readonly viewports: ZoomPanViewport[] = [];

  constructor(deps: MermaidEnhancerDeps) {
    this.deps = deps;
  }

  ensureLoaded(): Promise<MermaidApi> {
    return this.deps.loader.ensureLoaded();
  }

  syncTheme(themeName: string): void {
    this.deps.loader.syncTheme(themeName);
  }

  teardown(preserveSizes?: boolean): void {
    this.deps.resizer.teardown(preserveSizes);
    while (this.viewports.length > 0) {
      this.viewports.pop()!.dispose();
    }
  }

  private transformCodeBlocks(root: HTMLElement): void {
    const mermaidBlocks = root.querySelectorAll<HTMLElement>('pre > code.language-mermaid');
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

  private replaceWithErrorCard(mDiv: HTMLElement, rawDiagramCode: string, err: unknown): void {
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

  private async renderSingleNode(mermaidApi: MermaidApi, mDiv: HTMLElement): Promise<void> {
    const rawDiagramCode = mDiv.dataset.rawCode!;
    try {
      await mermaidApi.run({ nodes: [mDiv] });
      if (!mDiv.isConnected) return;
      const svgEl = mDiv.querySelector('svg');
      if (svgEl) this.deps.sanitizer.sanitizeMermaidSvg(mDiv, svgEl);
    } catch (e) {
      this.replaceWithErrorCard(mDiv, rawDiagramCode, e);
    }
  }

  private replaceAllWithErrors(mermaidDivs: NodeListOf<HTMLElement>, err: unknown): void {
    for (let k = 0; k < mermaidDivs.length; k++) {
      this.replaceWithErrorCard(mermaidDivs[k], mermaidDivs[k].dataset.rawCode!, err);
    }
  }

  private async resolveApi(mermaidDivs: NodeListOf<HTMLElement>): Promise<MermaidApi | null> {
    try {
      return await this.deps.loader.ensureLoaded();
    } catch (loadErr) {
      this.replaceAllWithErrors(mermaidDivs, loadErr);
      return null;
    }
  }

  private async renderNodes(mermaidDivs: NodeListOf<HTMLElement>): Promise<void> {
    const mermaidApi = typeof window.mermaid !== 'undefined'
      ? window.mermaid
      : await this.resolveApi(mermaidDivs);
    if (!mermaidApi) return;
    for (let j = 0; j < mermaidDivs.length; j++) {
      const mDiv = mermaidDivs[j];
      if (!mDiv.isConnected) continue;
      await this.renderSingleNode(mermaidApi, mDiv);
    }
  }

  private wrapSingleDiagram(diagram: HTMLElement, index: number): void {
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
    const zoomController = new ZoomPanViewport({
      toolbar: container,
      viewport: viewport,
      inner: inner,
      fallbackWidth: 600,
      fallbackHeight: 400,
      onReset: (): void => this.deps.resizer.reset(container, viewport, blockKey)
    });
    this.viewports.push(zoomController);

    container.querySelector<HTMLButtonElement>('.fullview-btn')!.addEventListener('click', (): void => {
      this.deps.modal.open(inner.cloneNode(true) as HTMLElement, true);
    });

    this.deps.resizer.attach(container, viewport, blockKey);
  }

  wrapDiagrams(root: HTMLElement): void {
    const diagrams = root.querySelectorAll<HTMLElement>('.mermaid');
    diagrams.forEach((diagram: HTMLElement, index: number): void => {
      this.wrapSingleDiagram(diagram, index);
    });
  }

  async enhance(root: HTMLElement): Promise<void> {
    this.transformCodeBlocks(root);
    const mermaidDivs = root.querySelectorAll<HTMLElement>('.mermaid');
    if (mermaidDivs.length > 0) {
      await this.renderNodes(mermaidDivs);
    }
    this.wrapDiagrams(root);
  }
}

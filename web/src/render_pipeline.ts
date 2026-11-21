// Async render mutex and linear document rendering pipeline

class RenderMutex {
  private pending: Promise<void> | null = null;

  enqueue(task: () => Promise<void>): Promise<void> {
    const nextRun = this.pending
      ? this.pending.then(task, task)
      : task();
    const trackedRun = nextRun.finally((): void => {
      if (this.pending === trackedRun) {
        this.pending = null;
      }
    });
    this.pending = trackedRun;
    return trackedRun;
  }
}

interface RenderPipelineLayers {
  compiler: DocumentCompiler;
  enhancers: IDomEnhancer[];
  navigator: ScrollNavigator;
  bridge: IBridgeHost;
  modal: FullViewModal;
}

interface RenderPipelineState {
  mutex: RenderMutex;
  currentMarkdown: string | null;
}

class RenderPipeline implements IDocumentRenderer {
  private readonly layers: RenderPipelineLayers;
  private readonly state: RenderPipelineState;

  constructor(layers: RenderPipelineLayers) {
    this.layers = layers;
    this.state = {
      mutex: new RenderMutex(),
      currentMarkdown: null
    };
  }

  currentMarkdown(): string | null {
    return this.state.currentMarkdown;
  }

  render(markdownText: unknown, options?: RenderOptions): Promise<void> {
    return this.state.mutex.enqueue((): Promise<void> => {
      return this.executeRender(markdownText, options);
    });
  }

  setTheme(themeName?: string): void {
    if (!themeName) return;
    document.documentElement.setAttribute('data-theme', themeName);
    this.layers.enhancers.forEach((enhancer: IDomEnhancer): void => {
      if (enhancer.syncTheme) enhancer.syncTheme(themeName);
    });
    this.state.mutex.enqueue((): Promise<void> => {
      return this.executeRender(this.state.currentMarkdown, { force: true });
    });
  }

  private async executeRender(markdownText: unknown, options?: RenderOptions): Promise<void> {
    if (typeof markdownText !== 'string') return;
    if (markdownText === this.state.currentMarkdown && (!options || !options.force)) return;
    this.state.currentMarkdown = markdownText;

    this.layers.modal.close(true);

    const preserveSizes = !!(options && (options.preserveScroll === true || options.force === true));
    this.layers.enhancers.forEach((enhancer: IDomEnhancer): void => {
      enhancer.teardown(preserveSizes);
    });

    const contentEl = document.getElementById('content')!;
    const prevScrollY = window.scrollY || 0;
    const existingFrontmatter = contentEl.querySelector<HTMLDetailsElement>('.markd-frontmatter');
    const preserveFrontmatterOpen = !!(existingFrontmatter && existingFrontmatter.open);

    contentEl.innerHTML = this.layers.compiler.compile(markdownText, preserveFrontmatterOpen);

    for (let i = 0; i < this.layers.enhancers.length; i++) {
      await this.layers.enhancers[i].enhance(contentEl);
    }

    this.layers.navigator.setupScrollSpy();
    this.layers.navigator.resolvePostRender(options, prevScrollY);

    document.body.classList.remove('is-hydrating');
    const headings = contentEl.querySelectorAll('h1[id], h2[id], h3[id], h4[id], h5[id], h6[id]');
    this.layers.bridge.notifyDocumentRendered(headings.length);
  }
}

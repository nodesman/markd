// MarkdRuntime composition root: instantiates the entity graph once and publishes window.Markd + thin delegators

interface RuntimeServices {
  pipeline: RenderPipeline;
  navigator: ScrollNavigator;
  modal: FullViewModal;
  mermaid: MermaidEnhancer;
  tables: TableEnhancer;
  codeBlocks: CodeBlockEnhancer;
}

class MarkdRuntime {
  private readonly bridge: BridgeChannel;
  private readonly services: RuntimeServices;

  constructor() {
    const sanitizer = new DomSanitizer();
    const compiler = new DocumentCompiler(sanitizer);
    const sizeMemory = new BlockSizeMemory();
    const resizer = new BlockResizer(sizeMemory);
    const modal = new FullViewModal();
    const mermaidLoader = new MermaidLoader();
    const mermaid = new MermaidEnhancer({ loader: mermaidLoader, sanitizer: sanitizer, resizer: resizer, modal: modal });
    const tables = new TableEnhancer(modal, resizer);
    const codeBlocks = new CodeBlockEnhancer(resizer);

    this.bridge = new BridgeChannel();
    const navigator = new ScrollNavigator(this.bridge);
    const pipeline = new RenderPipeline({
      compiler: compiler,
      enhancers: [mermaid, tables, codeBlocks],
      navigator: navigator,
      bridge: this.bridge,
      modal: modal
    });
    this.bridge.bindTargets(pipeline, navigator);
    new DomInteractionRouter(this.bridge, navigator);

    pipeline.render = pipeline.render.bind(pipeline);
    navigator.scrollToHeading = navigator.scrollToHeading.bind(navigator);
    navigator.scrollToRatio = navigator.scrollToRatio.bind(navigator);

    this.services = {
      pipeline: pipeline,
      navigator: navigator,
      modal: modal,
      mermaid: mermaid,
      tables: tables,
      codeBlocks: codeBlocks
    };

    this.publishWindowApi();
    this.subscribeColorScheme();
    this.bridge.bootstrap();
    this.bootstrapReady();
  }

  private publishWindowApi(): void {
    const openModal = (contentNode: HTMLElement, isDiagram: boolean): void => {
      this.services.modal.open(contentNode, isDiagram);
    };
    const closeModal = (immediate?: boolean): void => {
      this.services.modal.close(immediate);
    };

    window.Markd = {
      version: '1.0.0',
      isReady: false,
      render: this.services.pipeline.render,
      scrollToHeading: this.services.navigator.scrollToHeading,
      scrollToRatio: this.services.navigator.scrollToRatio,
      modal: {
        open: openModal,
        close: closeModal
      },
      get bridge(): MarkdBridge | null {
        return window.markdBridge;
      },
      get session(): MarkdSession | null {
        return window.session;
      }
    };

    this.defineDelegatedWindowMethod(
      'renderMarkdown',
      (): ((markdownText: unknown, options?: RenderOptions) => Promise<void>) => this.services.pipeline.render,
      (fn: (markdownText: unknown, options?: RenderOptions) => Promise<void>): void => { this.services.pipeline.render = fn; }
    );
    this.defineDelegatedWindowMethod(
      'scrollToHeading',
      (): ((id: string, behavior?: ScrollBehavior) => void) => this.services.navigator.scrollToHeading,
      (fn: (id: string, behavior?: ScrollBehavior) => void): void => { this.services.navigator.scrollToHeading = fn; }
    );
    this.defineDelegatedWindowMethod(
      'scrollToRatio',
      (): ((ratio: unknown) => void) => this.services.navigator.scrollToRatio,
      (fn: (ratio: unknown) => void): void => { this.services.navigator.scrollToRatio = fn; }
    );

    window.renderDocument = (markdown: unknown, options?: RenderOptions): void => {
      this.bridge.queueOrRender(markdown, options);
    };
    window.openFullView = openModal;
    window.closeFullView = closeModal;
    window.setupScrollSpy = (): void => { this.services.navigator.setupScrollSpy(); };
    window.wrapMermaidDiagrams = (): void => { this.services.mermaid.wrapDiagrams(document.getElementById('content')!); };
    window.wrapTables = (): void => { this.services.tables.enhance(document.getElementById('content')!); };
    window.attachCodeBlockResizeHandles = (): void => { this.services.codeBlocks.enhance(document.getElementById('content')!); };
    window.handleSessionMarkdownChanged = (newMarkdown: string): void => { this.bridge.handleSessionMarkdownChanged(newMarkdown); };
    window.ensureMermaidLoaded = (): Promise<MermaidApi> => this.services.mermaid.ensureLoaded();
    window.setTheme = (themeName?: string): void => { this.services.pipeline.setTheme(themeName); };
    window.cleanupDiagramListeners = (): void => { this.services.mermaid.teardown(true); };
    window.initQWebChannel = (): void => { this.bridge.connect(); };
  }

  private defineDelegatedWindowMethod<T>(key: string, read: () => T, write: (fn: T) => void): void {
    Object.defineProperty(window, key, {
      configurable: true,
      get: read,
      set: write
    });
  }

  private subscribeColorScheme(): void {
    window.matchMedia('(prefers-color-scheme: dark)').addEventListener('change', (e: MediaQueryListEvent): void => {
      this.services.pipeline.setTheme(e.matches ? 'dark' : 'default');
    });
  }

  private bootstrapReady(): void {
    if (document.readyState === 'loading') {
      document.addEventListener('DOMContentLoaded', (): void => { window.Markd.isReady = true; });
      return;
    }
    window.Markd.isReady = true;
  }
}

void new MarkdRuntime();

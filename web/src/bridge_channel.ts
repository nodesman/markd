// QWebChannel Bridge host: sole entity talking to C++ WebBridge and session over QWebChannel

interface BridgeState {
  connected: boolean;
  pendingQueue: PendingRenderSignal[];
}

interface BridgeTargets {
  renderer: IDocumentRenderer;
  navigator: ScrollNavigator;
}

class BridgeChannel implements IBridgeHost {
  private readonly state: BridgeState;
  private targets!: BridgeTargets;

  constructor() {
    window.markdBridge = null;
    window.session = null;
    this.state = {
      connected: false,
      pendingQueue: []
    };
  }

  bindTargets(renderer: IDocumentRenderer, navigator: ScrollNavigator): void {
    this.targets = { renderer: renderer, navigator: navigator };
  }

  queueOrRender(markdown: unknown, options?: RenderOptions): void {
    if (!this.state.connected && !window.markdBridge) {
      this.state.pendingQueue.push({ markdown: markdown, options: options });
      return;
    }
    this.targets.renderer.render(markdown, options);
  }

  handleSessionMarkdownChanged(newMarkdown: string): void {
    if (window.markdBridge && window.markdBridge.renderDocument) {
      return;
    }
    if (newMarkdown === this.targets.renderer.currentMarkdown()) {
      return;
    }
    this.targets.renderer.render(newMarkdown);
  }

  notifyDocumentRendered(headingCount: number): void {
    if (window.markdBridge && typeof window.markdBridge.onDocumentRendered === 'function') {
      window.markdBridge.onDocumentRendered(headingCount);
    }
  }

  private reportBridgeActiveHeading(headingId: string): void {
    if (window.markdBridge && typeof window.markdBridge.onActiveHeadingChanged === 'function') {
      window.markdBridge.onActiveHeadingChanged(headingId);
      return;
    }
    if (window.markdBridge && typeof window.markdBridge.setActiveHeading === 'function') {
      window.markdBridge.setActiveHeading(headingId);
    }
  }

  notifyActiveHeading(headingId: string): void {
    if (window.session && typeof window.session.reportActiveHeading === 'function') {
      window.session.reportActiveHeading(headingId);
    }
    this.reportBridgeActiveHeading(headingId);
  }

  openExternal(url: string): void {
    if (window.markdBridge && typeof window.markdBridge.openExternalUrl === 'function') {
      window.markdBridge.openExternalUrl(url);
      return;
    }
    if (window.session && typeof window.session.openExternal === 'function') {
      window.session.openExternal(url);
    }
  }

  copyToClipboard(text: string): void {
    if (window.session && typeof window.session.copyToClipboard === 'function') {
      window.session.copyToClipboard(text);
      return;
    }
    if (navigator.clipboard && typeof navigator.clipboard.writeText === 'function') {
      navigator.clipboard.writeText(text);
    }
  }

  private connectBridgeSignals(bridge: MarkdBridge): void {
    if (bridge.renderDocument && typeof bridge.renderDocument.connect === 'function') {
      bridge.renderDocument.connect((markdown: unknown, options?: RenderOptions): void => {
        this.targets.renderer.render(markdown, options);
      });
    }
    if (bridge.navigateToHeading && typeof bridge.navigateToHeading.connect === 'function') {
      bridge.navigateToHeading.connect((headingId: string, behavior?: ScrollBehavior): void => {
        this.targets.navigator.scrollToHeading(headingId, behavior);
      });
    }
    if (bridge.setTheme && typeof bridge.setTheme.connect === 'function') {
      bridge.setTheme.connect((themeName: string): void => {
        this.targets.renderer.setTheme(themeName);
      });
    }
    if (bridge.scrollToRatio && typeof bridge.scrollToRatio.connect === 'function') {
      bridge.scrollToRatio.connect((ratio: unknown): void => {
        this.targets.navigator.scrollToRatio(ratio);
      });
    }
  }

  private flushPendingSignals(): void {
    while (this.state.pendingQueue.length > 0) {
      const queued = this.state.pendingQueue.shift()!;
      this.targets.renderer.render(queued.markdown, queued.options);
    }
  }

  private attachBridge(bridge: MarkdBridge): void {
    if (typeof Object.freeze === 'function') {
      Object.freeze(bridge);
    }
    bridge.logMessage('QWebChannel connected to render.js');
    this.connectBridgeSignals(bridge);
    this.flushPendingSignals();
    if (typeof bridge.bridgeReady === 'function') {
      bridge.bridgeReady();
    }
  }

  private attachSessionSignals(session: MarkdSession): void {
    if (session.markdownChanged && typeof session.markdownChanged.connect === 'function') {
      session.markdownChanged.connect((newMarkdown: string): void => {
        this.handleSessionMarkdownChanged(newMarkdown);
      });
    }
    if (session.selectedHeadingChanged && typeof session.selectedHeadingChanged.connect === 'function') {
      session.selectedHeadingChanged.connect((headingId: string): void => {
        this.targets.navigator.scrollToHeading(headingId);
      });
    }
  }

  private handleChannelConnected(channel: QWebChannelResult): void {
    window.markdBridge = channel.objects.markdBridge;
    window.session = channel.objects.session;
    this.state.connected = true;

    if (window.markdBridge) {
      this.attachBridge(window.markdBridge);
    }
    if (window.session) {
      this.attachSessionSignals(window.session);
    }
  }

  connect(): void {
    if (typeof QWebChannel === 'undefined' || typeof qt === 'undefined' || !qt.webChannelTransport) {
      return;
    }
    new QWebChannel(qt.webChannelTransport, (channel: QWebChannelResult): void => {
      this.handleChannelConnected(channel);
    });
  }

  bootstrap(): void {
    if (document.readyState === 'loading') {
      document.addEventListener('DOMContentLoaded', (): void => this.connect());
      return;
    }
    this.connect();
  }
}

// Lazy Mermaid bundle loader and theme palette synchronization

class MermaidLoader {
  private activeTheme: string;
  private loadPromise: Promise<MermaidApi> | null = null;

  constructor() {
    this.activeTheme = window.matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'default';
    if (typeof window.mermaid !== 'undefined') {
      this.applyConfig();
    }
  }

  private applyConfig(): void {
    window.mermaid!.initialize({
      startOnLoad: false,
      theme: this.activeTheme,
      securityLevel: 'strict',
      fontFamily: '-apple-system, BlinkMacSystemFont, "SF Pro Text", sans-serif'
    });
  }

  private mountScript(resolve: (api: MermaidApi) => void, reject: (err: Error) => void): void {
    const script = document.createElement('script');
    script.src = 'vendor/mermaid/mermaid.min.js';
    script.setAttribute('data-mermaid-lazy', '1');
    script.onload = (): void => {
      this.applyConfig();
      resolve(window.mermaid!);
    };
    script.onerror = (): void => {
      script.remove();
      reject(new Error('Failed to load Mermaid bundle'));
    };
    document.head.appendChild(script);
  }

  ensureLoaded(): Promise<MermaidApi> {
    if (typeof window.mermaid !== 'undefined') {
      return Promise.resolve(window.mermaid);
    }
    if (this.loadPromise) {
      return this.loadPromise;
    }
    const pending: Promise<MermaidApi> = new Promise<MermaidApi>((resolve, reject): void => {
      this.mountScript(resolve, reject);
    }).catch((err: unknown): never => {
      this.loadPromise = null;
      throw err;
    });
    this.loadPromise = pending;
    return pending;
  }

  private isDarkPalette(themeName: string): boolean {
    const lower = themeName.toLowerCase();
    return lower.indexOf('dark') !== -1 || lower.indexOf('obsidian') !== -1;
  }

  syncTheme(themeName: string): void {
    this.activeTheme = this.isDarkPalette(themeName) ? 'dark' : 'default';
    if (typeof window.mermaid !== 'undefined') {
      this.applyConfig();
    }
  }
}

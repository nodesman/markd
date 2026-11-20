// Lazy Mermaid bundle loader, theme synchronization, and diagram listener teardown registry

let activeMermaidTheme: string = window.matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'default';
let mermaidLoadPromise: Promise<MermaidApi> | null = null;
const diagramTeardowns: Array<() => void> = [];

function applyMermaidConfig(): void {
    window.mermaid!.initialize({
        startOnLoad: false,
        theme: activeMermaidTheme,
        securityLevel: 'strict',
        fontFamily: '-apple-system, BlinkMacSystemFont, "SF Pro Text", sans-serif'
    });
}

if (typeof window.mermaid !== 'undefined') {
    applyMermaidConfig();
}

function mountMermaidScript(resolve: (api: MermaidApi) => void, reject: (err: Error) => void): void {
    const script = document.createElement('script');
    script.src = 'vendor/mermaid/mermaid.min.js';
    script.setAttribute('data-mermaid-lazy', '1');
    script.onload = function (): void {
        applyMermaidConfig();
        resolve(window.mermaid!);
    };
    script.onerror = function (): void {
        script.remove();
        reject(new Error('Failed to load Mermaid bundle'));
    };
    document.head.appendChild(script);
}

function ensureMermaidLoaded(): Promise<MermaidApi> {
    if (typeof window.mermaid !== 'undefined') {
        return Promise.resolve(window.mermaid);
    }
    if (mermaidLoadPromise) {
        return mermaidLoadPromise;
    }
    const pending: Promise<MermaidApi> = new Promise<MermaidApi>(mountMermaidScript).catch(function (err: unknown): never {
        mermaidLoadPromise = null;
        throw err;
    });
    mermaidLoadPromise = pending;
    return pending;
}
window.ensureMermaidLoaded = ensureMermaidLoaded;

function isDarkMermaidPalette(themeName: string): boolean {
    const lower = themeName.toLowerCase();
    return lower.indexOf('dark') !== -1 || lower.indexOf('obsidian') !== -1;
}

function syncMermaidTheme(themeName?: string): void {
    if (!themeName) return;
    document.documentElement.setAttribute('data-theme', themeName);
    activeMermaidTheme = isDarkMermaidPalette(themeName) ? 'dark' : 'default';
    if (typeof window.mermaid !== 'undefined') {
        applyMermaidConfig();
    }
    if (currentRenderedMarkdown) {
        renderMarkdown(currentRenderedMarkdown, { force: true });
    }
}
window.setTheme = syncMermaidTheme;

window.matchMedia('(prefers-color-scheme: dark)').addEventListener('change', function (e: MediaQueryListEvent): void {
    syncMermaidTheme(e.matches ? 'dark' : 'default');
});

function cleanupDiagramListeners(): void {
    stopActiveBlockResize();
    while (diagramTeardowns.length > 0) {
        const teardown = diagramTeardowns.pop()!;
        teardown();
    }
}
window.cleanupDiagramListeners = cleanupDiagramListeners;

// Ambient declarations for Qt QWebChannel IPC, vendor libraries, and window.Markd API

interface QWebChannelSignal<Args extends unknown[]> {
    connect?: (handler: (...args: Args) => void) => void;
}

interface RenderOptions {
    force?: boolean;
    preserveScroll?: boolean;
    targetHeading?: string;
    scrollRatio?: number;
}

interface MarkdBridge {
    logMessage: (message: string) => void;
    renderDocument?: QWebChannelSignal<[markdown: unknown, options?: RenderOptions]>;
    navigateToHeading?: QWebChannelSignal<[headingId: string, behavior?: ScrollBehavior]>;
    setTheme?: QWebChannelSignal<[themeName: string]>;
    scrollToRatio?: QWebChannelSignal<[ratio: unknown]>;
    bridgeReady?: () => void;
    openExternalUrl?: (url: string) => void;
    onDocumentRendered?: (headingCount: number) => void;
    onActiveHeadingChanged?: (headingId: string) => void;
    setActiveHeading?: (headingId: string) => void;
}

interface MarkdSession {
    markdownChanged?: QWebChannelSignal<[newMarkdown: string]>;
    selectedHeadingChanged?: QWebChannelSignal<[headingId: string]>;
    copyToClipboard?: (text: string) => void;
    openExternal?: (url: string) => void;
    reportActiveHeading?: (headingId: string) => void;
}

interface QWebChannelResult {
    objects: {
        markdBridge: MarkdBridge | null;
        session: MarkdSession | null;
    };
}

interface DOMPurifyApi {
    addHook?: (entryPoint: 'afterSanitizeAttributes', hook: (node: Element) => void) => void;
    sanitize: (dirty: string, config?: Record<string, unknown>) => string;
}

interface HighlightJsApi {
    getLanguage: (languageName: string) => unknown;
    highlight: (code: string, options: { language: string; ignoreIllegals?: boolean }) => { value: string };
}

interface MarkdownItToken {
    info: string;
    content: string;
}

interface MarkdownItRendererRules {
    fence?: (tokens: MarkdownItToken[], idx: number) => string;
}

interface MarkdownItUtils {
    escapeHtml: (str: string) => string;
    unescapeAll: (str: string) => string;
}

interface MarkdownItInstance {
    utils: MarkdownItUtils;
    renderer: { rules: MarkdownItRendererRules };
    use: (plugin: unknown, options?: Record<string, unknown>) => MarkdownItInstance;
    render: (src: string) => string;
}

interface MermaidConfig {
    startOnLoad: boolean;
    theme: string;
    securityLevel: string;
    fontFamily: string;
}

interface MermaidApi {
    initialize: (config: MermaidConfig) => void;
    run: (options: { nodes: HTMLElement[] }) => Promise<void>;
}

interface MarkdModalApi {
    open: (contentNode: HTMLElement, isDiagram: boolean) => void;
    close: (immediate?: boolean) => void;
}

interface MarkdApi {
    version: string;
    isReady: boolean;
    render: (markdownText: unknown, options?: RenderOptions) => Promise<void>;
    scrollToHeading: (id: string, behavior?: ScrollBehavior) => void;
    scrollToRatio: (ratio: unknown) => void;
    modal: MarkdModalApi;
    readonly bridge: MarkdBridge | null;
    readonly session: MarkdSession | null;
}

interface CopyButtonElement extends HTMLElement {
    _copyTimeoutId?: ReturnType<typeof setTimeout> | null;
}

declare var DOMPurify: DOMPurifyApi | undefined;
declare var hljs: HighlightJsApi;
declare var QWebChannel: (new (transport: unknown, callback: (channel: QWebChannelResult) => void) => unknown) | undefined;
declare var qt: { webChannelTransport?: unknown } | undefined;

interface Window {
    markdBridge: MarkdBridge | null;
    session: MarkdSession | null;
    renderDocument: (markdown: unknown, options?: RenderOptions) => void;
    ensureMermaidLoaded: () => Promise<MermaidApi>;
    setTheme: (themeName?: string) => void;
    cleanupDiagramListeners: () => void;
    Markd: MarkdApi;
    markdownit: (options: { html: boolean; linkify: boolean; typographer: boolean }) => MarkdownItInstance;
    markdownItAnchor?: unknown;
    markdownitTaskLists?: unknown;
    texmath?: unknown;
    katex?: unknown;
    mermaid?: MermaidApi;
    renderMarkdown: (markdownText: unknown, options?: RenderOptions) => Promise<void>;
    scrollToHeading: (id: string, behavior?: ScrollBehavior) => void;
    scrollToRatio: (ratio: unknown) => void;
    openFullView: (contentNode: HTMLElement, isDiagram: boolean) => void;
    closeFullView: (immediate?: boolean) => void;
    setupScrollSpy: () => void;
    wrapMermaidDiagrams: () => void;
    wrapTables: () => void;
    attachCodeBlockResizeHandles: () => void;
    handleSessionMarkdownChanged: (newMarkdown: string) => void;
}

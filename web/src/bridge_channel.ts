// QWebChannel Bridge setup and Qt C++ IPC downlink/uplink lifecycle

window.markdBridge = null;
window.session = null;

let isBridgeConnected = false;
const pendingRenderSignals: PendingRenderSignal[] = [];

function queueOrRenderDocument(markdown: unknown, options?: RenderOptions): void {
    if (!isBridgeConnected && !window.markdBridge) {
        pendingRenderSignals.push({ markdown: markdown, options: options });
        return;
    }
    renderMarkdown(markdown, options);
}
window.renderDocument = queueOrRenderDocument;

function handleSessionMarkdownChanged(newMarkdown: string): void {
    if (window.markdBridge && window.markdBridge.renderDocument) {
        return;
    }
    if (newMarkdown === currentRenderedMarkdown) {
        return;
    }
    renderMarkdown(newMarkdown);
}

function onBridgeRenderDocument(markdown: unknown, options?: RenderOptions): void {
    if (typeof renderMarkdown === 'function') {
        renderMarkdown(markdown, options);
    }
}

function onBridgeNavigateHeading(headingId: string, behavior?: ScrollBehavior): void {
    if (typeof scrollToHeading === 'function') {
        scrollToHeading(headingId, behavior);
    }
}

function onBridgeScrollRatio(ratio: unknown): void {
    if (typeof scrollToRatio === 'function') {
        scrollToRatio(ratio);
    }
}

function onSessionHeadingSelected(headingId: string): void {
    if (typeof scrollToHeading === 'function') {
        scrollToHeading(headingId);
    }
}

function connectBridgeSignals(bridge: MarkdBridge): void {
    if (bridge.renderDocument && typeof bridge.renderDocument.connect === 'function') {
        bridge.renderDocument.connect(onBridgeRenderDocument);
    }
    if (bridge.navigateToHeading && typeof bridge.navigateToHeading.connect === 'function') {
        bridge.navigateToHeading.connect(onBridgeNavigateHeading);
    }
    if (bridge.setTheme && typeof bridge.setTheme.connect === 'function') {
        bridge.setTheme.connect(syncMermaidTheme);
    }
    if (bridge.scrollToRatio && typeof bridge.scrollToRatio.connect === 'function') {
        bridge.scrollToRatio.connect(onBridgeScrollRatio);
    }
}

function flushPendingRenderSignals(): void {
    while (pendingRenderSignals.length > 0) {
        const queued = pendingRenderSignals.shift()!;
        renderMarkdown(queued.markdown, queued.options);
    }
}

function attachMarkdBridge(bridge: MarkdBridge): void {
    if (typeof Object.freeze === 'function') {
        Object.freeze(bridge);
    }
    bridge.logMessage('QWebChannel connected to render.js');
    connectBridgeSignals(bridge);
    flushPendingRenderSignals();
    if (typeof bridge.bridgeReady === 'function') {
        bridge.bridgeReady();
    }
}

function attachSessionSignals(session: MarkdSession): void {
    if (session.markdownChanged && typeof session.markdownChanged.connect === 'function') {
        session.markdownChanged.connect(handleSessionMarkdownChanged);
    }
    if (session.selectedHeadingChanged && typeof session.selectedHeadingChanged.connect === 'function') {
        session.selectedHeadingChanged.connect(onSessionHeadingSelected);
    }
}

function handleChannelConnected(channel: QWebChannelResult): void {
    window.markdBridge = channel.objects.markdBridge;
    window.session = channel.objects.session;
    isBridgeConnected = true;

    if (window.markdBridge) {
        attachMarkdBridge(window.markdBridge);
    }
    if (window.session) {
        attachSessionSignals(window.session);
    }
}

function initQWebChannel(): void {
    if (typeof QWebChannel === 'undefined' || typeof qt === 'undefined' || !qt.webChannelTransport) {
        return;
    }
    new QWebChannel(qt.webChannelTransport, handleChannelConnected);
}

function bootstrapQWebChannel(): void {
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', initQWebChannel);
        return;
    }
    initQWebChannel();
}

// Scroll spy observation, heading navigation throttling, and post-render scroll restoration

let scrollSpyObserver: IntersectionObserver | null = null;
let lastScrolledHeadingId: string | null = null;
let lastScrollTimestamp = 0;

function findTopIntersectingHeading(entries: IntersectionObserverEntry[]): Element | null {
    let topHeading: Element | null = null;
    let topY = Infinity;
    for (let i = 0; i < entries.length; i++) {
        const entry = entries[i];
        if (!entry.isIntersecting || entry.boundingClientRect.top >= topY) continue;
        topY = entry.boundingClientRect.top;
        topHeading = entry.target;
    }
    return topHeading;
}

function reportBridgeActiveHeading(headingId: string): void {
    if (window.markdBridge && typeof window.markdBridge.onActiveHeadingChanged === 'function') {
        window.markdBridge.onActiveHeadingChanged(headingId);
        return;
    }
    if (window.markdBridge && typeof window.markdBridge.setActiveHeading === 'function') {
        window.markdBridge.setActiveHeading(headingId);
    }
}

function notifyActiveHeading(headingId: string): void {
    if (window.session && typeof window.session.reportActiveHeading === 'function') {
        window.session.reportActiveHeading(headingId);
    }
    reportBridgeActiveHeading(headingId);
}

function handleScrollSpyEntries(entries: IntersectionObserverEntry[]): void {
    const topHeading = findTopIntersectingHeading(entries);
    if (topHeading) {
        notifyActiveHeading(topHeading.id);
    }
}

function setupScrollSpy(): void {
    if (scrollSpyObserver) {
        scrollSpyObserver.disconnect();
    }

    const headings = document.querySelectorAll('#content h1[id], #content h2[id], #content h3[id], #content h4[id], #content h5[id], #content h6[id]');
    if (headings.length === 0) return;

    scrollSpyObserver = new IntersectionObserver(handleScrollSpyEntries, {
        rootMargin: '0px 0px -70% 0px',
        threshold: 0
    });

    headings.forEach(function (h: Element): void {
        scrollSpyObserver!.observe(h);
    });
}

function scrollToHeading(id: string, behavior?: ScrollBehavior): void {
    const now = Date.now();
    if (id === lastScrolledHeadingId && now - lastScrollTimestamp < 150) {
        return;
    }
    lastScrolledHeadingId = id;
    lastScrollTimestamp = now;

    const el = document.getElementById(id);
    if (el) {
        el.scrollIntoView({ behavior: behavior || 'smooth', block: 'start' });
    }
}

function scrollToRatio(ratio: unknown): void {
    if (typeof ratio !== 'number' || isNaN(ratio)) return;
    const maxScroll = Math.max(0, document.documentElement.scrollHeight - window.innerHeight);
    const clampedRatio = Math.max(0, Math.min(1, ratio));
    window.scrollTo({ top: clampedRatio * maxScroll, behavior: 'instant' });
}

function resolvePostRenderScroll(options: RenderOptions | undefined, prevScrollY: number): void {
    if (options && options.preserveScroll === true) {
        window.scrollTo(0, prevScrollY);
        return;
    }
    if (options && options.targetHeading) {
        scrollToHeading(options.targetHeading, 'instant');
        return;
    }
    if (options && typeof options.scrollRatio === 'number') {
        scrollToRatio(options.scrollRatio);
        return;
    }
    window.scrollTo(0, 0);
}

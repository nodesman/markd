// Scroll spy observation, heading navigation throttling, and post-render scroll restoration

interface ScrollNavigatorState {
  observer: IntersectionObserver | null;
  lastHeadingId: string | null;
  lastTimestamp: number;
}

class ScrollNavigator {
  private readonly bridge: IBridgeHost;
  private readonly state: ScrollNavigatorState;

  constructor(bridge: IBridgeHost) {
    this.bridge = bridge;
    this.state = {
      observer: null,
      lastHeadingId: null,
      lastTimestamp: 0
    };
  }

  private findTopIntersectingHeading(entries: IntersectionObserverEntry[]): Element | null {
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

  private handleScrollSpyEntries(entries: IntersectionObserverEntry[]): void {
    const topHeading = this.findTopIntersectingHeading(entries);
    if (topHeading) {
      this.bridge.notifyActiveHeading(topHeading.id);
    }
  }

  setupScrollSpy(): void {
    if (this.state.observer) {
      this.state.observer.disconnect();
    }

    const headings = document.querySelectorAll('#content h1[id], #content h2[id], #content h3[id], #content h4[id], #content h5[id], #content h6[id]');
    if (headings.length === 0) return;

    this.state.observer = new IntersectionObserver((entries: IntersectionObserverEntry[]): void => {
      this.handleScrollSpyEntries(entries);
    }, {
      rootMargin: '0px 0px -70% 0px',
      threshold: 0
    });

    headings.forEach((h: Element): void => {
      this.state.observer!.observe(h);
    });
  }

  scrollToHeading(id: string, behavior?: ScrollBehavior): void {
    const now = Date.now();
    if (id === this.state.lastHeadingId && now - this.state.lastTimestamp < 150) {
      return;
    }
    this.state.lastHeadingId = id;
    this.state.lastTimestamp = now;

    const el = document.getElementById(id);
    if (el) {
      el.scrollIntoView({ behavior: behavior || 'smooth', block: 'start' });
    }
  }

  scrollToRatio(ratio: unknown): void {
    if (typeof ratio !== 'number' || isNaN(ratio)) return;
    const maxScroll = Math.max(0, document.documentElement.scrollHeight - window.innerHeight);
    const clampedRatio = Math.max(0, Math.min(1, ratio));
    window.scrollTo({ top: clampedRatio * maxScroll, behavior: 'instant' });
  }

  resolvePostRender(options: RenderOptions | undefined, prevScrollY: number): void {
    if (options && options.preserveScroll === true) {
      window.scrollTo(0, prevScrollY);
      return;
    }
    if (options && options.targetHeading) {
      this.scrollToHeading(options.targetHeading, 'instant');
      return;
    }
    if (options && typeof options.scrollRatio === 'number') {
      this.scrollToRatio(options.scrollRatio);
      return;
    }
    window.scrollTo(0, 0);
  }
}

// Delegated DOM click router for code block clipboard copy, internal anchors, and external links

class DomInteractionRouter {
  private readonly bridge: IBridgeHost;
  private readonly navigator: ScrollNavigator;

  constructor(bridge: IBridgeHost, navigator: ScrollNavigator) {
    this.bridge = bridge;
    this.navigator = navigator;
    document.addEventListener('click', (e: MouseEvent): void => {
      this.handleDocumentClick(e);
    });
  }

  private scheduleCopyReset(copyBtn: CopyButtonElement): void {
    if (copyBtn._copyTimeoutId) {
      clearTimeout(copyBtn._copyTimeoutId);
    }
    copyBtn._copyTimeoutId = setTimeout((): void => {
      copyBtn.textContent = 'Copy';
      copyBtn.classList.remove('copied');
      copyBtn._copyTimeoutId = null;
    }, 1500);
  }

  private handleCodeCopy(copyBtn: CopyButtonElement): void {
    const card = copyBtn.closest('.code-block-card');
    const codeEl = card ? card.querySelector('pre > code') : null;
    if (!codeEl) return;
    const rawCode = codeEl.textContent!.replace(/\n$/, '');
    this.bridge.copyToClipboard(rawCode);
    copyBtn.textContent = 'Copied!';
    copyBtn.classList.add('copied');
    this.scheduleCopyReset(copyBtn);
  }

  private handleDocumentClick(e: MouseEvent): void {
    const target = e.target as (EventTarget & { closest?: (selector: string) => Element | null }) | null;
    if (!target || typeof target.closest !== 'function') return;
    const copyBtn = target.closest('.copy-btn') as CopyButtonElement | null;
    if (copyBtn) {
      this.handleCodeCopy(copyBtn);
      return;
    }
    const anchor = target.closest('a');
    if (!anchor) return;
    const href = anchor.getAttribute('href');
    if (!href) return;
    e.preventDefault();
    if (href.startsWith('#')) {
      const targetId = decodeURIComponent(href.slice(1));
      this.navigator.scrollToHeading(targetId);
      return;
    }
    this.bridge.openExternal(href);
  }
}

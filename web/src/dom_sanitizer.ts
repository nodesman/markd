// DOMPurify security hooks, document HTML sanitization, and Mermaid SVG scrubbing

const RESERVED_BRIDGE_NAMES = new Set<string>([
  'markdBridge',
  'session',
  'Markd',
  'renderDocument'
]);

class DomSanitizer {
  constructor() {
    this.installHooks();
  }

  private installHooks(): void {
    if (typeof DOMPurify !== 'undefined' && typeof DOMPurify.addHook === 'function') {
      DOMPurify.addHook('afterSanitizeAttributes', (node: Element): void => {
        this.guardDomClobbering(node);
      });
      DOMPurify.addHook('afterSanitizeAttributes', (node: Element): void => {
        this.hardenOutboundAnchor(node);
        this.neutralizePositionStyle(node);
      });
    }
  }

  private prefixReservedAttribute(node: Element, attributeName: string): void {
    if (!node.hasAttribute(attributeName)) return;
    const value = node.getAttribute(attributeName)!;
    if (RESERVED_BRIDGE_NAMES.has(value)) {
      node.setAttribute(attributeName, 'user-content-' + value);
    }
  }

  private guardDomClobbering(node: Element): void {
    this.prefixReservedAttribute(node, 'id');
    this.prefixReservedAttribute(node, 'name');
  }

  private hardenOutboundAnchor(node: Element): void {
    if (node.tagName !== 'A') return;
    const target = node.getAttribute('target');
    const href = node.getAttribute('href') || '';
    const isOutbound = target === '_blank' || /^(?:https?:\/\/|\/\/)/i.test(href);
    if (isOutbound) {
      node.setAttribute('rel', 'noopener noreferrer');
    }
  }

  private neutralizePositionStyle(node: Element): void {
    if (!node.hasAttribute('style')) return;
    const style = node.getAttribute('style')!;
    if (!/position\s*:\s*(?:fixed|absolute)/i.test(style)) return;
    const cleanStyle = style.replace(/position\s*:\s*(?:fixed|absolute);?/gi, '').trim();
    if (cleanStyle) {
      node.setAttribute('style', cleanStyle);
      return;
    }
    node.removeAttribute('style');
  }

  sanitizeDocument(html: string): string {
    return DOMPurify!.sanitize(html, {
      ADD_TAGS: ['semantics', 'annotation'],
      ADD_ATTR: ['target', 'encoding', 'rel', 'role', 'aria-label', 'data-lang', 'type'],
      FORBID_TAGS: ['style'],
      ALLOW_DATA_ATTR: true,
      SANITIZE_NAMED_PROPS: false,
      RETURN_TRUSTED_TYPE: false
    });
  }

  private scrubStyleElement(styleNode: HTMLStyleElement): void {
    const safeCss = styleNode.textContent!
      .replace(/position\s*:\s*(?:fixed|absolute)/gi, 'position: static')
      .replace(/@import\b[^;]*;?/gi, '');
    styleNode.textContent = safeCss;
  }

  sanitizeMermaidSvg(targetDiv: HTMLElement, svgEl: SVGSVGElement): void {
    targetDiv.innerHTML = DOMPurify!.sanitize(svgEl.outerHTML, {
      USE_PROFILES: { html: true, svg: true, svgFilters: true },
      ADD_TAGS: ['foreignObject'],
      ADD_ATTR: ['dominant-baseline', 'xmlns'],
      HTML_INTEGRATION_POINTS: { foreignobject: true }
    });
    const styles = targetDiv.querySelectorAll('style');
    for (let i = 0; i < styles.length; i++) {
      this.scrubStyleElement(styles[i]);
    }
    targetDiv.setAttribute('data-processed', 'true');
  }
}

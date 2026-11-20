// DOMPurify security hooks: DOM clobbering guard, outbound anchor hardening, CSS position neutralization

const RESERVED_BRIDGE_NAMES = new Set<string>([
    'markdBridge',
    'session',
    'Markd',
    'renderDocument'
]);

function prefixReservedAttribute(node: Element, attributeName: string): void {
    if (!node.hasAttribute(attributeName)) return;
    const value = node.getAttribute(attributeName)!;
    if (RESERVED_BRIDGE_NAMES.has(value)) {
        node.setAttribute(attributeName, 'user-content-' + value);
    }
}

function guardDomClobbering(node: Element): void {
    prefixReservedAttribute(node, 'id');
    prefixReservedAttribute(node, 'name');
}

function hardenOutboundAnchor(node: Element): void {
    if (node.tagName !== 'A') return;
    const target = node.getAttribute('target');
    const href = node.getAttribute('href') || '';
    const isOutbound = target === '_blank' || /^(?:https?:\/\/|\/\/)/i.test(href);
    if (isOutbound) {
        node.setAttribute('rel', 'noopener noreferrer');
    }
}

function neutralizePositionStyle(node: Element): void {
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

function sanitizePostAttributes(node: Element): void {
    hardenOutboundAnchor(node);
    neutralizePositionStyle(node);
}

if (typeof DOMPurify !== 'undefined' && typeof DOMPurify.addHook === 'function') {
    DOMPurify.addHook('afterSanitizeAttributes', guardDomClobbering);
    DOMPurify.addHook('afterSanitizeAttributes', sanitizePostAttributes);
}

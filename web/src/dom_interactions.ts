// Global anchor navigation interception and code block copy delegation

function dispatchClipboardWrite(rawCode: string): void {
    if (window.session && typeof window.session.copyToClipboard === 'function') {
        window.session.copyToClipboard(rawCode);
        return;
    }
    if (navigator.clipboard && typeof navigator.clipboard.writeText === 'function') {
        navigator.clipboard.writeText(rawCode);
    }
}

function scheduleCopyButtonReset(copyBtn: CopyButtonElement): void {
    if (copyBtn._copyTimeoutId) {
        clearTimeout(copyBtn._copyTimeoutId);
    }
    copyBtn._copyTimeoutId = setTimeout(function (): void {
        copyBtn.textContent = 'Copy';
        copyBtn.classList.remove('copied');
        copyBtn._copyTimeoutId = null;
    }, 1500);
}

function handleCodeCopyClick(copyBtn: CopyButtonElement): void {
    const card = copyBtn.closest('.code-block-card');
    const codeEl = card ? card.querySelector('pre > code') : null;
    if (!codeEl) return;
    const rawCode = codeEl.textContent!.replace(/\n$/, '');
    dispatchClipboardWrite(rawCode);
    copyBtn.textContent = 'Copied!';
    copyBtn.classList.add('copied');
    scheduleCopyButtonReset(copyBtn);
}

function dispatchExternalLink(href: string): void {
    if (window.markdBridge && typeof window.markdBridge.openExternalUrl === 'function') {
        window.markdBridge.openExternalUrl(href);
        return;
    }
    if (window.session && typeof window.session.openExternal === 'function') {
        window.session.openExternal(href);
    }
}

function handleDocumentClick(e: MouseEvent): void {
    const target = e.target as (EventTarget & { closest?: (selector: string) => Element | null }) | null;
    if (!target || typeof target.closest !== 'function') return;
    const copyBtn = target.closest('.copy-btn') as CopyButtonElement | null;
    if (copyBtn) {
        handleCodeCopyClick(copyBtn);
        return;
    }
    const anchor = target.closest('a');
    if (!anchor) return;
    const href = anchor.getAttribute('href');
    if (!href) return;
    e.preventDefault();
    if (href.startsWith('#')) {
        const targetId = decodeURIComponent(href.slice(1));
        scrollToHeading(targetId);
        return;
    }
    dispatchExternalLink(href);
}

document.addEventListener('click', handleDocumentClick);

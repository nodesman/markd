// Full-view diagram modal zoom and pan interaction controller

function attachFullViewDiagramZoom(
    toolbar: HTMLElement,
    body: HTMLElement,
    inner: HTMLElement
): FullViewZoomHandlers {
    let fvScale = 1;
    let fvPanX = 0;
    let fvPanY = 0;
    let fvIsPanning = false;
    let fvStartX = 0;
    let fvStartY = 0;
    let fvStartPanX = 0;
    let fvStartPanY = 0;
    const fvLabel = toolbar.querySelector<HTMLElement>('.zoom-label')!;

    function fvApply(smooth: boolean): void {
        if (Math.abs(fvScale - 1) < 0.02) fvScale = 1;
        inner.style.transition = smooth ? 'transform 0.15s ease-out' : 'none';
        inner.style.transform = 'translate(' + fvPanX + 'px,' + fvPanY + 'px) scale(' + fvScale + ')';
        fvLabel.textContent = Math.round(fvScale * 100) + '%';
    }

    function fvClampPan(x: number, y: number): PanCoordinates {
        const limitX = Math.max(500, (body.clientWidth || 800) * fvScale);
        const limitY = Math.max(500, (body.clientHeight || 600) * fvScale);
        return { x: Math.max(-limitX, Math.min(limitX, x)), y: Math.max(-limitY, Math.min(limitY, y)) };
    }

    toolbar.querySelector<HTMLButtonElement>('.zoom-in-btn')!.addEventListener('click', function (): void {
        fvScale = Math.min(fvScale * 1.25, 5);
        fvApply(true);
    });

    toolbar.querySelector<HTMLButtonElement>('.zoom-out-btn')!.addEventListener('click', function (): void {
        fvScale = Math.max(fvScale / 1.25, 0.2);
        fvApply(true);
    });

    toolbar.querySelector<HTMLButtonElement>('.zoom-reset-btn')!.addEventListener('click', function (): void {
        fvScale = 1;
        fvPanX = 0;
        fvPanY = 0;
        fvApply(true);
    });

    body.addEventListener('wheel', function (e: WheelEvent): void {
        e.preventDefault();
        if (Math.abs(e.deltaY) < 1e-3) return;
        const factor = e.deltaY < 0 ? 1.1 : 0.9;
        fvScale = Math.max(0.2, Math.min(5, fvScale * factor));
        fvApply(false);
    }, { passive: false });

    body.addEventListener('mousedown', function (e: MouseEvent): void {
        fvIsPanning = true;
        fvStartX = e.clientX;
        fvStartY = e.clientY;
        fvStartPanX = fvPanX;
        fvStartPanY = fvPanY;
        e.preventDefault();
    });

    const mouseMove = function (e: MouseEvent): void {
        if (!fvIsPanning) return;
        const clamped = fvClampPan(fvStartPanX + (e.clientX - fvStartX), fvStartPanY + (e.clientY - fvStartY));
        fvPanX = clamped.x;
        fvPanY = clamped.y;
        fvApply(false);
    };

    const mouseUp = function (): void {
        fvIsPanning = false;
    };

    const exit = function (): void {
        fvIsPanning = false;
    };

    document.addEventListener('mousemove', mouseMove);
    document.addEventListener('mouseup', mouseUp);
    document.addEventListener('mouseleave', exit);
    window.addEventListener('blur', exit);

    fvApply(false);
    return { mouseMove: mouseMove, mouseUp: mouseUp, exit: exit };
}

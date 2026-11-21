// Wide table DOM enhancer: expandable wrapper, FullViewModal trigger, and horizontal BlockResizer

class TableEnhancer implements IDomEnhancer {
  private readonly modal: FullViewModal;
  private readonly resizer: BlockResizer;

  constructor(modal: FullViewModal, resizer: BlockResizer) {
    this.modal = modal;
    this.resizer = resizer;
  }

  teardown(preserveSizes?: boolean): void {
    this.resizer.teardown(preserveSizes);
  }

  private wrapSingleTable(table: HTMLTableElement, index: number): void {
    if (table.parentElement!.classList.contains('table-wrapper')) return;

    const wrapper = document.createElement('div');
    wrapper.className = 'table-wrapper';

    const btn = document.createElement('button');
    btn.className = 'table-expand-btn';
    btn.title = 'Full view';
    btn.textContent = '⛶';

    table.parentNode!.insertBefore(wrapper, table);
    wrapper.appendChild(btn);
    wrapper.appendChild(table);

    btn.addEventListener('click', (): void => {
      this.modal.open(table.cloneNode(true) as HTMLElement, false);
    });

    this.resizer.attach(wrapper, null, 'table:' + index);
  }

  enhance(root: HTMLElement): void {
    const tables: HTMLTableElement[] = [];
    for (let i = 0; i < root.children.length; i++) {
      const child = root.children[i];
      if (child.tagName === 'TABLE') tables.push(child as HTMLTableElement);
    }
    tables.forEach((table: HTMLTableElement, index: number): void => {
      this.wrapSingleTable(table, index);
    });
  }
}

// Code block DOM enhancer: horizontal resize handles and block size memory teardown

class CodeBlockEnhancer implements IDomEnhancer {
  private readonly resizer: BlockResizer;

  constructor(resizer: BlockResizer) {
    this.resizer = resizer;
  }

  teardown(preserveSizes?: boolean): void {
    this.resizer.teardown(preserveSizes);
  }

  enhance(root: HTMLElement): void {
    const cards = root.querySelectorAll<HTMLElement>('.code-block-card');
    cards.forEach((card: HTMLElement, index: number): void => {
      this.resizer.attach(card, null, 'code:' + index);
    });
  }
}

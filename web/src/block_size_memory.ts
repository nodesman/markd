// First-class collection encapsulating remembered inline block dimensions

class BlockSizeMemory {
  private entries: Record<string, BlockSizeEntry> = Object.create(null);

  record(key: string, width: number | undefined, height: number | undefined): void {
    const entry = this.entries[key] || {};
    if (typeof width === 'number') entry.width = width;
    if (typeof height === 'number') entry.height = height;
    this.entries[key] = entry;
  }

  restore(key: string, apply: (entry: BlockSizeEntry) => void): void {
    const saved = this.entries[key];
    if (saved) apply(saved);
  }

  forget(key: string): void {
    delete this.entries[key];
  }

  clear(): void {
    this.entries = Object.create(null);
  }
}

// Shared internal domain types and the 3 architectural interfaces for the Markd web runtime

interface IBridgeHost {
  bootstrap(): void;
  queueOrRender(markdown: unknown, options?: RenderOptions): void;
  notifyDocumentRendered(headingCount: number): void;
  notifyActiveHeading(headingId: string): void;
  openExternal(url: string): void;
  copyToClipboard(text: string): void;
}

interface IDocumentRenderer {
  render(markdown: unknown, options?: RenderOptions): Promise<void>;
  setTheme(themeName?: string): void;
  currentMarkdown(): string | null;
}

interface IDomEnhancer {
  enhance(root: HTMLElement): Promise<void> | void;
  teardown(preserveSizes?: boolean): void;
  syncTheme?(themeName: string): void;
}

interface PendingRenderSignal {
  markdown: unknown;
  options?: RenderOptions;
}

interface FrontmatterError {
  line: number;
  message: string;
}

type FrontmatterEntry =
  | { key: string; type: 'array'; value: string[] }
  | { key: string; type: 'bool'; value: boolean }
  | { key: string; type: 'nested'; value: string }
  | { key: string; type: 'scalar'; value: string };

type NestedBlockResult =
  | { error: FrontmatterError }
  | { error?: undefined; nextIndex: number; type: 'array'; value: string[] }
  | { error?: undefined; nextIndex: number; type: 'nested'; value: string };

interface ParsedFrontmatterYaml {
  entries: FrontmatterEntry[];
  error: FrontmatterError | null;
}

interface FoundFrontmatter {
  found: true;
  yamlLines: string[];
  entries: FrontmatterEntry[];
  error: FrontmatterError | null;
  body: string;
}

type FrontmatterExtraction =
  | { found: false; body: string }
  | FoundFrontmatter;

type ResizeAxis = 'n' | 'w' | 'e' | 's' | 'nw' | 'ne' | 'sw' | 'se';

interface BlockSizeEntry {
  width?: number;
  height?: number;
}

interface PanCoordinates {
  x: number;
  y: number;
}

interface ZoomPanOptions {
  toolbar: HTMLElement;
  viewport: HTMLElement;
  inner: HTMLElement;
  fallbackWidth: number;
  fallbackHeight: number;
  onReset?: () => void;
}

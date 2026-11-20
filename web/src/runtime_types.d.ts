// Shared internal domain types for the Markd web runtime modules

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

interface FullViewZoomHandlers {
    mouseMove: (e: MouseEvent) => void;
    mouseUp: () => void;
    exit: () => void;
}

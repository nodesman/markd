#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
QT_PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
FOUNDRY_ROOT="$(cd "$QT_PROJECT_DIR/../../.." && pwd)"
DEFAULT_MANIFEST="$FOUNDRY_ROOT/lab/markd-website/api/v1/updates/check"

MANIFEST_PATH="$DEFAULT_MANIFEST"
VERSION=""
DARWIN_ARM64_SHA=""
DARWIN_ARM64_URL=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --manifest)
            MANIFEST_PATH="$2"
            shift 2
            ;;
        --version)
            VERSION="$2"
            shift 2
            ;;
        --darwin-arm64-sha256)
            DARWIN_ARM64_SHA="$2"
            shift 2
            ;;
        --darwin-arm64-url)
            DARWIN_ARM64_URL="$2"
            shift 2
            ;;
        *)
            shift
            ;;
    esac
done

node - "$MANIFEST_PATH" "$VERSION" "$DARWIN_ARM64_SHA" "$DARWIN_ARM64_URL" << 'EOF'
const fs = require('node:fs');
const [manifestPath, version, darwinSha, darwinUrl] = process.argv.slice(2);
const raw = fs.readFileSync(manifestPath, 'utf8');
const doc = JSON.parse(raw);

if (darwinSha && !/^[0-9a-fA-F]{64}$/.test(darwinSha.trim())) {
    console.error('Error: Invalid SHA-256 digest supplied to update_manifest.sh: ' + darwinSha);
    process.exit(1);
}

if (version) {
    const cleanVersion = version.replace(/^v/i, '');
    doc.version = cleanVersion;
    if (doc.artifacts && typeof doc.artifacts === 'object') {
        for (const key of Object.keys(doc.artifacts)) {
            if (doc.artifacts[key] && typeof doc.artifacts[key].url === 'string') {
                doc.artifacts[key].url = doc.artifacts[key].url.replace(/\/releases\/v[^/]+\//, '/releases/v' + cleanVersion + '/');
            }
        }
    }
}
if (darwinSha && doc.artifacts && doc.artifacts['darwin-arm64']) {
    doc.artifacts['darwin-arm64'].sha256 = darwinSha.trim().toLowerCase();
}
if (darwinUrl && doc.artifacts && doc.artifacts['darwin-arm64']) {
    doc.artifacts['darwin-arm64'].url = darwinUrl;
}

fs.writeFileSync(manifestPath, JSON.stringify(doc, null, 2) + '\n', 'utf8');
console.log('Updated update manifest at ' + manifestPath + ' (version=' + doc.version + ')');
EOF

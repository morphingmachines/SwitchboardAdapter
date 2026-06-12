#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

if ! command -v doxygen &>/dev/null; then
    echo "error: doxygen not found" >&2
    exit 1
fi

doxygen Doxyfile

echo "docs generated: $SCRIPT_DIR/docs/html/index.html"

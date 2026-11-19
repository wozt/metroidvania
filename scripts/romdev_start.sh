#!/bin/sh
# Start the romdev MCP server (pinned romdevtools in tools/romdev).
# The server binds to localhost:7331 only. Idempotent: exits 0 if already up.
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
pkg="$root/tools/romdev"
url="http://127.0.0.1:7331"

if curl -sf --max-time 2 "$url/documentation" >/dev/null 2>&1; then
    echo "romdev already running at $url"
    exit 0
fi

if [ ! -d "$pkg/node_modules/romdevtools" ]; then
    echo "romdevtools not installed; run: cd tools/romdev && npm ci" >&2
    exit 1
fi

mkdir -p "$pkg"
nohup "$pkg/node_modules/.bin/romdev-mcp" >"$pkg/romdev.log" 2>&1 &
echo "romdev starting (pid $!) - log: tools/romdev/romdev.log"

for i in 1 2 3 4 5 6 7 8 9 10; do
    if curl -sf --max-time 2 "$url/documentation" >/dev/null 2>&1; then
        echo "romdev up at $url/mcp"
        exit 0
    fi
    sleep 1
done
echo "romdev failed to start; see tools/romdev/romdev.log" >&2
exit 1

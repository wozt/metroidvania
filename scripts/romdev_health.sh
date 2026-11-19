#!/bin/sh
# Health-check the romdev MCP server: documentation endpoint + MCP initialize.
set -eu

url="${ROMDEV_URL:-http://127.0.0.1:7331}"

if ! curl -sf --max-time 3 "$url/documentation" >/dev/null 2>&1; then
    echo "FAIL: $url/documentation not reachable (is scripts/romdev_start.sh running?)" >&2
    exit 1
fi
echo "OK: $url/documentation"

resp=$(curl -sf --max-time 5 -X POST "$url/mcp" \
    -H 'Content-Type: application/json' \
    -H 'Accept: application/json, text/event-stream' \
    -d '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-03-26","capabilities":{},"clientInfo":{"name":"romdev-health","version":"0"}}}')

if printf '%s' "$resp" | grep -q '"serverInfo"'; then
    echo "OK: $url/mcp MCP initialize"
    printf '%s' "$resp" | grep -o '"version":"[^"]*"' | head -1
    exit 0
fi
echo "FAIL: MCP initialize did not return serverInfo" >&2
printf '%s\n' "$resp" >&2
exit 1

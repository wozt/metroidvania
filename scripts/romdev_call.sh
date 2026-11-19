#!/bin/sh
# One-shot romdev MCP tool call over Streamable HTTP (JSON-RPC).
# Usage: scripts/romdev_call.sh <tool-name> '<json-arguments>'
# Example: scripts/romdev_call.sh memory '{"op":"regions","platform":"gba"}'
# Requires the server running (scripts/romdev_start.sh).
set -eu

url="${ROMDEV_URL:-http://127.0.0.1:7331}/mcp"
tool=${1:?usage: romdev_call.sh <tool> '<json-args>'}
if [ $# -ge 2 ]; then args=$2; else args='{}'; fi

hdrs=$(mktemp)
trap 'rm -f "$hdrs"' EXIT

# initialize: capture Mcp-Session-Id response header
curl -sf --max-time 10 -X POST "$url" \
    -H 'Content-Type: application/json' \
    -H 'Accept: application/json, text/event-stream' \
    -D "$hdrs" \
    -d '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-03-26","capabilities":{},"clientInfo":{"name":"romdev-call","version":"0"}}}' \
    >/dev/null

sid=$(grep -i '^mcp-session-id:' "$hdrs" | tr -d '\r' | awk '{print $2}')
[ -n "$sid" ] || { echo "no MCP session id from $url" >&2; exit 1; }

# initialized notification
curl -sf --max-time 10 -X POST "$url" \
    -H 'Content-Type: application/json' \
    -H 'Accept: application/json, text/event-stream' \
    -H "Mcp-Session-Id: $sid" \
    -d '{"jsonrpc":"2.0","method":"notifications/initialized"}' >/dev/null || true

# tools/call
curl -sf --max-time "${ROMDEV_TIMEOUT:-120}" -X POST "$url" \
    -H 'Content-Type: application/json' \
    -H 'Accept: application/json, text/event-stream' \
    -H "Mcp-Session-Id: $sid" \
    -H 'MCP-Protocol-Version: 2025-03-26' \
    -d "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/call\",\"params\":{\"name\":\"$tool\",\"arguments\":$args}}"

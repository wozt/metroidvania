#!/bin/sh
# Stop the romdev MCP server listening on port 7331.
set -eu

if command -v fuser >/dev/null 2>&1; then
    fuser -k 7331/tcp 2>/dev/null || true
else
    pkill -f 'romdevtools.*server' 2>/dev/null || true
fi
sleep 1
if curl -sf --max-time 2 http://127.0.0.1:7331/documentation >/dev/null 2>&1; then
    echo "romdev still running" >&2
    exit 1
fi
echo "romdev stopped"

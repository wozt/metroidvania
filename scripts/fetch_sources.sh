#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
if [ ! -f "$root/.gitmodules" ]; then
    echo "Error: .gitmodules is missing; run this script from the repository." >&2
    exit 1
fi
git -C "$root" submodule sync --recursive
git -C "$root" submodule update --init --recursive
git -C "$root" submodule status --recursive

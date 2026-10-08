#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
if [ ! -f "$root/.gitmodules" ]; then
    echo "Erreur: .gitmodules absent; exécutez ce script depuis le dépôt." >&2
    exit 1
fi
git -C "$root" submodule sync --recursive
git -C "$root" submodule update --init --recursive
git -C "$root" submodule status --recursive

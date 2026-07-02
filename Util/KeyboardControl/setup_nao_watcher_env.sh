#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
VENV_DIR="${NAO_WATCHER_VENV:-$SCRIPT_DIR/.venv}"
PYTHON_BIN="${PYTHON:-python3}"

"$PYTHON_BIN" -m venv "$VENV_DIR"
"$VENV_DIR/bin/python3" -m pip install --upgrade pip
"$VENV_DIR/bin/python3" -m pip install -r "$SCRIPT_DIR/requirements.txt"

printf 'Watcher env ready: %s\n' "$VENV_DIR"
printf 'Run: %s/watch_nao.sh\n' "$SCRIPT_DIR"

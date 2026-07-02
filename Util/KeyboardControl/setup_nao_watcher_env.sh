#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
VENV_DIR="${NAO_WATCHER_VENV:-$SCRIPT_DIR/.venv}"
PYTHON_BIN="${PYTHON:-python3}"

"$PYTHON_BIN" -m venv --system-site-packages "$VENV_DIR"

if ! "$VENV_DIR/bin/python3" -m pip install -r "$SCRIPT_DIR/requirements.txt"; then
  printf '\nCould not install Python packages with pip.\n' >&2
  printf 'If you are on the robot network, switch to an internet-connected network and rerun this script.\n' >&2
  printf 'Alternatively install system packages and rerun:\n' >&2
  printf '  sudo apt install python3-numpy python3-opencv python3-fastapi python3-uvicorn\n\n' >&2
fi

"$VENV_DIR/bin/python3" - <<'PY'
import importlib.util
import sys

missing = [m for m in ("cv2", "numpy", "fastapi", "uvicorn") if importlib.util.find_spec(m) is None]
if missing:
    print("Watcher env is missing: " + ", ".join(missing), file=sys.stderr)
    print("Install them with pip on an internet network or with apt system packages.", file=sys.stderr)
    sys.exit(1)
PY

printf 'Watcher env ready: %s\n' "$VENV_DIR"
printf 'Run: %s/watch_nao.sh\n' "$SCRIPT_DIR"

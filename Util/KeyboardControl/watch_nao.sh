#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd -- "$SCRIPT_DIR/../.." && pwd)
ENV_FILE="${NAO_WATCHER_ENV_FILE:-$SCRIPT_DIR/watcher.env}"

if [ -f "$ENV_FILE" ]; then
  set -a
  # shellcheck source=/dev/null
  . "$ENV_FILE"
  set +a
fi

VENV_DIR="${NAO_WATCHER_VENV:-$SCRIPT_DIR/.venv}"
PYTHON_BIN="${NAO_WATCHER_PYTHON:-$VENV_DIR/bin/python3}"

if [ ! -x "$PYTHON_BIN" ]; then
  printf 'Python env not found: %s\n' "$PYTHON_BIN" >&2
  printf 'Create it with: %s/setup_nao_watcher_env.sh\n' "$SCRIPT_DIR" >&2
  exit 1
fi

IP="${NAO_WATCHER_IP:-192.168.49.2}"
CAMERA="${NAO_WATCHER_CAMERA:-dual}"
SCALE="${NAO_WATCHER_SCALE:-1}"
KEY="${NAO_WATCHER_SSH_KEY:-$REPO_ROOT/Install/Keys/id_rsa_nao}"

exec "$PYTHON_BIN" "$SCRIPT_DIR/15_watch_nao.py" \
  --ip "$IP" \
  --camera "$CAMERA" \
  --scale "$SCALE" \
  --key "$KEY" \
  "$@"

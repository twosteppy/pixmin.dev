#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MIN_PYTHON_MAJOR=3
MIN_PYTHON_MINOR=10

check_python() {
    local python_bin
    for candidate in python3 python; do
        if command -v "$candidate" &>/dev/null; then
            local ver
            ver=$("$candidate" -c "import sys; print(f'{sys.version_info.major}.{sys.version_info.minor}')" 2>/dev/null)
            local major minor
            IFS='.' read -r major minor <<< "$ver"
            if [ "$major" -ge "$MIN_PYTHON_MAJOR" ] && [ "$minor" -ge "$MIN_PYTHON_MINOR" ]; then
                echo "$candidate"
                return 0
            fi
        fi
    done
    echo ""
}

PYTHON=$(check_python)
if [ -z "$PYTHON" ]; then
    echo "[!] python 3.10+ required. install it first:"
    echo "    ubuntu/debian: sudo apt install python3"
    echo "    mac:           brew install python3"
    exit 1
fi

exec "$PYTHON" "$REPO_ROOT/installer/install.py" "$@"

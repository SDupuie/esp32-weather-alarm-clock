#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FLASH_BACKUP_SIZE_BYTES="0x2000000"

require_idf() {
  if [[ -z "${IDF_PATH:-}" ]]; then
    echo "ERROR: Activate ESP-IDF 5.5.5 first (see docs/build.md)." >&2
    exit 1
  fi
  # Installer-provided shell functions are not inherited by child scripts.
  export PATH="$IDF_PATH/tools:$PATH"
  if ! command -v idf.py >/dev/null 2>&1; then
    echo "ERROR: idf.py was not found in PATH."
    echo "Activate your ESP-IDF environment first, then retry."
    exit 1
  fi
}

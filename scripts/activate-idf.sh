#!/usr/bin/env bash
# Source the environment supplied by the existing ESP-IDF installation.
# This also sets IDF_PATH for the native tests.
if [[ ! -f "$HOME/.espressif/tools/activate_idf_v5.5.5.sh" ]]; then
  echo "ERROR: ESP-IDF 5.5.5 activation script is missing. See docs/build.md." >&2
  return 1 2>/dev/null || exit 1
fi
source "$HOME/.espressif/tools/activate_idf_v5.5.5.sh"

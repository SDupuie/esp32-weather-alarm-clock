#!/usr/bin/env bash
set -euo pipefail

# -----------------------------------------------------------------------------
# Activate ESP-IDF first, as documented in docs/build.md.
# Builds the ESP32-P4 firmware and copies the release artifacts into docs/,
# which GitHub Pages serves as the browser-flashing page
# (https://sdupuie.github.io/esp32-weather-alarm-clock/). Run this after any
# firmware change you want end users to be able to install via that page --
# it is NOT run automatically, so the hosted page only ever has whatever was
# last copied here.
#
# Before a versioned build, align PROJECT_VER in CMakeLists.txt with the
# version in docs/firmware/manifest.json. This script does not choose a
# version or publish the installation page.
# -----------------------------------------------------------------------------

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
DOCS_FIRMWARE_DIR="$PROJECT_DIR/docs/firmware"

echo "Building firmware..."
"$SCRIPT_DIR/build.sh"

mkdir -p "$DOCS_FIRMWARE_DIR"
cp "$PROJECT_DIR/build/bootloader/bootloader.bin" "$DOCS_FIRMWARE_DIR/bootloader.bin"
cp "$PROJECT_DIR/build/partition_table/partition-table.bin" "$DOCS_FIRMWARE_DIR/partition-table.bin"
cp "$PROJECT_DIR/build/round_weather_display_esp32_p4.bin" "$DOCS_FIRMWARE_DIR/app.bin"
cp "$PROJECT_DIR/build/storage.bin" "$DOCS_FIRMWARE_DIR/storage.bin"

echo ""
echo "Copied release artifacts to $DOCS_FIRMWARE_DIR"
echo "Check that the installer manifest and compiled firmware versions match."
echo "The installation page has not been published."

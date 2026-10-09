#!/usr/bin/env bash
set -euo pipefail

# -----------------------------------------------------------------------------
# ESP32-P4 build and flash of the current checkout.
# Requires an activated ESP-IDF environment and a connected board.
# Repository updates are performed separately.
# -----------------------------------------------------------------------------

echo "-------------------------------"
echo "ESP32-P4 Build and Flash"
echo "-------------------------------"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=/dev/null
source "$SCRIPT_DIR/common.sh"
require_idf

# -----------------------------------------------------------------------------
# Set target and build
# -----------------------------------------------------------------------------
"$SCRIPT_DIR/set-target.sh"

echo "Building firmware..."
"$SCRIPT_DIR/build.sh"

# -----------------------------------------------------------------------------
# Detect serial port
# -----------------------------------------------------------------------------
PORT="${WEATHER_ESP32_PORT:-}"
if [[ -z "$PORT" ]]; then
  shopt -s nullglob
  candidates=(/dev/cu.usbmodem* /dev/cu.usbserial* /dev/ttyACM* /dev/ttyUSB*)
  shopt -u nullglob

  if [[ ${#candidates[@]} -eq 1 ]]; then
    PORT="${candidates[0]}"
    echo "Found one board on: $PORT"
  elif [[ ${#candidates[@]} -eq 0 ]]; then
    echo "ERROR: No serial device found."
    echo "Plug in the ESP32-P4 board over USB and re-run this script."
    echo "(Or set WEATHER_ESP32_PORT=/dev/tty... to skip detection.)"
    exit 1
  else
    echo "Multiple serial devices found:"
    select choice in "${candidates[@]}"; do
      if [[ -n "$choice" ]]; then
        PORT="$choice"
        break
      fi
    done
  fi
fi

# -----------------------------------------------------------------------------
# Flash
# -----------------------------------------------------------------------------
echo "Flashing $PORT..."
"$SCRIPT_DIR/flash.sh" "$PORT"

echo "---------------------------------------"
echo " ESP32-P4 setup complete!"
echo "---------------------------------------"
echo "Flashed via: $PORT"
echo "Re-run this script to build and flash the current checkout."
echo ""

read -r -p "Open the serial monitor now? (y/N) [N]: " openMonitor
openMonitor="${openMonitor:-N}"
case "$openMonitor" in
  Y|y) "$SCRIPT_DIR/monitor.sh" "$PORT" ;;
  *) echo "Skipping monitor. Run: targets/esp32-p4/scripts/monitor.sh $PORT" ;;
esac

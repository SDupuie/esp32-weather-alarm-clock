#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
REPO_ROOT="$(cd "$PROJECT_DIR/../.." && pwd)"
PYTHON_BIN="${PYTHON_BIN:-$REPO_ROOT/.venv/bin/python}"

if [[ ! -x "$PYTHON_BIN" ]]; then
  echo "ERROR: expected CairoSVG virtualenv at $PYTHON_BIN"
  exit 1
fi

settings_only=false
if [[ $# -gt 0 ]]; then
  if [[ $# -ne 1 || "$1" != "--settings-only" ]]; then
    echo "Usage: $0 [--settings-only]" >&2
    exit 1
  fi
  settings_only=true
fi

# Settings icons share one approved output. Keep SVG and PNG
# conversion in the existing tools, without another maintained asset input.
"$PYTHON_BIN" - "$PROJECT_DIR" <<'PY'
import sys
import tempfile
from pathlib import Path
import cairosvg

project = Path(sys.argv[1])
sys.path.insert(0, str(project / "tools"))
from render_screen_stage_svg import build_settings_icon
from generate_lvgl_image import decode_png, convert_rows, render_source

sources = []
symbols = []
with tempfile.TemporaryDirectory(prefix="wac-settings-icons-") as temporary:
    for variant, suffix, size in (
        ("keyboard_delete", "keyboard_delete", 24), ("keyboard_shift", "keyboard_shift", 24),
        ("wifi", "wifi", 32), ("wifi_high", "wifi_high", 32),
        ("wifi_low", "wifi_low", 32), ("refresh", "refresh", 32),
        ("chevron_left", "chevron_left", 32), ("chevron_right", "chevron_right", 32),
        ("chevron_up", "chevron_up", 32), ("chevron_down", "chevron_down", 32),
        ("check", "check", 32), ("plus", "plus", 32), ("minus", "minus", 32),
        ("volume_2", "volume_2", 32),
        ("wifi", "wifi_mark", 80), ("wifi_off", "wifi_off_mark", 80),
        ("circle_check", "circle_check_mark", 80),
        ("clock", "clock", 28),
        ("alarm_clock", "alarm_clock", 32), ("alarm_clock_off", "alarm_clock_off", 32),
        ("alarm_clock", "alarm_clock_small", 28), ("alarm_clock_off", "alarm_clock_off_small", 28),
        ("close", "close", 24),
    ):
        symbol = "wac_icon_" + suffix
        png = Path(temporary) / (suffix + ".png")
        cairosvg.svg2png(bytestring=build_settings_icon(variant, size).encode(), write_to=str(png))
        width, height, pixels = decode_png(png)
        values, stride = convert_rows(pixels, "a8", 4)
        sources.append(render_source(symbol, "wac_icons.h", "a8", width, height, stride, values))
        symbols.append(symbol)
(project / "main/assets/wac_icons.c").write_text("\n".join(sources))
(project / "main/assets/wac_icons.h").write_text('#pragma once\n\n#include "lvgl.h"\n\n' + "".join(f"extern const lv_image_dsc_t {symbol};\n" for symbol in symbols))
PY

if $settings_only; then
  echo "Settings icons regenerated."
  exit 0
fi

render_stage_asset() {
  local variant="$1"
  local mode="$2"
  local symbol="$3"
  local svg_path="$PROJECT_DIR/assets-src/${symbol}.svg"
  local png_path="/private/tmp/${symbol}.png"

  "$PYTHON_BIN" "$PROJECT_DIR/tools/render_screen_stage_svg.py" \
    --variant "$variant" \
    --mode "$mode" \
    --output "$svg_path"
  "$PYTHON_BIN" "$PROJECT_DIR/tools/rasterize_svg.py" "$svg_path" "$png_path"
  "$PYTHON_BIN" "$PROJECT_DIR/tools/generate_lvgl_image.py" \
    --input "$png_path" \
    --symbol "$symbol" \
    --format rgb565 \
    --dither \
    --output-c "$PROJECT_DIR/main/assets/${symbol}.c" \
    --output-h "$PROJECT_DIR/main/assets/${symbol}.h"
}

render_alpha_asset() {
  local variant="$1"
  local mode="$2"
  local symbol="$3"
  local svg_path="$PROJECT_DIR/assets-src/${symbol}.svg"
  local png_path="/private/tmp/${symbol}.png"

  "$PYTHON_BIN" "$PROJECT_DIR/tools/render_screen_stage_svg.py" \
    --variant "$variant" \
    --mode "$mode" \
    --output "$svg_path"
  "$PYTHON_BIN" "$PROJECT_DIR/tools/rasterize_svg.py" "$svg_path" "$png_path"
  "$PYTHON_BIN" "$PROJECT_DIR/tools/generate_lvgl_image.py" \
    --input "$png_path" \
    --symbol "$symbol" \
    --format argb8888 \
    --output-c "$PROJECT_DIR/main/assets/${symbol}.c" \
    --output-h "$PROJECT_DIR/main/assets/${symbol}.h"
}

render_stage_asset digital_stage day digital_stage_day
render_stage_asset digital_stage night digital_stage_night
render_stage_asset round_stage day round_stage_day
render_stage_asset round_stage night round_stage_night
render_alpha_asset forecast_frame day forecast_tomorrow_frame_day
render_alpha_asset forecast_frame night forecast_tomorrow_frame_night

echo "Screen parity assets regenerated."

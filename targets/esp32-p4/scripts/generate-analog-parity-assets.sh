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

hands_only=false
if [[ $# -gt 0 ]]; then
  if [[ $# -ne 1 || "$1" != "--hands-only" ]]; then
    echo "Usage: $0 [--hands-only]" >&2
    exit 1
  fi
  hands_only=true
fi

PNG_DIR="$(mktemp -d "${TMPDIR:-/tmp}/wac-analog-assets.XXXXXX")"
trap 'rm -rf "$PNG_DIR"' EXIT

render_stage_asset() {
  local mode="$1"
  local symbol="$2"
  local svg_path="$PROJECT_DIR/assets-src/analog/${symbol}.svg"
  local png_path="$PNG_DIR/${symbol}.png"

  "$PYTHON_BIN" "$PROJECT_DIR/tools/render_analog_stage_svg.py" \
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
  local svg_name="$1"
  local symbol="$2"
  local format="${3:-argb8888}"
  local svg_path="$PROJECT_DIR/assets-src/analog/${svg_name}.svg"
  local png_path="$PNG_DIR/${svg_name}.png"

  "$PYTHON_BIN" "$PROJECT_DIR/tools/rasterize_svg.py" "$svg_path" "$png_path"
  "$PYTHON_BIN" "$PROJECT_DIR/tools/generate_lvgl_image.py" \
    --input "$png_path" \
    --symbol "$symbol" \
    --format "$format" \
    --output-c "$PROJECT_DIR/main/assets/${symbol}.c" \
    --output-h "$PROJECT_DIR/main/assets/${symbol}.h"
}

render_edge_indicator_asset() {
  local symbol="$1"
  local png_path="$PNG_DIR/${symbol}.png"

  # The angle-based gradient, radial mask and blur are rendered procedurally.
  # See render_edge_indicator.py for the retained effect's calculations.
  "$PYTHON_BIN" "$PROJECT_DIR/tools/render_edge_indicator.py" --output "$png_path"
  "$PYTHON_BIN" "$PROJECT_DIR/tools/generate_lvgl_image.py" \
    --input "$png_path" \
    --symbol "$symbol" \
    --format argb8888 \
    --output-c "$PROJECT_DIR/main/assets/${symbol}.c" \
    --output-h "$PROJECT_DIR/main/assets/${symbol}.h"
}

if ! $hands_only; then
  render_stage_asset day analog_stage_day
  render_stage_asset night analog_stage_night
fi
render_alpha_asset hour_hand analog_hour_hand rgb565a8
render_alpha_asset minute_hand analog_minute_hand rgb565a8
render_alpha_asset second_hand analog_second_hand rgb565a8
if $hands_only; then
  echo "Analog hands regenerated."
  exit 0
fi
render_alpha_asset center_cap analog_center_cap
render_edge_indicator_asset analog_edge_indicator

echo "Analog parity assets regenerated."

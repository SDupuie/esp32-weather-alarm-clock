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
weather_only=false
if [[ $# -gt 0 ]]; then
  if [[ $# -ne 1 || ( "$1" != "--settings-only" && "$1" != "--weather-only" ) ]]; then
    echo "Usage: $0 [--settings-only | --weather-only]" >&2
    exit 1
  fi
  if [[ "$1" == "--weather-only" ]]; then weather_only=true; else settings_only=true; fi
fi

# Utility and weather icons share one approved output. Keep SVG and PNG
# conversion in the existing tools, without another maintained asset input.
"$PYTHON_BIN" - "$PROJECT_DIR" "$weather_only" <<'PY'
import sys
import tempfile
from pathlib import Path
import cairosvg

project = Path(sys.argv[1])
sys.path.insert(0, str(project / "tools"))
from render_screen_stage_svg import build_settings_icon, build_weather_icon
from generate_lvgl_image import decode_png, convert_rows, crop_transparent_margin, render_header, render_source

weather_only = sys.argv[2] == "true"
asset_dir = project / "main/assets"
# A focused run replaces only weather blocks; utility and storm bytes stay exact.
existing_sources = (asset_dir / "wac_icons.c").read_text()
sources = []
symbols = []
weather_sources = {}
geometry = []

def render_weather(symbol, icon_name, svg, png, header):
    cairosvg.svg2png(bytestring=svg, write_to=str(png), output_width=400, output_height=400)
    width, height, full = decode_png(png)
    if (width, height) != (400, 400):
        raise ValueError(f"{symbol}: unexpected logical canvas")
    pixels, crop_x, crop_y = crop_transparent_margin(full, 6)
    height, width = len(pixels), len(pixels[0])
    values, stride = convert_rows(pixels, "rgb565a8", 4)
    geometry.append((symbol, icon_name, crop_x, crop_y))
    print(f"{symbol}: canvas=400 crop={crop_x},{crop_y},{width},{height} "
          f"color_stride={stride} alpha_stride={stride // 2} bytes={len(values)}")
    return render_source(symbol, header, "rgb565a8", width, height, stride, values)

with tempfile.TemporaryDirectory(prefix="wac-settings-icons-") as temporary:
    for variant, suffix, size in (
        ("keyboard_delete", "keyboard_delete", 24), ("keyboard_shift", "keyboard_shift", 24),
        ("wifi", "wifi", 32), ("wifi_high", "wifi_high", 32),
        ("wifi_low", "wifi_low", 32), ("refresh", "refresh", 32),
        ("chevron_left", "chevron_left", 32), ("chevron_right", "chevron_right", 32),
        ("chevron_up", "chevron_up", 32), ("chevron_down", "chevron_down", 32),
        ("check", "check", 32), ("plus", "plus", 32), ("minus", "minus", 32),
        ("volume_2", "volume_2", 32), ("volume_off", "volume_off", 32),
        ("play", "play", 32), ("square", "square", 32),
        ("wifi", "wifi_mark", 80), ("wifi_off", "wifi_off_mark", 80),
        ("circle_check", "circle_check_mark", 80),
        ("clock", "clock", 28),
        ("alarm_clock", "alarm_clock", 32), ("alarm_clock_off", "alarm_clock_off", 32),
        ("alarm_clock", "alarm_clock_small", 28), ("alarm_clock_off", "alarm_clock_off_small", 28),
        ("close", "close", 24),
    ):
        if weather_only:
            continue
        symbol = "wac_icon_" + suffix
        png = Path(temporary) / (suffix + ".png")
        cairosvg.svg2png(bytestring=build_settings_icon(variant, size).encode(), write_to=str(png))
        width, height, pixels = decode_png(png)
        values, stride = convert_rows(pixels, "a8", 4)
        sources.append(render_source(symbol, "wac_icons.h", "a8", width, height, stride, values))
        symbols.append(symbol)

    for variant, suffix in (
        ("freezing_rain", "freezing_rain"),
        ("hail", "hail"),
        ("ice", "ice"),
        ("wind", "wind"),
        ("tornado", "tornado"),
    ):
        symbol = "wac_weather_" + suffix
        png = Path(temporary) / (suffix + ".png")
        source = render_weather(symbol, suffix.replace("_", "-"),
                                build_weather_icon(variant).encode(), png, "wac_icons.h")
        sources.append(source)
        symbols.append(symbol)
        weather_sources[symbol] = source

    for stem in ("clear-day", "clear-night", "cloudy", "fog", "partlycloudy-day",
                 "partlycloudy-night", "rain", "showers-day", "showers-night", "sleet",
                 "snow", "thunderstorm", "thundersnow"):
        symbol = "weather_icon_" + stem.replace("-", "_")
        source = render_weather(symbol, "rain-snow" if stem == "sleet" else stem,
                                (project.parent.parent / "shared/assets/icons" / (stem + ".svg")).read_bytes(),
                                Path(temporary) / (stem + ".png"), symbol + ".h")
        (asset_dir / (symbol + ".c")).write_text(source)
        (asset_dir / (symbol + ".h")).write_text(render_header(symbol))

    if not weather_only:
        from render_edge_indicator import render as render_edge_indicator
        # Both clock faces use this storm glow; it is not a separate weather page.
        symbol = "weather_edge_indicator"
        png = Path(temporary) / (symbol + ".png")
        render_edge_indicator(png, variant="weather")
        width, height, pixels = decode_png(png)
        values, stride = convert_rows(pixels, "a8", 4)
        sources.append(render_source(symbol, "wac_icons.h", "a8", width, height, stride, values))
        symbols.append(symbol)

if weather_only:
    blocks = existing_sources.split('#include "lvgl.h"')
    replaced = set()
    for index, block in enumerate(blocks):
        for symbol, source in weather_sources.items():
            if f"uint8_t {symbol}_map[]" in block:
                blocks[index] = source.removeprefix('#include "lvgl.h"').rstrip("\n") + ("\n\n" if index < len(blocks) - 1 else "\n")
                replaced.add(symbol)
                break
    if replaced != weather_sources.keys():
        raise ValueError("Shared icon output is missing a weather asset block")
    (asset_dir / "wac_icons.c").write_text('#include "lvgl.h"'.join(blocks))
else:
    (asset_dir / "wac_icons.c").write_text("\n".join(sources))
    (asset_dir / "wac_icons.h").write_text('#pragma once\n\n#include "lvgl.h"\n\n' + "".join(f"extern const lv_image_dsc_t {symbol};\n" for symbol in symbols))

includes = [f'#include "{symbol}.h"' for symbol, _, _, _ in geometry if symbol.startswith("weather_icon_")]
rows = [f'  {{ "{name}", &{symbol}, {{ 400, {x}, {y} }} }},' for symbol, name, x, y in geometry]
lookup_source = '''#include "weather_icons.h"

__INCLUDES__
#include "weather_icon_names.h"
#include "wac_icons.h"

#include <string.h>

/* Generated by generate-screen-parity-assets.sh from the original vectors. */
static const struct {
  const char *name;
  const lv_image_dsc_t *image;
  weather_icon_geometry_t geometry;
} weather_icons[] = {
__ROWS__
};

const lv_image_dsc_t *weather_icon_image_for_name(const char *icon_name)
{
  const char *resolved_name = weather_icon_name_or_fallback(icon_name);
  for (size_t i = 0; i < sizeof(weather_icons) / sizeof(weather_icons[0]); ++i) {
    if (strcmp(resolved_name, weather_icons[i].name) == 0) return weather_icons[i].image;
  }
  return NULL;
}

const lv_image_dsc_t *weather_icon_image_for_snapshot(const app_weather_snapshot_t *weather)
{
  return weather && weather->valid ? weather_icon_image_for_name(weather->icon_name) : NULL;
}

const weather_icon_geometry_t *weather_icon_geometry_for_image(const lv_image_dsc_t *image)
{
  for (size_t i = 0; i < sizeof(weather_icons) / sizeof(weather_icons[0]); ++i) {
    if (image == weather_icons[i].image) return &weather_icons[i].geometry;
  }
  return NULL;
}
'''
(asset_dir / "weather_icons.c").write_text(lookup_source.replace("__INCLUDES__", "\n".join(includes)).replace("__ROWS__", "\n".join(rows)))

PY

if $settings_only || $weather_only; then
  echo "Icon assets regenerated."
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

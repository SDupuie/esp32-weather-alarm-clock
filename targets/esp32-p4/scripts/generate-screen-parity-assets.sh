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
"$PYTHON_BIN" - "$PROJECT_DIR" "$weather_only" "$settings_only" <<'PY'
import sys
import tempfile
import math
from pathlib import Path
import cairosvg

project = Path(sys.argv[1])
sys.path.insert(0, str(project / "tools"))
from render_screen_stage_svg import build_settings_icon, build_weather_icon
from generate_lvgl_image import decode_png, convert_rows, crop_transparent_margin, render_header, render_source

weather_only = sys.argv[2] == "true"
settings_only = sys.argv[3] == "true"
asset_dir = project / "main/assets"
# Focused runs preserve the other artwork and its original crop metadata.
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
        ("volume_2", "volume_2", 32),
        ("volume_2", "volume_2_40", 40), ("volume_off", "volume_off_40", 40),
        ("settings_2", "settings_2_40", 40),
        ("triangle_outline", "triangle_outline_40", 40),
        ("triangle_alert", "triangle_alert_32", 32),
        ("play", "play", 32), ("square", "square", 32),
        ("wifi", "wifi_mark", 80), ("wifi_off", "wifi_off_mark", 80),
        ("circle_check", "circle_check_mark", 80),
        ("clock", "clock", 28),
        ("alarm_clock", "alarm_clock", 32), ("alarm_clock_off", "alarm_clock_off", 32),
        ("alarm_clock", "alarm_clock_small", 28),
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

    # Precompose the subtle Radar glow before RGB565 display conversion. Using
    # an alpha overlay here would repeatedly round the very dark surface colors.
    if not weather_only:
        for symbol, background, accent in (
            ("wac_radar_unavailable_glow", 0x07101c, 0x93d1ff),
            ("wac_radar_unavailable_glow_night", 0x18090c, 0xe1b6b6),
        ):
            values = []
            for y in range(64):
                for x in range(64):
                    distance = math.hypot(
                        ((x + .5) * 12.5 - 400) / (400 * math.sqrt(2) * .68),
                        ((y + .5) * 12.5 - 344) / (456 * math.sqrt(2) * .68))
                    opacity = .07 * max(0, 1 - distance)
                    values.extend(round(((background >> shift) & 255) * (1 - opacity)
                                        + ((accent >> shift) & 255) * opacity)
                                  for shift in (0, 8, 16))
            sources.append(render_source(symbol, "wac_icons.h", "rgb888", 64, 64, 192, values))
            symbols.append(symbol)

    for variant, suffix in (
        ("freezing_rain", "freezing_rain"),
        ("hail", "hail"),
        ("ice", "ice"),
        ("wind", "wind"),
        ("tornado", "tornado"),
    ):
        if settings_only:
            continue
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
        if settings_only:
            continue
        symbol = "weather_icon_" + stem.replace("-", "_")
        source = render_weather(symbol, "rain-snow" if stem == "sleet" else stem,
                                (project.parent.parent / "shared/assets/icons" / (stem + ".svg")).read_bytes(),
                                Path(temporary) / (stem + ".png"), symbol + ".h")
        (asset_dir / (symbol + ".c")).write_text(source)
        (asset_dir / (symbol + ".h")).write_text(render_header(symbol))


if settings_only:
    utility_sources = dict(zip(symbols, sources))
    blocks = existing_sources.split('#include "lvgl.h"')
    for index, block in enumerate(blocks):
        for symbol in tuple(utility_sources):
            if f"uint8_t {symbol}_map[]" in block:
                blocks[index] = utility_sources.pop(symbol).removeprefix('#include "lvgl.h"').rstrip("\n") + ("\n\n" if index < len(blocks) - 1 else "\n")
                break
    if utility_sources:
        insert_at = next(index for index, block in enumerate(blocks)
                         if "uint8_t wac_weather_" in block)
        additions = [source.removeprefix('#include "lvgl.h"').rstrip("\n") + "\n\n"
                     for source in utility_sources.values()]
        blocks[insert_at:insert_at] = additions
    (asset_dir / "wac_icons.c").write_text('#include "lvgl.h"'.join(blocks).rstrip() + "\n")
    header = (asset_dir / "wac_icons.h").read_text()
    for symbol in symbols:
        declaration = f"extern const lv_image_dsc_t {symbol};\n"
        if declaration not in header:
            header += declaration
    (asset_dir / "wac_icons.h").write_text(header)
elif weather_only:
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
    (asset_dir / "wac_icons.c").write_text('#include "lvgl.h"'.join(blocks).rstrip() + "\n")
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

const weather_icon_geometry_t *weather_icon_geometry_for_image(const lv_image_dsc_t *image)
{
  for (size_t i = 0; i < sizeof(weather_icons) / sizeof(weather_icons[0]); ++i) {
    if (image == weather_icons[i].image) return &weather_icons[i].geometry;
  }
  return NULL;
}
'''
if not settings_only:
    (asset_dir / "weather_icons.c").write_text(lookup_source.replace("__INCLUDES__", "\n".join(includes)).replace("__ROWS__", "\n".join(rows)))

PY

if $settings_only || $weather_only; then
  echo "Icon assets regenerated."
  exit 0
fi

PNG_DIR="$(mktemp -d "${TMPDIR:-/tmp}/wac-screen-assets.XXXXXX")"
trap 'rm -rf "$PNG_DIR"' EXIT

render_stage_asset() {
  local variant="$1"
  local mode="$2"
  local symbol="$3"
  local svg_path="$PROJECT_DIR/assets-src/${symbol}.svg"
  local png_path="$PNG_DIR/${symbol}.png"

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
  local png_path="$PNG_DIR/${symbol}.png"

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

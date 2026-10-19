#!/usr/bin/env bash
set -euo pipefail

# -----------------------------------------------------------------------------
# Generates the custom LVGL fonts used for settings, clock time and temperature
# labels, at their true rendered pixel size, from the same Montserrat-Medium
# source LVGL's own built-in fonts use (managed_components/lvgl__lvgl/
# scripts/built_in_font/Montserrat-Medium.ttf) -- so they match the rest of
# the UI's typeface exactly.
#
# These labels used to render at lv_font_montserrat_48 (LVGL's largest
# built-in size) and get stretched 2-3x larger via a style transform_scale.
# Upscaling an already-rasterized bitmap glyph like that blurs it -- this is
# what produced the "low resolution / fuzzy" look reported on real hardware.
# Generating a font at the actual on-screen size (with anti-aliasing computed
# natively at that size) and dropping the transform_scale fixes it.
#
# Each font's glyph range is restricted to just the characters that label
# ever displays (digits, plus ':'/'-' for time or '-'/'°' for temperature),
# not the full ASCII range LVGL's own built-ins cover -- keeping each font
# small despite the much larger pixel size.
#
# Requires network access the first time (downloads lv_font_conv via npx).
# -----------------------------------------------------------------------------

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
OUTPUT_DIR="$PROJECT_DIR/main/assets"
FONT_TTF="$PROJECT_DIR/managed_components/lvgl__lvgl/scripts/built_in_font/Montserrat-Medium.ttf"
LV_FONT_CONV="lv_font_conv@1.5.3"

if [[ ! -f "$FONT_TTF" ]]; then
  echo "ERROR: Montserrat-Medium.ttf not found at $FONT_TTF"
  echo "Run 'idf.py build' at least once first so managed_components/ is populated."
  exit 1
fi

# Generate UI supplements without touching the large clock/weather fonts.
# Default invocation continues to regenerate all custom fonts.
settings_only=false
if [[ $# -gt 0 ]]; then
  if [[ $# -ne 1 || "$1" != "--settings-only" ]]; then
    echo "Usage: $0 [--settings-only]" >&2
    exit 1
  fi
  settings_only=true
fi

mkdir -p "$OUTPUT_DIR"

generate_font() {
  local symbol="$1"
  local size="$2"
  local range="$3"
  local stem="${4:-$symbol}"
  local fallback="${5:-}"
  local destination="${6:-$OUTPUT_DIR}"
  local extra_args=()
  if [[ -n "$fallback" ]]; then
    extra_args+=(--lv-font-name "$symbol" --lv-fallback "$fallback")
  elif [[ "$symbol" != "$stem" ]]; then
    extra_args+=(--lv-font-name "$symbol")
  fi

  echo "Generating ${symbol} (${size}px)..."
  npx -y "$LV_FONT_CONV" \
    --bpp 4 \
    --size "$size" \
    --font "$FONT_TTF" \
    -r "$range" \
    --format lvgl \
    --no-compress \
    --no-prefilter \
    --force-fast-kern-format \
    ${extra_args[@]+"${extra_args[@]}"} \
    -o "$destination/${stem}.c"

  # lv_font_conv emits an LV_LVGL_H_INCLUDE_SIMPLE-gated include that falls
  # back to "lvgl/lvgl.h" -- a path this project's include layout doesn't
  # have (every other asset here just does #include "lvgl.h" directly, per
  # main/assets/weather_icon_clear_day.c). Collapse it to match.
  python3 - "$destination/${stem}.c" "$fallback" "$PROJECT_DIR" <<'PYEOF'
import re
import sys
path = sys.argv[1]
with open(path) as f:
    content = f.read()
old = '#ifdef LV_LVGL_H_INCLUDE_SIMPLE\n#include "lvgl.h"\n#else\n#include "lvgl/lvgl.h"\n#endif'
new = '#include "lvgl.h"'
if old not in content:
    sys.exit(f"ERROR: expected include guard not found in {path} -- lv_font_conv's output format may have changed")
content = content.replace(old, new)
if sys.argv[2]:
    # Sparse punctuation needs the fallback's complete line metrics. Its glyph
    # offsets remain the converter's offsets relative to the font baseline.
    fallback_path = f"{sys.argv[3]}/managed_components/lvgl__lvgl/src/font/{sys.argv[2]}.c"
    with open(fallback_path) as f:
        fallback_content = f.read()
    for field in ("line_height", "base_line", "underline_position", "underline_thickness"):
        match = re.search(rf"\.{field}\s*=\s*(-?\d+)", fallback_content)
        if not match:
            sys.exit(f"ERROR: expected {field} in {fallback_path}")
        content, count = re.subn(rf"(\.{field}\s*=\s*)-?\d+", rf"\g<1>{match[1]}", content)
        if count != 1:
            sys.exit(f"ERROR: expected one {field} in {path}")
with open(path, "w") as f:
    f.write(content)
PYEOF

  cat > "$destination/${stem}.h" <<EOF
#pragma once

#include "lvgl.h"

extern const lv_font_t ${symbol};
EOF
}

# Keep the supplements in the one approved settings-font source. Converter
# fragments are temporary tool outputs, removed even if generation fails.
settings_tmp_root="${TMPDIR:-/tmp}"
settings_tmp="$(mktemp -d "${settings_tmp_root%/}/wac-settings-fonts.XXXXXX")"
trap 'rm -rf "$settings_tmp"' EXIT
# UI captions use these additional characters; ordinary text falls back
# to the matching bundled font. Add only punctuation used by current views.
generate_font "wac_font_settings_24" 24 "0xB7,0x2013,0x2019" "wac_fonts" "lv_font_montserrat_24" "$settings_tmp"
generate_font "wac_font_settings_26" 26 "0xB7,0x2019,0x2026" "wac_fonts_26" "lv_font_montserrat_26" "$settings_tmp"
generate_font "wac_font_settings_34" 34 "0x2019,0x2026" "wac_fonts_34" "lv_font_montserrat_34" "$settings_tmp"
generate_font "wac_font_settings_28" 28 "0xB7,0x2013,0x2026" "wac_fonts_28" "lv_font_montserrat_28" "$settings_tmp"
# Time picker and unsigned solar-offset magnitude use their true pixel sizes.
generate_font "wac_font_settings_104" 104 "0x30-0x39,0x3A" "wac_fonts_104" "" "$settings_tmp"
generate_font "wac_font_settings_96" 96 "0x2D,0x30-0x39,0x3A" "wac_fonts_96" "" "$settings_tmp"
generate_font "wac_font_weather_18" 18 "0xB7" "wac_fonts_18" "lv_font_montserrat_18" "$settings_tmp"
generate_font "wac_font_weather_32" 32 "0x2026" "wac_fonts_32" "lv_font_montserrat_32" "$settings_tmp"
python3 - "$settings_tmp" "$OUTPUT_DIR" <<'PYEOF'
import pathlib
import re
import sys
scratch, output = map(pathlib.Path, sys.argv[1:])
fragments = []
for size, stem in ((24, "wac_fonts"), (26, "wac_fonts_26"), (34, "wac_fonts_34"), (28, "wac_fonts_28"), (104, "wac_fonts_104"), (96, "wac_fonts_96"), (32, "wac_fonts_32"), (18, "wac_fonts_18")):
    content = (scratch / f"{stem}.c").read_text()
    # Record the canonical output path, not the random temporary directory.
    content = content.replace(str(scratch / f"{stem}.c"), str(output / "wac_fonts.c"))
    if size in (104, 96):
        # The mockup's numeric labels use line-height:1.13. Both its embedded
        # Montserrat and this TTF have ascent968/descent251 per1000 units:
        # baseline from top = (1.13 + .968 - .251)/2 * size. Retain the
        # converter's glyph offsets/bitmaps, but not its tight digit-only box.
        line_height = round(size * 1.13)
        base_line = line_height - round(size * .9235)
        for field, value in (("line_height", line_height), ("base_line", base_line)):
            content, count = re.subn(rf"(\.{field}\s*=\s*)-?\d+", rf"\g<1>{value}", content)
            if count != 1:
                sys.exit(f"ERROR: expected one {field} in {stem}")
    if size != 24:
        # Converter internals have generic names. Give each supplement private
        # names so the supplements share this single compilation unit.
        names = set(re.findall(r"^static[^\n;=]*?\b(\w+)\s*(?:\[[^\n]*?\])?\s*[=;]", content, re.M))
        if not {"glyph_bitmap", "glyph_dsc", "cmaps", "font_dsc"}.issubset(names):
            sys.exit("ERROR: unexpected converter static declarations")
        for name in sorted(names, key=len, reverse=True):
            content = re.sub(rf"(?<!\.)\b{re.escape(name)}\b", f"settings_{size}_{name}", content)
    fragments.append(content)
(output / "wac_fonts.c").write_text("\n".join(fragments).rstrip("\n") + "\n")
(output / "wac_fonts.h").write_text(
    '#pragma once\n\n#include "lvgl.h"\n\n' +
    ''.join(f'extern const lv_font_t wac_font_settings_{size};\n' for size in (24, 26, 34, 28, 104, 96)) +
    'extern const lv_font_t wac_font_weather_32;\nextern const lv_font_t wac_font_weather_18;\n')
PYEOF
rm -rf "$settings_tmp"
trap - EXIT
if $settings_only; then
  echo "Done. UI font supplements generated in $OUTPUT_DIR."
  exit 0
fi

# Digits + colon + hyphen -- live time ("10:42") and unavailable time ("--:--").
generate_font "lv_font_clock_time_144" 144 "0x2D,0x30-0x39,0x3A"

# Digits + minus + degree sign -- temperature can go below zero.
generate_font "lv_font_temp_102" 102 "0x2D,0x30-0x39,0xB0"

echo "Done. Regenerated fonts are in $OUTPUT_DIR -- rebuild the firmware to pick them up."

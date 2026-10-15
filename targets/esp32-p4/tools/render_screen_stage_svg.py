#!/usr/bin/env python3

from __future__ import annotations

import argparse
from pathlib import Path

STAGE_SIZE = 800
CENTER = STAGE_SIZE // 2
# See render_analog_stage_svg.py for why this is baked into the asset rather
# than applied as a runtime lv_obj_align offset in ui/ui.c.
STAGE_Y_TRIM = -18
FACE_CY = CENTER + STAGE_Y_TRIM
FACE_RADIUS = 390
FORECAST_FRAME_SIZE = 500
FORECAST_FRAME_CENTER = FORECAST_FRAME_SIZE // 2
WEATHER_ICON_SIZE = 250
WEATHER_ICON_VARIANTS = (
    "freezing_rain",
    "hail",
    "ice",
    "wind",
    "tornado",
)

PALETTE_DAY = {
    "stage_start": "rgba(39, 53, 79, 0.55)",
    "stage_mid": "rgba(8, 12, 18, 0.96)",
    "stage_end": "#010203",
    "round_face_start": "rgba(60, 79, 111, 0.28)",
    "round_face_mid": "rgba(19, 26, 38, 0.80)",
    "round_face_inner": "rgba(10, 14, 22, 0.96)",
    "round_face_end": "rgba(2, 4, 8, 0.98)",
    "digital_face_start": "rgba(37, 74, 134, 0.28)",
    "digital_face_mid": "rgba(20, 39, 72, 0.40)",
    "digital_face_inner": "rgba(10, 20, 35, 0.86)",
    "digital_face_end": "rgba(4, 10, 18, 0.98)",
    "edge": "rgba(255, 255, 255, 0.08)",
    "inner_edge": "rgba(255, 255, 255, 0.04)",
    "glow": "rgba(103, 139, 255, 0.08)",
    "digital_ring": "rgba(89, 190, 255, 0.36)",
    "digital_ring_glow": "rgba(49, 146, 230, 0.16)",
    "digital_ring_inner": "rgba(49, 146, 230, 0.08)",
    "forecast_disc_start": "rgba(98, 141, 214, 0.22)",
    "forecast_disc_mid": "rgba(53, 78, 126, 0.16)",
    "forecast_disc_end": "rgba(14, 22, 39, 0.12)",
    "forecast_ring_outer": "rgba(214, 226, 240, 0.28)",
    "forecast_ring_inner": "rgba(196, 210, 226, 0.22)",
}

PALETTE_NIGHT = {
    "stage_start": "rgba(36, 10, 10, 0.72)",
    "stage_mid": "rgba(10, 4, 4, 0.98)",
    "stage_end": "#010101",
    "round_face_start": "rgba(82, 30, 30, 0.18)",
    "round_face_mid": "rgba(27, 11, 11, 0.78)",
    "round_face_inner": "rgba(11, 6, 6, 0.96)",
    "round_face_end": "rgba(3, 1, 1, 0.99)",
    "digital_face_start": "rgba(86, 34, 34, 0.18)",
    "digital_face_mid": "rgba(38, 18, 18, 0.28)",
    "digital_face_inner": "rgba(15, 8, 8, 0.90)",
    "digital_face_end": "rgba(5, 3, 3, 0.99)",
    "edge": "rgba(179, 94, 94, 0.18)",
    "inner_edge": "rgba(255, 255, 255, 0.04)",
    "glow": "rgba(110, 28, 28, 0.08)",
    "digital_ring": "rgba(165, 86, 86, 0.22)",
    "digital_ring_glow": "rgba(140, 52, 52, 0.12)",
    "digital_ring_inner": "rgba(140, 52, 52, 0.08)",
    "forecast_disc_start": "rgba(98, 50, 50, 0.18)",
    "forecast_disc_mid": "rgba(52, 24, 24, 0.16)",
    "forecast_disc_end": "rgba(14, 8, 8, 0.12)",
    "forecast_ring_outer": "rgba(186, 122, 122, 0.22)",
    "forecast_ring_inner": "rgba(163, 100, 100, 0.20)",
}


def stage_background_svg(palette: dict[str, str], inner: str) -> str:
    return f"""<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="{STAGE_SIZE}" height="{STAGE_SIZE}" viewBox="0 0 {STAGE_SIZE} {STAGE_SIZE}">
  <defs>
    <radialGradient id="stage-bg" cx="50%" cy="50%" r="60%">
      <stop offset="0%" stop-color="{palette["stage_start"]}"/>
      <stop offset="58%" stop-color="{palette["stage_mid"]}"/>
      <stop offset="100%" stop-color="{palette["stage_end"]}"/>
    </radialGradient>
    <filter id="face-shadow" x="-12%" y="-12%" width="124%" height="136%">
      <feOffset dy="20" result="offset"/>
      <feGaussianBlur in="offset" stdDeviation="24" result="blur"/>
      <feColorMatrix
        in="blur"
        type="matrix"
        values="0 0 0 0 0
                0 0 0 0 0
                0 0 0 0 0
                0 0 0 0.45 0"
        result="shadow"/>
      <feMerge>
        <feMergeNode in="shadow"/>
        <feMergeNode in="SourceGraphic"/>
      </feMerge>
    </filter>
  </defs>

  <rect width="{STAGE_SIZE}" height="{STAGE_SIZE}" fill="url(#stage-bg)"/>
{inner}
</svg>
"""


def build_round_stage(mode: str) -> str:
    palette = PALETTE_NIGHT if mode == "night" else PALETTE_DAY
    inner = f"""
  <defs>
    <radialGradient id="face-fill" cx="50%" cy="46%" r="54%">
      <stop offset="0%" stop-color="{palette["round_face_start"]}"/>
      <stop offset="38%" stop-color="{palette["round_face_mid"]}"/>
      <stop offset="72%" stop-color="{palette["round_face_inner"]}"/>
      <stop offset="100%" stop-color="{palette["round_face_end"]}"/>
    </radialGradient>
    <radialGradient id="face-glow" cx="50%" cy="46%" r="54%">
      <stop offset="0%" stop-color="{palette["glow"]}"/>
      <stop offset="44%" stop-color="rgba(0, 0, 0, 0)"/>
      <stop offset="100%" stop-color="rgba(0, 0, 0, 0)"/>
    </radialGradient>
  </defs>

  <g filter="url(#face-shadow)">
    <circle cx="{CENTER}" cy="{FACE_CY}" r="{FACE_RADIUS}" fill="url(#face-fill)"/>
    <circle cx="{CENTER}" cy="{FACE_CY}" r="{FACE_RADIUS}" fill="url(#face-glow)"/>
    <circle cx="{CENTER}" cy="{FACE_CY}" r="{FACE_RADIUS - 1}" fill="none" stroke="{palette["edge"]}" stroke-width="2"/>
    <circle cx="{CENTER}" cy="{FACE_CY}" r="{FACE_RADIUS - 18}" fill="none" stroke="{palette["inner_edge"]}" stroke-width="1"/>
  </g>
"""
    return stage_background_svg(palette, inner)


def build_digital_stage(mode: str) -> str:
    palette = PALETTE_NIGHT if mode == "night" else PALETTE_DAY
    inner = f"""
  <defs>
    <radialGradient id="digital-face-fill" cx="50%" cy="34%" r="56%">
      <stop offset="0%" stop-color="{palette["digital_face_start"]}"/>
      <stop offset="24%" stop-color="{palette["digital_face_mid"]}"/>
      <stop offset="60%" stop-color="{palette["digital_face_inner"]}"/>
      <stop offset="100%" stop-color="{palette["digital_face_end"]}"/>
    </radialGradient>
  </defs>

  <g filter="url(#face-shadow)">
    <circle cx="{CENTER}" cy="{FACE_CY}" r="{FACE_RADIUS}" fill="url(#digital-face-fill)"/>
    <circle cx="{CENTER}" cy="{FACE_CY}" r="{FACE_RADIUS - 1}" fill="none" stroke="{palette["edge"]}" stroke-width="2"/>
    <circle cx="{CENTER}" cy="{FACE_CY}" r="{FACE_RADIUS - 18}" fill="none" stroke="{palette["inner_edge"]}" stroke-width="1"/>
    <circle cx="{CENTER}" cy="{FACE_CY}" r="{FACE_RADIUS - 32}" fill="none" stroke="{palette["digital_ring"]}" stroke-width="2"/>
    <circle cx="{CENTER}" cy="{FACE_CY}" r="{FACE_RADIUS - 32}" fill="none" stroke="{palette["digital_ring_glow"]}" stroke-width="18" opacity="0.32"/>
    <circle cx="{CENTER}" cy="{FACE_CY}" r="{FACE_RADIUS - 32}" fill="none" stroke="{palette["digital_ring_inner"]}" stroke-width="36" opacity="0.12"/>
  </g>
"""
    return stage_background_svg(palette, inner)


def build_forecast_frame(mode: str) -> str:
    palette = PALETTE_NIGHT if mode == "night" else PALETTE_DAY
    return f"""<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="{FORECAST_FRAME_SIZE}" height="{FORECAST_FRAME_SIZE}" viewBox="0 0 {FORECAST_FRAME_SIZE} {FORECAST_FRAME_SIZE}">
  <defs>
    <radialGradient id="forecast-disc-gradient" cx="46%" cy="34%" r="72%">
      <stop offset="0%" stop-color="{palette["forecast_disc_start"]}"/>
      <stop offset="52%" stop-color="{palette["forecast_disc_mid"]}"/>
      <stop offset="100%" stop-color="{palette["forecast_disc_end"]}"/>
    </radialGradient>
  </defs>
  <circle cx="{FORECAST_FRAME_CENTER}" cy="{FORECAST_FRAME_CENTER}" r="232" fill="url(#forecast-disc-gradient)"/>
  <circle cx="{FORECAST_FRAME_CENTER}" cy="{FORECAST_FRAME_CENTER}" r="248" fill="none" stroke="{palette["forecast_ring_outer"]}" stroke-width="3"/>
  <circle cx="{FORECAST_FRAME_CENTER}" cy="{FORECAST_FRAME_CENTER}" r="234" fill="none" stroke="{palette["forecast_ring_inner"]}" stroke-width="2"/>
</svg>
"""


def build_settings_icon(variant: str, size: int = 24) -> str:
    # Exact paths from the mockup's pinned Lucide 1.17.0. Keyboard utility
    # marks use integer 24px lines; scaled 32px marks retain SVG antialiasing.
    paths = {
        "chevron_left": ("m15 18-6-6 6-6",),
        "chevron_right": ("m9 18 6-6-6-6",),
        "chevron_up": ("m18 15-6-6-6 6",),
        "chevron_down": ("m6 9 6 6 6-6",),
        "check": ("M20 6 9 17l-5-5",),
        "plus": ("M12 5v14", "M5 12h14"),
        "minus": ("M5 12h14",),
        "volume_2": (
            "M11 4.702a.705.705 0 0 0-1.203-.498L6.413 7.587A1.4 1.4 0 0 1 5.416 8H3a1 1 0 0 0-1 1v6a1 1 0 0 0 1 1h2.416a1.4 1.4 0 0 1 .997.413l3.383 3.384A.705.705 0 0 0 11 19.298z",
            "M16 9a5 5 0 0 1 0 6", "M19.364 18.364a9 9 0 0 0 0-12.728",
        ),
        "volume_off": ("M16 9a5 5 0 0 1 .95 2.293", "M19.364 5.636a9 9 0 0 1 1.889 9.96", "m2 2 20 20", "m7 7-.587.587A1.4 1.4 0 0 1 5.416 8H3a1 1 0 0 0-1 1v6a1 1 0 0 0 1 1h2.416a1.4 1.4 0 0 1 .997.413l3.383 3.384A.705.705 0 0 0 11 19.298V11", "M9.828 4.172A.686.686 0 0 1 11 4.657v.686"),
        "play": ("M5 5a2 2 0 0 1 3.008-1.728l11.997 6.998a2 2 0 0 1 .003 3.458l-12 7A2 2 0 0 1 5 19z",),
        "square": ("M5 3h14a2 2 0 0 1 2 2v14a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2z",),
        "keyboard_delete": (
            "M10 5a2 2 0 0 0-1.344.519l-6.328 5.74a1 1 0 0 0 0 1.481l6.328 5.741A2 2 0 0 0 10 19h10a2 2 0 0 0 2-2V7a2 2 0 0 0-2-2z",
            "m12 9 6 6", "m18 9-6 6",
        ),
        "keyboard_shift": (
            "M9 19a1 1 0 0 0 1 1h4a1 1 0 0 0 1-1v-6a1 1 0 0 1 1-1h3.293a.707.707 0 0 0 .5-1.207l-7.086-7.086a1 1 0 0 0-1.414 0l-7.086 7.086a.707.707 0 0 0 .5 1.207H8a1 1 0 0 1 1 1z",
        ),
        "wifi": (
            "M12 20h.01", "M2 8.82a15 15 0 0 1 20 0",
            "M5 12.859a10 10 0 0 1 14 0", "M8.5 16.429a5 5 0 0 1 7 0",
        ),
        "wifi_high": (
            "M12 20h.01", "M5 12.859a10 10 0 0 1 14 0",
            "M8.5 16.429a5 5 0 0 1 7 0",
        ),
        "wifi_low": ("M12 20h.01", "M8.5 16.429a5 5 0 0 1 7 0"),
        "wifi_off": (
            "M12 20h.01", "M8.5 16.429a5 5 0 0 1 7 0",
            "M5 12.859a10 10 0 0 1 5.17-2.69", "M19 12.859a10 10 0 0 0-2.007-1.523",
            "M2 8.82a15 15 0 0 1 4.177-2.643", "M22 8.82a15 15 0 0 0-11.288-3.764",
            "m2 2 20 20",
        ),
        "circle_check": ("m9 12 2 2 4-4",),
        "clock": ("M12 6v6l4 2",),
        "alarm_clock": (
            "M12 9v4l2 2", "M5 3 2 6", "m22 6-3-3",
            "M6.38 18.7 4 21", "M17.64 18.67 20 21",
        ),
        "alarm_clock_off": (
            "M6.87 6.87a8 8 0 1 0 11.26 11.26",
            "M19.9 14.25a8 8 0 0 0-9.15-9.15", "m22 6-3-3",
            "M6.26 18.67 4 21", "m2 2 20 20", "M4 4 2 6",
        ),
        "close": ("M18 6 6 18", "m6 6 12 12"),
        "refresh": (
            "M3 12a9 9 0 0 1 9-9 9.75 9.75 0 0 1 6.74 2.74L21 8",
            "M21 3v5h-5", "M21 12a9 9 0 0 1-9 9 9.75 9.75 0 0 1-6.74-2.74L3 16",
            "M8 16H3v5",
        ),
    }
    inner = "".join(f'<path d="{path}"/>' for path in paths[variant])
    if variant in ("circle_check", "clock"):
        inner = '<circle cx="12" cy="12" r="10"/>' + inner
    elif variant == "alarm_clock":
        inner = '<circle cx="12" cy="13" r="8"/>' + inner
    return f'<svg xmlns="http://www.w3.org/2000/svg" width="{size}" height="{size}" viewBox="0 0 24 24" fill="none" stroke="white" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">{inner}</svg>'


def build_weather_icon(variant: str) -> str:
    """Return approved weather artwork that is absent from the existing icon set."""
    if variant not in WEATHER_ICON_VARIANTS:
        raise ValueError(f"Unsupported weather icon: {variant}")

    shared = f'width="{WEATHER_ICON_SIZE}" height="{WEATHER_ICON_SIZE}" viewBox="0 0 250 250"'
    cloud = (
        '<path fill="#bfc3c9" transform="translate(125 70) scale(.85 .75) translate(-125 -70)" '
        'd="M77 130c-25 0-43-18-43-42 0-22 17-40 39-41 C81 25 101 12 124 12c28 0 49 20 51 48 '
        '23-3 42 13 44 36 3 19-11 34-31 34z"/>'
    )

    if variant == "freezing_rain":
        icicles = (
            '<path fill="#5590cc" d="M77 108h16l-8 31-17 28 4-27zm43 0h15l-7 46-8 14-7-14zm44 0h16l15 31 4 27-19-28z"/>'
        )
        inner = cloud + icicles
    elif variant == "hail":
        inner = cloud + ''.join(
            f'<circle cx="{x}" cy="{y}" r="11" fill="#fff"/>'
            for x, y in ((75, 166), (125, 183), (175, 166))
        )
    elif variant == "ice":
        inner = '''
  <g fill="none" stroke="#fff" stroke-linecap="round" stroke-linejoin="round" stroke-width="14">
    <path d="M125 60v130M68 92l114 66M68 158l114-66"/>
  </g>'''
    elif variant == "wind":
        inner = '''
  <g fill="none" stroke="#fff" stroke-linecap="round" stroke-linejoin="round" stroke-width="13">
    <path d="M75 87h69c12 0 17-9 11-16-5-6-14-5-17 2"/>
    <path d="M55 125h140"/>
    <path d="M71 163h74c12 0 17 10 11 17-5 6-14 5-17-2"/>
  </g>'''
    else:  # tornado
        inner = '''
  <g fill="#aeb4bc" transform="translate(125 125) scale(.75 .8) translate(-125 -125)">
    <path d="M29 44c57 15 135 15 192 0l-12 24c-48 12-120 12-168 0z"/>
    <path d="M48 91c43 13 111 13 154 0l-12 23c-36 10-94 10-130 0z"/>
    <path d="M70 137c31 11 79 11 110 0l-13 23c-24 8-60 8-84 0z"/>
    <path d="M94 183c17 7 45 7 62 0l-13 21c-10 5-26 5-36 0z"/>
  </g>'''

    return f'<svg xmlns="http://www.w3.org/2000/svg" {shared}>{inner}</svg>'


def build_svg(variant: str, mode: str) -> str:
    if variant == "round_stage":
        return build_round_stage(mode)
    if variant == "digital_stage":
        return build_digital_stage(mode)
    if variant == "forecast_frame":
        return build_forecast_frame(mode)
    if variant in WEATHER_ICON_VARIANTS:
        return build_weather_icon(variant)
    raise ValueError(f"Unsupported variant: {variant}")


def main() -> int:
    parser = argparse.ArgumentParser(description="Render non-analog screen SVG assets.")
    parser.add_argument("--variant", choices=("round_stage", "digital_stage", "forecast_frame", *WEATHER_ICON_VARIANTS), required=True)
    parser.add_argument("--mode", choices=("day", "night"), default="day")
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(build_svg(args.variant, args.mode), encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Procedurally renders the house-messages edge-glow indicator.

The effect combines angle-based color and opacity, a radial alpha mask, and
a soft blur. This script computes the per-pixel values because the current
cairosvg rasterization pipeline does not render that gradient, mask, and
filter combination. The ``message`` variant is red and the ``weather``
variant is an amber mask tinted by the firmware at runtime.
"""

from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np
from PIL import Image

from rasterize_svg import apply_gaussian_blur_rgba

STAGE_SIZE = 800
# Matches the live face/hand coordinate system in ui/ui.c (ANALOG_CENTER_X/Y,
# ANALOG_FACE_SIZE=760/2=380) -- NOT the baked stage image's Y-trim, since this
# is a separate runtime lv_image object aligned directly on the 800x800 canvas.
FACE_CENTER = STAGE_SIZE / 2  # 400
FACE_BOX_RADIUS = 380  # ANALOG_FACE_SIZE / 2
MASK_BOX_RADIUS = FACE_BOX_RADIUS - 10  # 10px inside the face edge
BLUR_RADIUS_PX = 6  # Gaussian blur radius for the base state

GRADIENT_FROM_DEG = 218.0
# (angle_deg, alpha) stops, measured clockwise from GRADIENT_FROM_DEG.
ANGLE_STOPS = [
    (0.0, 0.0),
    (2.0, 0.0),
    (10.0, 0.08),
    (30.0, 0.28),
    (54.0, 0.52),
    (78.0, 0.34),
    (94.0, 0.12),
    (106.0, 0.0),
    (360.0, 0.0),
]
COLOR_RGB = (255, 74, 74)

# Weather variant: same mask and blur with a different angle profile. The
# firmware stores this one as A8 and tints it amber at runtime.
WEATHER_GRADIENT_FROM_DEG = 36.0
WEATHER_ANGLE_STOPS = [
    (0.0, 0.0),
    (2.0, 0.0),
    (12.0, 0.1),
    (28.0, 0.34),
    (52.0, 0.62),
    (76.0, 0.34),
    (92.0, 0.1),
    (102.0, 0.0),
    (360.0, 0.0),
]
WEATHER_COLOR_RGB = (255, 176, 64)

VARIANTS = {
    "message": (GRADIENT_FROM_DEG, ANGLE_STOPS, COLOR_RGB),
    "weather": (WEATHER_GRADIENT_FROM_DEG, WEATHER_ANGLE_STOPS, WEATHER_COLOR_RGB),
}

# (pixels inward from the mask radius, alpha) stops, converted below to
# absolute radii from the center.
_MASK_STOPS_FROM_EDGE = [
    (132, 0.0),
    (108, 0.12),
    (80, 0.42),
    (46, 0.82),
    (14, 1.0),
    (2, 1.0),
    (0, 0.0),
]
RADIUS_STOPS = sorted(
    ((MASK_BOX_RADIUS - offset, alpha) for offset, alpha in _MASK_STOPS_FROM_EDGE),
    key=lambda pair: pair[0],
)


def _piecewise_interp(x: np.ndarray, stops: list[tuple[float, float]]) -> np.ndarray:
    out = np.zeros_like(x)
    out[x <= stops[0][0]] = stops[0][1]
    out[x >= stops[-1][0]] = stops[-1][1]
    for (x0, y0), (x1, y1) in zip(stops[:-1], stops[1:]):
        mask = (x >= x0) & (x < x1)
        span = (x1 - x0) or 1.0
        t = (x[mask] - x0) / span
        out[mask] = y0 + t * (y1 - y0)
    return out


def render_alpha(size: int, from_deg: float, angle_stops: list[tuple[float, float]]) -> np.ndarray:
    ys, xs = np.mgrid[0:size, 0:size].astype(np.float64)
    dx = xs - FACE_CENTER
    dy = ys - FACE_CENTER
    radius = np.sqrt(dx * dx + dy * dy)

    # Angle convention: 0 degrees points up and positive angles go clockwise.
    angle_deg = np.degrees(np.arctan2(dx, -dy)) % 360.0
    rel_deg = (angle_deg - from_deg) % 360.0

    angle_alpha = _piecewise_interp(rel_deg, angle_stops)
    radius_alpha = _piecewise_interp(radius, RADIUS_STOPS)
    # Clip the radial alpha outside the mask radius.
    radius_alpha = np.where(radius <= MASK_BOX_RADIUS, radius_alpha, 0.0)

    return angle_alpha * radius_alpha


def render(output_path: Path, size: int = STAGE_SIZE, variant: str = "message") -> None:
    from_deg, angle_stops, color_rgb = VARIANTS[variant]
    alpha = render_alpha(size, from_deg, angle_stops)

    rgba = np.zeros((size, size, 4), dtype=np.uint8)
    rgba[..., 0] = color_rgb[0]
    rgba[..., 1] = color_rgb[1]
    rgba[..., 2] = color_rgb[2]
    rgba[..., 3] = np.clip(alpha * 255.0, 0, 255).astype(np.uint8)

    image = Image.fromarray(rgba, mode="RGBA")
    if BLUR_RADIUS_PX > 0:
        # Blur over the full canvas first (so the kernel has real context at
        # the edges of the visible glow, not an artificial crop boundary),
        # THEN crop down to the bounding box below. Cropping before blurring
        # would risk edge artifacts for no size benefit, since the crop
        # happens right after anyway.
        image = apply_gaussian_blur_rgba(image, BLUR_RADIUS_PX)

    # Only ~12% of the full 800x800 canvas is ever non-transparent (this is
    # a glow confined to one side of the face). Storing the full canvas as
    # ARGB8888 costs 2.56MB of flash for a background that's ~88% blank --
    # on top of the other analog assets, that was enough to push this
    # firmware's total flash-mapped (DROM/IROM) content past the ~16MB
    # window the ESP32-P4's bootloader can map, which silently truncated the
    # image on flash (confirmed on-device: bootloader read real segment data
    # up to ~16.55MB in, then found zeros where the next segment header
    # should have been -- reproduced identically across a full chip erase +
    # reflash, ruling out a bad/worn flash sector). Cropping to the actual
    # content bounding box (plus a small margin) avoids storing dead
    # transparent space. ui/ui.c's ANALOG_EDGE_INDICATOR_X/Y must match
    # this crop's offset -- this prints it so that's easy to keep in sync.
    alpha_channel = np.asarray(image)[..., 3]
    nonzero_rows, nonzero_cols = np.nonzero(alpha_channel > 0)
    margin = 4
    min_x = max(0, int(nonzero_cols.min()) - margin)
    max_x = min(size, int(nonzero_cols.max()) + 1 + margin)
    min_y = max(0, int(nonzero_rows.min()) - margin)
    max_y = min(size, int(nonzero_rows.max()) + 1 + margin)

    image = image.crop((min_x, min_y, max_x, max_y))
    image.save(output_path)
    print(f"edge indicator crop: x={min_x} y={min_y} size={max_x - min_x}x{max_y - min_y}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, help="Output PNG path")
    parser.add_argument("--size", type=int, default=STAGE_SIZE)
    parser.add_argument("--variant", choices=sorted(VARIANTS), default="message")
    args = parser.parse_args()

    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    render(output_path, args.size, args.variant)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

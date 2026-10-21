# Six-frame Radar test data

This versioned dataset supplies the optional six-frame Radar check in the [test instructions](../../README.md). It contains six consecutive ten-minute Florida frames captured on October 19, 2026, covering 75 miles at 800×800 pixels.

- `provider-metadata.json`: the captured RainViewer timeline response.
- `frames/manifest.json`: location, sampling geometry, frame identities, source URLs and tile sizes/checksums. Its `file` entries are relative to this folder.
- `frames/`: twelve original 512×512 smoothed precipitation tiles and the 800×800 USGS background.
- `expected-frame-0.png` through `expected-frame-5.png`: composed reference images, in chronological order.

The check compares every output pixel, exercises single-pass playback and verifies that the newest image is held without further requests. It makes no live requests and does not write test outputs into this folder. These fixed samples do not establish physical appearance, board timing or audio continuity. Six ten-minute samples have fifty minutes between their first and last frames.

Radar overlays are from RainViewer; the map background is USGS USGSTopo. See the project's [source credits](../../../README.md#weather-location-and-map-sources).

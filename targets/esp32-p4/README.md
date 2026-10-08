# ESP32-P4 Target

This target is the native firmware path for the Waveshare `ESP32-P4-WIFI6-Touch-LCD-3.4C` board family.

It is intentionally separate from the Raspberry Pi runtime, but it follows the same shared product contract in:

- `../../shared/spec/product-spec.md`

For this fork's enhancements, read the [project instructions](../../AGENTS.md), [development policy](../../docs/wac/development.md), and [architecture and contracts](../../docs/wac/architecture.md). Those contracts govern the new alarm, backup-time, Night Shift, and replacement UI work; the scope below describes the existing firmware.

## Current Scope

This target includes:

- A real ESP-IDF project layout, with WiFi via ESP-Hosted over SDIO to a companion ESP32-C6
- Analog, digital, forecast, and message screens with Pi-matched layout/behavior
- Live weather fetch (Open-Meteo) with icon/condition mapping
- On-device WiFi provisioning (network scan + on-screen keyboard) and location setup/geocoding, driven by a touch UI
- House messaging: the full JSON API (`/api/messages`, `/api/message-targets`, `/api/message-runtime`, ack/deactivate/sync) plus UDP hub discovery/election for shared mode across multiple clocks, and a served composer webpage at `http://<device-ip>/` (or `/messages`) so you can send a message from any browser on the LAN, the same way you would from the Pi's `/messages` page
- NVS-backed device config with boot-count persistence
- A `scripts/setup.sh` one-command install/update flow (see below)

Known gap:

- No wireless OTA firmware updates yet -- updating means re-running `scripts/setup.sh` with the board plugged in over USB

## Setup / Update

From the repo root, with the board connected over USB:

```bash
targets/esp32-p4/scripts/setup.sh
```

This is both the first-time setup path and the update path. It's safe to re-run any time: it pulls the latest source (skipping the pull if you have uncommitted local changes, so nothing gets clobbered), installs ESP-IDF `v5.5.5` if it isn't present yet (one-time, several GB), builds, detects the board's serial port (prompting you to pick if more than one is attached), and flashes. It'll also ask if you want to open the serial monitor afterward.

Choosing hardware target `2` in the root `setup.sh` delegates straight to this script.

## Manual build/flash (for iterative dev work)

Run these commands from the repository root. First activate the project's pinned ESP-IDF **v5.5.5** environment. With the VS Code extension, use its ESP-IDF terminal for the selected installation. With ESP-IDF Installation Manager, use the activation script supplied for that installation. On the current Mac it is:

```bash
source "$HOME/.espressif/tools/activate_idf_v5.5.5.sh"
```

For an installation made by the repository's setup script, use its existing activation helper instead:

```bash
source targets/esp32-p4/scripts/activate-idf.sh
```

Confirm the active version with `idf.py --version`. The active `IDF_PATH` should point to the selected framework directory. The repository activation helper expects a repository-local installation; use the Installation Manager environment for an Installation Manager installation.

For an unconfigured checkout, select the chip once. Changing the target regenerates configuration and clears its build directory, so this is not a routine pre-build step:

```bash
targets/esp32-p4/scripts/set-target.sh
```

Build the firmware and inspect memory usage:

```bash
targets/esp32-p4/scripts/build.sh
idf.py -C targets/esp32-p4 size
```

Run the existing C weather tests on the computer, using the active installation's cJSON source:

```bash
npm run test:esp32-parity
```

See the [test instructions](tests/README.md) for when other existing checks apply and what needs hardware verification. Building and testing do not flash the board. When flashing is intended, select the connected board's actual port:

```bash
targets/esp32-p4/scripts/flash.sh /dev/tty.usbmodemXXXX
targets/esp32-p4/scripts/monitor.sh /dev/tty.usbmodemXXXX
```

## Verified Baseline

- Waveshare recommends `ESP-IDF` for the `ESP32-P4-WIFI6-Touch-LCD-XC` series.
- Waveshare documents the `3.4C` as the default `800x800` round display variant in the `XC` BSP.
- Waveshare recommends `ESP-IDF v5.5.0` or newer for the `ESP32-P4-WIFI6-Touch-LCD` line; this project is pinned to `v5.5.5`.

Sources:

- <https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-XC>
- <https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-X/Development-Environment-Setup-IDF>
- <https://components.espressif.com/components/waveshare/esp32_p4_wifi6_touch_lcd_xc/versions/3.0.0/readme>

## Project Layout

- `CMakeLists.txt`
  Top-level ESP-IDF project file
- `sdkconfig.defaults`
  Default project settings for the 3.4C display variant
- `main/idf_component.yml`
  Managed component dependencies
- `main/main.c`
  Firmware entrypoint
- `main/app_ui.*`
  Native LVGL UI: analog/digital/forecast/message screens and on-device setup flow
- `main/connectivity.*`
  WiFi station connect, scanning, and time sync
- `main/device_config.*`
  NVS-backed config (WiFi, location, room/device id, message sharing mode, display prefs)
- `main/location_lookup.*`
  On-device Open-Meteo geocoding for the setup UI
- `main/message_service.*`
  House messaging JSON API, UDP hub discovery/election, and the served composer page
- `main/web/messages.html`
  The composer page served by `message_service.c`, embedded into the firmware image
- `main/weather_client.*`
  Open-Meteo forecast fetch
- `main/bsp_shims.h`
  Small forward declarations for BSP backlight helpers documented upstream
- `scripts/`
  Shell helpers: `setup.sh` (install/update/build/flash), plus `activate-idf.sh`, `set-target.sh`, `build.sh`, `flash.sh`, `monitor.sh` for manual use

## Notes

- The project is set up to use the Waveshare `XC` BSP with the `3.4C` round display as the intended default.
- Screen layout constants are derived from the Pi reference (`targets/pi/public/style.css` and `targets/pi/public/renderer/clock.js`) to keep both targets visually matched.

# ESP32-P4 Target

This target is the native firmware path for the Waveshare `ESP32-P4-WIFI6-Touch-LCD-3.4C` board family.

The [architecture and product specification](../../docs/wac/architecture.md) defines approved behavior, defaults, the visual reference and source ownership. This checkout contains baseline firmware; the planned enhancements are not implemented yet.

## Prepare the environment (no board needed)

Use the existing ESP-IDF **5.5.5** installation outside the repository. On this workstation it is at `$HOME/.espressif/v5.5.5/esp-idf`. The installed environment script sets `IDF_PATH`, the Python environment and tool paths together:

```bash
source targets/esp32-p4/scripts/activate-idf.sh
idf.py --version
```

The helper sources `$HOME/.espressif/tools/activate_idf_v5.5.5.sh`, supplied by Espressif's installation manager. On another workstation, install version 5.5.5 with Espressif's installer and source its supplied activation script. Build scripts use the activated environment; native tests read cJSON from `$IDF_PATH/components/json/cJSON`. Do not copy the toolchain into the repository.

## Build without a board

Run from the repository root after activation:

```bash
targets/esp32-p4/scripts/build.sh
```

The checked-in configuration selects ESP32-P4. Use `scripts/set-target.sh` only when intentionally resetting the target configuration. See [test instructions](tests/README.md) for native checks.

## Flash with a board

After activation and building:

```bash
targets/esp32-p4/scripts/flash.sh /dev/tty.usbmodemXXXX
targets/esp32-p4/scripts/monitor.sh /dev/tty.usbmodemXXXX
```

Alternatively, after activating the environment above, run `targets/esp32-p4/scripts/setup.sh` to reset the target configuration, build, detect the connected board, and flash it. It optionally opens the monitor. It does not install a toolchain or update repository source. Root `setup.sh` hardware choice `2` delegates to this script and requires the same prior activation.

## Prepare browser-installation files

After activating the environment above, run:

```bash
targets/esp32-p4/scripts/build-release.sh
```

This builds through the normal build helper and copies the three firmware images into `docs/firmware/`. It does not publish the website or flash a board.

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
  Shell helpers: `setup.sh` (build/flash), plus `activate-idf.sh`, `set-target.sh`, `build.sh`, `flash.sh`, `monitor.sh` for manual use

## Notes

- The project is set up to use the Waveshare `XC` BSP with the `3.4C` round display as the intended default.

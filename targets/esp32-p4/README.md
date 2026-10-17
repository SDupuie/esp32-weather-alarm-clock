# ESP32-P4 Target

This target is the native firmware path for the Waveshare `ESP32-P4-WIFI6-Touch-LCD-3.4C` board family.

The [architecture and product specification](../../docs/architecture.md) defines approved behavior, defaults, the visual reference and source ownership. The root [README](../../README.md) covers hardware, setup and using the clock. Development status, completed work, test and flash records, and remaining verification are kept in [docs/work.md](../../docs/work.md).

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

The checked-in configuration uses the BSP's returned-error option so speaker setup failures can be reported without its optional whole-device abort. This also affects checked display/touch/bus/backlight failures; it is not a promise that every board failure is recoverable. Healthy paths and the standard touch/display setup are preserved.

The application uses the board's 32 MB flash, including addresses above 16 MiB.
The configuration enables ESP-IDF's extended cache-address option and its
experimental-feature gate; setting the flash size alone does not enable those
reads. Keep the matching bootloader and application together when flashing.
Board verification and chip-specific limits belong in the [work record](../../docs/work.md).

The build also generates `storage.bin` from the nine prepared Sound Machine MP3s
in `shared/assets/sounds/clock/`. Ordinary USB flashing includes that image in
the existing storage partition; it does not erase NVS settings. Source recordings
are not packaged. Playback uses the managed `esp_audio_codec` dependency pinned
to 2.5.0 for the development board's revision 1.3; newer releases require newer
P4 silicon. Its package retains its own license and notices.

The checked-in configuration selects ESP32-P4. From the repository root, use `targets/esp32-p4/scripts/set-target.sh` only when intentionally resetting the target configuration. See [test instructions](tests/README.md) for native checks.

Artwork generators update existing outputs. When adding or retiring a generated source, update `main/CMakeLists.txt` under the [source-list rules](../../docs/architecture.md#existing-code-changes-that-matter); there is no separate generated build manifest.

## Flash with a board

Run these commands only when flashing is authorized, after activation and building:

```bash
targets/esp32-p4/scripts/flash.sh /dev/tty.usbmodemXXXX
targets/esp32-p4/scripts/monitor.sh /dev/tty.usbmodemXXXX
```

Alternatively, after activating the environment above, run `targets/esp32-p4/scripts/setup.sh` to reset the target configuration, build, detect the connected board, and flash it. It optionally opens the monitor. It does not install a toolchain or update repository source.

For a deliberate whole-device backup before flashing, run `targets/esp32-p4/scripts/backup-device.sh <serial-port> [output-dir]` from the repository root. It defaults to the ignored `backups/<timestamp>/` directory under this target. Keep backups local. This is a manual hardware tool, separate from application settings saves.

For a diagnostic serial capture, run `targets/esp32-p4/scripts/capture-serial-log.sh <serial-port> <output-path> [duration-seconds]` from the repository root. It captures for the requested duration (150 seconds by default). A successful capture means the log was collected; it does not mean the firmware passed a benchmark or hardware test.

That wrapper requests a restart before capture. To preserve an already frozen state, use `python targets/esp32-p4/tools/capture_serial.py --port <serial-port> --output <output-path> --duration 150` from the repository root, without `--reset-before-capture`; do not reset or flash before collecting the stopped-state evidence.

## Prepare browser-installation files

After activating the environment above, run:

```bash
targets/esp32-p4/scripts/build-release.sh
```

This builds through the normal build helper and copies four firmware images into
`docs/firmware/`: bootloader, partition table, application and `storage.bin` with
the nine Sound Machine recordings. The installer manifest includes all four at
the current partition offsets. It does not publish the website or flash a board.

For a versioned release, update the version in `docs/firmware/manifest.json` manually. The helper does not choose or change it.

## Verified Baseline

- Waveshare recommends `ESP-IDF` for the `ESP32-P4-WIFI6-Touch-LCD-XC` series.
- Waveshare documents the `3.4C` as the default `800x800` round display variant in the `XC` BSP.
- Waveshare recommends `ESP-IDF v5.5.0` or newer for the `ESP32-P4-WIFI6-Touch-LCD` line; this project is pinned to `v5.5.5`.

Sources:

- <https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-XC>
- <https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-X/Development-Environment-Setup-IDF>
- <https://components.espressif.com/components/waveshare/esp32_p4_wifi6_touch_lcd_xc/versions/3.0.0/readme>

## Project Layout

One `main` component holds the application, grouped by function. UI files use `main/ui/`; assets use `main/assets/`. The [approved source tree](../../docs/architecture.md#planned-file-structure) also identifies optional files that are created only when needed.

- `CMakeLists.txt`
  Top-level ESP-IDF project file
- `sdkconfig.defaults`
  Default project settings for the 3.4C display variant
- `main/idf_component.yml`
  Managed component dependencies
- `main/main.c`
  Firmware entrypoint
- `main/app_ui.h` and `main/ui/`
  Public UI interface, retained clock/weather/message rendering and settings draft editors
- `main/ui/clock_views.c`
  Private clock, unavailable-time, status and backup-notice drawing objects; `ui.c` retains navigation and notice actions
- `main/ui/weather_views.c`
  Private Forecast, Radar, message and weather-status drawing objects; retained Conditions renderer without a device route. `ui.c` retains navigation, display snapshots and message-dismissal decisions
- `main/connectivity.*`
  WiFi station connect, scanning, and time sync
- `main/device_config.*`
  Applied settings, validation and typed NVS saves
- `main/clock_time.*`
  Time source/trust, system timezone application, RTC correction and in-memory warnings
- `main/alarm.*`
  Live alarm decisions, snooze/dismissal/missed-notice recovery and the separate operating record
- `main/audio.*`
  Alarm sounds and previews, Sound Machine playback, recording decoding and speaker ownership
- `main/alarm_schedule.*`
  Stateless recurring-occurrence and daylight-saving calculations using the applied system timezone
- `main/ds3231.*`
  DS3231 register access using the existing board I2C bus; UTC dates from 2000 through 2099
- `main/night.*`
  Off/Always On/Fixed Schedule/Sunset to Sunrise appearance, private adjacent-day solar cache, copied Settings times, remembered startup fallback and gradual configured backlight changes
- `main/solar_times.*`
  Pure local sunrise/sunset calculation returning complete UTC event dates or unavailable results
- `main/location_lookup.*`
  On-device Open-Meteo geocoding for the setup UI
- `main/message_service.*`
  House messaging JSON API, UDP hub discovery/election, and the served composer page
- `main/web/messages.html`
  The composer page served by `message_service.c`, embedded into the firmware image
- `main/weather_client.*`
  Open-Meteo/Visual Crossing forecasts and serialized RainViewer/USGS Radar preparation, using the bundled LVGL PNG decoder and RAM image buffers
- `main/bsp_shims.h`
  Small forward declarations for BSP backlight helpers documented upstream
- `tests/`
  Native settings, time, connectivity and UI checks, plus weather/location and message checks
- `scripts/`
  Shell helpers: `setup.sh` (build/flash), plus `activate-idf.sh`, `set-target.sh`, `build.sh`, `flash.sh`, `monitor.sh` for manual use

## Notes

- The project is set up to use the Waveshare `XC` BSP with the `3.4C` round display as the intended default.

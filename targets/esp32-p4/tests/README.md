# ESP32-P4 native checks

These checks compile the actual firmware logic on the development computer.
They do not establish touchscreen behavior, real network traffic, physical
power-cut outcomes, RTC hardware, speaker sound or backlight behavior.

## Weather and location

Activate ESP-IDF 5.5.5 as described in the [target guide](../README.md#prepare-the-environment-no-board-needed), then run from the repository root:

```bash
targets/esp32-p4/scripts/test-weather-parity.sh
```

The retained script name runs C forecast, location and conditions checks against
fixtures in `shared/test-data/`. It also checks that provider search names retain
state/country qualifiers before the provider's result limit. No Raspberry Pi or
JavaScript suite is required. cJSON is read from `$IDF_PATH/components/json/cJSON`.

If the selected macOS compiler is broken, use the installed Command Line Tools
compiler for this run:

```bash
CC=/Library/Developer/CommandLineTools/usr/bin/clang targets/esp32-p4/scripts/test-weather-parity.sh
```

## Settings, timezone and Save controls

```bash
cmake -S targets/esp32-p4/tests/wac -B /tmp/wac-native-tests
cmake --build /tmp/wac-native-tests
ctest --test-dir /tmp/wac-native-tests --output-on-failure
```

Managed dependencies must already be available from a normal firmware build.
The UI target uses the existing managed LVGL library with a native configuration.

The settings tests compile `main/device_config.c` with typed NVS substitutes and
ordinary thread locks. They cover defaults, validation, editor changes merged
into current settings, full selected-group saves, failure/corrected-retry reads,
missing keys, returned storage errors, accepted partial saves and readers during
a blocked save. The substitute deliberately permits stored keys
to change before commit; this is a test example of allowed partial outcomes,
not evidence of a physical power-cut result. Time checks exercise the existing
timezone mapping, including winter/summer local offsets; RTC behavior is not yet
implemented or tested.

The settings-control target compiles production `wac/ui/settings.c` with real LVGL
objects and events. It checks nested Done/Cancel, shared Wi-Fi drafts, deferred
Save, duplicate blocking, validation, save failure/retry, resolved-location retry
without Wi-Fi, immediate enable, stale completion handling, and character entry
and editing through actual keyboard events. Network work and
tasks are substitutes, and navigation callbacks from `ui.c` are substituted;
full-shell first-use routing is reviewed from source rather than executed here.
The UI target also checks the actual generated clock font for live-time and
unavailable-time characters through LVGL glyph lookup.

## Message acknowledgement

With `IDF_PATH` set as above:

```bash
python3 targets/esp32-p4/tests/test_message_ack.py
```

This compiles the actual acknowledgement and record-lookup functions with native
network and task-lock substitutes. It covers displayed-message identity, shared
requests, and missing, inactive, expired and differently targeted messages.

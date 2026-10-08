# Weather-logic parity tests

The ESP32-P4 firmware can't run JavaScript, so the shared business logic in
`shared/logic/` (forecast selection, geocoding scoring, and storm conditions)
is hand-ported to C:

- `shared/logic/forecast-representative.js` &harr; `main/forecast_representative.c`
- `shared/logic/open-meteo-location.js` &harr; `main/location_scoring.c`
- `shared/logic/storm-conditions.js` &harr; `main/storm_conditions.c`

These C ports are deliberately free of ESP-IDF dependencies (only `cJSON` +
the C standard library), so they compile natively on a dev machine. This
directory tests them against the *exact same* JSON fixtures the JS side is
tested against:

- `shared/test-data/forecast-representative-cases.json`
- `shared/test-data/location-resolution-cases.json`
- `shared/test-data/storm-conditions-cases.json`

If the JS and C implementations ever disagree on a fixture case, one of the
two test runs fails loudly. That's the whole point: this is a
development-time / CI-time check, not something an end user ever sees. If
you change the heuristic or the scorer on one side, run both suites before
shipping:

```bash
npm run test:forecast          # JS, targets/pi/scripts/validate-forecast-representative.js
npm run test:location          # JS, shared/scripts/validate-location-resolution.js
npm run test:conditions        # JS, targets/pi/scripts/validate-storm-conditions.js
npm run test:esp32-parity      # C, this directory
npm run test:all               # version consistency plus all JS and C checks
```

`test:esp32-parity` compiles with plain `cc`; no board or cross-compiler is
required. It reuses `components/json/cJSON/` from the active `IDF_PATH`.
Activate your installed ESP-IDF v5.5.5 environment as described in the
[development instructions](../README.md#manual-buildflash-for-iterative-dev-work),
or set `IDF_PATH` to that installation's framework directory for this command.
An explicit path must contain both `cJSON.c` and `cJSON.h`; an invalid path
fails rather than silently selecting another installation.

When `IDF_PATH` is unset or empty, the runner uses the existing repository-local
path `.esp-idf/esp-idf-v5.5.5/components/json/cJSON/`. It does not install or
download dependencies. Test executables are created in a temporary directory
and removed when the runner exits.

## Choosing verification

Follow the [development policy](../../../docs/wac/development.md). Run the
relevant existing logic suites when their behavior or test runner changes.
When shared weather logic changes, run both its JavaScript and C checks;
this comparison protects the retained weather behavior without requiring
Raspberry Pi hardware. The C checks compare against shared expected fixtures;
the runner does not execute JavaScript itself.

Compile the firmware for firmware source, dependency, or build-configuration
changes using the [build instructions](../README.md#manual-buildflash-for-iterative-dev-work).
Review documents and affected links for documentation-only changes.

The proposed tests under `tests/wac/` are not yet implemented. Add focused
tests in approved files as those features are implemented. These existing
weather tests do not verify alarm scheduling, settings recovery, or the new UI.
Actual RTC operation, audio, touch, brightness, and power-interruption behavior
require appropriate board checks. Report compilation, computer-based tests,
and hardware observations separately; a passing build does not establish
hardware behavior.

## macOS: "xcode-select" / broken `cc`

If `cc` fails with something like `xcodebuild ... failed` or `Failed to
locate 'clang'`, that's a broken Xcode Command Line Tools selection on your
Mac -- unrelated to this project. Either fix it (`sudo xcode-select
--reset`, or reinstall the Command Line Tools), or point the script at the
CommandLineTools clang directly for one run:

```bash
CC=/Library/Developer/CommandLineTools/usr/bin/clang npm run test:esp32-parity
```

## Adding a new fixture case

Add it to the relevant JSON file in `shared/test-data/` -- both the JS test
and this C test read the same file, so one new fixture exercises both
implementations. No need to duplicate a case by hand in two languages.

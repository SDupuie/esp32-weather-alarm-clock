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

Conditions cases exercise the fixed [weather-warning limits](../../../docs/architecture.md#conditions-contract),
measurements, outlook and time formatting. The two gust-boundary cases supply
the same 64 km/h gust: metric warns at its inclusive limit, while imperial stays
below its limit. These test inputs preserve the existing difference between
the unit-specific defaults; the fixture adapter accepts only units/time format.

If the selected macOS compiler is broken, use the installed Command Line Tools
compiler for this run:

```bash
CC=/Library/Developer/CommandLineTools/usr/bin/clang targets/esp32-p4/scripts/test-weather-parity.sh
```

## Settings, time, backup clock, Night Shift, connectivity and UI controls

```bash
cmake -S targets/esp32-p4/tests -B /tmp/esp32-p4-native-tests
cmake --build /tmp/esp32-p4-native-tests
ctest --test-dir /tmp/esp32-p4-native-tests --output-on-failure
```

Managed dependencies must already be available from a normal firmware build.
The UI targets use the existing managed LVGL library with a native configuration.
The CMake suite contains settings, time, Night Shift, settings-control, UI-shell,
weather-service, connectivity and touchscreen-recovery targets.

The settings tests compile `main/device_config.c` with typed NVS substitutes and
ordinary thread locks. They cover defaults, validation, editor changes merged
into current settings, full selected-group saves, failure/corrected-retry reads,
missing keys, returned storage errors, accepted partial saves and readers during
a blocked save. Brightness checks cover the 20–100% range, Day/Night defaults,
rejected saves below the minimum, successful endpoint saves and reloads, and
loading low stored values at 20% only in memory. They verify unchanged stored
values and unrelated settings, no startup rewrite, and preserved read-error handling.
The substitute deliberately permits stored keys
to change before commit; this is a test example of allowed partial outcomes,
not evidence of a physical power-cut result.

Time checks compile production `main/clock_time.c` and `main/ds3231.c` with simulated
register access and a substitute system clock. They cover timezone offsets,
trusted startup, lost time, access failures, correction ordering and retries,
failed calendar/status writes, warning dismissal, new warnings during dismissal,
calendar validation and snapshots during blocked register access. Simulated
failed initialization results at startup and during correction check that
calendar access waits for successful initialization, retry timing and limits
remain, and lost-time evidence is inspected before a later correction. They supply
network-completion counts; they do not perform real network synchronization or
change the development computer's clock. No user-settings writes occur in these
checks.

Night Shift checks compile production `main/night.c`. They cover
Off/Always On/Fixed Schedule decisions, equal endpoints, ranges across midnight,
local timezone rules, saved startup fallback and retention while time is
unavailable. Typed storage substitutes check that only effective appearance
changes write the separate operating record, including failures and restart
outcomes; settings are not rewritten. Substitute brightness calls and elapsed
time check startup readiness, intermediate fade steps, changed targets, bounded
steps after a delay and retries from the last successful level when the BSP
reports failure. Readers are checked while storage or backlight work is pending.
Invalid schedules and returned failures are controlled test examples. The clock
task/startup integration is reviewed from source and compiled by the firmware
build; these native checks do not execute the full startup or task. Solar
calculations and physical brightness/fades remain unverified.

Connectivity checks compile production `main/connectivity.c` with native network,
Wi-Fi driver and task substitutes. They exercise initialization shared by connecting
and scanning, RAM credential storage, initialization/scan requests overlapping in
either order, repeated initialization, returned errors and successful retries.
They also check scan-result filtering, empty scans, disconnect handling during
scanning and cleanup, and callbacks completing while an operation holds its lock.
Zero-wait network-time checks cover startup once, pending synchronization,
initialization error/retry and callbacks advancing the actual completion count.
These are controlled concurrency checks; they do not establish
ESP-Hosted behavior or real Wi-Fi driver and network timing.

The weather-service target compiles production `main/weather_client.c` with
substitute HTTP responses and the existing weather calculations. Its build also
extracts the actual private weather worker and publication helpers from
`main/main.c`; source changes regenerate that build include. It checks provider
date/temperature/icon pairing, missing or invalid dates, downloads without
usable time, retained payloads after failure, ten-minute refreshes and independent
twenty-second weather and network-time-startup retries, including slow requests
whose retry times overlap. It also checks that a saved location is required.
The service test checks the resulting forecast icons; the separate representative
calculation check verifies the chosen weather codes directly.
The transport itself is substituted in these worker checks; the connectivity
checks above exercise actual synchronization startup/completion. Live HTTP,
certificate validation and network-time traffic remain unverified.

The settings-control target compiles production `main/ui/settings.c` with real LVGL
objects and events. It checks nested Done/Cancel, shared Wi-Fi drafts, deferred
Save, duplicate blocking, validation, save failure/retry, resolved-location retry
without Wi-Fi, immediate enable, stale completion handling, and character entry
and editing through actual keyboard events. Every settings view is checked for
keyboard allocation: only Device, Weather Location and Wi-Fi Credentials editors
allocate one, with the existing layouts preserved. Real pointer taps check all
five text fields and reopening after hiding the keyboard. Resolved screen
coordinates check that the centered keyboard's corners are inside the round
800×800 panel, rather than checking only square-buffer bounds or requested
offsets. Pointer presses/releases on LVGL's actual
key rectangles exercise character entry, layout switching and hide controls.
The shell also checks a pointer tap on the
empty first-use location field and keyboard retention through a runtime update.
Network work and tasks are substitutes, and navigation callbacks from `ui.c` are substituted;
first-use routing is also exercised by the shell target below.
The UI target also checks the actual generated clock font for live-time and
unavailable-time characters through LVGL glyph lookup.
Real LVGL brightness sliders check both endpoints, clamping a below-minimum
selection to 20%, nested draft values and Cancel without saving.

The shell target compiles production `main/ui/ui.c`, `main/ui/clock_views.c` and
the settings editor with real LVGL objects and events. It checks authoritative usable/unavailable time,
backup-clock notice wording, layout and fonts, failed dismissal submission,
duplicate blocking, owner acknowledgment and a different warning arriving while
dismissal is pending. It also checks first-use routing, settings draft retention,
modal gesture blocking, home-view indicators and day/night colors supplied by the
appearance owner, including a snapshot that differs from the settings mode. Both pairs of
edge indicators are exercised through repeated pulse cycles with one-second UI
refreshes, plus tint changes and transitions to normal and hidden states. Message
checks use real pointer taps and swipes to cover
pending/duplicate dismissal, submission and worker-result errors, retaining the
tapped message when a newer one arrives, retry, success, and
completion after leaving/reopening the view or editing settings. The message
worker is substituted in these UI checks. Forecast checks pair provider dates
with distinct temperatures and icons through unavailable/corrected time,
midnight and stale payloads, invalid dates, month/year/leap-day and daylight-saving
boundaries. They check the normal weekday capitalization and truthful `Tomorrow`
caption. Images use
small substitutes, so these checks do not establish artwork rendering, complete
visual parity or physical touch behavior. The main task's command queue is
reviewed from source and compiled by the firmware build; the actual network-time
callback is exercised by the connectivity checks.

Clock checks cover both configured faces, analog hand rotation, midnight/noon,
12/24-hour and leading-zero formats, and recovery through unavailable Day/Night
surfaces. Night digit size remains fixed across minute and AM/PM changes, with
measured centering, baseline alignment and room for the period inside the round
face; returning to Day restores native font size and position. The existing
135-second paused-clock threshold and its face-specific notice are checked with
controlled stalled-minute snapshots. Pointer gestures and one visible pair of
message/weather edges are checked through unavailable time and recovery. These
object checks do not establish the quality of the scaled raster font or physical
rendering. The unavailable screen omits the still-unimplemented alarm controls.

The touchscreen-recovery target extracts the actual private read callback from
`main/main.c`, using the installed touch header and real LVGL. Hardware read
substitutes check every returned error, including `ESP_ERR_INVALID_STATE`, zero
reported points, failure-only cached-point clearing and recovery on a fresh
sample. A real LVGL button press interrupted by a failed read must receive press
cancellation without a click; subsequent normal clicks must work. Successful
held samples retain the original behavior. The BSP owns display/touch startup;
firmware compilation and source review cover the public callback installation
under the initial display lock. The tests do not reproduce the board's I2C
interrupt sequence, exercise BSP tasks or establish physical gestures and
shared-bus recovery. The BSP's default unlimited touch wait is unchanged by the
callback; a read that never returns cannot be recovered there. See the
[work record](../../../docs/work.md) for the board comparison and its limits.

## Message acknowledgement, browser listing, discovery and shared handoff

With `IDF_PATH` set as above:

```bash
python3 targets/esp32-p4/tests/test_message_ack.py
```

This compiles the actual acknowledgement and record-lookup functions with native
network and task-lock substitutes. It covers displayed-message identity, shared
requests, and missing, inactive, expired and differently targeted messages.
It also compiles the production background handoff and worker operation with
native lock/network substitutes. Checks cover copied IDs, submission while the
message owner is busy, queued/running/unconsumed-result rejection, retained
completion, and local/shared success or failure. Source review and the
firmware build check integration into the existing worker and heartbeat, including
consumption of message results only after the display lock is acquired. The
heartbeat itself is not executed by these native checks; the handoff checks
exercise retained completion in the message owner. These checks do not establish
physical input latency or live hub behavior.

The same check also executes the production message-worker loop one iteration at
a time with substitute network and task operations. It covers retained changes
through Wi-Fi loss/reconnection, failed requests and HTTP rejection, 15-second
retry spacing, coordinator/address changes before a retry, and stopping after
success without replaying an unchanged copy on reconnection. Production
serialization and merge checks cover message content, expiration, duplicate IDs,
acknowledgments and deactivation, including a local operation finishing during
an outgoing transfer. These are controlled failure examples, not board
observations. The existing finite RAM store and full-store merge limits remain.

The same harness executes the production discovery sender and captures its
outgoing packet bytes. It checks the existing five fields with ordinary names,
quotation marks and backslashes, the retained packet-size limit, allocation and
encoding failures, and send errors. Failure checks verify cleanup and that no
partial or oversized packet is sent. Received values are decoded with the actual
cJSON library; the socket is substituted, so these checks do not establish live
discovery between boards.

Browser checks execute the production list, snapshot, action and forwarding
functions with substitute HTTP server and transport operations. They cover
all-target browser listing, per-clock filtering, dismissed messages, expiration,
deactivation, priority/date ordering and exact request-path/query/body forwarding.
The browser's existing `deviceId=all&includeDismissed=true` request lists every
target; normal polling remains filtered even for a clock whose ID is `all`.
These checks do not run a browser or establish live traffic between clocks.

The synchronous acknowledgment operation is private to the message worker; the
UI checks use copied-ID submission and retained completion. Snapshot checks keep
displayed ID, text and formatted metadata, while record priority/sender and
aggregate important/unread state remain covered. A controlled long-sender
response verifies the existing sender limit before shared metadata formatting.

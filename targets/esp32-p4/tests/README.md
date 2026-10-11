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
allocate one. Real pointer taps check all five text fields and reopening after
hiding the keyboard. The mockup's four maps, key widths, gaps, style and keyboard
box are checked against actual LVGL objects. Resolved screen coordinates verify
round-panel fit and the active field above the keyboard. Pointer presses/releases
on LVGL's actual key rectangles exercise character entry, layout switching,
held-key editing, popovers and hide controls.
The shell also checks a pointer tap on the
empty first-use location field and keyboard retention through a runtime update.
Network work and tasks are substitutes, and navigation callbacks from `ui.c` are substituted;
first-use routing is also exercised by the shell target below.
The UI target also checks the actual generated clock font for live-time and
unavailable-time characters through LVGL glyph lookup.
Real LVGL Volume and Day/Night Brightness sliders check the [approved slider
geometry](../../../docs/architecture.md#ui-and-visual-reference), existing Day/Night
colors and resolved touch bounds. Actual pointer taps near the top/bottom of the
48px touch area and held-pointer dragging check selection and live percentage
updates. Brightness checks retain both endpoints, clamping a below-minimum
selection to 20%, nested draft values and Cancel without saving.

The Settings Entry, Device Setup and Display checks compare actual LVGL row,
heading, footer, switch and chevron boxes, typography, border/radius and
Day/Night colors against the approved mockup. Pointer taps exercise switches
and navigation. Live appearance changes retain nested drafts, the active field,
keyboard, focus and cursor. Captions use saved alarm/repeat values and distinguish
the applied Wi-Fi network from a new draft; font checks verify middle dot/en dash
and ordinary-character fallback. Save/Cancel, nested Done and first-use exit
checks remain. These object checks do not establish physical pixel parity.

Device, Weather Location and Network Details checks compare normal fields,
labels, notes and footer boxes, and the complete temporary typing composition.
Closing restores inactive controls and unsaved values without saving. Missing
Room/ID/location/network-name errors focus the correct field; Device Setup still
permits no Wi-Fi profile. Actual substituted lookup/storage failures check concise
typing notices, no overlap with the field, and restoration of the full error on
Close without additional writes or lookup. Password masking, configured length
limits, first-use guards, save failure and resolved-location retry remain covered.
Generated curved keyboard masks and punctuation coverage are checked; the exact
SVG paths come from the mockup's pinned Lucide source through existing artwork
tools. These checks do not establish physical raster or touch compliance.

Nearby-network checks exercise real LVGL pointer scrolling beyond the first three
rows, actual Open/Secured and dBm captions, scan loading/empty/error states,
Refresh/Manual/Back behavior and stale completion handling. Weather checks cover
actual pending lookup, retained resolved values after failed storage, concise
lookup errors and saved values. Separate fresh-process invocations check Weather
and credentials restart compositions without adding a production reset hook.
Device Setup captions consume a substituted connectivity-owner phase and reject
claims that a different draft network is connected. Connectivity tests exercise
the production phase publication, returned connect failures and overlapping
callback attempts; existing retry calls remain. Native checks do not establish
radio timing, physical rendering or restart navigation on the board.

Shared arrow-picker checks use real pointer events for five-minute steps,
minute carry/borrow through hour, noon and midnight boundaries, separate hour
buttons, and AM/PM selection in Alarm and both Night endpoints in 12/24-hour
formats. They retain arbitrary saved minutes when opening/canceling and check
five-minute wrapping from a saved 59 without rounding. Done remains
draft-only; the existing Alarm Save checks verify applied values and unrelated
room/brightness preferences.
Repeat checks cover shortcuts, full day captions and zero-day Done blocking.
Night checks cover mode descriptions/selections, fixed-schedule boundary notes,
offset direction/steps/limits and brightness label updates in place. Cancel and
nested drafts cause no writes or backlight preview; production saves verify the
actual Alarm and Night values. Generated numeric-font line boxes/baselines and
glyph offsets are checked against the reference font metrics; 32px icon sources,
boxes and tint are checked in both appearances. Physical rendering remains
unverified; the mockup’s browser-native slider track is not a fixed raster asset.

Alarm/Sound/Snooze/Solar menu checks compare actual rows, switches, footers,
descriptions, selected checks and icon/text spacing with both reference palettes.
They verify draft-only time/repeat/sound/snooze edits, Alarm Save without restart,
immediate enabled-only writes, failed Save/retry and pending controls. Volume
checks open the real Sound editor, use Done and save unrelated alarm edits while
preserving stored 0/7/37 values. Actual pointer and keyboard input then checks
five-point steps, 10–100% selections and label updates without rebuilding it.
Preview is disabled until audio is implemented. Solar uses the applied location,
unavailable event times and truthful appearance-retention guidance; offsets
remain draft-only. These checks do not establish playback, solar calculations,
physical touch or complete raster equality with a browser-native range control.

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

Digital Night sizing additionally checks all 4,320 hour/minute/format combinations
using actual LVGL objects. It verifies the selected nominal 250px formats and
retained padded 12-hour fit, steady scale, all transformed time/AM-PM corners
inside the round-screen bound, centering, AM/PM baseline/gap and restored Day
positions. These layout checks do not establish physical clipping or font sharpness.

The approved screen-walkthrough checks exercise first-download waiting on both
clock faces, Day/Night transitions, hidden fallback artwork and restored live
weather. Forecast keeps its date/temperature layout while all five pictures are
invisible; actual font lookup verifies that the loading ellipses exist. Failed
requests, retained weather through failed refreshes and later success check the
Forecast/Conditions notice wording, boxes and styling, including immediate
repositioning during pointer swipes and preserved home/progress messages.
Checks also retain provider-date pairing, the declined Night typography and
Conditions presentation, and verify the approved AM/PM, temperature, High/Low,
divider and Dismiss-button changes. Synthetic RTC/network snapshots check
“Waiting for sync,” clearing despite a remaining backup warning, unusable time,
paused-warning priority after 135 seconds and hiding during navigation/settings.
These are actual LVGL object/event checks using supplied snapshots, not
observations of weather failures, synchronization or rendering on the board.

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

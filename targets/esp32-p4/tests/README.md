# ESP32-P4 native checks

These checks compile the actual firmware logic on the development computer.
They do not establish touchscreen behavior, real network traffic, physical
power-cut outcomes, RTC hardware, speaker sound or backlight behavior.
The native semaphore substitutes check the weather gate's paired take/give
usage and overlapping requests; they do not model ESP-IDF task-priority
inheritance or scheduling. Board timing evidence belongs in the
[work record](../../../docs/work.md).

Use the [current work list](../../../docs/work.md#current-work)
for completed board checks and remaining work. In particular, the
[attached RTC checks](../../../docs/work.md#attached-rtc-unattended-checks--authorized-october-16-2026)
use the real module through powered restarts and bounded offline operation.
They do not establish battery retention after removing all main power, genuine
RTC faults, physical touch or acoustic continuity. Native checks below describe
their own coverage, independently of those board observations.

## Weather and location

Activate ESP-IDF 5.5.5 as described in the [target guide](../README.md#prepare-the-environment-no-board-needed), then run from the repository root:

```bash
targets/esp32-p4/scripts/test-weather-parity.sh
```

The retained script name now runs only C forecast-representative and location
scoring checks against fixtures in `shared/test-data/`. It also checks that
provider search names retain state/country qualifiers before the provider's
result limit. NWS alert behavior is covered by the native service and UI tests
below. No Raspberry Pi or JavaScript suite is required. cJSON is read from
`$IDF_PATH/components/json/cJSON`.

Weather fixtures exercise the [approved provider mappings and daily rules](../../../docs/weather.md),
including compound facts, unknown/no-picture inputs, current-only exclusions,
the paired ordinary-rain gate, independent Hail reports and mixed-snow majority.
The retained daily cold-weather guesses are checked separately from current
classification, which requires reported snow evidence. Constructed rare-condition
cases establish decoding behavior, not how often a provider returns those
conditions in practice.

If the selected macOS compiler is broken, use the installed Command Line Tools
compiler for this run:

```bash
CC=/Library/Developer/CommandLineTools/usr/bin/clang targets/esp32-p4/scripts/test-weather-parity.sh
```

## NWS alert services and screens

After configuring and building the native suite below, run the focused NWS and
screen checks with:

```bash
ctest --test-dir /tmp/esp32-p4-native-tests -R '^(device_config|weather_service|settings_alerts|ui_shell)$' --output-on-failure
```

`weather_service` compiles the production `weather_client.c` alert owner and its
existing weather worker directly. Controlled HTTP,
clock, audio and storage substitutes cover the complete 55-event catalog,
cross-family NWS priority, event details, CAP update/cancel relationships,
shared dismissal, new and escalated events, configured sounds, expiry, saved
events and failure recovery. Request-spanning dusk/dawn cases change the controlled
Night value during HTTP, checking both Night sound preferences, retained visual
alerts and no delayed or unchanged replay. A dismissal during response cleanup checks that a
sound cannot start after its headline was just dismissed, while a later eligible
revision can still sound. A captured historical NWS feature checks real CAP
field shapes and protective text; follow-up cases are constructed. The tests do
not make live NWS requests.

`settings_alerts` uses 55 constructed catalog rows to check scrollability,
selection behavior and nested draft handling, plus update-sound and Night Alert
Sounds choices. `device_config` checks alert preference defaults, validation,
save/reload and failure handling. Catalog membership and ordering are checked by
`weather_service`. `ui_shell` uses production LVGL screens and controlled alert
snapshots to check count badges and status marks, and headline display and
dismissal on both clock faces in Day and Night. It also checks the Alerts list,
scrollable event details, back navigation, and checking/failure/save/unavailable
states.
These software checks do not establish live provider responses, physical touch
or display behavior, or alert audio on the board. Approved behavior and current
device verification are recorded in the [architecture](../../../docs/architecture.md#nws-watches-and-warnings)
and [work record](../../../docs/work.md#current-work).

## Settings, time, backup clock, alarms, Night Shift, connectivity and UI controls

```bash
cmake -S targets/esp32-p4/tests -B /tmp/esp32-p4-native-tests
cmake --build /tmp/esp32-p4-native-tests
ctest --test-dir /tmp/esp32-p4-native-tests --output-on-failure
```

Managed dependencies must already be available from a normal firmware build.
The UI targets use the existing managed LVGL library with a native configuration.

For a visual review, the optional capture targets render the production UI with
its compiled artwork, fonts and RGB565 software renderer. They are excluded from
the normal build and CTest run; they are not firmware build modes. Hardware,
network and feature-owner publications use controlled test inputs.

```bash
cmake -S targets/esp32-p4/tests -B /tmp/esp32-p4-render-review
cmake --build /tmp/esp32-p4-render-review --target test_settings_capture test_ui_capture -j 6
mkdir -p /tmp/esp32-p4-render-review/settings /tmp/esp32-p4-render-review/ui
/tmp/esp32-p4-render-review/test_settings_capture --capture /tmp/esp32-p4-render-review/settings all
/tmp/esp32-p4-render-review/test_ui_capture --capture /tmp/esp32-p4-render-review/ui all
```

Each capture writes an 800×800 PPM and JSON with text, resolved font, ordinary and
transformed bounds, colors and control styles. A named scenario can replace
`all`. `WAC_CAPTURE_RADAR_BGRA` optionally supplies an 800×800 BGRA map input;
without it Radar uses a clearly artificial flat sample. These renders establish
software layout, not physical panel, touch, brightness, service or audio behavior.
The [work record](../../../docs/work.md#rendered-firmware-visual-review--october-19-2026)
links the completed comparison and its coverage limits.

The CMake suite registers 49 cases across settings, time, alarm-calendar,
alarm-recovery, alarm-command, audio, Night Shift, solar-event, settings-control,
UI-shell, NWS/weather service, connectivity and touchscreen-recovery targets.
This count describes the configured suite; completed runs belong in the
[work record](../../../docs/work.md).

The settings tests compile `main/device_config.c` with typed NVS substitutes and
ordinary thread locks. They cover defaults, validation, editor changes merged
into current settings, full selected-group saves, failure/corrected-retry reads,
missing keys, returned storage errors, accepted partial saves and readers during
a blocked save. Display writes its five existing entries; Device writes its ten
remaining entries. Checks cover unchanged-group writes, invalid unrelated drafts,
older drafts preserving newer unchanged fields, concurrent writers, every Display
write failure, retained-draft retries and partial-store reloads. Brightness checks cover the 20–100% range, Day/Night defaults,
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

Alarm-calendar checks compile production `main/alarm_schedule.c` and compare
36 explicit UTC/date cases, including repeat masks, equality, date boundaries,
spring/fall changes and a half-hour daylight-saving transition. Expectations
are stated UTC times rather than a second copy of the production conversion.
The calculator uses the timezone already applied by the time owner; it does
not change timezone, settings or operating state.

Alarm-recovery checks compile production `main/alarm.c` with that calculator
and ordinary native NVS substitutes. They exercise the
[approved alarm recovery rules](../../../docs/architecture.md#alarm-and-snooze):
usable-time gating, catch-up endpoints, future/overdue snooze, repeated snooze
and restart during resumed ringing, saved dismissal, backward suppression and
one pending missed notice. Returned read/write/commit failures and blocked writes
check error reporting and immediate copied in-memory action state without
tick-based storage retries. Recurring operating transitions do not rewrite
settings; one-time completion has its approved enabled-only Off save. These are
controlled test examples, not physical power-cut evidence. Off checks exercise
cancellation of ringing and snooze, including a restored snooze without usable time, preserved
configuration/missed notice, failed cancellation saves, restart suppression and
copied canceled state while a save is blocked. Runtime-enable checks cover late
and within-window enabling without a retroactive ring/notice, before-due and
exact-due enabling, deferred usable time, backward suppression, preserved pending
notices and a single operating-record save for idle arming. Runtime schedule-edit
checks cover hour, minute and repeat-day changes separately and together,
future/recently-past occurrences, before/exact due time, deferred usable time, preserved captured
ringing/snooze and missed notices, and unchanged/sound/volume/snooze-only updates.
They reconstruct the reported false Saturday notice without claiming a new
board observation. Restart checks keep skipped past occurrences quiet after idle
enable/edits, while a later genuinely due alarm retains catch-up/missed behavior.
Failed arming saves preserve live suppression with the accepted older-record
restart limitation. Edits during captured ringing/snooze leave their saved
recovery intact, including repeated changes and future/overdue snooze deadlines.
These checks use ordinary substitute storage, not physical power-cut evidence.
One-time checks cover first use and exact-due today/tomorrow selection, fixed-date
restart recovery through midnight and DST, ringing/snooze/expiry/Dismiss and
rearming, edits between recurring/None schedules, retained missed notices,
target/read/write/commit failures, consumed-target suppression after failed Off
and no tick retries. Existing 56-byte snooze/dismissal records remain unchanged;
the optional 16-byte target uses the same alarm namespace. Settings checks run
the real conditional completion operation: only the matching applied None
schedule saves Off, with one enabled write, replacement schedules left alone,
unchanged preferences, normal failure publication and configuration reload.
The clock-task connection is compiled by the firmware build and reviewed from
source. Speaker output and physical alarm operation remain board checks; this
suite does not establish them.

Alarm-command checks reuse `test_alarm.c` with the actual private command/result
helpers extracted from `main.c` at native configure time. A copied two-entry
queue substitute checks submission without storage work, queue-full failure,
warning compatibility, captured occurrence identity and rejection of stale taps.
The real owner checks Snooze, Dismiss and missed-notice actions, including
failed operating-record writes whose live action still applies. Completion is
retained and consumed once; reading the owner's current state after delivery
preserves a resumed or later alarm. The same target executes main's actual
status-collection function with controlled weather-copy success and failure.
It checks that fresh clock, message, alarm, audio, Night Shift and alert state
still reach the display when weather cannot be copied. The full clock task and heartbeat are not
executed here; their delivery order and short copied read are source-reviewed
and compiled in the firmware build.

Audio checks execute the actual private worker and shared generator from
`main/audio.c`, included by the approved `test_audio.c`, without firmware testing
hooks. Queue/task, codec, configuration and alarm inputs are substituted. Nine
fresh-process cases cover patterns, playback and queue/task/BSP/open/volume/write/
close failures. Independent silence/cadence, spectral separation, PCM bounds,
zero/arbitrary volume and ramp points supplement exact helper/envelope checks;
those helper comparisons alone are not independent waveform proof. The optimized
Classic Beep and Rising Tones generators are also compared against the original
double equations across all five seconds, four volume values and three ringing
ramp levels. Differences
must stay within two PCM counts, apart from narrowly checked isolated square
zero crossings. Matching phases remain sample-identical after hours/days of
ringing, including chunks that cross a pattern boundary. Classic Bell checks
independently cover its rapid attack, two inharmonic pitch groups, alternating
21-strike-per-second cadence and overlapping decays. Actual chunks cover reset,
continuity, large integer sample positions, output/volume/ramp bounds and fresh
preview, sound-switch and resumed-ring state. These checks supplement listening
and board timing; they do not establish them. A gated
worker thread checks hardware operations outside caller/snapshot locks, lazy
one-time BSP creation, retained handle reopening, copied latest commands,
five seconds of PCM/time, Stop and ring priority during slow setup, cancellation
between writes, and new/resumed-ring gain restart. A failed request does not
repeatedly recreate the BSP or retry a still-ringing occurrence. The real alarm
owner's separate blocked-save checks establish that its live canceled/snoozed
phase is published before storage returns; audio tests consume controlled copies
of that phase rather than executing actual NVS in their worker.

Sound Machine checks extend the same audio target. They cover all timers and
live changes, the final fade, preview/ring priority, no automatic resume,
cancellation during setup/read/decode, returned failures and resource cleanup.
Two million samples per noise and seed, including the firmware's starting seed,
check complete retained-state chunks, finite output/state, mean, RMS and different
high-frequency energy. Frozen results from the linked website's source equations
check White scaling, Pink/Brown samples above the former clipping limit and
Brown's unscaled internal state. The roughness comparison follows the restored
equations; the retired Brown state/flat-run limits are removed. These native
checks do not establish audible character, worst-case peaks or uninterrupted
board playback. Recording checks parse the actual nine MP3
headers and frame lengths against independent expected sample counts, then use
simulated decoder PCM to exercise consumption, exact-length caps and writes
containing both a repeat's tail and its next head. Two repeats of each recording
check every simulated output sample and stream
open/close accounting. Independent dB-derived expectations check the approved
recording boosts for all nine selections, zero/partial/full volume, and the
existing sleep fade. The same gain checks cover the approved noise-output
reductions without changing source equations; recording worker checks cover live volume and fade
changes without restarting decoding. Repeat reopen/close failures and Stop during reopening
check cleanup, no automatic retry and explicit retry.
Constructed loud Campfire decoder samples check both PCM conversion limits and
correct scaling of quieter samples under its approved larger boost. This checks
bounded conversion rather than establishing absence of audible clipping.
The host includes the actual managed API headers but cannot execute the target
decoder library. These checks
do not prove actual decoded PCM, audible seams or board refill timing.

Actual LVGL Sound Machine checks cover all sound/timer choices, list and scrollbar
dragging, volume geometry, immediate saves and retry, pending choices surviving
editor closure, unrelated drafts and keyboard/cursor preservation, copied audio
status, Day/Night clock shortcuts and preserved alarm tap areas. Owner copies
and background saves are substituted; physical rendering and touch remain board
checks. Clock shortcut checks measure actual text bounds for all seven weekday
names on both faces in Day/Night appearance, keeping both icons clear of the
weekday/date labels. Pointer checks cover speaker and backup-warning actions.
Configuration checks cover its three preferences, typed defaults/reload,
validation, partial-save failures/retry and concurrent unrelated saves.

Actual LVGL pointer checks cover copied unsaved sound/volume, Start/Stop,
Day/Night geometry and icon/text alignment, completion/error/readiness updates
in place, selection/navigation/close/hide cancellation, and draft preservation
through ringing. A stale heartbeat does not prevent cancellation; a ring-masked
inactive flag still causes a definite Stop on hide. Preview never saves settings
or performs codec operations under the display lock. Audio failures retain
the approved logging behavior without an on-screen diagnostic caption.

The native task substitute checks valid allocation arguments; it does not allocate or measure an ESP32-P4 task stack. Hardware stack results belong in the [work record](../../../docs/work.md).

The final firmware build verifies the public codec integration and BSP returned-
error configuration. These checks do not establish physical output, loudness,
muting, clicks/reopen quality or every hidden register failure. The installed
write API retains its fixed 1000ms wait; short chunks do not guarantee that all
hardware faults cancel sooner. The generator uses single-precision calculations
and integer pattern timing. Real synthesis time, continuity during weather/RTC
work and stack headroom require board measurements; current results are recorded
in the work record.

Solar-event checks compile production `main/solar_times.c` without board facilities.
Seven dated US Naval Observatory reference cases cover seasons, hemispheres,
leap day, the date line and a year boundary, with a two-minute tolerance.
The tests also check invalid coordinates, polar no-event results, complete UTC
event dates and independence from the process timezone. Reference URLs are
recorded beside the fixtures in `test_solar_times.c`; these are calculated
reference comparisons, not observed sunrise/sunset measurements. High-latitude
season transitions and unusual atmospheric or horizon conditions are not
established by these fixtures.

Night Shift checks compile production `main/night.c` with the real solar calculator.
They cover Off/Always On/Fixed Schedule decisions, equal fixed endpoints, ranges
across midnight, and Sunset to Sunrise's exact start/end boundaries. Solar cases
cover signed offsets, shifted midnight boundaries, offsets that remove a short
night, daylight-saving transitions, local-date/location/timezone changes and
backward clock corrections. Missing time/location and unavailable required events
exercise saved startup fallback and running appearance retention. Copied Settings
events remain raw and independent of the selected mode and saved offsets.
Typed storage substitutes check that only effective appearance
changes write the separate operating record, including failures and restart
outcomes; settings are not rewritten. Substitute brightness calls and elapsed
time check startup readiness, intermediate fade steps, changed targets, bounded
steps after a delay and retries from the last successful level when the BSP
reports failure. Readers are checked while storage or backlight work is pending.
Invalid schedules and returned failures are controlled test examples. The clock
task/startup integration is reviewed from source and compiled by the firmware
build; these native checks do not execute the full startup or task. They do not
establish physical solar transitions, brightness/fades or the populated Settings
page on the panel. Completed board observations and remaining checks are in the
[work record](../../../docs/work.md).

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

The weather-service target includes production `main/weather_client.c` with
substitute HTTP responses and the existing weather calculations, allowing checks
of private normalized values and the actual worker without a firmware test
interface or main-source weather-worker extraction. It checks provider
date/temperature/icon pairing, missing or invalid dates, downloads without
usable time, retained payloads after failure, ten-minute refreshes and independent
twenty-second weather and network-time-startup retries, including slow requests
whose retry times overlap. It also checks that a saved location is required.
No-profile, missing-location, Wi-Fi initialization/connection failures and live
request checks verify that routine display progress is no longer published.
Retained payloads stay stale during the next request, including Wi-Fi loss,
and a successful download clears that state. Download timestamps are published
only with usable network time; unavailable and RTC-only time publish no age.
Retained values, stale state and the existing refresh/retry timing are checked
together.
Both providers are exercised using retained real response data and explicitly
constructed current/error/rare-condition supplements. Checks cover selected
units, raw fractional amounts, provider epoch validation, provider local times
across DST, repeated-hour identity, missing optional fields, required daily
values and bounded responses. Request checks cover encoded dummy keys, including
a maximum-length key whose characters all require URL encoding,
serialized admission, key-specific daily-quota pauses, generic rate errors,
retry/reset waits and hourly failed-check recovery. No real credential or quota
exhaustion is needed. To check a separately captured, sanitized full Visual
Crossing response with the same decoder, pass its path to `/tmp/esp32-p4-native-tests/test_weather_service`
or set `WAC_WEATHER_LIVE_RESPONSE`; this reads a file and makes no live request.
The separate representative calculation check verifies the selected pictures.
The transport itself is substituted in these worker checks; the connectivity
checks above exercise production synchronization startup/completion with network
substitutes. These checks do not establish live HTTP, certificate validation or
network-time traffic; board observations are recorded in the
[work record](../../../docs/work.md).

Radar checks in the same target use the bundled LVGL LodePNG decoder with
constructed PNG/HTTP inputs. They check exact 512px smoothed Radar requests and
dimensions separately from the 800px basemap, response bounds, RGB palette
matching with retained received alpha, duplicate colors and unknown visible
color rejection before composition. They also check odd 512px source pixels,
partial-alpha tile seams and longitude wrapping, 75-mile coverage at different latitudes, source
alignment, opaque map edges, the approved brighter map colors, unchanged frames,
failed retries, offline stale marking and
safe image replacement after display acknowledgment. Worker checks verify the
independent five-minute checks and 100ms preparation turns without changing
forecast/time retry or quota deadlines. Binary download-growth checks preserve
all received bytes, including zero bytes, and retain the earlier buffer after
allocation failure or an oversized response. Pending publications survive a failed
short copy. Native allocation uses a 16MiB LVGL heap and enforces the same
4MiB decoder-allocation cap as firmware; firmware routes large decoder/image
allocations to PSRAM. These checks do not measure board memory,
decode latency, stack use or live HTTPS behavior.

After the native build above, `/tmp/esp32-p4-native-tests/test_weather_service --radar-only`
runs the focused constructed checks.
`/tmp/esp32-p4-native-tests/test_weather_service --radar-saved-data <capture-folder>` separately processes
the retained Leander/Colorado 800×800 USGS and required RainViewer tiles and
writes raw BGRA images and codec-memory JSON reports beside them. It makes no live
requests. Capture filenames are listed in `check_saved_radar_data` in the test.
Actual provider samples and data-only previews are review evidence, not finished
screen or physical-board verification.

`/tmp/esp32-p4-native-tests/test_weather_service --radar-saved-tiles <capture-folder>`
checks six retained 512px smoothed precipitation tiles through the actual request,
decode, full-source recoloring, nearest sampling and blending path. It expects
`z7-X-Y-512-smoothing1-snow0.png` for tile coordinates 27/46, 28/53, 24/50, 32/49,
35/53 and 35/54. It makes no requests or metadata assumptions; the capture report
records provider/frame identity. Its flat background and half-native-pixel
sampling layout are constructed references, not matching USGS geography or a
finished screen. Live source captures, host comparisons and board results are
recorded separately in the [Radar sprint](../../../docs/work.md#sprint-512px-radar-tiles-and-provider-smoothing--prepared-october-18-2026).

`/tmp/esp32-p4-native-tests/test_weather_service --radar-six-frame-data <capture-folder>`
checks the six retained Florida frames and twelve source tiles against their
original full-image reference pixels, including completion and holding the newest
image without additional HTTP requests. Use
`.reviews/main/radar-six-frame-test-2026-10-19` as the capture folder.
The ordinary constructed checks also cover the exact color
lookup across every source-table row and alpha value, compressed-history bounds
and reuse, single-pass chronological playback and newest-image completion,
displayed-frame Pause races, resume/Latest,
controls during metadata checks and offline/failure handling. These checks do
not measure animation speed or audio continuity; the
[playback work record](../../../docs/work.md#radar-color-lookup-and-six-frame-playback--authorized-october-19-2026)
keeps the separate board measurements and their limits.

UI checks cover the fixed Clock/Forecast/Radar navigation, Day/Night,
full-screen map and horizontal header fit, equal-width Light/Moderate/Heavy key
sections with centered labels, an unshaded footer, and no added town-label objects,
frame time and old/unavailable states, image-source retirement and alarm
preemption. Alert activity does not reorder the weather pages. Non-U.S.,
missing-country and unfinished locations create no Radar objects or route;
Forecast remains the only weather page. Country changes remove even a hidden
Radar image, and U.S. re-enablement restores navigation. Native pixels and
pointer events do not establish display quality, physical gestures or
concurrent board performance.
Playback checks exercise the actual Play/Pause hit target and swipe routing,
disabled/loading states, the Paused timestamp, image retirement and lifecycle
stop/Latest commands. The optional capture scenarios `radar-playing` and
`radar-paused` render these states in Day and Night appearance.

The settings-control target compiles production `main/ui/settings.c` with real LVGL
objects and events. It checks nested Done/Cancel, shared Wi-Fi drafts, deferred
Save, duplicate blocking, validation, save failure/retry, resolved-location retry
without Wi-Fi, immediate enable, stale completion handling, and character entry
and editing through actual keyboard events. Every settings view is checked for
keyboard allocation: only Device, Weather and Wi-Fi Credentials editors
allocate one. Real pointer taps check all six text fields and reopening after
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
They open the Display row with pointer input and check its edited summary,
both Back/Done return destinations, the blank Night Shift fourth slot and
retained slider objects/values through alarm interruption. Display Cancel/Back
discards its separate draft; Save writes only its five choices without restarting.
Device Save & Restart cannot apply unsaved Display choices. Weather Back/Cancel
retains pending Device edits, while Weather Save applies only its own group.

The Settings Entry, Device Setup and Display checks compare actual LVGL row,
heading, footer, switch and chevron boxes, typography, border/radius and
Day/Night colors against the approved mockup. Pointer taps exercise switches
and navigation. Live appearance changes retain nested drafts, the active field,
keyboard, focus and cursor. Captions use saved alarm/repeat values and distinguish
the applied Wi-Fi network from a new draft; font checks verify middle dot/en dash
and ordinary-character fallback. Save/Cancel, nested Done and first-use exit
checks remain. These object checks do not establish physical pixel parity.

Shared-header checks use the reviewed lower Back/title coordinates in both
appearances, including Weather's single-line title. Pointer navigation
checks that Device Setup's Weather entry opens that existing editor and returns
normally; its save scope and behavior remain the same.

Messaging, Weather and Network Details checks compare normal fields,
labels, notes and footer boxes, and the complete temporary typing composition.
Messaging checks cover its one-line heading, two label-over-value rows, the
House Messages row and plain distinct-ID guidance in the fourth-row space in
both appearances, using its retained 26px guidance font. They check
vertical centering in the unused fourth row, clearance from the third row and
Done, guidance hiding/restoration during typing, and normal error replacement in
the same space.
Actual pointer taps on labels, values, chevrons and row padding
open the existing text editor; keyboard Close/Done restores the rows and draft
values. Shared-mode validation still rejects the default ID. These object and
event checks do not establish physical panel rendering or touch compliance.
Closing restores inactive controls and unsaved values without saving. Missing
Room/ID/location/network-name errors focus the correct field; Device Setup still
permits no Wi-Fi profile. Actual substituted lookup/storage failures check concise
typing notices, no overlap with the field, and restoration of the full error on
Close without additional writes or lookup. Password masking, configured length
limits, first-use guards, save failure and resolved-location retry remain covered.
Generated curved keyboard masks and punctuation coverage are checked; the exact
SVG paths come from the mockup's pinned Lucide source through existing artwork
tools. These checks do not establish physical raster or touch compliance.

Weather checks compare its four standard rows, whole-row location/key input,
shared raised typing geometry across all six fields and all keyboard pages,
and the single-line automatic-timezone note between the input and keyboard.
They measure the note and real error bands using existing fonts, including
glyph coverage, single-line fit and clearance from the first row or keyboard.
Open-Meteo's Not required key row cannot open a keyboard. Visual Crossing checks
cover masked Configured/Entered states, missing keys, Clear, replacement and
Cancel; successful activation precedes the seven ordinary Weather writes.
Controlled failed checks write nothing and retain the editable draft for retry.
Unchanged provider/key saves make no activation request; switching Open-Meteo
retains a key until explicit clearing is saved. Fresh-process `vc_day`, `vc_night`,
`vc_location`, `vc_clear` and `vc_key` cases cover restart completion, including
closed/replaced/hidden editors. Retained Device edits stay isolated and alarm
hiding/resumption preserves the existing Weather editor and its draft.
These checks do not establish physical Day/Night readability or provider traffic.

Nearby-network checks exercise real LVGL pointer scrolling beyond the first three
rows, actual Open/Secured and dBm captions, scan loading/empty/error states,
Refresh/Manual/Back behavior and stale completion handling. Weather checks cover
actual pending lookup, retained resolved values after failed storage, concise
lookup errors and saved values. Separate fresh-process invocations check Weather
and credentials restart compositions without adding a production reset hook.
The `settings_display_save` invocation checks Display success without restart,
followed by the retained Device/Wi-Fi restart. Closed/replaced-editor Device
success runs confirm that its submitted Save still restarts after its editor is
gone. The UI-shell checks
use real pointer Weather Save, ringing, Dismiss and Alarm Settings actions, then
a controlled delayed lookup completion. Success saves the new timezone and
shows the generic restart notice without restoring the Weather editor; failure
keeps the newer editor usable without stale error text or restarting. A separate
hidden-editor run keeps ringing visible while the existing 900ms restart fires.
Duplicate Save remains blocked. These checks establish substituted timing and
UI behavior, not physical reboot or provider latency.
Display shell checks exercise a saved home/format change and manual navigation
afterward, success while ringing hides the editor, Snooze countdown preservation,
and success after the old editor is replaced. Old completion cannot replace a
newer destination or draft; errors retain the owning Display draft for retry.
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
Repeat checks cover all 128 weekday masks, preset selection and summaries,
the nested Custom Days editor, both appearance palettes, centered day rows and
full day captions. Back/Done retain the shared draft, including None; Save,
Cancel, failure/retry, immediate-toggle isolation and ringing preemption retain
their existing behavior. Custom Days row positions round the reference's two
half-pixel values up, preserving the gap with a half-pixel vertical center offset.
Night checks cover mode descriptions/selections, fixed-schedule boundary notes,
offset direction/steps/limits and brightness label updates in place. Cancel and
nested drafts cause no writes or backlight preview; production saves verify the
actual Alarm and Night values. Generated numeric-font line boxes/baselines and
glyph offsets are checked against the reference font metrics; 32px icon sources,
boxes and tint are checked in both appearances. These native checks do not
establish physical rendering; the mockup’s browser-native slider track is not a fixed raster asset.

The approved October 19 presentation checks also cover one-line long Wi-Fi and
Messaging summaries with complete editor values, the single-line Warning Sounds
and Night Alert Sounds labels, selected radio fills, and the 520px content /
12px gap / 10px outside scrollbar layout. They check actual scrollbar bounds and
overflow visibility, common keyboard proportions, alarm time/period and Snooze
label positions, disabled Snooze colors, the Night unread tint, and natural
centering of alert titles with their expiry text. These are native software
checks; physical LCD and touch appearance remain separate.

Alarm/Sound/Snooze/Solar menu checks compare actual rows, switches, footers,
descriptions, selected checks and icon/text spacing with both reference palettes.
They verify draft-only time/repeat/sound/snooze edits, Alarm Save without restart,
immediate enabled-only writes, failed Save/retry and pending controls. Toggle
checks also verify that the Alarm Settings switch preserves all other drafts,
that Cancel/Back retain a successful toggle, and that task-start/commit failures
leave On/Off unchanged for retry, including with an unfinished repeat-day draft.
Volume checks open the real Sound editor, use Done and save unrelated alarm edits while
preserving stored 0/7/37 values. Actual pointer and keyboard input then checks
five-point steps, 10–100% selections and label updates without rebuilding it.
Preview is enabled when its audio worker accepts requests; hardware readiness
and returned failures use the existing caption/button. Solar uses the applied location
and the Night owner's copied events. Checks cover available/unavailable layouts,
12/24-hour labels, both appearance palettes and in-place event refreshes. Offset
edits preview the displayed times without changing applied preferences or writing
storage; runtime refreshes preserve an open offset editor. The display tests use
controlled illustrative times, separately from the astronomy reference tests.
These checks do not establish playback, physical touch or complete raster equality
with a browser-native range control.

The shell target compiles production `main/ui/ui.c`, `main/ui/clock_views.c`,
`main/ui/weather_views.c` and
the settings editor with real LVGL objects and events. It checks authoritative usable/unavailable time,
backup-clock notice wording, layout and fonts, failed dismissal submission,
duplicate blocking, owner acknowledgment and a different warning arriving while
dismissal is pending. It also checks first-use routing, settings draft retention,
modal gesture blocking, home-view indicators and day/night colors supplied by the
appearance owner, including a snapshot that differs from the settings mode. Both
House Messages edge indicators are exercised through repeated pulse cycles with one-second UI
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
substitutes, so these checks do not establish real-artwork quality, complete
visual parity or physical touch behavior. Crop-placement checks use differently
shaped images at all five logical weather sizes, including crops larger than
the displayed box. They verify original-canvas scale, offsets and pivot, retained
Analog center, tint/opacity and that source changes leave no old pixels by
comparing partial refreshes with full redraws. Actual RGB565A8 artwork and board
checks belong in the [icon sprint](../../../docs/work.md#sprint-trimmed-400px-weather-icons--authorized-october-18-2026). The main task's command queue is
reviewed from source and compiled by the firmware build; the actual network-time
callback is exercised by the connectivity checks.

Leading Zero checks cover both saved 12-hour preferences and both appearances.
The real row shows On/disabled in 24-hour mode, rejects pointer and directly
delivered taps, and restores the current 12-hour draft when switching back.
Cancel/Back discard changes; Save and ordinary configuration reload preserve
the 12-hour preference under saved 24-hour mode. Existing geometry, palettes,
five-entry Display saves and no-restart behavior remain checked. These checks
establish LVGL controls and settings behavior, not physical touch or pixels.

Clock checks cover both configured faces, analog hand rotation, midnight/noon,
12/24-hour and leading-zero formats, and recovery through unavailable Day/Night
surfaces. Night digit size remains fixed across minute and AM/PM changes, with
measured centering, baseline alignment and room for the period inside the round
face; returning to Day restores native font size and position. The existing
135-second paused-clock threshold and its face-specific notice are checked with
controlled stalled-minute snapshots. Pointer gestures and one visible pair of
message/weather edges are checked through unavailable time and recovery. These
object checks do not establish the quality of the scaled raster font or physical
rendering. The unavailable screen includes the saved-alarm caption and the
Alarm Settings shortcut, checked with actual pointer input.

Analog tick checks cover all 60 original absolute endpoints, compact nonempty
bounds, line widths, rounded ends, drawing margins and Day/Night styles. They
also retain the non-clickable line objects. Real-art pixel comparisons and board
redraw measurements for this change are recorded in the [work record](../../../docs/work.md).

Notification checks cover initial waiting and failed retries on both clock
faces and Forecast; retained age boundaries and missing, future or untrusted
timestamps on the clock faces; fixed Forecast stale wording
directly above the primary day; Night visibility, notice positions and no
duplicate footer. Removed backup/alarm technical captions stay absent while
failed submissions, pending actions, live state and retry controls remain
truthful. Playback failure tests retain stopped/error/explicit-retry behavior;
the native logging substitute checks format arguments without capturing output.

Alarm presentation checks compare Ringing and Snoozed objects against the
approved Day/Night geometry, colors and typography, with real pointer input
on Snooze, the ringing slide, snoozed Dismiss, missed notices and Alarm Settings.
The ringing control uses press–move–release tests: track/outside-origin taps,
short/reversed/canceled/lost drags, the completion threshold, pending duplicates,
rejected submissions, stale completions, and ordinary updates during a held drag.
Both appearances check caption bounds and centering, reset animation, applied
dismissal despite a storage error, and the brief confirmation's expiry or
replacement by a new alarm. These checks preserve the existing storage and
navigation assertions; dragging and confirmation add no settings writes.
Captured schedule dates
and times, countdown/deadline formatting, midnight, 12/24-hour and unavailable
time, next/off/missed rows, warning priority and generated countdown glyphs are
checked. Queue-full retry, duplicate blocking, mismatched completion, returned
storage errors, configured-home return and stale completion after navigation
or a new ring use fabricated owner publications. An unsaved active keyboard
survives preemption and resumes; hidden lookup failure and explicit Alarm Save
success cannot hide the ringing page or save an unsubmitted draft. Settings
swipe resume and explicit Alarm-shortcut cancellation of an unrelated draft
are checked separately. These tests establish LVGL object/event behavior,
not actual alarm/task timing, hardware pixels, physical touch or audio. The
Alarm editor previews its unsaved schedule through the real pure calendar calculator.
Fifteen explicit dated cases check before/equal/after, repeat days, midnight/year
boundaries, 12/24-hour formatting and the intended date/minute through spring/fall
changes. Actual controls check draft time/day edits, Cancel without writes, Off,
unavailable/usable RTC time, in-place heartbeat refresh without rebuilding an
editor, and Save/progress/error priority. None checks exercise all-days-cleared
and shortcut selection, enabled Done/Save, reload/Cancel, Today/Tomorrow/exact
due, fixed owner target through midnight, captured snooze preview, consumed
no-next state and automatic Off without losing or reapplying other draft edits.
Day/Night checks use actual LVGL fonts, single-line shortcut labels and round
screen geometry. Alarm text keeps its existing unpadded
12-hour formatter independently of the normal-clock Leading Zero choice.

Normal clock alarm-notice checks include the reported `Tomorrow · 7:00 AM`
caption and longer 12-hour, leading-zero and 24-hour captions in both appearances.
They verify complete single-line text, icon/text containment and centering,
unchanged fonts and touch areas, opening Alarm Settings, and retained missed-row
padding. Actual fonts and LVGL layout are used; physical rendering remains a
board check.

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
Forecast notice wording, boxes and styling, including immediate
repositioning during pointer swipes and preserved home weather-failure notices.
Checks also retain provider-date pairing and the unchanged Night typography,
and verify the approved AM/PM, temperature, High/Low,
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

The worker's actual shared-cache refresh also runs through controlled transport,
HTTP and invalid-JSON failures while hub heartbeats continue. Checks retain prior
messages/unread/importance, enforce the 15-second failed-poll interval, and cover
successful recovery and an immediate explicit refresh. Replacement-hub selection
retains its existing timer behavior.

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
The Python harness also requires **Node.js on PATH** to execute the page's actual
JavaScript with controlled DOM/fetch objects. It checks that HTTP, returned-error,
schema, JSON and network failures retain existing cards and show the existing
error status; recovery, confirmed empty success and Deactivate still work.
Destination-refresh checks preserve selected IDs, labels and subsequent Send
targets after failed responses or valid lists temporarily missing the chosen
room. They also cover updated labels without duplicate options, ordinary
explicit choices, removal of unselected retained rooms, and choices made while
the request or JSON decoding is pending. These use the actual page functions;
their room names and HTTP responses are controlled examples.
These checks do not render a browser or establish live traffic between clocks.

The synchronous acknowledgment operation is private to the message worker; the
UI checks use copied-ID submission and retained completion. Snapshot checks keep
displayed ID, text and formatted metadata, while record priority/sender and
aggregate important/unread state remain covered. A controlled long-sender
response verifies the existing sender limit before shared metadata formatting.

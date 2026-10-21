# ESP32-P4 native checks

These checks compile actual firmware logic on the development computer. They exercise software behavior with controlled inputs; they do not establish physical hardware behavior or live service responses. Limits specific to each suite are listed with it. Controlled failures and fabricated owner snapshots are test examples, not observations of failures on the board.

## Weather and location

Activate ESP-IDF 5.5.5 as described in the [build guide](../docs/build.md#prepare-the-environment-no-board-needed), then run from the repository root:

```bash
scripts/test-weather-parity.sh
```

The retained script name runs only C forecast-representative and location scoring checks against fixtures in `tests/data/`. It checks the [approved provider mappings and daily rules](../docs/weather.md), compound and unknown conditions, current-only exclusions, rain/hail/mixed-snow selection, and state/country qualifiers before provider search limits. Daily cold-weather guesses are checked separately from current classification, which requires reported snow evidence. Constructed rare conditions establish decoding behavior, not their frequency in provider traffic. NWS checks are described below. No Raspberry Pi or JavaScript suite is required; cJSON comes from `$IDF_PATH/components/json/cJSON`.

If the selected macOS compiler is broken, use the installed Command Line Tools compiler for this run:

```bash
CC=/Library/Developer/CommandLineTools/usr/bin/clang scripts/test-weather-parity.sh
```

## NWS alert services and screens

After configuring and building the native suite below, run the focused NWS and screen checks with:

```bash
ctest --test-dir /tmp/esp32-p4-native-tests -R '^(device_config|weather_service|settings_alerts|ui_shell)$' --output-on-failure
```

- `weather_service` compiles the production alert owner and weather worker in `weather_client.c`. Controlled HTTP, clock, audio and storage inputs cover the 55-event catalog and priority, CAP updates/cancellations, shared dismissal, escalation, sounds, expiry, saved events and recovery. Dusk/dawn changes during requests and dismissal during response cleanup check sound eligibility and prevent delayed replay. A captured historical NWS feature supplies real CAP field shapes and protective text; follow-up cases are constructed.
- `settings_alerts` checks catalog scrolling, selection and nested sound drafts; `device_config` checks defaults, validation, saves, reloads and failures. Catalog membership and ordering belong to `weather_service`.
- `ui_shell` checks badges, status marks, headlines and dismissal on both clock faces in Day/Night, plus the Alerts list, scrollable details, navigation and loading/error/save/unavailable states. **No active alerts** appears only after empty success, with text clear of the Back button.

These checks make no live NWS requests and do not establish physical touch, display behavior or alert audio. Approved behavior belongs in the [architecture](../docs/architecture.md#nws-watches-and-warnings).

## Settings, time, backup clock, alarms, Night Shift, connectivity and UI controls

```bash
cmake -S tests -B /tmp/esp32-p4-native-tests
cmake --build /tmp/esp32-p4-native-tests
ctest --test-dir /tmp/esp32-p4-native-tests --output-on-failure
```

Managed dependencies must already be available from a normal firmware build. UI targets use the existing managed LVGL library with a native configuration. CMake registers 49 cases across settings, time, alarm-calendar, alarm-recovery, alarm-command, audio, Night Shift, solar-event, settings-control, UI-shell, NWS/weather service, connectivity and touchscreen-recovery targets. This is the configured suite count, not a report of a test run.

### Optional rendered captures

The capture targets render the production UI with its compiled artwork, fonts and RGB565 software renderer. They are excluded from the normal build and CTest run; they are not firmware build modes. Hardware, network and feature-owner publications use controlled inputs.

```bash
cmake -S tests -B /tmp/esp32-p4-render-review
cmake --build /tmp/esp32-p4-render-review --target test_settings_capture test_ui_capture -j 6
mkdir -p /tmp/esp32-p4-render-review/settings /tmp/esp32-p4-render-review/ui
/tmp/esp32-p4-render-review/test_settings_capture --capture /tmp/esp32-p4-render-review/settings all
/tmp/esp32-p4-render-review/test_ui_capture --capture /tmp/esp32-p4-render-review/ui all
```

Each capture writes an 800×800 PPM and JSON containing text, resolved fonts, ordinary/transformed bounds, colors and control styles. Replace `all` with a named scenario for a focused capture. `WAC_CAPTURE_RADAR_BGRA` optionally supplies an 800×800 BGRA map; otherwise Radar uses a clearly artificial flat sample. `radar-playing` and `radar-paused` capture playback states in Day and Night. Captures establish software layout, not physical panel, touch, brightness, service or audio behavior.

### Settings storage

`test_device_config` compiles production `device_config.c` with typed NVS substitutes and ordinary thread locks. It checks defaults and validation, selected-group saves, merged old/new drafts, concurrent writers and readers, missing keys, returned read/write/commit errors, accepted partial saves and corrected retries. Display writes its five entries and Device its ten remaining entries. Brightness checks cover the 20–100% range and loading low stored values at 20% in memory without rewriting storage or unrelated settings.

The storage substitute deliberately permits keys to change before commit. Partial outcomes illustrate the allowed behavior; they are not physical power-cut evidence.

### Time and backup clock

`test_time` compiles production `clock_time.c` and `ds3231.c` with simulated register access and a substitute system clock. It covers timezone offsets, trusted startup, lost-time evidence, initialization/access/calendar/status failures, correction ordering and retry limits, warning dismissal and new warnings, calendar validation, and snapshots during blocked access.

These checks supply network-completion counts, perform no real synchronization, do not change the development computer's clock and write no user settings.

### Alarm calendar, recovery and commands

- `test_alarm_schedule` compares the production calculator against 36 explicit UTC/date expectations covering repeat masks, equality, boundaries, spring/fall changes and a half-hour daylight-saving transition. Expectations are stated UTC times, not a second implementation of the conversion. The calculator uses the time owner's applied timezone and changes no timezone, settings or state.
- `test_alarm` combines production `alarm.c`, the calculator and native storage. It covers the [approved recovery rules](../docs/architecture.md#alarm-and-snooze), usable-time gating, catch-up/missed notices, snooze/restart/dismissal, backward suppression, Off cancellation, runtime enable/schedule edits and one-time None targets through midnight and daylight-saving changes. Failure and blocked-save cases check immediate copied action state, retained recovery, no tick-based retries and the accepted older-record restart limit after failed arming saves. The reported false Saturday notice is reconstructed as a test example. Settings checks execute conditional None completion: only the matching applied schedule saves enabled-only Off, preserving replacement schedules/preferences. Existing 56-byte recovery records and the optional 16-byte target share the alarm namespace.
- `test_alarm_commands` reuses `test_alarm.c` with the actual private command, result and status-collection helpers extracted from `main.c` at configure time. A copied two-entry queue checks submission without storage, queue-full/stale actions, occurrence identity, failed saves whose live action still applies, and retained one-time completion. Status collection checks fresh clock, message, alarm, audio, Night and alert state when weather cannot be copied.

Native storage failures are controlled examples, not physical power-cut results. The full clock task and heartbeat are not executed; source review and firmware compilation cover their integration, delivery order and short copied read. Speaker output and physical alarm operation remain board checks.

### Audio and Sound Machine

`test_audio.c` includes the actual private worker and shared generator from `audio.c`, without production test hooks. Queue/task, codec, settings and alarm inputs are substituted. Its fresh-process cases cover patterns, playback, priority/cancellation, lazy one-time BSP creation and reopening, latest-command copies, cleanup and returned queue/task/BSP/open/volume/write/close failures. Slow-setup and gated-worker cases check that hardware work occurs outside caller/snapshot locks, Stop/ring priority and new/resumed-ring gain restart. Failed requests do not repeatedly recreate the BSP or retry a ringing occurrence. The alarm owner's separate blocked-save checks establish publication before storage returns; audio consumes controlled phase copies rather than real NVS.

Waveform checks include independent cadence/silence, spectral separation, PCM bounds, volume/ramp behavior and Classic Bell attack, pitch groups, cadence and overlapping decays. Helper/envelope agreement alone is not independent waveform proof. Optimized Classic Beep/Rising Tones are compared with original double equations over five seconds, four volumes and three ramp levels: differences stay within two PCM counts except narrowly checked isolated square zero crossings. Long-position/phase checks cover hours/days and pattern boundaries.

Sound Machine cases cover all timers, live changes, final fade, preview/ring priority, no automatic resume, cancellation and explicit retry after failure. Two million samples per noise/seed, including the firmware seed, check retained state, finite output, mean/RMS and frequency-energy differences. Frozen results from the linked website's source equations check White scaling, formerly clipped Pink/Brown values and Brown's unscaled state; retired Brown limits are absent. Recording checks parse all nine actual MP3 headers/frame lengths against independent sample counts. Simulated decoder PCM checks exact-length repeats, open/close accounting, tail/head writes, live volume/fade and approved recording boosts/noise reductions. dB-derived gain expectations and constructed loud Campfire samples check bounded conversion, not absence of audible clipping.

Audio verification limits:

- Managed decoder API headers compile on the host, but the target decoder library does not run there. Simulated PCM does not prove decoded samples, audible seams or board refill timing. Noise checks do not establish audible character, worst-case peaks or uninterrupted playback.
- Native task substitutes validate allocation arguments; they do not allocate or measure an ESP32-P4 task stack. Native checks do not measure synthesis time, continuity during weather/RTC work or board stack headroom.
- Firmware compilation covers public codec integration and BSP returned-error configuration. It does not establish physical output, loudness, muting, clicks/reopen quality or every hidden register failure. The write API retains its fixed 1000ms wait; short chunks do not guarantee faster cancellation of every hardware fault. The generator uses single precision and integer timing.
- The native logging substitute checks format arguments without capturing output. Listening and board timing remain separate checks.

### Solar events and Night Shift

`test_solar_times` compiles the production calculator without board facilities. Seven dated US Naval Observatory cases cover seasons, hemispheres, leap day, the date line and year boundary within two minutes, plus invalid coordinates, polar no-event results, complete UTC dates and process-timezone independence. Reference URLs accompany fixtures in `test_solar_times.c`. These are calculated comparisons, not observed sunrise/sunset measurements; they do not establish high-latitude season transitions or unusual atmosphere/horizon conditions.

`test_night` combines the production owner and solar calculator. It checks all modes, equal/overnight fixed endpoints, solar boundaries and signed offsets, short-night removal, daylight-saving/local-date/location/timezone changes and backward corrections. Missing time/location/events check startup fallback and running appearance retention. Copied Settings events stay raw and independent of mode/offset. Storage substitutes check appearance-only operating writes, failures/restarts and pending readers; brightness substitutes check readiness, fade steps, delayed/changed targets and retry from the last successful level.

Source review and firmware compilation cover startup/task integration; native checks do not execute the full startup/task or establish physical solar transitions, brightness/fades or populated Settings on the panel.

### Connectivity

`test_connectivity` compiles production `connectivity.c` with network, Wi-Fi and task substitutes. It checks shared initialization/scanning, RAM credentials, overlapping/repeated requests, filtering/empty scans, disconnect cleanup, callbacks during locked operations and returned failures/retries. Zero-wait network-time checks cover startup once, pending sync, initialization retry and actual completion-count advancement.

These controlled concurrency checks do not establish ESP-Hosted behavior or real driver/network timing. Semaphore substitutes check the weather gate's paired take/give use and overlapping requests; they do not model ESP-IDF priority inheritance or scheduling.

### Weather service and provider fixtures

`test_weather_service` includes production `weather_client.c`, its worker and weather calculations with substituted HTTP responses. It checks provider date/temperature/icon pairing, unavailable/corrected time, retained stale payloads, ten-minute refreshes, independent twenty-second weather/time-startup retries (including overlapping slow requests), saved-location gating and failure recovery. Download ages require usable network time; RTC-only time has no age. Routine connection/request progress stays absent from the display.

Both providers use retained real responses plus explicitly constructed current/error/rare-condition inputs. Coverage includes units/fractions, epoch and local/DST/repeated-hour identity, required/optional fields, response bounds, encoded dummy keys, serialized admission, key-specific quota pauses and generic rate/retry/reset recovery. No real credentials or exhausted quota are needed. The separate representative calculation check verifies selected pictures.

To check a separately captured, sanitized full Visual Crossing response, pass its path to `/tmp/esp32-p4-native-tests/test_weather_service` or set `WAC_WEATHER_LIVE_RESPONSE`. This reads a file and makes no live request. Transport is substituted; connectivity checks exercise synchronization with network substitutes. Neither establishes live HTTP, certificate validation or network-time traffic.

### Radar decoding, playback and retained data

The weather-service target uses bundled LVGL LodePNG with constructed PNG/HTTP inputs. It checks 512px smoothed precipitation requests separately from the 800px basemap, bounds, palette/alpha handling, unknown-color rejection, recoloring/sampling/blending, seams/wrapping, 75-mile coverage and alignment. Worker cases cover independent five-minute checks and 100ms preparation turns, unchanged frames, stale/failure/retry behavior, acknowledged image retirement, binary-buffer growth/failure, retained pending copies, compressed-history reuse, chronological playback/newest hold, Pause races, resume/Latest and controls during checks/offline operation. Palette checks span every source-table row/alpha.

After the native build above, `/tmp/esp32-p4-native-tests/test_weather_service --radar-only` runs the focused constructed checks. `/tmp/esp32-p4-native-tests/test_weather_service --radar-saved-data <capture-folder>` processes retained Leander/Colorado 800×800 USGS maps and required RainViewer tiles, writing raw BGRA images and codec-memory JSON reports beside them. Capture filenames are listed in `check_saved_radar_data` in the test.

`/tmp/esp32-p4-native-tests/test_weather_service --radar-saved-tiles <capture-folder>` checks six retained 512px smoothed tiles through the actual request/decode/ recolor/sample/blend path. Expected filenames are `z7-X-Y-512-smoothing1-snow0.png` for 27/46, 28/53, 24/50, 32/49, 35/53 and 35/54. It makes no metadata assumptions; the capture report records provider/frame identity. The flat background and half-native-pixel sampling are constructed references, not matching USGS geography or a finished screen.

The optional six-frame check uses the versioned [Radar dataset](data/radar-six-frame/README.md). Run from the repository root:

```bash
/tmp/esp32-p4-native-tests/test_weather_service --radar-six-frame-data tests/data/radar-six-frame
```

It compares six captured Florida frames/twelve tiles against complete reference images, including playback completion and newest-image hold without more HTTP requests. Inputs are included; no local review folder is needed. It writes no output files. All saved-data commands make no live requests. Provider samples and data-only previews are evidence, not finished-screen or physical-board verification.

Native allocation uses a 16MiB LVGL heap and the firmware's 4MiB decoder cap; firmware sends large image/decoder allocations to PSRAM. These checks do not measure board memory, decode latency, stack use, live HTTPS, animation speed or audio continuity.

### Settings controls and presentation

`test_ui` compiles production `ui/settings.c` with real LVGL objects/events. Coverage is grouped here; assertions and scenario names remain in the tests.

- **Drafts and saves:** nested Done/Cancel, separate group drafts, shared Wi-Fi drafts, deferred/duplicate saves, validation, failure/retry, stale completion, immediate Alarm enable and Display saves without restart. Device/Wi-Fi and Weather restart cases use separate processes; `settings_display_save` checks Display success followed by Device/Wi-Fi restart. Closed/replaced editor saves still restart. `vc_day`, `vc_night`, `vc_location`, `vc_clear` and `vc_key` cover Visual Crossing restart completion after editor changes.
- **Text and keyboard:** all six fields, actual key rectangles, layout switching, held editing, popovers/hide, masks/punctuation and the four reference key maps. Only Device, Weather and Wi-Fi Credentials allocate keyboards. Checks retain field/focus/cursor/drafts through runtime/appearance changes, validate required fields, mask passwords and check lengths, round-panel fit, raised typing/error bands and restoring rows/errors without extra work on Close. Messaging checks whole-row input, distinct-ID guidance and shared-mode default-ID rejection.
- **Weather and networks:** provider/key state, explicit clearing, activation before the seven Weather writes, no writes on failed activation, unchanged-key request avoidance and Open-Meteo's noneditable key row. Network checks cover pointer scrolling, security/dBm labels, scan states, Refresh/Manual/Back and stale results; resolved locations survive failed storage for retry.
- **Pickers and sliders:** five-minute Alarm/Night arrows through noon/midnight, arbitrary saved minutes without opening-time rounding, all 128 repeat masks, nested Custom Days/None, Night modes/offsets and no unsaved backlight preview. Alarm/Sound Machine volume and Day/Night brightness use real pointer and keyboard inputs for five-point steps, touch bounds, live labels and endpoints. Off-step stored values survive opening/unrelated saves; Alarm volume preserves stored 0/7/37, while new selections use 10–100%. Brightness clamps at 20%; Sound Machine retains 0–100% and immediate-save retry.
- **Appearance:** both palettes, reference rows/headers/footers/icons/fonts, full editor text with one-line summaries, common keyboard proportions, scrollbar fit and the approved [slider geometry](../docs/architecture.md#ui-and-visual-reference). Numeric font metrics/glyph offsets, 32px icon sources/tint, curved keyboard masks from the mockup's pinned Lucide paths and live/unavailable clock glyphs are checked. Custom Days rounds two half-pixel positions up, retaining their gap with a half-pixel center offset.
- **Alarm, Solar and Sound Machine:** draft-only edits, enabled-only writes, failure/retry, previews and readiness/error updates, applied-location/raw-event Solar display with unsaved offset preview, Sound Machine choices/timers, scrolling, immediate preferences and Day/Night shortcuts. All weekday captions are measured for shortcut clearance. Preview/hide/navigation/ringing checks retain drafts, definitely Stop despite stale/ring-masked copies, and perform no preview settings writes or codec work under the display lock.
- **Leading Zero:** 24-hour mode shows On/disabled, rejects taps and restores the 12-hour draft on return. Cancel/Back, Save/reload, five-entry Display saves and no-restart behavior remain covered.

Network/tasks/navigation callbacks, owner copies and background saves are substituted here; the shell target also checks first-use routing. Solar display uses illustrative times, separate from astronomy references. Object/pointer and captured-pixel checks do not establish physical rendering, readability, legibility, touch compliance, provider traffic/latency or playback. Substituted restart timing does not establish physical reboot or restart navigation on the board; connectivity substitutes do not establish radio timing. The browser-native slider track is not a fixed raster asset; these checks do not establish complete raster equality with that control.

### UI shell, clock faces, Forecast and Radar

`test_ui_shell` compiles production `ui.c`, `clock_views.c`, `weather_views.c` and Settings with real LVGL objects/events and supplied owner snapshots.

- **Routing and asynchronous work:** first use, modal gestures, home indicators, owner-supplied appearance, nested drafts/keyboards across ringing, copied warning/message identity, rejected/duplicate/stale actions and retained completion after navigation. Weather Save during ringing/changed editors checks timezone save, the generic restart notice and the existing 900ms hidden-editor restart; failure leaves the newer editor usable. Display completion preserves the current destination, drafts and Snooze countdown. Main's queue/integration are source-reviewed and firmware-compiled; connectivity exercises the actual network-time callback with substitutes.
- **House Messages:** both pulse indicators across one-second refreshes, tints/hiding, taps/swipes, newer arrivals, retry/success and leaving/reopening. LVGL press cancellation sends no acknowledgment; later completed taps work. The worker is substituted. These are controlled inputs, not physical touch-read failures.
- **Clock faces:** [approved spacing](../docs/architecture.md#clock-display), dates/status/alarm rows, analog rotation and all 60 nonclickable tick endpoints, 12/24-hour/leading-zero formats, unavailable time and recovery, saved-alarm shortcuts and Day/Night colors. Controlled stalled snapshots check the 135-second paused-clock notice. Digital Night checks all 4,320 hour/minute/format combinations with actual fonts/objects: steady nominal 250px scaling, round-screen bounds, baseline/AM-PM fit and restored Day layout. Alarm notices include `Tomorrow · 7:00 AM` and longer formats with complete, centered single-line text and unchanged touch areas.
- **Weather/Forecast:** provider-date/temperature/icon pairing across invalid, corrected/stale time, midnight, month/year/leap/DST boundaries and truthful weekday/`Tomorrow` labels. First-download waiting hides fallback pictures; failures/retained data/recovery check notices during swipes. Synthetic RTC/ network copies check Waiting for sync and warning priority. Provider arc credits use the payload provider, including retained/invalid weather and settings disagreement. Crop tests span all five logical sizes, comparing partial refreshes with full redraws to check scale/pivot/tint and old-pixel removal. Ordinary shell images are substitutes.
- **Radar:** fixed weather-page order, U.S. availability/location gating, removal of hidden images on country change and re-enablement, header/key/footer fit, frame/old/unavailable states, marker center/outline/fill pixels and unchanged surroundings, safe image retirement and alarm preemption. Actual Play/Pause hit targets, swipes, loading/disabled states and lifecycle stop/Latest are exercised; alert activity does not reorder pages.

These checks do not establish real-artwork quality, complete visual parity, scaled-font sharpness, physical clipping/appearance/touch, board redraw time or concurrent board performance. Supplied snapshots are not observations of weather failures, synchronization or rendering on the board. Fractional LVGL geometry matches firmware, but software credit pixels do not prove physical legibility.

### Alarm screens and editor

Shell checks compare Ringing/Snoozed and heading geometry/fonts/colors in both appearances. Real pointer events exercise Snooze, slide-to-dismiss, snoozed Dismiss, missed notices and Alarm Settings. Drag cases cover invalid origins, short/reversed/canceled/lost drags, threshold, pending/rejected/stale results, updates during a hold, reset animation and confirmation expiry/new-ring replacement. Applied dismissal survives storage error and adds no settings writes.

Fabricated owner publications cover captured dates/deadlines/countdowns, unavailable time, warning priority, queue-full retry and return/navigation races. Ringing preserves an unsaved keyboard/draft; hidden lookup or Save completion cannot hide it or submit an unsaved draft. Alarm shortcuts return to their entry point, including unavailable time and interrupted child editors; explicit shortcut cancellation and Settings-swipe resume are checked separately.

The editor previews its unsaved schedule through the real calendar calculator. Fifteen explicit dated cases cover due boundaries, repeat days, midnight/year and spring/fall changes. Controls cover edits/Cancel, Off, RTC availability, heartbeat refresh without rebuild and progress/error priority. None cases cover cleared days, shortcut selection, Today/Tomorrow/exact-due targets, fixed target through midnight, snooze preview, consumed no-next state and automatic Off without losing other edits. Alarm text retains its unpadded 12-hour formatter independent of normal-clock Leading Zero.

These are LVGL object/event checks, not actual alarm/task timing, physical taps, hardware pixels or audio; supplied state does not change board settings.

### Touchscreen recovery

`test_touch` extracts the actual private read callback from `main.c`, using the installed touch header and real LVGL. Hardware-read substitutes check every returned error (including `ESP_ERR_INVALID_STATE`), zero points, failure-only cached-point clearing and fresh-sample recovery. An interrupted real LVGL button press must cancel without clicking, then accept later clicks; successful held samples retain their behavior.

The BSP owns startup; source review and firmware compilation cover callback installation under the initial display lock. These checks do not reproduce the board's I2C interrupt sequence, run BSP tasks or establish physical gestures and shared-bus recovery. The BSP's unlimited touch wait is unchanged: a read that never returns cannot be recovered by this callback.

## Message acknowledgement, browser listing, discovery and shared handoff

With `IDF_PATH` set as above:

```bash
python3 tests/test_message_ack.py
```

The harness compiles production functions with native network/task-lock/HTTP substitutes and runs the page's actual JavaScript with controlled DOM/fetch objects. **Node.js must be on PATH** for those page checks.

- **Acknowledgment and handoff:** displayed identity and eligibility, copied IDs, busy-owner submission, queued/running/unconsumed-result rejection, retained completion and local/shared success/failure. Snapshots retain ID/text/metadata, record sender/priority and aggregate important/unread state, including the sender-length limit. The synchronous operation remains private to the worker; UI checks use submission and retained completion.
- **Worker and cache:** one actual worker iteration at a time checks retained changes through disconnect/reconnect, HTTP/transport/JSON failures, 15-second retry/poll spacing, coordinator changes, success without unchanged replay, explicit refresh and retained prior messages/unread/importance. Serialization/ merge covers content, expiry, IDs, acknowledgment/deactivation and local changes during outgoing transfer. Finite RAM/full-store merge limits and replacement-hub timer behavior remain.
- **Discovery:** captured sender bytes cover its five fields, escaping, packet-size limit, allocation/encoding/send failure cleanup and no partial or oversized send. Received values use actual cJSON; sockets are substituted.
- **Browser and routing:** production list/snapshot/action/forwarding checks all-target listing, per-clock filtering, dismissed/expired/deactivated records, ordering and exact path/query/body forwarding. `deviceId=all&includeDismissed=true` lists all targets while ordinary polling still filters a clock named `all`. Production query encoding/decoding and ESP-IDF's extractor run through the hub handler with reserved characters, spaces, literal `%26` and a 63-byte ID expanding to 189 bytes. They check identity/forwarding headers, local fallback and HTTP 400 for malformed/NUL/overlong/oversized selectors.
- **Page recovery:** HTTP/schema/JSON/network/returned errors retain cards and show error status; recovery, confirmed empty success and Deactivate work. Destination refresh preserves selection/labels/Send targets through failures and temporarily missing rooms, updates labels without duplicates, removes unselected retained rooms and respects choices during fetch/JSON completion. Room names and responses are controlled examples.

Source review and firmware compilation cover worker/heartbeat integration, including consuming results only after acquiring the display lock. The heartbeat itself does not run here; handoff checks exercise retained owner completion. These checks do not render a browser or establish physical input latency, live hub behavior, discovery between boards or live traffic between clocks.

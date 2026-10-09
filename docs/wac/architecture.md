# Weather alarm clock architecture and product specification

Revised October 8, 2026. This is the single authoritative specification for approved product behavior, defaults, accepted tradeoffs, module/resource/task ownership, integration and source placement. It is not an implementation sequence or evidence of completed firmware. Undecided behavior is not implicitly approved.

Build the weather alarm clock for the Waveshare ESP32-P4-WIFI6-Touch-LCD-3.4C using the existing ESP-IDF application. Only ESP32-P4 is supported. There is no requirement to maintain Raspberry Pi behavior, another firmware implementation, or backward compatibility with their settings or interfaces.

Keep the existing weather, storm, location selection, Wi-Fi and House Messages capabilities, including the browser messaging page. No multiple-alarm feature, ambient-light sensor, battery-voltage monitor, outage log or separate diagnostics screen is introduced.

[AGENTS.md](../../AGENTS.md) defines working rules. [README.md](../../README.md) provides the project overview, implementation status, hardware, setup and user guide. Detailed build and test instructions remain in [targets/esp32-p4/README.md](../../targets/esp32-p4/README.md) and [targets/esp32-p4/tests/README.md](../../targets/esp32-p4/tests/README.md).

- [Product behavior and defaults](#product-behavior-and-defaults)
  - [Editing and saving user settings](#editing-and-saving-user-settings)
  - [Alarm and snooze](#alarm-and-snooze)
  - [UI and visual reference](#ui-and-visual-reference)
  - [Night Shift](#night-shift)
  - [Time and RTC](#time-and-rtc)
  - [Weather](#weather)
    - [Location resolution](#location-resolution)
    - [Weather icons](#weather-icons)
  - [House Messages](#house-messages)
- [Implementation architecture](#implementation-architecture)
- [Verification and limits](#verification-and-limits)

## Product behavior and defaults

### Editing and saving user settings

Use ordinary saves with ESP-IDF storage.

Cancel discards edits. Nested Done returns to the parent draft without saving. Alarm Save applies without restart; the Settings-entry alarm toggle applies immediately. Device Setup and Wi-Fi share pending edits for Save & Restart; Weather Location has its independent Save & Restart, including location, coordinates, timezone and units. Save only changes made in that editor, preserving unrelated applied choices. Validation errors remain in the editor; save failure retains the draft and does not automatically restart.

- Validate settings, perform ordinary saves, and report returned errors.
- Save user settings only when the user explicitly applies a change: for example, pressing Save or using a control intended to apply immediately, such as enabling an alarm.
- Editing a draft, refreshing weather, and normal clock operation must not rewrite user settings. There is no background autosave.
- If power fails during a save, some settings may be updated while others remain unchanged. The user may need to check and save them again. This is an accepted limitation, not a guarantee that the user will check.
- Do not add application-level backup snapshots, selectors, rollback, or a recovery coordinator to make settings saves all-or-nothing.

Creating initial defaults is a separate initialization action. Operating-state persistence is separate from user settings; approved operating-state behavior is defined below.

Device/display defaults are room name `Clock`, device ID `clock-esp32-p4`, digital face, 12-hour time, Leading Zero Off, imperial units and no Wi-Fi profile. Defaults and permitted choices in this specification are not inferred from preview samples.

### Alarm and snooze

One recurring alarm uses the saved local hour/minute and repeat days. For a skipped or repeated local time, use the UTC offset after the transition without changing the saved schedule. A skipped 2:30 AM in a one-hour spring jump therefore rings at 1:30 AM before the jump; a repeated fall time rings at its second occurrence. Preserve the intended schedule date when identifying a ringing occurrence. Backward clock corrections must not replay an already handled occurrence.

Defaults: alarm Off, 07:00 Monday–Friday, Classic Beep at 50% volume and ten-minute snooze. Snooze choices are 5/10/15 minutes. Alarm operation continues while menus are open and slow network work is pending.

Classic Beep, Gentle Chime and Rising Tones share sound definitions between preview and ringing. Ringing increases volume gradually. Preview stops after approximately five seconds or cancellation; ringing takes priority.

Use one fixed 30-minute catch-up window for ordinary alarms and snooze. Evaluate recovery once usable time is available from the RTC or network. An eligible unhandled alarm within the window rings; an older missed occurrence produces a missed notice.

- Save the snooze deadline when Snooze is pressed, and update it on subsequent snoozes. Clear it when dismissed or finished.
- If the snooze deadline is still ahead, resume waiting until that deadline.
- If the snooze deadline has passed by 30 minutes or less, ring when usable time becomes available.
- If it is more than 30 minutes overdue, discard the snooze.

Use the same overdue-deadline check for ordinary alarm recovery and snooze recovery; do not introduce separate configurable policies.

Remember which scheduled ringing was dismissed so, after its dismissal record is successfully saved, a restart does not replay it through the 30-minute catch-up rule. This refers to the individual occurrence of the one recurring alarm (for example, Monday at 7:00 AM), not multiple alarm schedules. Dismissal does not disable future scheduled alarms. Store only the information needed for this behavior, not an alarm-history log.

Keep one pending missed-alarm notice across restarts, including the missed alarm's date/time for display. Clear it when the user dismisses the notice or the next alarm actually rings. Dismissing the notice does not disable or change the recurring alarm schedule. This is a pending notice, not a list or history of missed alarms.

Use ordinary storage operations and error reporting for the snooze deadline, dismissal state, and pending missed-alarm notice. Do not introduce backup snapshots, a journal, migration, or a separate recovery coordinator for this feature. These operating-state writes do not authorize background rewriting of user settings. Operating-state persistence not explicitly approved in this document remains undecided.

Apply Snooze, Dismiss and missed-notice transitions in memory even if their operating-record write returns an error. Snooze defers the sound and Dismiss stops it without waiting for a successful save. Report the storage failure and retain the new state for the current run. A later restart may recover an older stored deadline, dismissal or missed notice; an alarm may ring again or a notice may reappear. This is an accepted limitation of failed operating-record saves. Settings Save follows the separate [editing and saving rules](#editing-and-saving-user-settings).

### UI and visual reference

Ringing preempts menus, notices and previews; preemption alone does not acknowledge a notice or apply a draft.

The single approved visual entry point is [mockups/alarm-design.html](../../mockups/alarm-design.html). It contains the complete combined device preview and the House Messages browser preview, selected using the controls above the previews. Images, fonts and both previews are embedded in that file. It owns appearance and controls; this specification owns behavior, defaults and source placement. Preview selectors and sample values are not production features. The digital ringing layout is the chosen ringing presentation; carousel alternatives are not additional screens.

### Night Shift

Night Shift modes remain Off, Always On, Fixed Schedule and Sunset to Sunrise. Fixed equal endpoints mean Night all day; a range across midnight is supported. Solar events are calculated locally with saved offsets. While running, retain the current effective appearance when required inputs/events become unavailable. Apply gradual backlight transitions, including startup.

Defaults: Off, fixed endpoints 22:00/06:00, zero solar offsets and Day/Night brightness 80%/15%. Solar-offset choices are -120..+120 minutes in five-minute steps; brightness choices are 5..100%.

Remember the last effective Day/Night appearance for startup fallback.

- Calculate appearance from the saved mode and available inputs whenever possible. Off selects Day and Always On selects Night without requiring time or location.
- At startup, when a scheduled mode lacks the inputs needed to calculate appearance, use the last saved Day/Night appearance. If no usable saved appearance exists, use Day.
- Always apply the current configured brightness for the selected appearance; do not save or restore a historical brightness value.
- Persist only effective Day/Night changes, not clock ticks, draft previews, or brightness-fade steps.
- Use ordinary storage operations and error reporting. A failed appearance save does not prevent the display from changing; a later restart may restore the older saved appearance. Do not add snapshots, a journal, or a recovery coordinator for this value.

It does not authorize background rewriting of user settings.

### Time and RTC

Network time is primary; trustworthy RTC time can start the clock while synchronization proceeds. The RTC stores UTC. An observed stop flag makes RTC time untrustworthy; failed access is a distinct connection warning. No compile-time substitute for the actual time: without usable time, show unavailable time and wait before triggering alarms. Keep the backup oscillator enabled and correct the RTC following successful network synchronization.

Do not persist RTC warnings or their acknowledgments across restarts. Keep a detected warning in memory for the current run, including after network time corrects the RTC, until the user dismisses it. At each startup, check the RTC again and report problems detected then; do not restore an earlier warning from storage.

Inspect the RTC for lost-time evidence before correcting it, distinguish lost time from failed access, and clear the hardware stop flag only after successful correction. Correcting the RTC must not depend on saving a warning.

### Weather

#### Weather Contract

- Weather source: Open-Meteo
- Refresh interval: every 10 minutes
- Until live weather data arrives, render unavailable measurements (`--`) and loading labels; do not present preview sample values as actual weather.
- If live weather fails:
  - continue serving the last known good payload when available
  - show `Weather updated <age>` or `Weather data stale`
- Current condition rendering includes:
  - current temperature
  - high/low temperatures
  - icon selection from weather code plus day/night state
- Icon names, mappings and assets are defined by the [weather icon contract](#weather-icons).

#### Forecast Contract

- Forecast view uses five upcoming days
- Each day includes:
  - representative icon
  - high
  - low
- The displayed forecast icon does not blindly use Open-Meteo daily `weathercode`
- Instead, daytime hourly samples from `08:00` through `20:00` are summarized

#### Conditions Contract

- Inputs: `current_weather` (live wind speed/direction) plus hourly
  `pressure_msl`, `wind_speed_10m`, `wind_gusts_10m`, `precipitation`,
  `precipitation_probability`, `snowfall`, `weathercode`, `temperature_2m`
  (in the configured temperature unit), plus daily `temperature_2m_max`, requested with
  `wind_speed_unit=kmh`; everything is computed in km/h, hPa, mm and
  converted for display (mph/inHg/in for imperial)
- Pressure trend is the 3h change (falling/rising beyond 1.5 hPa, otherwise
  steady -- wide enough to ignore the ~1 hPa twice-daily atmospheric tide); before 03:00 local, the next 3h forecast change is used instead
- Alerts consider now through the next 3h:
  - `thunder`: weathercode 95/96/99
  - `gusts`: gusts >= 40 mph / 64 km/h, else `wind`: sustained >= 25 mph / 40 km/h
  - `pressure`: 3h change <= -3 hPa
  - `precip`: >= 7.6 mm in an hour (`Heavy snow` if snowfall, else `Heavy rain`)
- `summary.headline` / `summary.detail`: the first alert as a sentence with
  its timing (`now` or `by <hour>`), the second alert (or the pressure
  meaning) as the detail.
- Things already happening say when they end, things on the way say when
  they start: `Thunderstorms until 8 PM`, `Gusts up to 58 mph until 9 PM`,
  `Heavy rain until 5 PM`, `Rain ending by 4 PM` vs `Thunderstorms likely by
  4 PM`. Quiet-day priority: raining now (ending or ongoing), thunder later,
  a front (`Turning much colder by 5 PM`: a drop of 15F / 8C within any 3h
  of the outlook, so evening cooling doesn't count), gusts over the alert
  threshold later, rain likely, chance of rain, a big day-to-day swing
  (`Much colder tomorrow`: tomorrow's high 15F / 8C off today's), falling
  pressure (`Dry for now` / `Falling pressure can mean rain later`), winds
  easing, then `Breezy and dry` / `Calm and dry`
- `summary.changeLevel` is `changing`, `may-change` or `steady` (concrete forecast change, early hints, or no change); `summary.changing` is true only for `changing`
- Times follow `timeFormat` (`6 PM` or `18:00`)

#### Location resolution

These rules define the ESP32-P4 user-visible behavior for turning a text location into Open-Meteo latitude, longitude, and timezone values.

##### First-time weather location setup

Start with no saved weather location or coordinates. During first-time setup, use the existing Weather Location editor with an empty location field. The user must select and successfully save a valid location before setup can finish. Location lookup supplies the coordinates and timezone; do not supply a default location.

Reuse the existing editor and its validation/error handling. Do not add “Not configured” menu labels or missing-location presentations across the normal clock and weather screens. The mockup work is limited to the existing editor's empty first-use state and its place in setup; it has not yet been implemented.

##### Preferred User Input

- Prefer `City, State` for US locations.
- Two segments can also specify a city and country name or code, such as `Paris,FR`. The provider interprets the second segment as a country or first-level administrative area. When an abbreviation denotes both, the administrative-area interpretation takes precedence; use the third segment for an explicit country filter.
- An optional two-letter country code may be supplied as a third segment:
  - `Cincinnati,OH,US`
- ZIP code entry is not part of the primary contract.

##### Search Rules

- Weather/location provider: Open-Meteo geocoding API
- Request shape:
  - endpoint: `https://geocoding-api.open-meteo.com/v1/search`
  - `name=<city>[, <second-segment qualifier>]`; retain the qualifier so the provider narrows results before applying `count`
  - `count=10`
  - `language=en`
  - `format=json`
  - include `countryCode=<CC>` when the third segment is a two-letter country code

Reuse the existing US state-abbreviation expansion where applicable. For example, `Cincinnati,OH,US` requests `name=Cincinnati,Ohio` with `countryCode=US`; `Paris,FR` requests `name=Paris,FR`. Do not reduce a qualified query to its city name. The provider's [documented location-search rules](https://open-meteo.com/en/docs/geocoding-api) define qualifier matching and apply country/administrative-area filters before the result limit.

##### Ranking Rules

For each candidate returned by the qualified search:

- exact place-name match: `+100`
- place name contains requested name: `+30`
- exact state/admin1 match: `+80`
- exact country code match: `+20`
- population bonus: `min(floor(population / 1000), 25)`

Highest score wins among those qualified results. If the qualified search returns no results, report that the location was not found and retain the editor; do not silently select a place in another requested state or country.

##### Output Rules

- `lat`: selected result latitude
- `lon`: selected result longitude
- `timezone`: selected result's timezone identifier, with local rules available in the existing timezone mapping. A missing or unsupported timezone is a lookup/validation failure; retain the editor and do not save the location. `auto` is a weather-server request option, not a saved clock timezone.
- resolved label:
  - `name, admin1, country_code`
  - omit missing segments

#### Weather icons

Canonical weather icon assets live in:

- `shared/assets/icons/`

Canonical icon names are the SVG basenames, currently:

- `clear-day`
- `clear-night`
- `cloudy`
- `fog`
- `partlycloudy-day`
- `partlycloudy-night`
- `rain`
- `showers-day`
- `showers-night`
- `sleet`
- `snow`
- `thunderstorm`
- `thundersnow`

Current-condition icon selection uses the shared canonical names:

- `0` -> `clear-day` or `clear-night`
- `1`, `2` -> `partlycloudy-day` or `partlycloudy-night`
- `3` -> `cloudy`
- `45`, `48` -> `fog`
- `51` through `57` -> `showers-day` or `showers-night`
- `61` through `65` -> `rain`
- `66`, `67` -> `sleet`
- `71` through `77` -> `snow`
- `80` through `82` -> `showers-day` or `showers-night`
- `85`, `86` -> `snow`
- `95`, `96`, `99` -> `thunderstorm`
- any `thundersnow` override -> `thundersnow`

Forecast icon selection uses the same canonical names after the representative daytime forecast heuristic has already chosen the forecast weather code.

ESP32-P4 rendering path:

1. Open-Meteo current weather or representative forecast code
2. canonical shared icon name
3. ESP32 icon lookup adapter
4. generated LVGL image descriptor
5. `lv_image_set_src()`

### House Messages

#### Message Contract

- Message polling interval: every 15 seconds
- Each device has:
  - `deviceId`
  - `roomName`
- Modes:
  - `single` (default)
  - `shared`
- Shared mode requires a user-selected device ID distinct from every other shared clock. Reject the default device ID when saving Shared mode; that ID is defined under [Editing and saving user settings](#editing-and-saving-user-settings). The settings editor explains that each shared clock needs a distinct ID, and `device_config` validates the Shared-mode default-ID restriction. No automatic identity generator or network uniqueness checker is required.
- Messages support:
  - `text`
  - `sender`
  - `target`
  - `priority`
  - `expiresAt`
- Important messages:
  - sort ahead of normal messages
  - light the edge indicator with an important state
- Message dismiss action:
  - acknowledges the current message for the local device
  - returns to the previous home view

#### Shared-Clock Message Behavior

- Shared mode discovers a LAN message hub over UDP broadcast
- One device becomes the hub if none is found
- Hub selection is deterministic by device ID sort order
- Shared clients proxy message API operations to the current hub

## Implementation architecture

### Structure

Keep the existing ESP-IDF application and its weather, location, connectivity, timezone, messaging, board support, and artwork. Add the clock features and our single UI under `main/wac/`, within the existing `main` component. No new component hierarchy or generic application framework is needed.

Copy and adapt useful rendering code into our UI. Replace its setup flow, navigation, touch handling, and status presentation where required by the approved design. At UI integration, replace `main/app_ui.c` with the files listed below. `main/app_ui.h` remains the sole public UI interface, implemented directly by `wac/ui/ui.c`; change that interface and its callers together when needed.

### Ownership

UI screen assignments follow the [approved visual reference](#ui-and-visual-reference).

| Owner | Responsibility and authoritative state |
|---|---|
| `main.c` | Startup and application task creation. Its existing weather worker owns refresh/retry timing, the last successful weather payload and fetch/stale status. The UI heartbeat assembles copied owner snapshots; UI selects their presentation. Alarm, time-trust and appearance decisions remain with their feature owners. |
| `device_config.c/.h` | Applied user settings, defaults, validation, serialized settings writes and publication after successful saves. Settings UI owns drafts. |
| `connectivity.c/.h` | Network initialization, connection and scan state, and network-time synchronization transport/completion. Time trust and system timezone application belong to `wac/time`. |
| `weather_client.c/.h` | Weather requests and response decoding. Refresh scheduling and the retained payload belong to the weather worker in `main.c`. |
| `forecast_representative.c/.h` | Representative forecast calculation; no live weather state. |
| `storm_conditions.c/.h` | Conditions and storm-summary calculation; no live weather state. |
| `location_lookup.c/.h` | Geocoding requests and lookup results. `device_config` owns saved location values. |
| `location_scoring.c/.h` | Query parsing, candidate ranking and label formatting; no saved location state. |
| `timezone_table.c/.h` | Timezone mapping data used for validation and conversion. `wac/time` applies the selected mapping to the system clock's local-time rules. |
| `weather_icon_names.c/.h` | Condition-to-icon mapping and rendering resources, including generated image assets; no weather state or refresh decisions. |
| `message_service.c/.h` | Message records, acknowledgments, shared-client cache, hub role/discovery, message network operations and browser delivery. UI owns device navigation. |
| `main/web/messages.html` | House Messages browser rendering and controls, outside the device LVGL views. `message_service` serves the page and owns message records and operations. |
| `wac/time.c/.h` | Time source/trust, system timezone application, RTC synchronization and in-memory RTC warning. Uses the system clock; connectivity supplies actual network synchronization completion. |
| `wac/ds3231.c/.h` | RTC register access through the board's existing I2C bus. RTC trust/correction decisions belong to `wac/time`; board facilities own the bus. |
| `wac/alarm.c/.h` | Live alarm state, snooze, dismissal, missed notice and their operating-record storage. Occurrence calculations belong to `alarm_schedule`. |
| `wac/alarm_schedule.c/.h` | Recurring-occurrence and DST calculations; no live alarm state, storage or execution task. |
| `wac/audio.c/.h` | Sound definitions, speaker/codec access, playback, volume progression, preview lifetime and ring/preview priority. Alarm owns whether ringing is required. |
| `wac/night.c/.h` | Effective appearance, its remembered value and backlight transitions. Saved preferences belong to `device_config`; solar calculations belong to `solar_times`. |
| `wac/solar_times.c/.h` | Solar-event calculations and unavailable-event results. `night` decides their effect on appearance. |
| `wac/ui/ui.c` | UI lifetime, active view and navigation, including return destinations, gestures and first-use entry; overlays, alarm preemption, input routing and presentation selection. Touch actions go to the correct feature owner; feature decisions remain with those owners. |
| `wac/ui/clock_views.c` | Rendering and object handles for analog/digital faces, unavailable time, ringing/snoozed clock surfaces, next alarm, missed alarm, clock status and backup-clock indicator/notice. Active-view and navigation state belong to `ui.c`. |
| `wac/ui/weather_views.c` | Rendering and object handles for forecast, conditions and storm treatment; messages and empty/unread/important states; weather warnings and clock-edge indicators supplied to clock views. Weather/message state belongs to the service owners; navigation belongs to `ui.c`. |
| `wac/ui/settings.c` | Rendering and controls for Settings entry; Device Setup; Display and Device settings; Weather Location; Wi-Fi list, credentials and progress; Alarm, Time, Repeat, Sound and Snooze editors; Night Shift mode, fixed/solar schedule, offsets and brightness. Owns drafts, nested editing, validation/save/lookup presentation and submission of user changes. Applied settings belong to `device_config`. |
| `wac/ui/controls.c` | Shared LVGL rows/buttons, styling, time input and keyboard helpers; no saved settings, active-view state, feature decisions/state or navigation authority. |

A copied value is a snapshot for a consumer, not another authority. Runtime fields about time and appearance come from their owners. Screens do not calculate alarm eligibility, infer RTC health, or drive the backlight. New clock-feature headers declare their own data; the existing `app_runtime.h` remains the display snapshot described below. There is no general `wac_types.h` container. DS3231 access reuses the board bus and existing driver facilities without creating a second bus or an application fork of downloaded drivers.

Scan failures, validation errors, keyboards, previews, day/night styles and waiting states are states of the assigned views. They do not require separate files or managers. Keep only one set of each live UI object's handles and one active-view owner. `wac/ui/ui_private.h` contains declarations shared only by these UI files, not a second state model.

### Execution

Retain the existing network/message workers, LVGL task, and UI heartbeat. Add two long-lived tasks:

- A clock-control task, started by `main.c`, runs time maintenance, alarm evaluation, and Night Shift updates by ordinary calls to their owners. It never waits for the display lock, network requests, or audio streaming. Evaluate due alarms before other optional work. RTC calls use finite timeouts; brightness fades advance in steps, not a sleep loop.
- The audio worker performs playback. Alarm requests take priority over preview requests, and stop/snooze remains responsive during playback.

Settings saves, Wi-Fi scans, and location lookup use short-lived background work, following the existing scan/lookup approach. A pending operation disables duplicate submission; completion is handed back as data, not a pointer to a screen that may have closed. Slow settings writes do not hold the lock used to read the current configuration. There is no permanent storage worker or general job system.

UI callbacks submit commands and render results; they do not perform network requests, flash writes, or RTC access. A small fixed command queue delivers alarm/notice actions to the clock task; audio has its own playback commands. Commands carry copied values with explicit lifetimes. A full queue returns failure rather than silently losing a Dismiss or Snooze. These queues are for those actual task boundaries, not an event bus.

For House Messages dismissal, UI submits copied displayed-message and device IDs to the existing message worker. `message_service` performs local acknowledgment or the shared-hub request and returns completion data. UI waits for successful acknowledgment before returning to the prior home view; failure retains the message and is reported. Completion must not override alarm preemption or navigate a view the user has already left. `ui.c` owns this navigation; `message_service` owns the acknowledgment and updated message snapshot. Disable duplicate dismissal while pending and report submission failure. No additional permanent worker or general job system is needed.

The existing UI heartbeat gathers owner snapshots before taking the display lock. Only UI code under the display lock changes LVGL objects. Keep state-copy locks short; never hold them during I/O. Hardware reads and small operating-record writes can take time; this design does not promise uninterrupted real-time execution during flash operations. Measure interaction and alarm latency during implementation rather than adding speculative workers now.

### Configuration and storage integration

Use the existing NVS partition and ordinary typed NVS operations. Settings stay with `device_config`; alarm operating data stays with `alarm`; the remembered appearance stays with `night`. Owners use distinct names/keys and call NVS directly. There is no shared persistence wrapper, whole-settings JSON codec, snapshot selector, migration path, journal, or cross-service partition-recovery protocol.

Extend the existing configuration structure for the approved settings. Use one Night Shift mode value, not both a mode and an independent enable flag. Derive readiness from the settings needed by the feature without a separate setup-version gate or location-confirmed flag. `device_config` validates saved location values against the [location output rules](#output-rules) and Shared-mode identity against the [message contract](#message-contract); the settings UI enforces the [first-time setup decision](#first-time-weather-location-setup) using the existing editor. Initialize the enhanced application independently of original-firmware settings, as required by the compatibility scope in AGENTS.md.

Use explicit update operations for the actual Save actions. The settings UI owns its current draft and the values with which editing began; nested editors share that draft. `device_config` applies only the submitted changes to its latest configuration. Comparing the known fields is sufficient; no reusable session manager, revision system, or public generic patch format is required. Alarm preemption does not make a draft authoritative. Closing/canceling an editor must not publish it.

Serialize settings writers. Validate before writing, check NVS return values, and publish a new in-memory configuration after a successful save. Use a short lock for copying the published configuration, distinct from the slow save operation. On failure, follow the [editing and saving rules](#editing-and-saving-user-settings) and keep the previously applied running configuration; do not report rollback. Some stored keys may already have changed and will be read on the next normal startup, as allowed under [Editing and saving user settings](#editing-and-saving-user-settings).

Alarm data needs only the current relevant occurrence, snooze, dismissal and one pending missed notice, not a history log. Use a small explicit representation for those fields. Failed operating-record writes follow the [alarm and snooze recovery rule](#alarm-and-snooze). Use one last-effective-appearance value in the Night Shift owner. Warning persistence is governed by [Time and RTC](#time-and-rtc) and introduces no record. These operating records are not part of settings Save and do not rewrite preferences.

Initialize NVS once in startup. A reported initialization failure must be visible and must not trigger a blanket erase/retry. Stop storage-dependent startup on that failure rather than adding an automatic repair service. Normal missing settings receive defaults; an actual read error is reported rather than treated as a missing key.

Apply configured Wi-Fi credentials through RAM driver storage in common preparation for both scanning and connecting. `device_config` is the application credential authority; connectivity must not create a second persistent credential copy. Check support through the existing ESP-Hosted setup during implementation.

### Existing-code changes that matter

- `main.c`: connect owner startup/tasks, remove the unconditional normal-operation backlight-on override, and stop treating an RTC-initialized date as proof of network synchronization.
- `connectivity.c/.h`: preserve networking and scanning; deliver actual synchronization completion to the time owner and apply the credential rule above. Inspect both the caller and `connectivity_sync_time()` shortcut so RTC startup does not suppress network synchronization.
- `wac/time.c/.h`: apply the selected system timezone using the existing mapping; move timezone application out of connectivity and UI paths so this owner controls it.
- `location_lookup.c/.h` and `location_scoring.c/.h`: preserve the query qualifier and enforce the location search/output rules above.
- `message_service.c/.h` and `main/web/messages.html`: provide the background dismissal path and adapt the existing browser page to its approved visual reference. Keep message records, cache, hub operations and browser delivery with the existing service.
- `app_runtime.h`: extend the existing display snapshot only as needed; remove competing time/appearance decisions from consumers. Update `main.c` consumers to read published configuration copies instead of a concurrently mutated global.
- `device_config.c/.h`: extend the single model and ordinary save operations. Do not carry over original setup migration or compatibility gates. During configuration implementation, remove the persistent diagnostic boot counter, its storage operations and startup-log field. No approved product behavior requires it.
- `main/CMakeLists.txt`: list the actual new sources directly, replacing `app_ui.c`; no additional source-list layer is needed.
- Existing asset/font scripts and `main/assets/`: reuse their conversion approach and existing assets. Add only artwork or glyphs required by the approved UI, not a new asset pipeline.

During UI integration, remove the old benchmark/parity renderers, their alternate startup paths, and files, assets and build options used only by those modes. Measure the actual application with existing ESP-IDF/LVGL timing and memory facilities when needed; do not retain separate experimental UIs. This retires the old layer-by-layer and alternative-renderer experiments without changing approved product behavior. Existing messaging, including its browser page and message storage, retains its own owner; no broad storage cleanup is implied.

### Planned file structure

Paths are relative to the repository root. This is the structural allocation, not an instruction to create empty files. Existing files that still serve the application or its development remain in place. Pure calculation files are kept separate because their date/astronomy logic can be tested without the board. UI files group related behavior instead of assigning one file to every screen.

```text
targets/esp32-p4/
  main/
    main.c                              Startup, application tasks and weather refresh
    app_runtime.h                       Copied status for display
    app_ui.h                            Sole public UI interface
    device_config.c / .h                Settings defaults, validation and NVS saves
    connectivity.c / .h                 Wi-Fi connections/scans and network time
    weather_client.c / .h               Weather requests and response decoding
    forecast_representative.c / .h      Representative forecast calculations
    storm_conditions.c / .h             Conditions and storm summaries
    location_lookup.c / .h              Geocoding requests and lookup results
    location_scoring.c / .h             Location query parsing, ranking and labels
    timezone_table.c / .h               Timezone mapping data
    weather_icon_names.c / .h           Weather code to icon-name mapping
    message_service.c / .h              Message records, hub and browser operations
    web/
      messages.html                     House Messages browser rendering and controls
    CMakeLists.txt                      Direct firmware source list
    wac/
      time.c / .h                       Time trust, timezone and RTC synchronization
      ds3231.c / .h                     DS3231 register access on the board I2C bus
      alarm.c / .h                      Alarm, snooze and missed notices; NVS records
      alarm_schedule.c / .h             Recurring alarm and DST calculations
      audio.c / .h                      Alarm/preview sounds, volume and playback
      night.c / .h                      Day/Night appearance, fallback and backlight
      solar_times.c / .h                Sunrise/sunset and unavailable-event calculations
      ui/
        ui.c                            Navigation, alarm preemption and input routing
        ui_private.h                    Declarations shared only by these UI files
        clock_views.c                   Clock, ringing and clock-status rendering
        weather_views.c                 Weather, message and indicator rendering
        settings.c                      Settings editors, drafts and save/lookup feedback
        controls.c                      Shared LVGL controls and input helpers
      assets/
        wac_icons.c / .h                Generated only for needed added icons
        wac_fonts.c / .h                Generated only for needed added glyphs
  tests/
    README.md                           Test commands and instructions
    wac/
      CMakeLists.txt                    Clock-feature test source/build definitions
      test_support.c / .h               Shared hardware substitutions only when used
      test_device_config.c              Settings validation and save-failure checks
      test_time.c                       Time trust, timezone and RTC correction checks
      test_alarm.c                      Alarm/snooze, dismissal and restart-recovery checks
      test_alarm_schedule.c             Recurring occurrences and DST boundary checks
      test_audio.c                      Sound preview and ringing-priority checks
      test_night.c                      Day/Night, fallback and brightness checks
      test_solar_times.c                Solar-event and unavailable-event checks
      test_ui.c                         Navigation, draft and alarm-preemption checks
```

In this tree, `name.c / .h` denotes the two exact filenames with that stem. No source files are created by this architecture revision. Concrete asset inputs can use the existing asset tools; their required filenames will be identified from the selected graphics, rather than inventing assets now.

## Verification and limits

When updating weather/location tests and build instructions, retain useful ESP32-P4 fixtures and native C checks, and remove the requirement to run or maintain Raspberry Pi/JavaScript equivalents. Reuse needed assets and fixtures in their existing locations; a `shared/` path does not impose another supported platform.

Existing fixtures for native C checks:

- Forecast: [forecast-representative-cases.json](../../shared/test-data/forecast-representative-cases.json).
- Conditions: [storm-conditions-cases.json](../../shared/test-data/storm-conditions-cases.json).
- Location: [location-resolution-cases.json](../../shared/test-data/location-resolution-cases.json).

During implementation, use focused tests for settings validation/save errors, DST and catch-up boundaries, dismissal/snooze/missed-notice restart behavior, RTC status/correction, solar/fallback/brightness decisions, preview preemption, and draft/navigation behavior. Include qualified location searches, rejection of unusable timezones, Shared-mode identity validation and background message-dismissal success/failure. Test ordinary interrupted-save outcomes against the accepted partial-save behavior and operating-record failures against their accepted in-memory/restart behavior.

Build with the preserved ESP-IDF 5.5.5 environment. Hardware is required to establish touch behavior, rendering, RTC operation, audible sound, brightness and physical power-cut results. Task timing, stack/memory use and asset sizes are implementation checks, not claims established by this document.

Enhancement implementation has not begun. This plan is not evidence of passing firmware tests.

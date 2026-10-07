# ESP32-P4 Weather Alarm Clock Architecture

Status: Architecture plan. Updated October 7, 2026.

**The architecture keeps our enhancements in a separate application directory, retains one configuration system, and uses only the layers needed to separate responsibilities.**

Keep the existing weather retrieval, location lookup, Wi-Fi, messaging, timezone conversion, and artwork. Add our timekeeping, alarm, audio, Night Shift, and redesigned screens under `targets/esp32-p4/main/wac/`.

The new code remains part of the existing ESP-IDF `main` component—the application’s current build unit. Separate directories and narrow interfaces provide isolation without reorganizing upstream code into additional build components.

The main boundaries are:

- Existing services continue providing weather, connectivity, location, and messaging.
- New services own time trust, alarms, audio, and enhanced Night Shift.
- A replacement UI implements the existing `app_ui.h` interface directly.
- The existing configuration module owns all user settings.
- One private persistence helper supports settings and internal operating records, keeping those categories logically separate.

Use ordinary function calls, small command queues where work must run in the background, and copied status snapshots. A general event framework is not part of this design.

**Replace the UI implementation while preserving its existing public interface.**

Today, `app_ui.h` exposes two functions: one creates the interface and one updates it. Our new `wac/ui/ui.c` implements those functions directly.

The enhanced firmware compiles `wac/ui/ui.c` as its UI implementation. Retain the original `app_ui.c` unchanged as a reference, excluded from the enhanced build. There is no original-UI build switch.

For original-firmware evaluation, preserve the original source and flashable firmware separately before implementation.

There is no forwarding-only `app_ui_bridge.c` or second public UI interface.

This avoids repeatedly modifying the original large UI file for every redesigned menu, warning, keyboard, and alarm screen. Future upstream UI fixes must still be reviewed and deliberately incorporated into our replacement. Preserving the original file reduces merge conflicts but does not eliminate that maintenance.

**Use the following proposed file structure for the additions.** Paths are relative to `/Users/Scott/Projects/ESP32 Weather Alarm Clock`. Existing files outside this tree remain in place.

The tree defines the intended organization, not a requirement to create every file immediately. Add a header, helper, or worker only when the implementation needs that boundary. Closely related code may remain together until a concrete need justifies splitting it. Preserve the ownership rules regardless of the number of files.

```text
targets/esp32-p4/
├── main/
│   └── wac/
│       ├── sources.cmake
│       ├── wac_app.c
│       ├── wac_app.h
│       ├── wac_types.h
│       │
│       ├── persistence/
│       │   ├── persistence.c
│       │   └── persistence.h
│       │
│       ├── time/
│       │   ├── time_service.c
│       │   ├── time_service.h
│       │   ├── ds3231.c
│       │   └── ds3231.h
│       │
│       ├── alarm/
│       │   ├── alarm_service.c
│       │   ├── alarm_service.h
│       │   ├── alarm_schedule.c
│       │   └── alarm_schedule.h
│       │
│       ├── audio/
│       │   ├── audio_service.c
│       │   └── audio_service.h
│       │
│       ├── night/
│       │   ├── night_service.c
│       │   ├── night_service.h
│       │   ├── solar_times.c
│       │   ├── solar_times.h
│       │   ├── brightness_service.c
│       │   └── brightness_service.h
│       │
│       ├── ui/
│       │   ├── ui.c
│       │   ├── ui_private.h
│       │   ├── ui_model.c
│       │   ├── ui_model.h
│       │   ├── navigation.c
│       │   ├── navigation.h
│       │   ├── settings_session.c
│       │   ├── settings_session.h
│       │   ├── theme.c
│       │   ├── theme.h
│       │   ├── layout.h
│       │   │
│       │   ├── screens/
│       │   │   ├── screens.h
│       │   │   ├── digital_clock.c
│       │   │   ├── analog_clock.c
│       │   │   ├── forecast.c
│       │   │   ├── conditions.c
│       │   │   ├── messages.c
│       │   │   ├── time_unavailable.c
│       │   │   ├── settings_entry.c
│       │   │   ├── device_setup.c
│       │   │   ├── display_settings.c
│       │   │   ├── device_settings.c
│       │   │   ├── weather_location.c
│       │   │   ├── wifi.c
│       │   │   ├── alarm_settings.c
│       │   │   ├── alarm_ringing.c
│       │   │   ├── alarm_snoozed.c
│       │   │   └── night_settings.c
│       │   │
│       │   └── widgets/
│       │       ├── menu_controls.c
│       │       ├── menu_controls.h
│       │       ├── time_picker.c
│       │       ├── time_picker.h
│       │       ├── keyboard.c
│       │       ├── keyboard.h
│       │       ├── clock_status.c
│       │       ├── clock_status.h
│       │       ├── backup_notice_view.c
│       │       └── backup_notice_view.h
│       │
│       └── assets/
│           ├── generated_sources.cmake
│           ├── wac_icons.c
│           ├── wac_icons.h
│           ├── wac_clock_fonts.c
│           └── wac_clock_fonts.h
│
├── assets-src/
│   └── wac/
│       ├── alarm.svg
│       ├── alarm_off.svg
│       ├── backup_warning.svg
│       └── clock_fonts.json
│
├── tools/
│   └── wac/
│       └── generate_assets.py
│
└── tests/
    └── wac/
        ├── CMakeLists.txt
        ├── test_support.c
        ├── test_support.h
        ├── test_device_config.c
        ├── test_config_consistency.c
        ├── test_persistence.c
        ├── test_time_service.c
        ├── test_backup_notice.c
        ├── test_alarm_schedule.c
        ├── test_alarm_recovery.c
        ├── test_audio_policy.c
        ├── test_night_service.c
        └── test_ui_model.c

docs/
└── wac/
    ├── architecture.md
    ├── storage-contract.md
    ├── behavior-contract.md
    └── screen-state-map.md
```

Substantial clock, weather, and message screens retain individual files. Closely related settings screens are grouped:

| File | Screens and states included |
|---|---|
| `wifi.c` | Network list, manual credentials, scanning, errors, connection progress, and restart presentation |
| `alarm_settings.c` | Alarm settings, repeat days, Sound & Snooze, sound selection and preview controls, and snooze duration |
| `night_settings.c` | Night Shift settings, mode selection, fixed schedule, solar schedule, offsets, and brightness |

The shared time-picker widget serves both alarm and Night Shift editing.

These groups share internal declarations through `screens.h`. A display state such as “No Networks Found” does not require its own source file. Files can be split later if their size or responsibilities justify it.

All new externally visible C names use a `wac_` prefix to avoid collisions with future upstream additions.

**Each area has a clear owner without requiring a separate forwarding layer.**

| Area | Responsibility |
|---|---|
| `wac_app` | Starts services, routes background commands and configuration updates, and collects status snapshots; feature services retain all feature decisions |
| Existing `device_config` | Owns settings meaning, defaults, validation, format conversions, and the public load/save interface |
| `persistence` | Encodes, reads, and writes records and provides verified write guarantees; callers own record meaning and conversion policy |
| `time` | Owns trustworthy-time status, DS3231 access, and backup-clock notices |
| `alarm` | Owns recurrence, ringing, snooze, dismissal, missed alarms, and restart recovery |
| `audio` | Owns the speaker, the three sound definitions, and previews |
| `night` | Owns schedule calculation, effective appearance, and physical brightness |
| `ui` | Presents service state and submits user commands |
| `assets` | Contains our added icons and fonts; existing artwork remains shared |

`wac_app` coordinates work but does not duplicate the alarm, time, audio, or Night Shift policies owned by the feature services. `device_config` decides how settings are interpreted and converted; the persistence helper carries out storage operations without inventing defaults or migration policy.

Use existing public service interfaces directly where they already fit. Add translation only where necessary, rather than wrapping every upstream service in a general bridge.

Screen callbacks submit commands through `wac_app` when work must run outside the UI. A separate forwarding-only `ui_actions` layer is unnecessary. `ui_model` remains because it makes actual presentation decisions, such as choosing between a weather age and an unknown-age warning.

**Separate responsibilities do not require a background task for every module.**

Alarm scheduling has its own lightweight task so it cannot be delayed by weather requests, settings screens, or display activity. Audio has its playback worker.

Time maintenance and Night Shift calculations may share a background worker. Their work must remain bounded, and RTC or storage delays must not block alarm scheduling. Slow network operations must not monopolize that worker.

The UI never decides when an alarm rings. Background services never modify LVGL screen objects directly.

Network requests, persistent writes, RTC access, and audio streaming run outside UI callbacks and the display lock. The UI receives results through copied status snapshots and command results.

**All user settings use one configuration model and interface.**

Extend the existing `device_config_t` rather than introducing a competing settings structure.

| Settings group | Contents |
|---|---|
| Existing device settings | Wi-Fi, location, timezone, units, home screen, time format, leading zero, and messaging |
| Alarm | Enabled, hour, minute, repeat days, snooze duration, volume, and sound |
| Night Shift | Mode, fixed start/end times, sunrise/sunset offsets, and day/night brightness |

`device_config.c` owns the complete configuration. Its public interface is the only route for loading or changing user settings.

The private persistence helper does not own another settings model. Screens never call it directly.

This intentionally requires more editing in `device_config.c/.h` than the original separate-store proposal, in exchange for one coherent configuration system.

**Keep storage changes additive and each Save all-or-nothing.**

Continue using NVS, the ESP32’s persistent flash storage. Preserve existing configuration keys and their meanings where practical.

- Add keys or explicitly encoded records for new settings.
- Supply documented defaults when fields are missing.
- Preserve saved preferences when defaults change.
- Convert values only when their representation or meaning changes.
- Use format versions where defaults alone cannot handle a change.
- Preserve stored fields the running firmware does not own.
- Detect unsupported newer formats before attempting incompatible writes.

Do not write the raw bytes of `device_config_t`. Changes to the in-memory C structure must not silently change the persistent format.

**Each Save must commit all requested settings together or retain the complete previously committed configuration.** This includes changes spanning settings groups, such as Wi-Fi Save & Restart and the pending Device Setup edits it includes. An interrupted save must never leave a mixture of old and new settings.

Use the simplest verified storage mechanism that meets this requirement. A general pending-save journal is not prescribed. The mechanism must cover the entire Save operation, including existing keys; consistency within individual records alone is insufficient.

Do not claim all-or-nothing behavior merely because there is one save function or one NVS commit. The required guarantees, recovery rules, and implementation verification criteria are in [storage-contract.md](storage-contract.md).

Publish the new configuration to services, report save success, and perform any requested restart only after the complete configuration is durably committed. If power fails after that commit but before the UI reports success, startup may correctly load the complete new configuration.

**Compatibility remains centralized.**

For Night Shift, import the original enabled flag and fixed schedule once, when the new Night Shift configuration is absent. Subsequent boots use the new configuration rather than repeatedly importing the old flag.

Leading Zero defaults to **Off** for new or missing settings. Existing saved choices remain intact.

These rules reduce migration work; they do not promise automatic compatibility with every future format or firmware downgrade. Supported compatibility rules belong in `storage-contract.md`.

**Settings and internal operating records share a helper but remain logically separate.**

The same private persistence helper supports:

| Record category | Owner and purpose |
|---|---|
| User settings | `device_config` owns preferences and schedules |
| Handled alarm occurrence | `alarm_service` prevents a dismissed occurrence from ringing again after restart |
| Interrupted snooze | `alarm_service` recovers using the agreed grace period |
| Missed-alarm notice | `alarm_service` retains it until acknowledged or cleared by the next ringing occurrence |
| Backup-clock notice | `time_service` preserves it through correction and restart |
| Last effective day/night appearance | `night_service` restores appearance when schedule inputs are unavailable; brightness comes from current committed settings |

Use distinct records and typed operations for these categories. Saving settings must not overwrite alarm history or warning acknowledgments. Dismissing a warning must not rewrite device preferences.

Alarm recovery policy stays in `alarm_service`; backup-notice policy stays in `time_service`. The persistence helper stores records without deciding what they mean.

No periodic timestamp checkpoint or additional flash partition is proposed for this scope. Storage capacity remains an implementation verification item.

**Settings edits retain explicit save boundaries.**

The configuration module serializes changes and publishes copies of committed settings. Services do not read editable UI drafts.

`settings_session` retains pending edits and tracks the fields changed. Saving applies those edits to the latest committed configuration rather than replacing unrelated settings with an older screen snapshot.

The user-facing behavior is:

- Alarm **Save** applies immediately without restarting; Cancel discards edits.
- Nested **Done** buttons return to the parent without prematurely saving.
- The Settings entry alarm toggle changes On/Off immediately while preserving the schedule.
- Device Setup and Wi-Fi share their pending device-settings draft.
- Wi-Fi **Save & Restart** includes those pending changes.
- Weather Location retains its independent save flow.
- Existing restart-based device changes keep their established behavior.

Alarm configuration changes reach the alarm service after a successful save.

**Timekeeping has one authoritative status.**

`time_service` distinguishes:

- Whether usable, trustworthy time is available.
- Whether network synchronization has actually succeeded.
- Whether the backup RTC is trustworthy.

`time_service` is the authority for time trust and network-synchronization status. Existing runtime fields exposed to the UI must reflect its snapshot rather than independently treating a plausible date as proof of trustworthy or network-synchronized time.

The DS3231 stores UTC. The existing timezone conversion supplies local clock and recurring-alarm times.

At startup, inspect the RTC before network synchronization can overwrite evidence of lost time. Persist a detected loss-of-time notice before correcting the RTC and clearing its stop flag.

After every successful network synchronization, arrange an RTC update. The network callback notifies the service; it does not perform RTC writes directly.

A usable RTC date must never prevent network synchronization from starting. Both the existing caller and synchronization function need their current plausible-date shortcut corrected.

The production UI never uses the compile-date fallback. Without trustworthy time, it shows the agreed unavailable-time presentation and keeps the alarm saved but inactive.

**Backup-notice state belongs to the time service.**

`time_service` owns the persistent warning and distinguishes:

- **Backup Clock Lost Time — Check Battery.**
- **Backup Clock Unavailable — Check Connection.**

Successful time correction restores normal clock and alarm operation without erasing an undismissed notice.

The notice opens only when the user taps its indicator and has one **DISMISS** action. Dismissal acknowledges the event; it does not establish that the battery or connection is healthy.

A read/write failure is not treated as proof that the oscillator-stop flag was set.

The separate `backup_notice_view` widget remains responsible only for presentation and touch interaction.

**Alarm scheduling and recovery have one service owner.**

`alarm_service` continues operating while settings are open, weather retrieval is delayed, or the display is busy.

| File | Responsibility |
|---|---|
| `alarm_schedule.c` | Calculates eligible occurrences from local time, repeat days, and the saved schedule; handles clock corrections and DST boundaries |
| `alarm_service.c` | Controls disabled, armed, ringing, and snoozed states; accepts Snooze and Dismiss; owns occurrence history and recovery policy |

The service uses the persistence helper for durable records rather than a separate alarm-history layer.

Recovery uses the agreed fixed **30-minute grace period**, including interrupted snoozes. An already dismissed occurrence stays dismissed after restart.

A missed-alarm notice never disables the next alarm. Dismissing it clears only the notice; the next actual ringing occurrence also clears it.

DST and network clock corrections belong in scheduling policy, not screen code. For a skipped or repeated local alarm time, use the offset in effect after the transition, matching the following day's time. Thus a skipped 2:30 AM in a one-hour spring jump rings at 1:30 AM before the jump; a repeated fall time rings only on its second occurrence. The saved time and repeat days remain unchanged. Compute spring occurrences before the jump. The 30-minute recovery grace is measured from the resolved occurrence, not the nonexistent local time. See [behavior-contract.md](behavior-contract.md) for the complete rule.

**One service owns the speaker and its sound definitions.**

`audio_service` uses the existing Waveshare board support and codec facilities. It contains the short definitions for:

- Classic Beep.
- Gentle Chime.
- Rising Tones.

Previews and alarms use the same definitions. Preview stops after approximately five seconds or when cancelled. Actual ringing takes priority over preview, and gradual volume increase is controlled here.

The UI never writes audio samples. No new UI library or general-purpose audio framework is proposed.

**Night Shift separates calculation, appearance, and brightness without requiring separate tasks.**

`night_service` supports Off, Always On, Fixed Schedule, and Sunset to Sunrise.

`solar_times` calculates sunrise and sunset locally using saved coordinates and trustworthy time. It reports unavailable inputs or dates without a sunrise/sunset explicitly rather than inventing times.

`brightness_service` is the sole owner of physical backlight changes, including gradual transitions. The current startup call sets brightness to 100%; startup must hand control to this service without subsequently overriding its selected brightness.

The UI consumes one effective day/night state. Individual screens do not calculate separate schedules.

If a schedule cannot be calculated, retain the current effective appearance and use its currently configured brightness. At startup, restore the last saved effective day/night appearance; if none exists, use Day appearance and configured Day Brightness. Off and Always On do not need time or location and take precedence over this fallback. See [behavior-contract.md](behavior-contract.md).

**The UI shares layout rules, navigation, and status presentation.**

`theme.c` and `layout.h` define common colors, typography, row heights, circular-screen boundaries, and button spacing.

The shared `time_picker` serves alarm time and fixed Night Shift times. The shared keyboard supplies the complete password symbol set and consistent error placement.

`navigation.c` owns:

- Swipe navigation between existing views.
- Swipe-down entry into Settings.
- Return to the saved home clock face.
- Return to the screen that opened Alarm Settings.
- First-use setup when Wi-Fi or location is missing.
- Ringing-alarm priority over menus and notices.

Touch controls and swipes are coordinated here. The existing full-screen gesture layer cannot simply cover the new tappable alarm and warning controls.

`navigation` decides which screen or overlay is active, including ringing-alarm preemption. `ui_model` supplies the content for that active view; it does not change navigation.

`ui_model` selects what occupies the shared clock status row: a clock warning, missed-alarm notice, snooze information, or next-alarm indicator. `clock_status` renders that selection and handles its touch target by submitting the corresponding command. Temporarily hiding a notice does not acknowledge or delete it.

`ui_model` also translates actual service state into other presentation choices. Stale weather with a known update time differs from stale weather whose age cannot be established. Mockup example values never become production status.

The digital clock owns the **enlarged, centered night layout** and consistent AM/PM alignment. The analog clock preserves the approved below-date alarm placement. Both share status and warning behavior.

**Limit changes to existing files to the following integration points.**

All paths in this table are under `targets/esp32-p4/`.

| Existing file | Planned change |
|---|---|
| `main/CMakeLists.txt` | Include our sources, replace `app_ui.c` with `wac/ui/ui.c` in the compiled source list, and declare direct dependencies |
| `main/main.c` | Connect service lifecycle, configuration publication, corrected synchronization decisions, and brightness ownership; revise NVS initialization/error handling so startup preserves recoverable settings and operating records instead of automatically erasing them |
| `main/connectivity.c` | Start network synchronization even with usable RTC time and report confirmed synchronization events |
| `main/connectivity.h` | Expose the small synchronization interface needed by the application |
| `main/device_config.c` | Own all settings, defaults, validation, compatibility, loading, saving, and committed configuration |
| `main/device_config.h` | Extend the configuration model and expose controlled configuration access/update operations |

Alarm, audio, and brightness services remain inactive in the existing benchmark and parity-lab modes.

These areas remain in place:

- `app_ui.c` and `app_ui.h`.
- `app_runtime.h`.
- Weather, forecast, storm, location, and timezone modules.
- Messaging services and the browser messaging page.
- Existing generated artwork and fonts.
- Raspberry Pi code.
- Downloaded board-support components.

The DS3231 attaches to the board support package’s existing I²C bus, shared with touch and audio. Our code does not create a competing bus or modify downloaded driver sources.

**The behavior decisions are recorded in supporting contracts.**

| Decision | Owner | Agreed rule |
|---|---|---|
| Skipped or repeated DST alarm time | `alarm_schedule` | Use the post-transition offset for that occurrence; keep the saved schedule unchanged. See [behavior-contract.md](behavior-contract.md). |
| Night Shift startup without usable schedule inputs | `night_service`, with `brightness_service` applying its result | Restore the last effective appearance with its current configured brightness; without a saved appearance, use Day and Day Brightness. See [behavior-contract.md](behavior-contract.md). |
| Interrupted settings save | `device_config`, supported by `persistence` | All changes in each Save commit together, or the previous configuration remains. See [storage-contract.md](storage-contract.md). |

These product decisions are settled. The persistence mechanism still requires implementation evidence that it meets the agreed contract.

**Keep assets and documentation separately owned without repeating architecture information.**

Our asset generator reuses the existing conversion approach and Montserrat font source. It produces uniquely named files under `wac/assets/`, leaving upstream generated assets untouched.

Replacement screens must carry forward relevant existing rendering behavior—including analog hand pivots, opaque backing layers, indicator offsets, and font alignment—not merely reuse the image files.

| Document | Purpose |
|---|---|
| `architecture.md` | File structure, ownership, exact upstream integration points, and what to review when incorporating upstream updates |
| `storage-contract.md` | Storage keys and records, defaults, format versions, conversions, verified interruption behavior, and compatibility limits |
| `behavior-contract.md` | Time trust, alarm recovery, save semantics, notice lifetime, and Night Shift rules |
| `screen-state-map.md` | Every approved screen and state mapped to its source file and the current mockup |

The proposed tests cover configuration compatibility, interrupted saves, separation of settings from operating records, time trust, alarm recovery, audio priority, Night Shift, and UI state selection. Test files may remain focused on individual behaviors even when the production implementation shares a source file.

These are planned verification files, not tests already performed.

The scope includes the existing weather, storm, messaging, and setup presentations alongside the enhancements. It excludes the discarded standalone Missed Alarm and Backup Clock diagnostic screens, multiple alarms, battery-voltage monitoring, ambient-light sensing, and power-outage history.

# Weather Alarm Clock Storage Contract

Status: Required storage behavior. Updated October 7, 2026. The physical encoding and power-interruption guarantees require verification during implementation; this document does not claim they are already implemented.

[architecture.md](architecture.md) defines ownership and the file structure. [behavior-contract.md](behavior-contract.md) defines alarm, notice, Night Shift, and save behavior. All durable data uses the existing NVS flash-storage system through one private persistence helper. There is one public configuration model, owned by `device_config`; screens do not access storage directly.

## Each Save is all-or-nothing

A Save starts with the latest committed configuration, applies the submitting session's edited fields, and validates the complete candidate. Configuration updates are serialized so concurrent actions cannot overwrite unrelated committed changes with an older draft.

The entire Save is one unit. After a power interruption or restart, the device must recover either:

- The complete previously committed configuration, if the new configuration did not reach its durable commit point.
- The complete new configuration, if it did reach that point.

Never load a mixture. This guarantee includes existing settings and new settings, even when they occupy different keys or records. In particular, Wi-Fi Save & Restart includes the pending Device Setup edits in the same unit. Alarm time, repeat days, enabled state, sound, volume, and snooze edits included in one Save must also take effect together.

“Durable commit” means the point after which recovery is guaranteed to select the complete new configuration. Publication to services, success acknowledgment, and any requested restart follow that point. Power can fail between commit and acknowledgment; the new configuration may then be present even though the user did not see success. No user choice between old and new is required after reboot.

On validation failure, leave storage and committed state unchanged. On storage failure, do not publish a partial candidate or report success. Retain the editable draft. If the storage result is uncertain, resolve which complete configuration is committed before accepting further configuration changes or publishing a replacement configuration. Existing alarm scheduling must not wait on storage I/O.

Use the simplest mechanism proven to provide this behavior. A single save function, a sequence of individually reliable writes, or one `nvs_commit()` call is not by itself proof that a multi-key Save is all-or-nothing. Explicitly encoded records may be used, but consistency of each record alone is insufficient for a Save spanning records. Any recovery metadata belongs inside the private persistence implementation; it is not a second user-settings model or public storage API.

Recover the committed configuration before publishing settings to services at startup. An incomplete candidate must not cause defaults to overwrite a recoverable previous configuration. Unexpected corruption is a separate failure from an interrupted valid save: report the failure and preserve recoverable data rather than silently describing partial data as a successful load.

This preservation requirement also applies before configuration loading. The existing `main.c:init_nvs()` automatically erases NVS on `ESP_ERR_NVS_NO_FREE_PAGES` or `ESP_ERR_NVS_NEW_VERSION_FOUND`; revise that startup path so an initialization error does not automatically erase settings or operating records before recovery can inspect them. If safe recovery is unavailable, report the storage failure and leave the data intact. The recovery mechanism remains an implementation choice. These NVS initialization errors are distinct from ordinary save-time capacity errors and unsupported application-record formats.

## Existing configuration and compatibility

The current configuration namespace is `rwd_cfg`. This inventory describes current keys, not a second proposed schema:

| Current keys | Meaning |
|---|---|
| `device_id`, `room_name` | Device identity and room |
| `location`, `timezone`, `lat`, `lon`, `loc_ready` | Weather location, timezone, coordinates, and readiness |
| `wifi_ssid`, `wifi_pass` | Wi-Fi credentials |
| `msg_share` | Message sharing preference |
| `clock_face`, `time_fmt`, `units`, `lead_zero` | Display preferences |
| `setup_ver` | Existing setup/version marker; do not silently repurpose it as a new record-format version |
| `night`, `night_st`, `night_end` | Legacy Night Shift enabled flag and fixed schedule |
| `boot_count` | Existing operating counter; preserve its meaning and keep it outside user-editable drafts |

Preserve existing keys and meanings where practical, while meeting the whole-Save guarantee. If an authoritative encoded record is necessary, conversion is centralized in `device_config`: import existing values once, switch authority only after successful complete persistence, and do not leave two competing settings sources. Legacy keys retained for compatibility must not be read as a fresher authority on later boots.

New settings cover alarm enabled/time/repeat days/sound/volume/snooze and Night Shift mode/fixed times/solar offsets/day brightness/night brightness. Define physical key names, field encodings, and format versions together with the selected persistence mechanism before implementing writes. Do not persist raw bytes of `device_config_t`; C structure layout is not a durable file format.

Compatibility rules are:

- Preserve existing saved preferences. A changed default applies only to a missing field or a new configuration.
- Leading Zero defaults to Off when absent; preserve an explicitly saved On or Off.
- When the new Night Shift configuration is absent, import the legacy enabled flag as Fixed Schedule when enabled or Off when disabled, along with its fixed times. Once converted, use the new configuration without repeating the import.
- Fill missing fields with documented defaults. Mockup sample values do not establish defaults. Record any new field defaults in the implementation's schema inventory before writing them.
- Use explicit format versions where a change in representation or meaning requires conversion. A format conversion must have the same interruption safety as a Save; retries must not progressively alter values or lose the source configuration.
- Preserve fields the running firmware does not own. Detect unsupported newer formats before attempting incompatible writes; do not “repair” them by silently replacing them with defaults.
- No general downgrade guarantee is made. Running original firmware against enhanced storage is not an approved compatibility test unless its read/write behavior has been verified. Keep original-firmware evaluation storage backups separate from configuration migration.

## Internal operating records

These records share the helper and physical storage system, but have separate owners and lifetimes. They are not part of UI settings drafts or a configuration rollback.

| Record | Owner | Required meaning and lifetime |
|---|---|---|
| Handled alarm occurrence | `alarm_service` | Identify the scheduled occurrence sufficiently to prevent replay after dismissal, restart, or repeated local time. Keep identity tied to the intended local schedule date and resolved occurrence. |
| Interrupted snooze | `alarm_service` | Retain the active occurrence and snooze due instant needed for the fixed 30-minute recovery rule. |
| Missed-alarm notice | `alarm_service` | Retain until acknowledged or cleared by the next actual ringing occurrence. |
| Backup-clock notice | `time_service` | Retain the detected reason independently of RTC correction and restart until acknowledgment; a later detected event may renew it. |
| Last effective appearance | `night_service` | Retain Day or Night for unavailable-input startup fallback. Do not store an old brightness value as the fallback authority. |

Related alarm recovery fields must themselves remain coherent after interruption. Saving settings cannot overwrite alarm history, snooze state, or notice acknowledgments. Dismissing a warning must not rewrite preferences. Shared helper access must serialize writes as needed without blocking the UI or time-critical alarm scheduling.

Persist the backup lost-time notice before correcting the RTC and clearing its hardware flag. If the notice cannot be stored, do not clear the evidence of that loss. Network time can still provide trustworthy current time.

Persist actual effective appearance changes, not draft previews, repeated clock ticks, or every brightness fade step. Restore the last successfully saved appearance with the current configured brightness. With no usable appearance record, use Day and configured Day Brightness. Off and Always On bypass this fallback.

No periodic timestamp checkpoints, additional flash partition, general event log, or outage history are required. Verify space and write frequency for the chosen representation, including temporary space required during a save or conversion.

## Required evidence before relying on the implementation

Document the selected physical records, versions, defaults, commit point, recovery selection, and compatibility limits here when the mechanism is chosen. Verify the underlying storage behavior and exercise interruption paths; do not infer guarantees merely from an API name.

Required acceptance cases include:

| Constructed interruption or failure | Required result |
|---|---|
| Before persistence begins | Complete previous configuration remains. |
| During candidate writes or recovery metadata writes | Recovery selects one complete committed configuration; no mixed fields. |
| At the durable commit boundary | Either complete old or complete new state according to whether commit completed; no partial configuration. |
| After durable commit, before UI acknowledgment or restart | Complete new configuration survives. |
| Wi-Fi Save & Restart interrupted between different groups of changes | Wi-Fi and pending Device Setup changes remain together. |
| Reboot during first import or a format conversion | Repeat recovery safely without destroying the source or repeatedly reimporting stale values. |
| Save with unrelated operating records already present | Alarm recovery, warning acknowledgments, and appearance history retain their intended independent state. |
| Storage full or a write fails | No false success, automatic restart, or partially published settings. Resolve any uncertain commit result. |
| Startup NVS initialization returns `ESP_ERR_NVS_NO_FREE_PAGES` or `ESP_ERR_NVS_NEW_VERSION_FOUND` | No automatic erase before recovery. Preserve recoverable settings and operating records; if safe recovery is unavailable, report failure and leave the data intact. |
| Unsupported newer format | No incompatible overwrite or silent factory reset. |

These are planned checks, not reproduction results or hardware test claims. The intended test owners are `test_device_config.c`, `test_config_consistency.c`, `test_persistence.c`, and the feature-specific recovery tests listed in the architecture.

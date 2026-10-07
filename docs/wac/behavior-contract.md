# Weather Alarm Clock Behavior Contract

Status: Agreed behavior for implementation. Updated October 7, 2026. Firmware implementation and hardware verification remain pending.

This document defines observable behavior. [architecture.md](architecture.md) assigns ownership and files; [storage-contract.md](storage-contract.md) defines durable saving; [screen-state-map.md](screen-state-map.md) maps the approved interface. Mockup sample dates, readings, and preview controls are illustrations, not device state or factory defaults.

## Time and backup-clock notices

Network time is primary. A trustworthy DS3231 may supply time at startup while network synchronization proceeds. A plausible date alone is not proof of trustworthy time or successful network synchronization. The RTC stores UTC; configured timezone rules supply local time.

Without trustworthy time, show the unavailable-time presentation. Keep saved alarms enabled or disabled as configured, but do not trigger them until trustworthy time is available. Do not display compile time as the current time. On recovery, evaluate eligible alarms using the recovery rule below.

Inspect the RTC before network synchronization can overwrite evidence of lost time. Persist a detected loss-of-time notice before correcting the RTC and clearing its oscillator-stop flag. Update the RTC after each successful network synchronization, outside the network callback and display lock. Failed RTC access does not invalidate independently trustworthy network time.

The accepted [DS3231 first-iteration specification](../../shared/spec/ds3231-backup-clock-first-iteration.md) governs the two notice reasons:

- **Backup Clock Lost Time — Check Battery.** The oscillator-stop flag was observed; this is not a measurement of battery health.
- **Backup Clock Unavailable — Check Connection.** RTC access failed; do not infer that the oscillator stopped.

Correction and restart retain an undismissed notice. Its indicator opens a notice only when tapped. DISMISS acknowledges it; outside taps do not close it. A ringing alarm preempts the notice without acknowledging it. A subsequent detected loss after dismissal creates a new notice. No battery-voltage measurement or diagnostic screen is added.

## Alarm recurrence and daylight saving time

The saved local hour, minute, and repeat days define the recurring schedule. Ordinary local times that occur once use their normal timezone conversion. The following exception applies only when the scheduled local time is skipped or repeated by a timezone transition:

**Resolve that occurrence using the UTC offset in effect after the transition, matching the following day's time.** Do not change the saved alarm time or repeat-day selection.

| Situation | Required behavior |
|---|---|
| Spring transition skips the saved local time | Apply the post-transition offset. For a one-hour jump from 2:00 AM to 3:00 AM, a saved 2:30 AM alarm rings at 1:30 AM before the jump. |
| Fall transition repeats the saved local time | Ring once, at the second occurrence, under the post-transition offset. |
| Local time is neither skipped nor repeated | Ring at its ordinary local time. |
| Subsequent ordinary day | Use the unchanged saved local time. |

These are illustrative transition examples, not hard-coded timezone dates. Use the configured timezone's actual offset change. The selected repeat day and occurrence identity remain tied to the intended local schedule date, even if resolving its instant shifts the displayed wall time. Compute upcoming occurrences far enough ahead to ring the spring occurrence before the clock jumps; discovering it only after the jump is too late.

`alarm_schedule` resolves recurrence; `alarm_service` tracks whether an occurrence has already rung or been handled. Repeated wall time, backward clock corrections, and restart must not create a second occurrence of the same scheduled alarm.

## Alarm operation and recovery

Alarm scheduling continues while settings are open, weather requests are delayed, and the display is busy. Ringing takes priority over menus, notices, and sound previews. Snooze and Dismiss operate on the active occurrence, not the recurring schedule.

The recovery grace period is **30 minutes**, including interrupted snoozes. When trustworthy time returns or the device restarts, an eligible, unhandled occurrence whose due time is no more than 30 minutes ago rings through recovery. An older occurrence becomes a missed-alarm notice instead of ringing late. A dismissed occurrence stays dismissed after restart.

Measure this grace from the resolved actual due instant: the DST-adjusted instant for a scheduled occurrence, or the snooze due instant for interrupted snooze. A spring gap itself never causes a missed alarm. A device that was off at the resolved instant is still subject to the normal recovery grace.

The missed-alarm notice persists until acknowledged or cleared by the next actual ringing occurrence. Acknowledgment clears only the notice and never disables future alarms. Temporarily hiding a notice behind higher-priority status does not acknowledge it.

## Sound

Classic Beep, Gentle Chime, and Rising Tones use the same definitions for previews and real alarms. The audio service owns playback and gradual volume increase. A preview stops after approximately five seconds or when cancelled; actual ringing immediately takes priority. UI callbacks never stream audio.

## Night Shift and brightness

One effective Day or Night appearance drives the UI and brightness. Individual screens do not calculate their own schedules.

| Mode | Inputs and behavior |
|---|---|
| Off | Day appearance and configured Day Brightness; no time or location required. |
| Always On | Night appearance and configured Night Brightness; no time or location required. |
| Fixed Schedule | Trustworthy local time and saved start/end times. Equal start/end means Night all day; an end before the start spans midnight. |
| Sunset to Sunrise | Trustworthy time and usable saved coordinates; calculate locally and apply the saved sunrise/sunset offsets. |

When a scheduled mode cannot determine its current appearance, retain the current effective appearance. On startup, restore the last durably saved effective appearance. If there is no usable saved appearance, start in Day appearance. In each case use the **current configured brightness for that appearance**. There is no separate arbitrary fallback brightness and no restoration of an obsolete brightness value.

Unavailable inputs include missing time, unusable coordinates for solar mode, or a solar calculation that cannot supply the required sunrise/sunset events. Off and Always On take precedence over the fallback. When usable inputs become available, evaluate the selected schedule and transition normally.

Persist effective appearance changes as an internal operating record, separate from user settings. Draft edits do not replace that record. Do not write every fade step or clock tick. `brightness_service` owns gradual backlight transitions, including startup; a later startup call must not override its selected brightness with 100%.

## Editing and saving

All settings have one authoritative committed configuration. Screens edit drafts; services use committed values. A Save applies only the fields edited by that session to the latest committed configuration, preserving unrelated changes made since the screen opened.

**Every Save is all-or-nothing across all settings included in that action.** After interruption, recovery selects the complete previous configuration or the complete newly committed configuration, never a mixture. The durable commit determines which one survives; power loss after commit but before the success message can leave the complete new configuration saved.

| Action | Save boundary |
|---|---|
| Alarm Save | Commit all alarm edits together and apply without restarting. Cancel discards drafts. |
| Nested Done | Return edits to the parent draft; do not persist them independently. |
| Settings entry alarm On/Off toggle | Commit the enabled change immediately, preserving the saved schedule. Confirm the changed state only after persistence succeeds. |
| Device Setup and Wi-Fi | Share pending device-setting edits. Wi-Fi Save & Restart commits those edits and Wi-Fi settings together. |
| Weather Location | Retain its independent save boundary; keep the resolved location, coordinates, and timezone consistent within that operation. |
| Existing restart-based device saves | Commit the full requested change before reporting success or restarting. |

On a save error, show failure and retain the draft for correction or retry; do not publish partial settings or restart. If an error makes the result uncertain, resolve the committed outcome before reporting the save result or publishing a replacement configuration; existing alarm scheduling must not wait on storage I/O. Operational records such as dismissals and backup notices are not rolled back by a settings save.

## Presentation boundaries

The [screen-state map](screen-state-map.md) owns the complete screen inventory. `navigation` selects the active screen and ringing preemption. `ui_model` chooses content and shared status; widgets render it and submit commands. Known-age stale weather and weather with an unknown update age must remain distinguishable.

The backup-clock indicator remains separate from alarm and missed-alarm controls. Both clock faces share alarm/status behavior; the digital Night appearance uses the approved enlarged, centered layout. Existing weather, storm, message, and setup behavior remains in scope.

## Required implementation checks

These are acceptance scenarios, not tests already run:

- A skipped 2:30 AM alarm rings at 1:30 AM before a one-hour spring jump; the saved value remains 2:30 AM.
- A repeated fall alarm rings only at its second occurrence, including after restart or a backward correction.
- Recovery just inside, exactly at, and just outside 30 minutes uses the resolved occurrence or snooze instant; dismissal prevents replay.
- Startup without schedule inputs restores Day or Night with its current configured brightness; absent history uses Day Brightness. Off and Always On work without time or location.
- An RTC correction preserves an undismissed notice; a read failure is not reported as an observed oscillator stop.
- Interrupted saves meet the whole-operation checks in [storage-contract.md](storage-contract.md), including Wi-Fi plus pending Device Setup edits.

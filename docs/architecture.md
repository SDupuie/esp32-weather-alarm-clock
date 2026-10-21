# Weather alarm clock architecture and product specification

This document defines the clock's product behavior, defaults, accepted tradeoffs, service and resource ownership, integration and source placement. It describes the current implementation rather than its development history. Detailed weather condition mappings are maintained in [weather.md](weather.md). Documenting an unresolved choice does not approve it.

Build the weather alarm clock for the Waveshare ESP32-P4-WIFI6-Touch-LCD-3.4C using the existing ESP-IDF application. Only ESP32-P4 is supported. There is no requirement to maintain Raspberry Pi behavior, another firmware implementation, or backward compatibility with their settings or interfaces.

The application provides weather, location selection, Wi-Fi, House Messages and its browser page, an alarm, Sound Machine, Night Shift, backup-clock support, Radar and independent [NWS watches and warnings](#nws-watches-and-warnings). The official mockup owns screen appearance. Multiple alarms, an ambient-light sensor, battery-voltage monitoring, an outage log and a separate diagnostics screen are outside the product scope.

[AGENTS.md](../AGENTS.md) defines working rules. [README.md](../README.md) provides the user-facing project overview, features, hardware, setup and user guide. Detailed build and test instructions remain in [docs/build.md](build.md) and [tests/README.md](../tests/README.md). Test instructions explain what each method establishes; local reports retain results and their qualifications.

- [Product behavior and defaults](#product-behavior-and-defaults)
  - [Editing and saving user settings](#editing-and-saving-user-settings)
  - [Alarm and snooze](#alarm-and-snooze)
  - [Sound Machine recordings](#sound-machine-recordings)
  - [Sound Machine controls](#sound-machine-controls)
  - [Sound Machine playback](#sound-machine-playback)
  - [UI and visual reference](#ui-and-visual-reference)
  - [Clock display](#clock-display)
  - [Night Shift](#night-shift)
  - [Time and RTC](#time-and-rtc)
  - [Weather](#weather)
    - [NWS watches and warnings](#nws-watches-and-warnings)
    - [Location resolution](#location-resolution)
    - [Radar source and palette](#radar-source-and-palette)
  - [House Messages](#house-messages)
- [Implementation architecture](#implementation-architecture)
  - [Ownership](#ownership)
  - [Execution](#execution)
  - [Configuration and storage integration](#configuration-and-storage-integration)
  - [Board and build integration](#board-and-build-integration)
  - [Artwork storage](#artwork-storage)
  - [Planned file structure](#planned-file-structure)
- [Verification and limits](#verification-and-limits)

## Product behavior and defaults

### Editing and saving user settings

Use ordinary saves with ESP-IDF storage.

Cancel discards unsaved edits. Nested Done returns to the parent draft without saving. Back from Provider or an alert-selection page restores the values present when that child opened; other editors retain or discard their drafts under their own navigation rules below. Sound Machine is the immediate-control exception described in its [playback rules](#sound-machine-playback); its Back and Done controls navigate without undoing applied choices. Display Settings has independent Cancel/Save controls. Save applies Home Screen, Time Format, Leading Zero and Day/Night brightness without restarting, then returns to Settings. Display Cancel and Back discard its edits and return to Settings. Apply the chosen Home Screen when returning to the clock, while preserving later manual clock-face navigation.

Alarm Save applies without restart. The On/Off switch in Alarm Settings applies and saves immediately, writing only the enabled value. The Alarm Settings switch does not apply pending time, repeat, sound, volume or snooze edits; those still require Save. A successful switch change remains applied after Cancel or Back, while those other edits are discarded. Failed switch saves retain the applied On/Off state and pending edits for retry. One-time alarm completion below also saves Off through the settings owner. Device Setup and Wi-Fi share pending edits for Save & Restart. Weather retains its independent save and restart, including location, coordinates, timezone, units and the alert sound settings. Its Back and Cancel return to Device Setup without discarding pending device edits. A Weather Save must not apply those unsubmitted device edits.

Choosing Manual Network starts empty network-name and password fields in the existing Device draft each time. It leaves other Device edits, saved credentials and the current connection unchanged. Ordinary Back from Network Details retains the pending credentials; selecting the same scanned network also retains its pending password. Device Setup Cancel or Back discards all pending Device/Wi-Fi edits. Only Save & Restart applies the credential changes.

Apply only changes made in that editor, preserving unrelated applied choices. Display saves cover only its five existing fields; Device saves cover the remaining Device Setup choices. Keep the existing settings owner, stored keys and ordinary save facilities. Validation errors remain in the editor; save failure retains the draft and does not automatically restart.

A submitted Save continues if its editor is hidden, closed or replaced. A successful Device or Weather Save still completes its required restart. Results specific to the old editor must not reopen it or change a newer editor; the general saved/restarting notice may appear on the current screen. Preserve the existing restart delay, including when ringing hides the editor. Failed saves never trigger that restart.

- Validate settings, perform ordinary saves, and report returned errors.
- Save user settings only when the user explicitly applies a change: for example, pressing Save or using a control intended to apply immediately, such as enabling an alarm. Completing a user-armed one-time alarm is the automatic Off exception below.
- Editing a draft, refreshing weather, and normal clock operation must not rewrite user settings. There is no background autosave.
- If power fails during a save, some settings may be updated while others remain unchanged. The user may need to check and save them again. This is an accepted limitation, not a guarantee that the user will check.
- Do not add application-level backup snapshots, selectors, rollback, or a recovery coordinator to make settings saves all-or-nothing.

Creating initial defaults is a separate initialization action. Operating-state persistence is separate from user settings; operating-state behavior is defined below.

Device/display defaults are room name `Clock`, device ID `clock-esp32-p4`, digital face, 12-hour time, Leading Zero Off, imperial units and no Wi-Fi profile. Defaults and permitted choices in this specification are not inferred from preview samples.

### Alarm and snooze

One alarm uses the saved local hour/minute and either selected repeat days or **None** for one-time operation. For a skipped or repeated local time, use the UTC offset after the transition without changing the saved schedule. A skipped 2:30 AM in a one-hour spring jump therefore rings at 1:30 AM before the jump; a repeated fall time rings at its second occurrence. Preserve the intended schedule date when identifying a ringing occurrence. Backward clock corrections must not replay an already handled occurrence.

Defaults: alarm Off, 07:00 Monday–Friday, Classic Beep at 50% volume and ten-minute snooze. Snooze choices are 5/10/15 minutes. Alarm operation continues while menus are open and slow network work is pending.

#### Editing and one-time operation

The Alarm editor previews its draft's next scheduled time through the existing pure `alarm_schedule` calculation using supplied usable clock time. For an unchanged saved one-time schedule, display the alarm owner's copied dated target or captured ringing/snoozed occurrence instead of calculating a new day. With usable time but no upcoming/captured occurrence, show **No upcoming alarm**; do not invent tomorrow's alarm or claim synchronization is pending. This is display-only: the editor does not handle, arm or save an occurrence. Live recovery, handled occurrences and snooze remain with `alarm`; the editor retains its existing alarm-time formatter and caption layout. Automatic Off refreshes only the immediate enabled control/baseline, preserving other draft edits.

Ringing hides the existing Settings root without closing it; the draft, active input and keyboard retain their single owner. A later Settings swipe resumes that editor. The explicit Alarm Settings shortcut resumes a held Alarm editor; otherwise it closes any unrelated held editor without saving and starts the Alarm editor. When opened or resumed through the clock's Alarm shortcut, Alarm's header Back and Cancel discard unsaved alarm edits and return to that clock, matching the Sound Machine shortcut. A successful Alarm Save also returns to that clock. Alarm opened through Settings returns to Settings after Back, Cancel or a successful Save. Nested editors retain their existing parent navigation. The immediate On/Off setting remains applied after Back or Cancel. Keep this entry distinction in the existing settings owner; no second draft or saved return route is created.

**One-time operation:** Repeat **None** means no weekdays are selected. The Alarm Settings Repeat row shows **None**. The Repeat editor presents **None**, **Every Day**, **Weekdays** and **Custom Days**, in that order. Derive its selected row from the existing weekday mask; no separate preset is stored. Custom Days opens the preserved draft selection, with Monday–Friday on the first centered row and Saturday/Sunday below. Both Repeat editors show **Rings once** for an empty selection. Clearing all weekdays selects None; selecting any weekday returns to recurring operation. Custom Days Back/Done returns to Repeat; Repeat Back/Done returns to Alarm, retaining the draft. Alarm Save applies it and Alarm Cancel/Back discards it, preserving the independent On/Off switch. The Alarm Repeat row summarizes actual selected days using the existing compact names/day-count rules rather than saying Custom. The visual reference owns geometry and styles; LVGL rounds its half-pixel Custom Days row positions consistently to whole pixels, retaining the gap and a half-pixel center offset.

Enabling a one-time alarm, or applying a changed time/repeat choice while it is On, selects the next occurrence of that local time: today if it has not passed, otherwise tomorrow. An exactly due occurrence may ring. Wait for usable time before choosing the date. Save that specific dated occurrence with the alarm owner and keep it across restarts; it must not advance to a later day merely because the clock restarts or the UI refreshes. Use the existing 30-minute recovery rule for that occurrence. A later sound, volume or snooze-only change does not choose a new date.

Snooze keeps the same one-time occurrence active. Dismiss consumes it and saves Alarm Off, retaining time, Repeat None, sound, volume and snooze preferences. If its ordinary occurrence is too old to recover, keep the existing missed notice and also consume it/save Off. An expired snooze is discarded under the existing snooze rule and finishes the one-time alarm. Turning it On again arms another one-time occurrence. Completing a captured alarm must not turn Off a newly applied different schedule.

Keep the dated target and whether it was consumed in the existing alarm storage namespace; preserve existing dismissal/snooze records. A consumed target must not rearm tomorrow if saving Off fails. Save Off through `device_config`, writing only the enabled entry and following ordinary settings-error rules. Operational state still changes in memory when its save fails, as below. There is no new alarm history, worker, automatic retry service or settings snapshot.

#### Running schedule changes

Turning Alarm Off stops current ringing and cancels a pending snooze, including one restored before usable time is available. Preserve the saved hour/minute, repeat days and any pending missed-alarm notice. Remember the canceled occurrence using the ordinary dismissal record; returned storage failures follow the same in-memory action and restart limitations as Dismiss below.

Switching Alarm from Off to On, or changing its hour/minute or repeat days while it remains On, arms the next scheduled occurrence while running. An already-past scheduled time does not cause retroactive ringing or a new missed notice. An occurrence due exactly when the change is observed may ring. If usable time is not yet available, make that arming decision when it becomes available. Preserve an existing captured ringing or snoozed alarm and pending missed notice. Changes only to sound, volume or snooze duration do not rearm the schedule. Once usable time permits this decision while no alarm is ringing or snoozed, save the skipped past occurrence in the existing handled-occurrence record. A successful save prevents a restart from treating that earlier time as missed or due. Returned save failures follow the operating-record limitations below. No separate arming history is stored. Edits during captured ringing or snooze retain their runtime skip without advancing the saved handled occurrence; preserve genuine captured alarm recovery. Later scheduled occurrences retain normal restart recovery below.

#### Alarm sound

Classic Beep, Classic Bell and Rising Tones share sound definitions between preview and ringing. Ringing increases volume gradually. Preview stops after approximately five seconds or cancellation; ringing takes priority.

Classic Bell uses bright alternating metallic bells with light hammer clatter, about 21 strikes per second in total and a second bell pitched 6.1% higher. The audio owner generates it live, using the same definition for preview and ringing. Generation feeds the shared 20ms chunks; there is no bell recording, separate worker or separate settings mechanism.

#### Restart recovery and storage failures

Use one fixed 30-minute catch-up window for ordinary alarms and snooze. Evaluate recovery once usable time is available from the RTC or network. An eligible unhandled alarm within the window rings; an older missed occurrence produces a missed notice.

- Save the snooze deadline when Snooze is pressed, and update it on subsequent snoozes. Clear it when dismissed or finished.
- If the snooze deadline is still ahead, resume waiting until that deadline.
- If the snooze deadline has passed by 30 minutes or less, ring when usable time becomes available.
- If it is more than 30 minutes overdue, discard the snooze.

Use the same overdue-deadline check for ordinary alarm recovery and snooze recovery; do not introduce separate configurable policies.

Remember which scheduled ringing was dismissed so, after its dismissal record is successfully saved, a restart does not replay it through the 30-minute catch-up rule. This refers to an individual occurrence (for example, Monday at 7:00 AM), not multiple alarm schedules. Dismissal does not disable future recurring occurrences; one-time completion follows the Off rule above. Store only the information needed for this behavior, not an alarm-history log.

Keep one pending missed-alarm notice across restarts, including the missed alarm's date/time for display. Clear it when the user dismisses the notice or the next alarm actually rings. Dismissing the notice does not disable or change the recurring alarm schedule. This is a pending notice, not a list or history of missed alarms.

Use ordinary storage operations and error reporting for the snooze deadline, dismissal state, and pending missed-alarm notice. Do not introduce backup snapshots, a journal, migration, or a separate recovery coordinator for this feature. These operating-state writes do not authorize background rewriting of user settings. Operating-state persistence not explicitly approved in this document remains undecided.

Apply Snooze, Dismiss and missed-notice transitions in memory even if their operating-record write returns an error. Snooze defers the sound and Dismiss stops it without waiting for a successful save. Log alarm operating-record read/write failures without replacing the on-screen alarm state with storage captions, and retain live transitions for the current run. A later restart may recover an older stored deadline, dismissal or missed notice; an alarm may ring again or a notice may reappear. This is an accepted limitation of failed operating-record saves. Settings Save follows the separate [editing and saving rules](#editing-and-saving-user-settings).

### Sound Machine recordings

The nine prepared recordings use a two-second crossfade, a 160 kbps average-bit-rate target and mono audio. Original source files are retained in `assets/sounds/originals/`; prepared playback files are in `assets/sounds/clock/`. Only the prepared set is packaged for the clock. The [recorded-sound playback gain](#recorded-sound-playback-gain) is applied on the clock, without changing either set of MP3 files.

The recordings are intended for repeating playback. Blend each ending into its beginning with the selected constant-power curve, gradually fading one out as the other fades in. The overlap shortens each repeat interval by two seconds. Prepare the blend in the recording, so playback does not need to mix two copies. The player must remove MP3 encoder delay and end padding rather than play that extra space, and keep audio buffered across each repeat to avoid adding a pause. Package the selected playback files in the existing storage partition. USB and prepared browser installations include its generated image with the application.

### Sound Machine controls

Settings includes a Sound Machine row, showing the chosen sound as subtext. Alarm Settings retains its immediate On/Off switch. The Sound Machine screen contains Sound, Sleep Timer and Volume in that order, with playback status below Volume, a Play/Stop button and Back. Volume is separate from alarm volume. The status supports Stopped, timed playback with minutes left, and continuous playback.

Choose one sound from Rain, Thunder, Ocean, Wind, Forest, Campfire, Stream, Night, Singing Bowl, White Noise, Pink Noise and Brown Noise. The sound selector uses one scrollable column of full-width choices with a clearly visible scrollbar on the right; scroll by dragging the list or scrollbar. The Sleep Timer selector shows all five choices: Continuous, 15, 30, 60 and 120 minutes. Both selectors have Back and Done. Their appearance and geometry are owned by the visual reference, using the existing settings controls and Day/Night styles.

Both clock faces show a speaker shortcut in playing and stopped states on the shared [bottom shortcut row](#clock-display). Both clock headers use title case for weekday and month. Tap-area outlines remain diagnostic previews and are not part of the device UI.

The interactive mockup illustrates choice changes, volume, Play/Stop and retained playback status while navigating. It does not play Sound Machine audio or run a countdown. Defaults and operating rules are defined below; preview samples are illustrative.

### Sound Machine playback

First-use defaults are Rain, 30% volume, a 30-minute sleep timer and Stopped. Volume uses the illustrated 0–100% range independently of alarm volume. Remember the chosen sound, volume and timer using ordinary configuration storage. Every startup, including recovery from power loss, begins Stopped. Do not persist or restore playback activity, a remaining duration or a timer deadline.

Sound and volume changes apply immediately during playback. Changing the sound keeps the current timer deadline; changing the timer starts the newly selected duration from that moment. Continuous removes the expiration. While Stopped, these choices configure the next Play without starting playback. Back and Done only navigate: applied choices remain and playback continues. Save only the Sound Machine preferences intentionally changed by the user through existing ordinary save/error handling; preserve unrelated applied settings and editor drafts. Play, Stop, countdown updates and fades do not write user settings.

Play starts the chosen sound and a fresh full timer, or continuous playback. The timer measures elapsed running time, independent of RTC/network corrections. Stop cancels playback immediately and leaves the selected preferences intact. For a timed session, fade volume over its final 30 seconds and stop at expiration. The fade changes playback gain, not the user's remembered volume setting. A subsequent Play starts a fresh full interval. This end-of-session fade is separate from the prepared recording blends; do not add another repeat crossfade.

Play one Sound Machine sound at a time. Generate White, Pink and Brown Noise live on the board; use the selected prepared MP3 files for the other nine sounds under the [recording rules](#sound-machine-recordings). Changing between recordings and noise does not reset the remaining time.

Generate the three noises efficiently in short audio chunks, using a lightweight pseudorandom source and small, fixed filter/integrator state. Keep that state across chunks so boundaries do not introduce discontinuities. Prepare filter coefficients outside the sample loop. Use efficient integer or single-precision arithmetic. Do not allocate memory, perform file/network I/O, log, or wait on shared locks for each sample. Generation must finish with headroom inside each chunk's playback interval so it does not cause audible hiccups or leave the speaker without samples. Verify worst-case generation/feed timing and sustained playback on the board during representative normal weather, network, display and touch activity; host timing or listening to a short sample does not establish that result. Retain the existing single audio worker rather than adding a noise worker to compensate for inefficient generation.

Use the White, Pink and Brown equations from soundmachine.app's [`noiseBuffer` source](https://soundmachine.app/playback.js), with no additional generator clipping or limits on their internal state. Retain the board's existing uniform pseudorandom input, 22,050 Hz stream and continuous state across chunks; the website's 32,000 Hz buffered mixer and looping machinery are not imported. For these generated noises, use the fixed playback-output reductions below, retaining the squared volume control and timer fade. Recording boosts do not apply. No loudness normalization is added. The existing final 16-bit PCM conversion guard remains outside the generator. The equations and fixed output gains do not promise a fixed peak ceiling or distortion-free playback at every level.

Ringing and alarm-sound previews take priority and stop Sound Machine, leaving it Stopped. There is no automatic resume after preview completion, Snooze or Dismiss; the user presses Play to start it again. Preserve existing ringing volume progression, preview duration, alarm settings and alarm recovery. A Sound Machine request cannot displace active ringing or an alarm-sound preview.

Use the existing audio owner for playback, noise generation, elapsed timer, expiration gain and copied playing/remaining/error status. Keep configuration preferences with `device_config` and navigation/rendering with the existing UI owners. Retain the existing single speaker owner, lazy initialization and returned-error policy. A failed Sound Machine playback attempt ends in Stopped with an error; an explicit later Play may retry an open/read/decode/write attempt when the retained speaker is usable. A failed speaker creation remains unavailable until restart, as for current alarm audio. Do not add automatic retry or resume.

#### Recorded-sound playback gain

Apply these fixed playback gains to the prepared mono MP3 recordings:

| Recording | Additional playback gain |
|---|---:|
| Rain, Ocean, Wind, Forest, Stream | +5.49 dB |
| Campfire, Night | +15.49 dB |
| Thunder | +1.12 dB |
| Singing Bowl | +2.80 dB |

Campfire's large crackles may exceed the available range at higher volume; that is an accepted tradeoff. Keep the final PCM conversion guard, without adding a limiter, compressor or changes to the MP3 files.

These fixed values do not target equal LUFS or RMS levels. Fixed gain does not avoid clipping when boosted peaks exceed the existing output range. Apply these boosts only to recordings. The three noises retain their source equations, with playback-output multipliers of approximately 0.25119 for White Noise (−12 dB) and 0.50119 for Pink/Brown Noise (−6 dB). Do not change their internal state or add generator clipping.

Sound Machine uses a base gain of `0.66 × (volume / 100)² × sleep fade`. Combine the selected recording or noise multiplier with that gain once per audio chunk, before final 16-bit PCM conversion. Keep the existing conversion guard and codec output setting. Use the existing audio owner and sample loop, with no additional sample-processing pass, worker, module, setting or storage. Do not add automatic loudness measurement, normalization, compression or limiting for this boost. The volume control, final fade, gapless repeat handling, alarm priority and playback error behavior otherwise retain their existing rules.

### UI and visual reference

Ringing preempts menus, notices and previews; preemption alone does not acknowledge a notice or apply a draft.

The visual reference is [docs/alarm-design.html](alarm-design.html). It contains the combined device preview and the House Messages browser preview, selected using the controls above the previews. Images, fonts and both previews are embedded in that file. It owns appearance and controls, except for the [provider-credit placement](#weather-provider-credits) specified here; this specification owns behavior, defaults and source placement. Preview selectors and sample values are not production features. The digital ringing layout is the chosen ringing presentation; carousel alternatives are not additional screens.

The ringing layout uses **SLIDE TO DISMISS** and a **SNOOZE** tap button. Start at the left handle and drag to the right end to dismiss. A track tap does nothing; an incomplete or canceled drag returns the handle and keeps ringing. Completing the slide uses the existing dismissal rules above. The visual reference owns the centered labels, Day/Night styling and **DISMISSED** confirmation. The snoozed screen's existing Dismiss control is unchanged. Submit the existing dismissal command immediately on a qualifying release. The confirmation follows the matching owner's applied live dismissal, including an operating-record save failure; it never delays stopping the alarm through its owners or requires storage success. Hold the confirmation briefly without blocking, then use the existing return destination. A newer alarm immediately replaces old input or confirmation; stale releases, completions and expiry must not act on it.

Menus follow the [official visual reference](alarm-design.html) for common headings, subtitles, ordinary rows, switches and footer alignment. Use its page-specific layouts for scrolling choices, Custom Days, forms, Fixed Schedule, Wi-Fi connection status and the Alerts failure footer. Repeat, Sleep Timer and Alarm Sound retain their illustrated layouts and control sizes. These appearance rules preserve each control's label, action and disabled state; navigation, defaults, saving, playback and retry behavior follow this specification.

Alarm Time and Fixed Schedule Night Starts/Night Ends share the reference's time-editor geometry. Their stepping and 12/24-hour behavior are defined below. The native Provider API-key form summary retains 22px text.

Keep long Wi-Fi network names and compact Messaging values on one line with an ellipsis; the editors retain their complete values. Warning Sounds has a single-line heading centered in its existing heading area. Night Alert Sounds also stays on one line. Only the parent Alerts menu shows **Uses alarm volume**; its Watch, Warning and Update Sounds children omit that repeated subtitle. Selected radio rows use the selected fill, accent border and dot; individual NWS checkbox rows retain their existing border selection.

Sound Machine sounds, Wi-Fi networks, Watch/Warning Sounds, Active Alerts and Alert Details use the reference's shared content and scrollbar layout, retaining each list's viewport height, row layout and text wrapping. Show the scrollbar only when the content exceeds its viewport. Sound Machine retains its interactive scrollbar; the other bars are visual indicators with ordinary content scrolling. Center alert title and expiry together using their complete measured text height. Active Alerts and Alert Details use the shared menu stage background; unavailable Radar uses its reference's subtle radial background.

All keyboard fields use the existing 520×96 active field at (140,256). The four shared keyboard pages retain their keys and actions; their bottom row uses the existing LVGL width weights 5:3:14:3:3:3, giving 85/51/240/51/51/52px keys on the 800px surface. The official reference follows those rendered widths. Sliders retain their existing ranges, steps and 450×8 rail at x175, with a 36px knob whose center reaches both rail endpoints. Radar retains the native Play/Pause glyphs.

Digital's weather condition-to-high/low gap is 12px, matching the Analog high/low gap. Ringing, Snoozed and dismissal confirmation align their first two rows, time, AM/PM and divider with Day Digital in both appearances. On the 800×800 surface, the weekday is 32px Montserrat at y90 with 1px letter spacing; the date is 28px Montserrat at y140 with no added letter spacing. Both calendar rows use Digital's weekday/date color at 70% opacity (Day `#f5f8ff`, Night `#ecc0c0`). The time and AM/PM start at y192 and y272, with the divider at y315. The enlarged Night Digital clock has its separate layout. Disabled Snooze retains its blue or maroon fill at 40% opacity. The unread-message Night edge uses the reference's muted pink tint; important-message coloring and pulsing follow their normal rules. The [test instructions](../tests/README.md) distinguish software rendering from physical board observations.

Ringing, Snoozed and dismissal confirmation use the reference's title-case weekday/month and shared date position. Without usable time, keep Sound Machine, NWS alerts and Settings reachable below Alarm Settings, using the Digital clock's shortcut row. Raise the unavailable-time content above that fixed row, leaving clear space between Alarm Settings and the shortcuts. Keep the missed-alarm notice above the sync message so it does not compete with that row. The RTC warning indicator appears only to the left of the day/date on the clock faces; hide it and its popup on Time Unavailable, retaining the warning. If time becomes unavailable with the popup open, close it without acknowledgment. When usable time returns, show the retained warning's indicator beside the date, opening the notice only when tapped. These shortcuts remain available without making time usable or enabling time-dependent alert headlines or alarms.

Weather's Save button keeps its label during location lookup and provider/key validation, retaining the existing disabled state while a save is pending. Both checking messages and provider-validation failure use the common short subtitle position and size. Keep provider failure guidance on one line so it clears the first row. Validation, retained drafts and retry behavior are unchanged.

Messaging's distinct Device ID guidance is vertically centered in the fourth-row space, with its three controls kept in their standard positions; normal messages replace that guidance in the same space. Guidance text sizes follow the visual reference. Device Setup's Weather entry opens the Weather editor with the common single-line heading placement. The official layout has four rows in this order: **Weather Location, Provider, Alerts, Units**. API-key entry is inside the Provider editor. Tapping the whole Weather Location row opens its input/keyboard; location, units, lookup and independent save/restart behavior remain.

Use LVGL for the keyboard, matching the mockup’s lowercase, uppercase, numbers and symbols arrangements, size and position. The temporary typing layout hides inactive controls and shows the active input above the keyboard. Show the enlarged character while a key is pressed. Caret and colon are available on the illustrated symbol layouts.

Keyboard Done and Close hide the keyboard and retain the typed field in its draft. Header Back keeps the enclosing editor's normal navigation and discard rules while typing. Input limits match the existing native fields: Room Name, Device ID and Weather Location allow 63 characters; Network Name allows 32, Password 64 and API Key 128. These input limits do not expand the underlying byte buffers or change storage validation.

Use the same raised active label/input placement for location, API key, room name, device ID, network name and password. While editing location, keep the exact muted note “Time zone is set automatically from this location.” between the input and keyboard. Keep real field errors above the active label, using the reference's single-line band; normal lookup notices use “Location not found.” and “Lookup unavailable.” without changing lookup or retry behavior.

Alarm Time and Fixed Schedule start/end use the same time control. Each minute-arrow press adds or subtracts five minutes, advancing or reducing the hour when minutes cross an hour boundary. This also changes AM/PM across noon or midnight and wraps between 23:55 and 00:00. Opening or leaving a time editor does not round an existing saved minute value. Hour and AM/PM have separate buttons.

Volume and Day/Night Brightness sliders use an 8px visible track and a 36px circular handle, centered in a 48px-high touch area. Alarm volume is 10–100%, Sound Machine volume is 0–100%, and Day/Night brightness is 20–100%. All four controls use 5-percentage-point increments when adjusted. Each editor follows its saving rules; a valid saved slider percentage remains unchanged until that slider is adjusted. Day/Night colors and control positions follow the visual reference.

Open the existing Brightness editor from Display Settings. Its Back and Done return to Display Settings, retaining the Display draft. Menu placement does not change brightness ownership, defaults, limits or backlight behavior.

Leading Zero controls the 12-hour preference. In 24-hour mode, hours always use two digits, and Display Settings shows Leading Zero On with its switch disabled. Returning to 12-hour mode enables the switch and restores the previous 12-hour choice, including when editing a draft. Saving 24-hour mode preserves that choice rather than forcing the stored preference On.

Normal Digital Night time uses a 250px nominal display size in 24-hour mode and in 12-hour mode with Leading Zero Off. Use the fitted size for 12-hour Leading Zero On. Each format's size stays steady as time advances; preserve proportional Montserrat Medium digits, the existing spacing, centering and AM/PM baseline and gap. Day and alarm-specific layouts retain their separate sizes.

The normal analog clock's next-alarm caption shows its complete text on one line, including Today/Tomorrow and 12/24-hour times. Preserve its illustrated font, centered position, icon and touch area.

After Save & Restart, retain normal startup navigation to the clock or required unfinished setup. Wi-Fi connection status belongs in Device Setup; the mockup's connection previews do not require a separate startup screen or a saved return-route flag.

### Clock display

Analog and Digital share the following clock presentation in Day and Night where applicable. The official visual reference owns the exact geometry; native pixel sizes below define shared layout requirements.

- Weekday and date occupy one line, with three-letter days, for example **Wed, Oct 14**. Use the same quiet Analog treatment on both faces and both appearances, with **1px letter spacing**. Retain the distinction between the quiet date and the accent-colored alarm setting. The date is 32px; do not restore a full weekday or a separate date row.
- The alarm row sits directly beneath the combined date. Match its font and spacing across faces, keep its complete caption on one line, and clearly distinguish scheduled, Off, Snoozed and Missed states. Use a **32×32 alarm icon with a 10px gap**; retain the reviewed regular 28px caption and 26px missed-notice caption. Alarm-related notices use the same alarm glyph; Clock paused and Waiting for sync retain their own clock-status symbols. Preserve the missed notice's dismiss action. Add no redundant Alarm On box.
- Sound Machine, Alerts and Settings occupy a straight bottom row, with **40×40 visible icons**, no visible titles and the larger tap areas. Sound Machine and Settings align beneath Tuesday and Thursday on Digital; keep the same horizontal icon spacing on Analog. Visible icon tops are y606 on Analog and y663 on Digital, within 88×48 tap areas at x260, x356 and x452. Analog icons remain inside the dial with clearance from its numerals.
- Normal combined-date tops are y150 on Analog and y90 on Digital; the alarm/status row starts at y191 and y131 respectively. The reference sample has approximately 16px between the date and alarm icon on both faces; visible letter shapes vary by date and caption. Digital's normal Day time starts at y192, with AM/PM at y272 on the 800×800 surface; its weather, notices and lower content share the same five-pixel layout offset. Normal header/shortcut positions also apply in Night, while Digital retains its enlarged, centered Night time. Ringing, snoozed, dismissed and unavailable-time surfaces have separate layouts.
- House Messages retain their navigation and left-edge glow independently of the weather-alert list and Alerts shortcut.
- Keep the alert triangle neutral/muted, including when alerts are active. Show a filled **red warning-count badge at its upper left** and a filled **yellow watch-count badge at its upper right**. Both badges are circular. Hide zero counts. The icon remains visible without active alerts; tapping it opens the active-alert list. The question-mark treatment for unavailable updates is the shape exception governed by [alert source and status](#alert-source-catalog-and-priority).
- On Analog, the central weather artwork occupies a **400×400** logical box. The dial, ticks, numerals and hands surround it. Center the temperature on the left and vertically stacked High above Low on the right as one group at the lower position. Match the Digital temperature's **102px** size; High/Low use 24px accent text with a 12px gap. Stored image format and source artwork resolution follow the [weather icon rules](#weather-icons).
- When alert text is shown, replace Digital's five-day forecast strip or Analog's temperature/High-Low group. Keep Digital's current conditions and Analog's central weather picture otherwise intact when weather is available. Show the event name on one line, omitting its trailing Watch/Warning word; below it show **Watch · Tap to dismiss** or **Warning · Tap to dismiss**. Use the matching 32px event name and 24px muted subtext, with the event name yellow for a watch or red for a warning. Do not add an extra count to this subtext; the icon already shows the two totals. Headline selection follows the [dismissal rules](#weather-alert-dismissal), not an unconditional highest-active-event rule.
- While that alert text is displayed, hide routine forecast-provider notices, including waiting, age and stale notices, on both faces. Restore the applicable notice and ordinary weather/forecast area after dismissal. When needed without alert text, routine age/stale notices use the fixed reviewed 22px row below Digital current conditions or above Analog's bottom icons. **Waiting for weather…** is the explicit exception: retain its larger 28px treatment in Digital's current-conditions area and Analog's temperature area. On Analog, align its vertical center with the alert subtext's height. Keep it hidden while alert text is shown; do not move it to the small status row.
- Show the dismissible on-clock alert text during Night Shift at each face's original daytime position and with the same text sizes and tap-area dimensions. Digital keeps its existing enlarged time size, fitting and centered position; add no Night-only alert offset or shorter alert area. Use the subdued Night watch/warning text colors. The normal headline and dismissal lifetime apply even when Night Alert Sounds is Off or sound has finished. Alert counts and list/details remain available. Preserve Night hiding of weather artwork, temperatures, forecast and routine weather notices, along with warm Night AM/PM, clock formats and other palettes.
- Preserve the backup-clock warning as a separate clock-status feature. On both normal faces, put the **existing warning triangle to the left of the combined day/date**, matching the alarm row's **32px icon and 10px gap**, and center triangle plus date together as one group. Retain its existing amber Day and muted Night warning colors; only the date gets the quieter date treatment. Show it only while a backup-clock notice is pending. Tapping opens the existing **Backup Clock Lost Time / Check Battery.** or **Backup Clock Unavailable / Check Connection.** notice, with its existing dismissal behavior. This does not change the [RTC warning lifetime](#time-and-rtc).

`Clock paused` uses a 135-second threshold. It reports that the displayed minute has stopped changing while UI updates continue.

When weather-alert text is inactive or dismissed, use the normal forecast or temperature area with no permanent alert-box placeholder. Preserve other clock behavior, including unavailable time, paused/sync notices, alarm operation, Sound Machine and settings actions. The unavailable-time surface is not redesigned by the normal clock examples.

### Night Shift

Night Shift modes remain Off, Always On, Fixed Schedule and Sunset to Sunrise. Fixed equal endpoints mean Night all day; a range across midnight is supported. Solar events are calculated locally with saved offsets. While running, retain the current effective appearance when required inputs/events become unavailable. Apply gradual backlight transitions, including startup.

Local solar calculation uses the saved coordinates and usable clock time, independently of weather downloads. Keep each event's full UTC date/time, using local calendar days rather than adding 24 hours across daylight-saving changes. Apply offsets as elapsed minutes to those events. Night begins at the shifted sunset and ends at the following shifted sunrise; if the offsets remove that interval by moving its start to or beyond its end, there is no Night interval for that pair. Missing required events use the existing appearance-retention/startup fallback rules.

The Sunrise/Sunset Offsets page shows tonight's start and tomorrow's end from the Night owner's copied raw events, formatted with the selected 12/24-hour preference. Draft offsets preview those displayed times without changing the running appearance or writing storage. Refresh the times in place; retain the specified unavailable layout when they cannot be calculated.

When solar scheduling lacks usable time, identify the wait for time sync. With usable time but no applied weather location, identify the missing location and direct the user to set one. If both inputs exist but the required solar events are unavailable, retain the generic unavailable-events guidance. Show the same reason in the Night Shift summary when Sunset to Sunrise is selected; refresh open views without discarding drafts. Trustworthy backup-clock time remains sufficient even while network synchronization is pending. These notices describe the existing appearance/brightness fallback; they do not change its behavior.

Defaults: Off, fixed endpoints 22:00/06:00, zero solar offsets and Day/Night brightness 80%/30%. Solar-offset choices are -120..+120 minutes in five-minute steps; brightness choices are 20..100%.

Remember the last effective Day/Night appearance for startup fallback.

- Calculate appearance from the saved mode and available inputs whenever possible. Off selects Day and Always On selects Night without requiring time or location.
- At startup, when a scheduled mode lacks the inputs needed to calculate appearance, use the last saved Day/Night appearance. If no usable saved appearance exists, use Day.
- Always apply the current configured brightness for the selected appearance; do not save or restore a historical brightness value.
- On loading, raise an existing brightness below 20% to 20% in memory so the display remains readable. Do not rewrite that stored preference during startup; an explicit settings save uses the normal save rules. Values above 100% and storage read errors still fail validation/startup. Intermediate startup fade steps may pass below 20%; the minimum applies to the selected brightness.
- Persist only effective Day/Night changes, not clock ticks, draft previews, or brightness-fade steps.
- Use ordinary storage operations and error reporting. A failed appearance save does not prevent the display from changing; a later restart may restore the older saved appearance. Do not add snapshots, a journal, or a recovery coordinator for this value.

It does not authorize background rewriting of user settings.

### Time and RTC

Network time is primary; trustworthy RTC time can start the clock while synchronization proceeds. The RTC stores UTC. An observed oscillator stop flag or an invalid RTC calendar makes RTC time untrustworthy; failed access is a distinct connection warning. RTC calendar access supports 2000–2099; later valid network dates can still run the system clock. Without usable time, show unavailable time and wait before triggering alarms; do not substitute a compile-time date. Keep the backup oscillator enabled and correct the RTC following successful network synchronization.

When the running clock uses trustworthy backup-clock time and has not yet accepted network synchronization in the current run, show the visual reference's muted clock icon and “Waiting for sync” notice on either clock face, in Day and Night. Clear it when network synchronization succeeds, independently of backup-clock correction. It may remain while offline; losing Wi-Fi after successful synchronization does not bring it back. The existing “Clock paused” warning takes priority. This notice does not make backup-clock time unusable or change alarm eligibility, weather downloads or Night Shift decisions.

Do not persist RTC warnings or their acknowledgments across restarts. Keep a detected warning in memory for the current run, including after network time corrects the RTC, until the user dismisses it. At each startup, check the RTC again and report problems detected then; do not restore an earlier warning from storage.

If a backup-clock Dismiss request cannot be submitted, keep the technical failure in logs rather than showing a diagnostic in the dialog. Preserve the existing warning, retry opportunity and acknowledgment behavior; do not treat a failed request as a successful dismissal.

Inspect the RTC for lost-time evidence before correcting it, distinguish lost time from failed access, and clear the hardware stop flag only after successful correction. After each accepted network synchronization, make at most three correction attempts, five seconds apart. Stop on success or an unsupported date. If RTC initialization fails, end that attempt before reading its calendar. Lost-time inspection waits until initialization succeeds and still precedes correction. Correcting the RTC does not depend on saving a warning.

Network-time startup failures retry after 20 seconds without preventing weather downloads. Successful startup leaves synchronization and its transport retries running in the background.

### Weather

#### Weather Contract

- Forecast providers and API-key behavior follow [Provider selection and API keys](#provider-selection-and-api-keys).
- Refresh interval: every 10 minutes
- Weather downloads require Wi-Fi and a saved location, independently of usable clock time or network-time synchronization. Failed weather downloads retry after 20 seconds, except for the confirmed daily-quota pause below.
- Until live weather data arrives, use the waiting presentation, with `--` for any visible unavailable measurements; do not present fallback pictures or preview sample values as actual weather on the clock or Forecast screens.
- On Analog and Digital, keep `Waiting for weather…` in the weather area during the initial download and failed retries before the first successful download, using the [clock notice placement and alert suppression](#clock-display). Hide weather values and artwork, and show no additional download-failure footer. Preserve the existing Night hiding of this weather presentation.
- On Forecast, show `Waiting for weather…` in the existing central waiting area before the first successful download, including failed retries. Show `--` for all five day labels and temperatures, hide the five weather icons, and retain the background and forecast frames. Keep this waiting presentation visible in both Day and Night using the existing colors. Show no additional failure notice at the top or bottom. Replace the waiting presentation with provider-dated forecasts when data arrives.
- Keep routine Wi-Fi connection, setup/location reminders and weather-download progress off the normal clock and Forecast screens. Setup pages retain their connection and validation feedback, and operational logs remain available.
- If a download fails after earlier weather was received:
  - continue serving the last known good payload when available
  - on Analog and Digital, show `Weather updated <age> ago` using the actual elapsed age since the last successful download when reliable; otherwise show `Weather data stale`
  - age is measured from a download timestamp captured with usable network time; show `just now` below one minute, whole minutes below one hour, whole hours below one day, then whole days. Use the stale fallback if that timestamp is missing, current network time is unavailable, or the clock is earlier than the timestamp
  - place that clock-face notice above the bottom shortcuts on Analog, and below current weather and above the forecast strip on Digital; suppress it while alert text is displayed as specified under [clock display changes](#clock-display); remove the additional failure footer
  - use no ending period on these short weather notices, and preserve their existing Night hiding
  - retain the failure/stale notice during retries until a successful download clears it; Wi-Fi loss still marks retained weather stale
- On Forecast, a failed refresh with retained weather shows `Weather data stale` directly above the primary forecast's day heading (`Tomorrow` when applicable). Preserve the notice's existing font, colors and Night hiding, the rest of the forecast layout and automatic retries. This notice does not calculate elapsed age.
- Current condition rendering includes:
  - current temperature
  - high/low temperatures
  - icon selection from reported condition facts plus day/night state
- Current-condition facts, captions, icon names, artwork and [daily picture rules](weather.md#five-day-forecasts) are owned by [weather.md](weather.md). Unknown conditions retain valid temperatures and dates with no invented weather picture. Provider/source acknowledgments and license links are maintained in the [README](../README.md#weather-location-and-map-sources).

#### Provider selection and API keys

Open-Meteo is the default provider and requires no API key. The Provider editor offers Open-Meteo and Visual Crossing, with the masked API-key editor under Visual Crossing. Selecting Visual Crossing without a retained key opens key entry; a retained key can be replaced there without a separate Weather-level API Key button. Visual Crossing requires a nonempty API key. Weather Save checks a newly selected Visual Crossing provider or changed key with a forecast request before saving; failed checks retain the editable draft without a restart. Saves using the unchanged selected provider/key do not make another validation request. Provider and key belong to the ordinary Weather save, alongside location and units. Switching to Open-Meteo retains the saved key for later reuse; only the Provider editor's API Key input's Clear control explicitly clears it. The key is always masked, and Configured does not reveal it. Cancel discards unsaved provider/key changes; alarm preemption preserves the existing Weather draft and keyboard.

#### Weather-provider credits

The combined mockup does not include provider credits; this section owns their placement.

Analog, Digital and Forecast show **Weather by Open-Meteo.com** or **Weather by Visual Crossing**, naming the provider supplying the displayed weather. Use **Weather by** consistently, with no trailing period. Retained or stale weather keeps the credit for its provider. The successful weather payload carries its provider through publication, so a settings change or failed refresh cannot relabel retained data.

Curve the credit along the lower perimeter, centered at six o'clock in the same location on all three screens. Place it outside Digital's blue ring and Analog's numerals/tick marks, completely within the round display with clear space from the edge. Use 18px Montserrat on an 800×800 surface, with the text baseline on a circle centered at (400,400) and radius 387px. Keep it muted and readable using the existing graphics facilities, without moving weather content, artwork or controls. Use LVGL's built-in arc label with fractional-angle geometry enabled; whole-degree angles visibly crowd the letters at this radius.

Hide the Analog/Digital credit with their hidden weather during Night Shift. Forecast retains its credit alongside visible weather, using the existing Night text colors. Keep Radar's existing **RainViewer · USGS** credit unchanged. Add no controls, settings or credits page.

#### Visual Crossing quota recovery

This is the exception to ordinary weather retry timing when Visual Crossing reports confirmed exhaustion of its daily allowance.

- A `429` alone does not establish daily-quota exhaustion; it can also report a temporary concurrency or rate limit. Apply this policy only to confirmed daily-quota exhaustion.
- Pause normal ten-minute downloads and ordinary twenty-second failure retries for the affected key. Retain the last good weather and existing stale-data presentation. Keep the selected provider; switching remains a user choice.
- Use a valid provider-supplied `Retry-After` delay/date or absolute reset time to schedule the next checking request. Honor the indicated wait; when both give usable times, respect the later time.
- If no usable retry/reset information is supplied, make **one checking request one hour after exhaustion is confirmed**. If that request again reports daily exhaustion without usable timing, wait another hour before the next check.
- Resume normal refreshes after a successful weather download. A failed check does not restart the ordinary retry loop; continue the quota-recovery schedule, using any newly supplied retry/reset information.
- Keep the quota pause in memory. After a restart, allow one normal startup request. If that request confirms daily exhaustion, restore the provider wait or hourly checking schedule. Do not add persistent quota state. This permits one additional attempt per restart.

The quota pause applies to this clock's requests using the affected key, including Save validation; a different draft key is not paused because the applied key exhausted its quota. Use the existing weather request and scheduling owners rather than a separate polling task. No limit on the number of clocks sharing a key is introduced.

The availability of reset metadata and the accounting of rejected requests remain unverified. Hourly checking is the clock's recovery policy; it is not a claim about Visual Crossing's reset boundary or a provider-guaranteed retry interval. See the [provider's error guidance](https://www.visualcrossing.com/resources/documentation/weather-api/http-error-codes/).

#### Forecast Contract

- A successful download selects the same five upcoming provider entries. Retained forecasts keep those entries and their original dates after midnight or a failed refresh; do not shift their values into newer days.
- Each day includes:
  - the provider's full calendar date, used for its weekday label
  - representative icon under the [daily forecast selection rules](weather.md#five-day-forecasts)
  - high
  - low
- Show `Tomorrow` for the first entry only when usable local time confirms its date is the next calendar date. Otherwise show that entry's weekday. A missing or invalid provider date shows `--` for the label without rejecting otherwise valid weather values.

#### NWS watches and warnings

Official NWS alerts are independent of current-weather classification and forecast-picture selection. Provider weather codes and measurements do not create a competing alert result. The [official visual reference](alarm-design.html) owns alert appearance; its sample dates, places, counts, sounds and preview states are illustrative, not live weather or product defaults.

##### Alert source, catalog and priority

- Obtain official watches and warnings from the National Weather Service, independently of the selected Open-Meteo or Visual Crossing forecast provider. Reuse the clock's saved weather location and coordinates. Add no separate alert-location input or NWS API-key control.
- When the clock's saved weather location changes, clear the old location's alerts, counts and dismissals, including saved copies so they cannot return after a restart. Check the new location immediately once Wi-Fi and its saved coordinates are available. Show the question-mark triangle until that location's check succeeds. Keep the Watch Sounds, Warning Sounds and Night Alert Sounds settings.
- Check NWS once per minute during both Day and Night Shift. This is the regular alert-check interval, independently of forecast refreshes. After a failed check, continue once per minute without rapid retries or progressively longer delays. Honor an explicit NWS request for a longer pause. The question mark remains until a check succeeds.
- At startup, show the question-mark triangle until the first successful NWS check. Start that check as soon as Wi-Fi and the saved location coordinates are available, without waiting for a full one-minute interval. After success, restore the normal triangle even if no alerts are active, then continue the regular once-per-minute checks. Add no startup setting.
- For a confirmed unsupported location, retain the neutral question-mark triangle and show **NWS alerts unavailable** / **for this location** in the Alerts list footer. Keep forecasts and sound preferences operating normally. Pause NWS checks until the saved location changes; this is the exception to the startup and one-minute check rules. Use this state only when the location is confirmed unsupported. Temporary failures retain **Unable to update alerts** and once-per-minute retries.
- When NWS alert updates cannot be obtained, the clock's alert triangle must visibly indicate that condition. A notice only inside the Alerts list is insufficient. Replace the exclamation mark inside the triangle with a **question mark**, keeping the normal muted icon color for Day and Night Shift. Warning/watch count badges keep their red/yellow colors. Show the question mark after the **first failed check** and restore the normal exclamation-mark triangle after the **next successful check**. Play **no error sound** for a failed check.
- When a check fails, keep the previously received alerts and their counts, respecting prior dismissals. A failed check alone must not clear alerts or reopen dismissed headlines. Apply the end-time rule below during failed checks as well.
- Keep the last received alerts across restarts. Restore them using the end-time rule and prior dismissals; do not restore expired events or reopen headlines covered by dismissal. Restored active events remain available in the list and counts. Restoring saved data does not count as a successful NWS check: retain the startup question mark until a fresh check succeeds, as specified above.
- If saving alerts or their dismissals fails, keep live alerts and eligible sounds working normally and honor dismissals in memory while the clock remains running. Retry the failed save during the next scheduled alert check. Show the silent [save-failure footer](#active-alert-list-and-details); play no error sound. A restart can lose the latest alerts or dismissals if they were not saved successfully.
- If saved alert data cannot be read or is unusable, continue without that alert record and download fresh NWS alerts under the existing startup/check rules. Apply the normal selected sound settings to the fresh alerts, including the first successful check; add no initial silent batch, additional setting or separate recovery system. With the saved history unavailable, a previously sounded or dismissed alert can be announced again. The ordinary no-replay rule still applies when saved alerts are restored successfully. This fallback concerns the alert record; it does not change existing handling of a general storage initialization or user-settings startup failure.
- Use the NWS event end time, **`ends`**, when available; otherwise use the message expiration time, **`expires`**. The selected time is the cutoff for including that alert in the clock headline, active-alert list and counts, including while checks are failing. Do not choose the earlier timestamp merely because both are supplied. An alert reaching its cutoff does not clear the failed-check question mark; only the next successful check does that.
- Keep visual alerts enabled; sound can be disabled independently. Add no master weather-alert On/Off setting. This feature covers watches and weather warnings; it does not silently add advisories or non-weather emergency warning types.
- Use NWS event names in the scrollable sound-selection catalogs. The firmware catalog contains 23 watches and 32 weather warnings, excluding non-weather warning types. Source references are the [NWS event-type list](https://api.weather.gov/alerts/types) and [event-code reference](https://www.weather.gov/dsb/eventcodes). The catalog includes marine, avalanche, tsunami and ashfall events. The two catalogs differ; do not reuse the warning list as the watch list.
- Follow the published [NWS Hazards Map priority](https://www.weather.gov/help-map) across both watches and warnings. Do not impose an unconditional warnings-before-watches rule. Apply that order to headline candidates and the full active-alert list. For example, Tornado Watch sorts before Freeze Warning.

The watch catalog is: Avalanche Watch; Coastal Flood Watch; Extreme Cold Watch; Extreme Heat Watch; Fire Weather Watch; Flash Flood Watch; Flood Watch; Freeze Watch; Gale Watch; Hazardous Seas Watch; Heavy Freezing Spray Watch; High Wind Watch; Hurricane Force Wind Watch; Hurricane Watch; Lakeshore Flood Watch; Severe Thunderstorm Watch; Storm Surge Watch; Storm Watch; Tornado Watch; Tropical Storm Watch; Tsunami Watch; Typhoon Watch; Winter Storm Watch.

The warning catalog is: Ashfall Warning; Blizzard Warning; Blowing Dust Warning; Coastal Flood Warning; Dust Storm Warning; Extreme Cold Warning; Extreme Heat Warning; Extreme Wind Warning; Flash Flood Warning; Flood Warning; Freeze Warning; Gale Warning; Hazardous Seas Warning; Heavy Freezing Spray Warning; High Surf Warning; High Wind Warning; Hurricane Force Wind Warning; Hurricane Warning; Ice Storm Warning; Lake Effect Snow Warning; Lakeshore Flood Warning; Red Flag Warning; Severe Thunderstorm Warning; Snow Squall Warning; Special Marine Warning; Storm Surge Warning; Storm Warning; Tornado Warning; Tropical Storm Warning; Tsunami Warning; Typhoon Warning; Winter Storm Warning.

##### Alert sound settings

Use the **Alerts** entry directly below Provider in Weather, as specified in the [Weather menu layout](#ui-and-visual-reference). Its summary describes enabled sound categories: **Watches**, **Warnings**, **Watches + Warnings**, or **Silent**. Silent means both sound families are Off; it does not disable visual alerts or mean that no watches or warnings are active.

The Alerts screen has the following settings, in this order. Headings and subtitles follow the [shared presentation rules](#ui-and-visual-reference).

- **Watch Sounds:** a scrollable multi-select list with **Off**, **All watches** and the named NWS watch types. Off and All watches are exclusive choices; individual watch types can be selected together.
- **Warning Sounds:** the same behavior with **Off**, **All warnings** and the named NWS weather warning types. Warning Types is not a separate setting.
- **Alert Update Sounds:** defaults to **Off**. Offers **Off**, **Increased danger only** and **All updates**. This permits sounds for updates to an existing event only when that alert type is enabled by Watch Sounds or Warning Sounds. Night Alert Sounds still controls permission during Night Shift. Off leaves updates silent without disabling the existing new-event sounds. Increased danger only uses the milestones below. All updates also permits sounds for substantive revisions, including changed wording, instructions, affected area, event times, forecast amounts or confirmation. Repeated unchanged downloads or a changed message identifier alone do not qualify as a substantive revision.
- **Night Alert Sounds:** On permits the selected watch/warning sounds during Night Shift; Off suppresses those sounds during Night Shift. It does not hide visual alert indicators or alter daytime sound selections. If a download spans a Day/Night transition, use the Night owner's current appearance when selecting an eligible sound, rather than its pre-download value.

Watch and warning selections are independent. Use the alarm volume, with no separate alert volume control. Retain full NWS Watch/Warning names in the selection lists. Use the existing nested draft rules: a sound list's Done returns its choices to Alerts, Alerts Done returns to Weather, and Weather Save applies them through its ordinary independent save/restart. Cancel/Back discards the relevant unsubmitted nested edits. These choices do not alter the existing Provider/API-key validation, masking, retention, explicit Clear or save rules. Mockup selections illustrate the controls. Starting settings and sound behavior are:

- **Watch Sounds defaults to Off.** Visual watches remain enabled; the user can choose All watches or individual watch types using the existing selector.
- **Warning Sounds defaults to All warnings.**
- **Night Alert Sounds defaults to Off.** Night appearance still shows active alert text and counts; the default suppresses weather-alert sounds at Night.
- **Alert Update Sounds defaults to Off.**
- **At least ten minutes must separate any two sounds for the same event.** It is a minimum separation, not a periodic reminder. Display updates received inside that interval silently, without queuing a delayed sound.
- Watches and warnings use distinct sounds. **The watch sound lasts 2½ seconds**, unless dismissed early: a single 1,050 Hz tone in three half-second bursts, with a half-second pause between bursts. The frequency comes from NOAA Weather Radio; the pulse pattern is this clock's design, not an official NOAA watch-only cadence.
- **The warning sound lasts 10½ seconds**, unless dismissed early: simultaneous 853 Hz and 960 Hz tones, played as two identical groups of 2/1/1-second bursts, with half-second gaps between all bursts. Both patterns are synthesized by the audio owner.
- Normalize weather-alert waveform magnitudes to the same peak range as the loudest alarm pattern and retain the existing alarm playback-gain and amplifier-headroom approach described under [audio output](#audio-output). Continue using alarm volume; add no separate alert volume control. This is magnitude matching, not a new perceived-loudness target.
- Alert playback ends automatically without requiring anyone to acknowledge it. Tapping the dismissible alert text stops a playing weather-alert sound early and applies the existing clock-text dismissal rules in both Day and Night. Night uses the same alert text and position as Day, as specified below. Opening the Alerts list also stops the playing weather-alert sound and applies the same [shared headline dismissal](#weather-alert-dismissal).
- Do not replay sounds for restored alerts after a restart, including when the first successful check confirms those same events. Restore their visual presentation, counts and prior dismissals under the existing rules. A genuinely new alert or increased danger remains eligible for sound under the selected settings; a restart alone is not a reason to sound again.
- A ringing wake-up alarm takes priority over weather-alert audio and continues uninterrupted. Keep the weather event active and available when the normal clock returns. Skip that event's weather sound; do not queue it to play after the alarm. This does not change the existing ringing screen or alarm actions.
- An enabled weather-alert sound briefly interrupts Sound Machine and plays at alarm volume. When the alert sound ends or is dismissed, automatically resume Sound Machine at its previous volume. Its Sleep Timer continues counting during the interruption; if the timer expires, leave Sound Machine stopped. If a wake-up alarm begins, preserve its existing priority and Sound Machine stop behavior, including no automatic resume after the alarm.
- If an eligible weather-alert sound arrives during an alarm-sound preview, stop the preview and play the weather-alert sound under the selected sound settings. Keep the user in the settings screen with all unsaved choices intact. Leave the preview stopped afterward; the user can start it again. This adds no setting and does not change the ringing wake-up alarm's priority above.

**Increased danger only:**

| Existing warning | Qualifying increase |
| --- | --- |
| Tornado Warning | Damage threat: Base → Considerable → Catastrophic |
| Severe Thunderstorm Warning | Damage threat: Base → Considerable → Destructive |
| Flash Flood Warning | Damage threat: Base → Considerable → Catastrophic |
| Snow Squall Warning | Newly receives the Significant impact designation |
| Red Flag, Blizzard or Ice Storm Warning | Newly receives an explicit Particularly Dangerous Situation designation |

Use NWS's documented damage/impact categories for the first four families, and its explicit designation for the last row. These meanings are documented in the [NWS impact tags](https://www.weather.gov/impacts/understanding), [CAP parameters](https://vlab.noaa.gov/web/nws-common-alerting-protocol/cap-documentation), [fire-weather guidance](https://www.weather.gov/media/twc/2024%20SW%20AOP.pdf) and [winter PDS notice](https://www.weather.gov/media/notification/pdf_2026/scn26-52_PDS_Wording_in_Blizzard_Warnings_and_IceStormWarnings.pdf). Do not invent a generic within-warning severity ladder or infer a new danger category from ordinary forecast-number changes.

Each newly higher category or newly added designation qualifies at most once per event. Dropping and returning to a category already reached does not rearm the sound. Several qualifying changes arriving together produce at most one sound. The sound selections, Night permission, ten-minute separation and no-replay-after-restart rules apply.

In Increased danger only mode, keep revised wording, extensions, ordinary movement and raw wind/hail/snowfall or other amount changes silent unless one of the listed escalation markers changes. Confirmation alone, such as radar indicated becoming observed, is also silent. Routine updates to other warning types without a documented escalation marker remain silent. Visual headline updates follow their separate rules below.

##### Active-alert list and details

Use the standard **Alerts** title treatment and the standard subtitle position for warning/watch totals. Show all active watches and warnings in NWS priority order, including events whose clock text has been dismissed. Rows use the severity-colored triangle, full event name including Watch/Warning, expiry and detail affordance; long names can wrap. An example with three rows is not a limit on the number of active alerts.

Selecting an event opens its details with Watch/Warning, **National Weather Service**, full event title, affected area, issue/end information, alert description and protective instructions. Use actual NWS content in operation; the mockup's explanatory examples are not live alerts or fixed message copy. Details return to Alerts, and the list returns to the clock. An empty successful check shows **No active alerts** as its subtitle. Unknown or unavailable status with no retained alerts keeps that subtitle blank; retained counts and the separate checking/failure notices follow their existing rules.

After a failed update, replace the list's bottom **Back to Clock** button with **Unable to update alerts** and the last successful check time. Keep that notice outside the scrolling rows, with three alert rows in their normal positions and unchanged sizes. Retain the upper-left Back arrow, the standard totals subtitle and scrolling for additional alerts. The list retains its bottom Back to Clock button when no footer notice is needed; event details retain Back to Alerts.

Show **Unable to save alerts** in the same bottom notice area when saving alert data or dismissals fails, replacing the bottom Back to Clock button. Keep the three alert rows in their normal positions and retain the upper-left Back arrow. This notice has no error sound. It reports a save failure rather than a failure to obtain current NWS alerts.

Before the first successful check, with no previous alert data, show **Checking for alerts…** in the list area while the first check runs. If that check fails, leave the list area blank and show only the footer **Unable to update alerts** / **No successful check yet**. Do not duplicate that failure with **Current alerts unavailable** in the list area. Use the same absence of a duplicate body notice when no retained alerts remain after a later failure; its footer still shows the last successful check time. Retain the question-mark triangle and upper-left Back action. Unknown alert status must not be presented as a successful check finding no active alerts.

##### Weather-alert dismissal

- Tapping the clock's alert text or opening the Alerts list dismisses the headline. A clock-text dismissal applies to **both clock faces** and survives a restart for the same active events. Forget the dismissal when those events end. It restores the weather/forecast presentation; it does not remove active alerts from the list or counts.
- A dismissal covers the alerts already present when the user taps. Do not immediately replace the dismissed headline with another already-present event and prevent the forecast/temperatures from returning.
- Repeated downloads and ordinary updates, such as wording or timestamp changes, remain dismissed. Show text again when danger increases, for example a watch becoming a warning, a tornado becoming confirmed or a stronger damage-threat designation. Use NWS update relationships rather than treating every updated message identifier as a genuinely new weather event; the [NWS alert documentation](https://www.weather.gov/media/alert/CAP_v12_guide_05-16-2017.pdf) describes those relationships. The [ownership rules](#ownership) define event matching and update comparison.
- A genuinely **new watch or warning reopens the text even if lower priority** than a dismissed event. Select the highest NWS priority among new or escalated events not covered by dismissal. An older, higher-priority dismissed event remains listed and counted, but does not displace the new headline solely because it is still active. For example, a new Freeze Warning can be displayed after a Tornado Watch was dismissed. The full list still sorts all active events by NWS priority.
- These decisions govern on-clock text. The ability to dismiss a playing sound early and the sound rules are recorded in [alert sound settings](#alert-sound-settings). Add no automatic reminder interval for unchanged dismissed text.

The [ownership rules](#ownership) define event matching, update comparison, equal-priority ties, bounded storage and playback coordination.

#### Location resolution

These rules define the ESP32-P4 user-visible behavior for turning a text location into Open-Meteo latitude, longitude, and timezone values.

##### First-time weather location setup

Start with no saved weather location or coordinates. During first-time setup, use the existing Weather editor with an empty location field. The user must select and successfully save a valid location before setup can finish. Location lookup supplies the coordinates and timezone; do not supply a default location.

Use the Weather editor's ordinary empty-field presentation, validation/error handling and required setup exit guard. Normal clock and weather screens do not acquire separate “Not configured” menu labels or missing-location presentations.

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

#### Radar source and palette

Radar uses RainViewer with the green/yellow/red palette below, independently of the forecast and current-condition provider.

Use the fixed weather order **Clock → Forecast → Radar** at supported U.S. locations, with the existing left/right swipes and the reverse order when returning. Active watches, warnings and their dismissal do not reorder these pages. Radar takes two left swipes from the clock. The Alerts shortcut opens the alert list, independently of this weather-page order.

Radar is disabled outside the supported U.S. locations: do not create its display objects or expose a route to it. Forecast is then the sole weather page. A country change that disables Radar exits an open Radar page to Forecast. Radar is not a settings/menu page and has no return button.

Use the dark USGS National Map USGSTopo background with brighter printed town labels, roads and terrain, following the Weather Radar page in the [official visual reference](alarm-design.html). Fill the round screen without an edge fade or surrounding ring. Provider labels retain the size and placement supplied in the map; do not add a separate name download, label service or text-extraction mechanism. Keep the center marker and location heading. The northern Colorado frame in the reference is recorded sample data, not a location default or live radar.

Center the title and frame timestamp on straight lines at baselines 78 and 114 on the 800-pixel display. Retain the 32-pixel title and 18-pixel timestamp fonts, Day/Night colors and thin darkening around those letters. For long location titles, use the existing 24-pixel fallback and an ellipsis if still too wide for the 390-pixel title area. Frame time keeps its date, timezone, 12/24-hour format and old-image indication.

The strength key keeps its colors, position and width, with three equal-width green, yellow and red sections and centered **Light**, **Moderate** and **Heavy** labels. These are broad intensity categories; the underlying radar palette and thresholds below are unchanged. Keep the source credits on their existing single centered line. The map remains visible behind the key and credits without shaded panels.

Admit the resolved location label's final `US` country code; other or missing country codes disable Radar without changing any other weather feature. Do not infer a country from a broad latitude/ longitude rectangle or add a saved country field, migration or extra lookup. Territories with separate country codes are not included. The screen heading capitalizes Radar. Credit the data providers actually used.

The full-screen map is fixed at **75 miles across** (about 120.7 km), centered on the saved weather location: 37.5 miles (about 60.4 km) from the center to the screen's circular edge. This is a ground-distance coverage requirement, not a provider zoom-level number. Keep this coverage when units or latitude change. Use an 18-pixel circular marker centered on that location, with a 2-pixel white outline and the existing Day blue / Night pink fill, without a halo. Retain automatic updates and a visible frame timestamp. The marker represents the resolved weather location, not the clock's precise physical position. Opening Radar shows the latest still image. Preserve the selected appearance, town labels, legend and source credits. After a failed update, keep the last image with its original timestamp and clearly mark it as old. For an enabled Radar page with no image yet received, show “Radar unavailable.”

Check RainViewer for a newer frame every **five minutes** after the initial attempt when Wi-Fi and the saved location are ready. Download replacement radar imagery only when needed for a newer frame, the current location, the six-frame history below or a retry of an incomplete image. Reuse unchanged cached frames; an unchanged frame is not a download failure and does not make its timestamp newer. The displayed time is the provider's frame timestamp, not the time this clock checked for updates. This interval does not change forecast/current-condition refresh, retry or quota behavior. Coverage and interval are built-in values, without radar settings or a zoom slider.

Retain up to six distinct ten-minute RainViewer frames from the latest hour and offer Play/Pause on Radar. Six ten-minute samples have a fifty-minute span between the first and last timestamps; do not fabricate an extra image or describe that endpoint span as sixty minutes. Fetch the latest image first, then fill older history. Playback runs oldest to newest **once**, at a target of **1.5 seconds per frame**. On reaching the newest available frame, stop in the ordinary latest-image state: hold that image, show Play again and omit the Paused label. The existing five-minute checks keep that still image current. Another Play starts a new pass from the oldest available frame. Prepare the next image ahead; if work runs late, hold the current image until ready, without a catch-up burst or skipping through intermediate frames.

Use one 80-pixel round Play/Pause control centered at `(400, 612)`, with a 104-pixel touch target, above the existing legend. Keep horizontal swipes working when they start on the control. Play from latest still mode starts at the oldest available frame; Pause holds the frame actually shown and appends **Paused** to its timestamp. Resuming advances to the next chronological frame; if already at the newest available frame, finish in the latest-image state without rewinding. Leaving Radar, opening settings or alarm preemption stops playback and returns to the latest still image; returning to Radar starts in that mode. No timeline, zoom, pan, playback-speed setting or saved playback preference is introduced.

The control remains disabled until history loading finishes and at least two complete frames are available. A failed older-image download must not discard a good latest image; a partial history may play once loading finishes. Metadata checks continue every five minutes during playback. Keep frame timestamps and existing failed-update/old-image handling, without labeling intentionally selected historical frames old solely because they precede the latest frame. There is one bounded in-memory cache of compressed source tiles, shared by both output image slots; no flash history or six-image decoded cache is introduced. Location changes discard the previous compressed history before preparing the new background.

Use green below 35 dBZ, yellow from 35 through 44 dBZ and red at 45 dBZ and above. dBZ measures radar echo strength; these colors are not a direct rainfall rate or a storm-warning classification. Preserve the selected gradients using these color stops, interpolating each RGB component between stops within a band:

| Echo strength (dBZ) | RGB color |
|---|---|
| -10 | `#C3E6BD` |
| 5 | `#99D98D` |
| 15 | `#52C24F` |
| 25 | `#169830` |
| 34 | `#056B1E` |
| 35 | `#FFE800` |
| 40 | `#FFC200` |
| 44 | `#FF9900` |
| 45 | `#F03124` |
| 55 | `#BC0C19` |
| 75 | `#730011` |
| 95 | `#55000C` |

RainViewer supplies [Universal Blue tiles and its color table](https://www.rainviewer.com/api/color-schemes.html). Request **512×512 radar tiles with RainViewer's built-in smoothing enabled**, Universal Blue selected and separate snow coloring disabled (`/512/7/{x}/{y}/2/1_0.png`). Keep the same geographic tile footprint, maximum zoom 7, 75-mile coverage and 800×800 output. The 512-pixel tiles retain that geographic footprint; their pixel dimensions do not establish finer underlying radar measurements.

Apply the custom mapping to decoded provider colors before output sampling or blending with the background, retaining the provider's smoothed transparency. Verify the smoothed source colors against the published table; transparency changes must not invalidate an otherwise supported color. Unknown visible colors must retain the existing failed-update handling rather than be assigned an invented echo strength. The exact source RGB-to-output RGB lookup preserves the first/lower echo strength for duplicate source colors, zero-alpha handling and validation of every visible source pixel before output sampling. It does not alter smoothing, transparency or blending. Some source colors represent several echo strengths, so the mapping must not imply exact strength within those ranges. Keep the current output sampling; no clock-side blur or bilinear filter is used. Separate snow coloring is disabled.

The [public API](https://www.rainviewer.com/api.html) needs no key, permits custom colors for personal use and requires visible RainViewer attribution. Its [Weather Maps API](https://www.rainviewer.com/api/weather-maps-api.html) supplies radar overlays, not a background map or town labels. The [service limits](https://www.rainviewer.com/api/transition-faq.html) provide two hours of past frames at ten-minute intervals, a maximum zoom of 7 and 100 requests per IP per minute; future radar frames are unavailable. These detailed limits govern the requests used by this implementation.

The [USGS map services](https://www.usgs.gov/faqs/what-are-terms-uselicensing-map-services-and-data-national-map) are public-domain sources. Requested acknowledgment: “Map services and data available from U.S. Geological Survey, National Geospatial Program.” The screen credit is `RainViewer · USGS` on one line. Keep the current location's background and names in RAM; no persistent map store is introduced.

### House Messages

Long or multiline messages can extend below the clock's fixed message area, which has no text scrolling. The complete text remains available in the browser. This display limitation is accepted.

#### Message Contract

- Message polling interval: every 15 seconds
- Failed shared-client polls retain the last good cache and use the same 15-second interval. The first poll after runtime reset and an explicit refresh after acknowledgment remain immediate. Selecting a replacement hub does not reset an existing poll interval.
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
  - requires a completed tap; canceled contact does not acknowledge the message
  - acknowledges the current message for the local device
  - returns to the previous home view
  - swiping back to the clock alone does not acknowledge the message
- The browser's Active Messages list shows all active, unexpired messages, including messages targeted to an individual clock. Dismissal on a clock does not remove a message from this list. The browser's Deactivate action makes the message inactive for every clock. Its existing `deviceId=all&includeDismissed=true` request selects the browser list; normal clock polling and device views remain filtered to messages addressed to that device or to all clocks, including when a clock's own ID is `all`.
- A failed browser list refresh retains any displayed cards and uses the existing error feedback. Only a successful response with a messages array can replace the list or report that no active messages exist.
- Refreshing browser destinations preserves the user's current selection. A failed response retains the current options; a successful list temporarily missing the selected room keeps that room and its label available. Refreshing discovery data must not silently change a room message to All clocks. A named destination need not be currently discovered for the existing service to accept a message for it.

#### Current storage limits

Message records, per-device acknowledgments and the shared-client cache are RAM-only. Restarting or removing power clears that clock's local store. A shared client can fetch messages again from a hub that still holds them. A simultaneous power loss to all clocks loses that in-memory collection.

The current implementation keeps at most 16 message records. When full, a new record replaces the oldest by creation time, including an active or important message. Each record holds at most 16 device acknowledgments, and discovery keeps at most eight destination entries. A destination need not appear in that list for a message addressed to its ID to be accepted. These are current implementation capacities, not additional persistence guarantees.

#### Shared-Clock Message Behavior

- Shared mode discovers a LAN message hub over UDP broadcast
- Discovery requests, hub replies and heartbeats encode accepted device IDs and room names as JSON strings. Retain the existing packet-size limit; an oversized or failed encoding is not sent as a truncated packet.
- One device becomes the hub if none is found
- Hub selection is deterministic by device ID sort order
- Shared clients proxy message API operations to the current hub, preserving the request path, query and body without repeating the query
- Clock polls percent-encode the `deviceId` query value; the final receiver extracts and decodes it once before selecting messages. Forwarders retain those encoded query bytes. The selector remains separate from the `x-clock-device-id` and `x-clock-room-name` headers, which identify the requesting or forwarding clock. Reserved characters such as `&`, `+` and `%` therefore remain part of the selected ID. Query decoding treats `+` as a space; clock polls send `%2B` for a literal plus and `%20` for a space. Malformed encoding, decoded NUL and oversized IDs/queries return HTTP 400 rather than selecting a truncated ID or falling back to the local clock. Missing or empty selectors retain the local-clock fallback; browser all-target selection is unchanged.
- Pending local message changes survive Wi-Fi loss and discovery. The existing message worker sends them to the selected hub and retries failed transfers every 15 seconds; a changed hub address is used on the next attempt.
- A successful transfer completes the unchanged copy that was sent. Local changes arriving during transfer remain pending. Reconnection alone does not resend an already delivered copy. Retain the existing RAM records and merge rules for acknowledgments and deactivation; no additional message store or worker is needed.

## Implementation architecture

### Structure

Keep one ESP-IDF application within the existing `main` component, grouped by function. Weather, location, connectivity, timezone, messaging, clock features, board support and artwork belong to that application. Service owners live directly under `main/`; device UI files live under `main/ui/`, and generated artwork and fonts use `main/assets/`. No separate original/new application layers, new component hierarchy or generic application framework are needed.

Reuse and adapt useful rendering code in the single device UI. Change setup flow, navigation, touch handling and status presentation where required by the visual reference. `main/ui/ui.c` implements `main/app_ui.h`, the sole public UI interface; change that interface and its callers together when needed. Upstream integration follows the working rules in [AGENTS.md](../AGENTS.md#current-checkout-and-retained-material).

Startup initializes storage/settings, time, alarm and Night Shift before the display. It then uses the BSP display/touch setup and starts audio after that shared bus is ready. The initial draw precedes the weather worker and UI heartbeat. Required unfinished setup opens the existing settings editors; otherwise the application opens the selected clock face. Rendering consumes feature-owner decisions rather than maintaining another implementation of those decisions.

### Ownership

UI screen assignments follow the [visual reference](#ui-and-visual-reference).

| Owner | Responsibility and authoritative state |
|---|---|
| `main.c` | Startup and application task creation, clock-control and UI integration. The UI heartbeat assembles independent copied owner snapshots and acknowledges actual safe Radar image replacement. Startup and the heartbeat use one static display staging record with a sequential ownership handoff. Main retains no authoritative weather/Radar store; UI selects presentation. |
| `device_config.c/.h` | Applied user settings, defaults, validation, serialized settings writes and publication after successful saves. Settings UI owns drafts. |
| `connectivity.c/.h` | Network initialization, connection and scan state, and network-time synchronization transport/completion. Time trust and system timezone application belong to `clock_time`. |
| `weather_client.c/.h` | The existing weather worker and forecast/retry, Radar and NWS deadlines; serialized requests, provider/PNG decoding and in-memory forecast-key quota admission. Owns retained applied weather, completed Radar publication/actual display acknowledgment, image preparation/buffers, bounded compressed history/playback and the single NWS catalog/event/dismissal/sound-history/operating-record state. Raw fetch remains nonpublishing for Settings validation. The worker consumes short Radar controls and prepares frames; UI performs no HTTP or image processing. |
| `forecast_representative.c/.h` | Representative forecast calculation; no live weather state. |
| `location_lookup.c/.h` | Geocoding requests and lookup results. `device_config` owns saved location values. |
| `location_scoring.c/.h` | Query parsing, candidate ranking, resolved-label formatting and country-token checks; no saved location state. |
| `timezone_table.c/.h` | Timezone mapping data used for validation and conversion. `clock_time` applies the selected mapping to the system clock's local-time rules. |
| `weather_icon_names.c/.h` | Provider condition facts, picture selection, icon names and current-condition captions; no live weather state or refresh decisions. |
| `assets/weather_icons.c/.h` and generated image assets | Lookup from selected icon names to LVGL images, original-canvas crop placement metadata and image data. Condition classification belongs to `weather_icon_names`; these resources make no weather decisions. |
| `message_service.c/.h` | Message records, acknowledgments, shared-client cache, hub role/discovery, message network operations and browser delivery. UI owns device navigation. |
| `main/web/messages.html` | House Messages browser rendering and controls, outside the device LVGL views. `message_service` serves the page and owns message records and operations. |
| `clock_time.c/.h` | Time source/trust, system timezone application, RTC synchronization and in-memory RTC warning. Uses the system clock; connectivity supplies actual network synchronization completion. |
| `ds3231.c/.h` | RTC register access through the board's existing I2C bus. RTC trust/correction decisions belong to `clock_time`; board facilities own the bus. |
| `alarm.c/.h` | Live alarm state, dated one-time target/completion, snooze, dismissal, missed notice and their operating-record storage. Occurrence calculations belong to `alarm_schedule`. |
| `alarm_schedule.c/.h` | Recurring and next one-time occurrence calculations, including DST; no live alarm state, storage or execution task. |
| `audio.c/.h` | Sound definitions, speaker/codec access, MP3 and generated-noise playback, Sound Machine timer/fade and status, volume progression, preview lifetime and ringing/weather-alert/preview/Sound Machine priority. Owns finite weather playback and completion receipts; the weather owner selects eligible events and the alarm owner decides when ringing is required. |
| `night.c/.h` | Effective appearance, its remembered value and backlight transitions. Owns the private adjacent-day solar cache and publishes raw events for Settings. Saved preferences belong to `device_config`; solar calculations belong to `solar_times`. |
| `solar_times.c/.h` | Solar-event calculations and unavailable-event results. `night` decides their effect on appearance. |
| `ui/ui.c` | UI lifetime, active view and navigation, including return destinations, gestures and first-use entry; overlays, alarm preemption, input routing and presentation selection. Also holds the small drawing helpers shared by the renderers. Touch actions go to the correct feature owner; feature decisions remain with those owners. |
| `ui/clock_views.c` | Rendering and object handles for analog/digital faces, unavailable time, ringing/snoozed clock surfaces, next alarm, missed alarm, clock status and backup-clock indicator/notice. Active-view and navigation state belong to `ui.c`. |
| `ui/weather_views.c` | Rendering and private object handles for Forecast, Radar, House Messages, NWS alert lists/details and their status presentation. Consumes supplied snapshots and presentation inputs without retaining another weather/message model or active-view state. Navigation, retained display snapshots, displayed-message identity and dismissal completion decisions belong to `ui.c`; message operations belong to `message_service`. Clock-edge objects remain with `clock_views.c`. |
| `ui/settings.c` | Rendering and controls for Settings entry; Device Setup; Display settings and Messaging; Weather; Wi-Fi list, credentials and progress; Alarm, Time, Repeat/Custom Days, Sound and Snooze editors; Sound Machine and its selectors; Night Shift mode, fixed/solar schedule, offsets and brightness. Owns drafts, nested editing, immediate Sound Machine controls, validation/save/lookup presentation and submission of user changes, with its row/button, time-input and keyboard helpers kept in this file. Applied settings belong to `device_config`. |

A copied value is a snapshot for a consumer, not another authority. Runtime fields about time and appearance come from their owners. Screens do not calculate alarm eligibility, infer RTC health, or drive the backlight. Feature-owner headers declare their own data; `app_runtime.h` holds data used for display, including weather age/stale presentation. `weather_client` supplies normalized provider facts to `forecast_representative`; the display consumes the selected icon. The forecast snapshot contains no competing storm-alert decision. There is no general `wac_types.h` container. DS3231 access reuses the board bus and existing driver facilities without creating a second bus or an application fork of downloaded drivers.

Keep the NWS catalog and complete active-event data in `weather_client`, with a task mutex separate from HTTP and NVS operations. Reconciliation remains interruptible by audio; it does not disable interrupts around parsing, memory allocation or record copies. UI receives a small summary and copies ordered rows or full selected details only when needed; it never retains pointers into mutable owner data. Dismissal covers all events present at the tap without storage I/O in the callback. The weather worker performs pending operating saves.

Match CAP references, expired references and validated operational VTEC relationships, retaining event identity across message revisions. Equal published priority sorts by stable event identity. Reject incomplete or oversized candidate collections as failures without truncating protective text. Evaluate cutoffs and dismissal under the short state lock even while request/save work is blocked.

Use ordinary per-event NVS blobs rather than rewriting a whole alert collection; replacement temporarily needs the old and new blob together. Keep records bound to the saved location and forget removed events, rather than keeping history. Store complete area/headline/description/instructions and a SHA-256 fingerprint of canonical non-display comparison fields, without retaining duplicate geometry and parameter text solely to identify a substantive update. These records share the existing 24 KiB NVS partition with settings and other operating data. Insufficient storage follows the alert save-failure rules; records are not compressed or shortened to fit.

Audio receives only the selected pattern and owns one bounded request/receipt, retained until the event owner consumes its actual-start time. Event eligibility and separation remain with the event owner. Normal checks are farther apart than either pattern; skip an exceptional busy request without queuing a delayed sound. Reuse Sound Machine's original timer and session during weather interruption, and discard its resume intent when a wake alarm or hardware playback failure stops it. Keep the existing stopped/error behavior after a hardware failure. The weather worker owns these operations without a separate alert worker or catalog copy.

Scan failures, validation errors, keyboards, previews, day/night styles and waiting states are states of the assigned views. They do not require separate files or managers. Keep only one set of each live UI object's handles and one active-view owner. Clock object handles remain private to `clock_views.c`; its private interface receives display inputs and callbacks from `ui.c` without exporting the shell's state or retaining another service/configuration model. Forecast, Radar, message and weather-status object handles remain private to `weather_views.c`; the shell supplies current drawing inputs and visibility, without directly manipulating those objects. `ui/ui_private.h` contains declarations shared only by these UI files, not a second state model.

`ui/controls.c` remains an optional approved path, not an implemented owner or a required extraction. Keep the existing helpers with their current UI owners unless sharing them in that file provides a concrete current benefit; any such helpers must have no settings, feature-state or navigation authority.

Repeated UI updates leave unchanged static image sources, palettes and playback labels alone. Compare immutable assets with the source already held by LVGL; small private applied-palette markers reset with their views. Weather validity, message importance, Day/Night changes and dynamic notices still update their existing objects. Apply Digital's final Day or Night layout directly. Mutable Radar image descriptors retain their explicit generation-based replacement and eviction; source-pointer equality alone cannot identify a new Radar frame.

Analog tick objects use small bounds around their unchanged line endpoints, with the line widget's normal stroke margin. This lets LVGL skip ticks outside the drawing strip without changing their appearance or adding an image cache.

Radar uses the bundled LVGL LodePNG decoder, with dimensions and decompressed sizes checked before decoding. Large request, decoder and pixel allocations use PSRAM. Grow Radar HTTP response and PNG decoder buffers by allocating, copying and freeing, so a large copy runs outside the SDK heap lock that masks interrupts. The existing PNG hook uses the SDK's allocated-size query, without a size registry. A failed allocation retains the old buffer and follows the existing failure path; growth temporarily holds both buffers. Initialize only the decoder's image-buffer allocation handlers under the initial display lock; preserve display, layer and font handlers. Decode and compose outside the display lock. The weather client owns one current-location luminance background and two final BGRA images. At 800×800 pixels these three buffers use 5,760,000 bytes; request and decoder scratch is separate and released after each image step. The same owner keeps 1,600 bytes of horizontal sampling scratch, calculating each output column's source coordinate once per tile. Validate all source colors before composition, and skip fully transparent blends without changing sampling, palette or alpha arithmetic. The final image is opaque through the circular rim; the physical screen clips its square corners. A complete image stays immutable while published. The heartbeat replaces or detaches the UI image source and evicts its cached descriptor under the display lock, then acknowledges that generation. The weather owner retains the completed display publication and actual accepted generation under its short copied-state mutex, separate from mutable worker preparation. A failed publication freezes the worker candidate and retries it before another Radar mutation; no intermediate descriptor is retained in main. Candidate finalization may run under the command critical section, but a potentially waiting publication mutex is acquired only after that section is released. Radar publications wake the existing UI task after releasing the copied-state mutex; its independent one-second clock deadline remains. This avoids rounding Radar updates to one-second polling without adding a UI task or increasing every ordinary heartbeat to animation speed. Only after the acknowledgment may the client reuse the previous slot, including when Radar is hidden. On a location change, acknowledge the invalid image publication before freeing both obsolete output slots, then decode the new background with that memory available. Playback-mode commands retain pending metadata and an already-running latest-image preparation; they cancel obsolete historical-image preparation when Latest is selected. Keep a failed short-copy publication pending instead of losing its buffer handoff. The existing weather worker advances one bounded image step between opportunities to service forecast and time work. A short control mailbox wakes that same worker for Play/Pause/Latest; no new worker or general job mechanism is introduced. Keep at most six frame records and compressed tile data, with the tile arrays and retained HTTP capacities included in a 4 MiB PSRAM cap. Allocate tile records for the current coverage rather than reserving maximum geometry in internal RAM. The two final images remain the only decoded output slots. Pause names the displayed frame; a publication copied just before that tap must not overwrite it or release its buffer early. The UI acknowledges only the generation it actually accepted or safely detached. The five-minute metadata deadline remains independent of active playback. Ordinary serialized network work can delay preparation; the current image stays visible during that delay.

### Execution

#### Requests and memory

The existing weather request gate uses a static binary semaphore to serialize whole requests and their quota-state updates. It does not retain a temporary SDK mutex priority boost across the remaining HTTP and decoding work. Current request callers are background workers; preserve serialization without forcing task priorities or adding a second request owner.

Initialize cJSON's allocation hooks once in `main.c`, before application JSON users start. Allocate JSON nodes and strings in the board's PSRAM so downloaded responses do not consume the internal memory needed by tasks and drivers. Existing cJSON allocation failures still return through the current caller; there is no new parser, pool or storage mechanism. Weather transport scratch ends before decoding, so request buffers do not occupy the decoding task stack at the same time.

Use ESP-IDF's existing memory-placement settings to keep larger software allocations and TLS working data in PSRAM, preserving internal memory for task stacks and hardware transfers. Ordinary allocations above 1,024 bytes prefer PSRAM, including typical received Wi-Fi packet copies and audio software buffers. Smaller ordinary allocations prefer internal RAM; the SDK may still fall back when its preferred region cannot satisfy a request. Explicit hardware memory requirements and the existing 32 KiB internal reserve remain unchanged. TLS selects external software allocation while its hardware transfer descriptors remain internal. Keep these choices in `sdkconfig.defaults`, without a custom allocator or driver fork. They preserve save behavior and weather validation and retries; physical verification and its limits belong in local review reports.

#### Tasks and cores

Retain the existing network/message workers, LVGL task, UI heartbeat, clock-control task and single audio worker:

- A clock-control task, started by `main.c`, runs time maintenance, alarm evaluation, and Night Shift updates by ordinary calls to their owners. It never waits for the display lock, network requests, or audio streaming. Evaluate due alarms before other optional work. RTC calls use finite timeouts; brightness fades advance in steps, not a sleep loop.
- The audio worker owns all playback and uses a 6 KiB stack. Playback priority and preemption follow the [Sound Machine rules](#sound-machine-playback) and [alert sound rules](#alert-sound-settings).

Create the existing audio worker on core 1 at its existing priority, keeping its execution and lazy speaker completion handling separate from the drawing work on core 0. Set the existing LVGL worker and UI heartbeat to core 0 through the public BSP display configuration and ESP-IDF task creation facilities; retain the other BSP defaults and task priorities. This prevents an unpinned display task or the P4's floating-point pinning from placing drawing on the audio core. Pin the existing weather worker to core 1 at its existing priority, shared with audio, so network and Radar processing do not consume the heavily loaded drawing core. Preserve normal request spacing and bounded publication under the display lock.

Initialize Night Shift's appearance before starting the clock task. Time maintenance can start before the display; signal backlight readiness only after board display initialization and the initial draw. The clock task then advances the configured brightness transition. The UI reads the owner's copied appearance without running its policy or touching the backlight.

#### Audio output

Recording repeats close and reopen the same retained filename, using SPIFFS's cached lookup rather than a backward seek through the partition; retain decoder reset, exact sample counts and joined tail/head chunks. Failed reopens follow the existing Stopped/error and explicit-retry rules.

Start audio after ordinary BSP display/bus initialization. Its worker reads the alarm owner's copied phase and applied sound/volume between short PCM writes; it does not wait for an alarm operating-record save or keep a second ringing decision. One overwritten preview-command entry carries the latest copied Start or Stop. Cancel replaces an older Start, including while speaker setup is pending; ringing discards previews. Extend this bounded copied-command facility only for current Sound Machine operations, keeping preview cancellation scoped so ordinary editor navigation cannot stop Sound Machine. UI reads short completion/error and Sound Machine playing/remaining copies; it does not keep a second playback state or deadline. Readiness means requests can be submitted, not that the speaker has been tested.

The existing public BSP owns speaker/codec wiring and the shared I2C bus. The audio worker alone attempts lazy speaker creation once per run, retains the handle and opens/closes playback sessions. A failed creation remains unavailable until restart; returned open/write failures end that attempt without repeated retry during the same ringing occurrence. A later explicit preview, Sound Machine Play, or new/resumed ring can open a retained handle again. Do not add another bus, copied codec setup, driver fork, reset service or playback persistence.

One generator supplies the three alarm tone patterns for preview and ringing. Use single-precision tone/envelope arithmetic and integer sample positions within each pattern. This avoids software double arithmetic on the P4 and preserves note boundaries during long ringing; it does not change the sound definitions. Classic Bell keeps fixed worker-owned damped-oscillator state so strikes and chunks retain earlier tails. Prepare its coefficients outside the sample loop; reset its state for fresh playback and when switching to it from another sound. Its internal normalization preserves the shared gain below and the other sounds. Preview uses copied draft sound/volume; its lifetime follows the [alarm sound rules](#alarm-sound). Ringing uses applied values and raises PCM gain from 10% of the selected level to that level over 30 seconds; a new or resumed ring starts that progression again. Preserve valid saved volume values, including zero. Short 20ms PCM chunks permit checks between writes. The volume percentage uses a squared fraction as a simple perceptual approximation: 50% produces one-quarter of maximum signal amplitude and 0% is silent. The common 0.82 PCM gain brings the loudest pattern to approximately full scale after the existing codec's configured gain at 100% volume; lower percentages remain available. The codec output control stays fixed at 100. Actual loudness and speaker distortion require listening on the board. The installed public codec's fixed write wait can still delay cancellation during a hardware fault; source inspection does not establish physical stopping latency, synthesis time or worst-case stack use.

#### Background work and UI publication

Settings saves, Wi-Fi scans, and location lookup use short-lived background work, following the existing scan/lookup approach. A pending operation disables duplicate submission; completion is handed back as data, not a pointer to a screen that may have closed. Slow settings writes do not hold the lock used to read the current configuration. There is no permanent storage worker or general job system.

UI callbacks submit commands and render results; they do not perform network requests, flash writes, or RTC access. A small fixed command queue delivers alarm/notice actions to the clock task; audio has its own playback commands. Commands carry copied values with explicit lifetimes. A full queue returns failure rather than silently losing a Dismiss or Snooze. These queues are for those actual task boundaries, not an event bus.

Alarm actions reuse the clock task's two-entry notice queue. Each request copies the displayed occurrence so a delayed tap cannot dismiss a different alarm. The UI keeps one action pending across navigation; its completion remains with `main.c` until the heartbeat obtains the display lock. Refresh the displayed alarm state before handling that completion. The clock owner decides the action and its storage result; `ui.c` decides whether returning to home or the snoozed page still belongs to the user's current view. Log rejected alarm submissions and actions that cannot be applied without replacing the on-screen alarm state with a diagnostic. Preserve the actual alarm state, completion matching and retry controls; do not show an unaccepted or unapplied action as successful. Alarm operating-record storage feedback follows the [alarm contract](#alarm-and-snooze). Log audio playback failures without showing an audio-failure caption on the ringing screen. Preserve playback failure handling, retry behavior, Snooze, Dismiss and alarm recovery. This does not change audio feedback inside the preview or Sound Machine editors.

For House Messages dismissal, UI submits copied displayed-message and device IDs to the existing message worker. `message_service` performs local acknowledgment or the shared-hub request and returns completion data. UI waits for successful acknowledgment before returning to the prior home view; failure retains the message and is reported. Completion must not override alarm preemption or navigate a view the user has already left. `ui.c` owns this navigation; `message_service` owns the acknowledgment and updated message snapshot. Disable duplicate dismissal while pending and report submission failure. No additional permanent worker or general job system is needed.

The existing UI heartbeat gathers owner snapshots before taking the display lock. Weather-copy failure does not suppress independent clock or message reads. Startup finishes using its static display staging record before creating the heartbeat; only that task then writes the same record. UI consumers copy retained inputs synchronously and do not retain a pointer to the outer staging object. Schedule its updates at fixed one-second intervals, including its work in that period. After an overrun, update current state without replaying missed frames. The UI heartbeat uses a 12 KiB stack and logs a new minimum spare-stack reading after valid weather updates, outside the display lock. Operational status and error logs remain; visual-only object, asset and hand-angle diagnostic interfaces are absent. It consumes a pending message-dismissal result only after acquiring that lock, so the message owner retains the result while the display is busy. On consuming an alarm completion, refresh only its short copied owner state under that lock so an earlier frame or result cannot replace a newly resumed ring. That read performs no I/O. Only UI code under the display lock changes LVGL objects. Keep state-copy locks short; never hold them during I/O. Hardware reads and small operating-record writes can take time; this design does not promise uninterrupted real-time execution during flash operations. Keep measured timing and its limits with the test evidence.

### Configuration and storage integration

Use the existing NVS partition and ordinary typed NVS operations. Settings stay with `device_config`; alarm operating data stays with `alarm`; the remembered appearance stays with `night`. Owners use distinct names/keys and call NVS directly. There is no shared persistence wrapper, whole-settings JSON codec, snapshot selector, migration path, journal, or cross-service partition-recovery protocol.

The configuration structure holds the user settings. Night Shift uses one mode value rather than a mode plus an independent enable flag. Readiness is derived from the settings needed by the feature, without a separate setup-version gate or location-confirmed flag. `device_config` validates saved location values against the [location output rules](#output-rules) and Shared-mode identity against the [message contract](#message-contract); the settings UI enforces [first-time setup](#first-time-weather-location-setup) using the existing editor. Initialization follows the compatibility scope in AGENTS.md; there is no migration from other firmware.

Use explicit update operations for the actual Save actions. The settings UI owns its current draft and the values with which editing began; nested editors share that draft. `device_config` applies only the submitted changes to its latest configuration. Comparing the known fields is sufficient for applying edits; no reusable session manager, revision system, or public generic patch format is required. Each explicit Display, Device, Weather or Alarm Save then writes all entries belonging to that group, including unchanged values, using ordinary individual writes. The immediate alarm toggle writes only the enabled entry. Do not compare against running settings to skip storage writes. This accepts extra writes on infrequent explicit saves and keeps retries straightforward; it does not rewrite other groups or add background saving. Alarm preemption does not make a draft authoritative. Closing/canceling an editor must not publish it.

Serialize settings writers. Validate before writing, check NVS return values, and publish a new in-memory configuration after a successful save. Use a short lock for copying the published configuration, distinct from the slow save operation. On failure, follow the [editing and saving rules](#editing-and-saving-user-settings) and keep the previously applied running configuration; do not report rollback. Some stored keys may already have changed and will be read on the next normal startup, as allowed under [Editing and saving user settings](#editing-and-saving-user-settings).

Alarm data contains the current relevant occurrence, one-time target/consumed state when used, snooze, dismissal and one pending missed notice, not a history log. Failed operating-record writes follow the [alarm and snooze recovery rule](#alarm-and-snooze). The Night Shift owner keeps one last-effective-appearance value. Backup-clock warnings follow [Time and RTC](#time-and-rtc) and have no persistent record. NWS event records belong to `weather_client` as specified under [Ownership](#ownership); [headline dismissals](#weather-alert-dismissal) survive restarts for the same active events. Retention and failure behavior follow the [alert service rules](#alert-source-catalog-and-priority). These operating records are separate from settings Save; one-time automatic Off is saved through the settings owner.

Initialize NVS once in startup. A reported initialization failure must be visible and must not trigger a blanket erase/retry. Stop storage-dependent startup on that failure rather than adding an automatic repair service. Normal missing settings receive defaults; an actual read error is reported rather than treated as a missing key.

Apply configured Wi-Fi credentials through RAM driver storage in common preparation for both scanning and connecting. `device_config` is the application credential authority; connectivity does not create a second persistent credential copy. Its preparation, initialization and scans share one operation lock so overlapping callers cannot prepare or change the driver together. Event callbacks do not take this lock; disconnect/reconnect handling remains responsive during those operations. Networking uses the existing ESP-Hosted setup.

### Board and build integration

`main.c` uses the BSP's display/touch startup and default transfer waits, with no copied startup helper or application touch-timeout override. A returned-read-error callback installed under the initial display lock clears failed touch points and cancels an interrupted press before normal polling continues. Board facilities and the driver own the shared I2C bus and recovery. The callback cannot recover a read that never returns. Normal backlight operation belongs to Night Shift rather than an unconditional startup brightness override.

`sdkconfig.defaults` disables the BSP's optional abort-on-error setting so speaker-creation failures return to the audio owner without resetting the clock. Checked display/touch, bus and backlight failures also return to their callers. This does not remove unconditional assertions or every board-library check, and returned codec status does not verify every underlying register operation.

The board uses 32 MB flash with ESP-IDF's extended cache-address option and its required experimental-feature gate for reads above 16 MiB. Flash-size selection alone does not enable those reads; bootloader and application must use matching settings. The 64-byte L2 cache line keeps ESP-Hosted's 1,600-byte PSRAM packet-buffer stride aligned for DMA transfers.

`main/CMakeLists.txt` is the sole firmware source list, explicitly including UI, generated artwork and fonts. Generators update asset contents without another manifest or a directory-scan fallback. Add or remove filenames there when the asset set changes. Asset/font generation uses the existing tools and `tools/generate_lvgl_image.py` PNG decoder; generator and build commands belong in the [build guide](build.md).

`docs/index.html` is the browser-installation page. The existing `build-release.sh` helper copies build outputs into `docs/firmware/`; it does not publish the page or choose a version. Installation and build commands remain in the README and build guide.

### Artwork storage

Maintained SVG inputs and recordings live in `assets/`. The existing screen and Analog renderers define the backgrounds and Forecast frames; their generated SVGs and PNGs use the generators' temporary directories and are removed on exit. Compiled artwork remains in `main/assets/`. The hand and center-cap SVG inputs stay in `assets/analog/`; weather SVG inputs stay in `assets/icons/`. That folder also holds the documentation-only Sleet reference; the [weather mapping reference](weather.md#current-firmware-mapping) distinguishes it from the 18 compiled weather pictures.

#### Weather icons

Use the [weather mapping reference](weather.md) for provider classifications, icon names, artwork, selection rules and the current firmware mapping. That document is the sole owner of these details. Follow the [behavior-change disclosure rule](../AGENTS.md) before implementing selection changes.

All 18 weather icons are generated directly from their original vector artwork in `assets/icons/` on a logical **400×400** canvas, rather than enlarged from raster images. They use uncompressed **RGB565A8**: a 16-bit color plane and an 8-bit transparency plane, using the format supported by the existing LVGL software renderer. Remove empty outer margins while retaining a six-source-pixel transparent guard, clamped to the canvas. Keep the original canvas size and crop origin as metadata so drawing preserves the artwork's intended scale and placement. Use direct drawing without a whole-image 32-bit conversion buffer.

Display sizes, positions and Day/Night appearance follow the [clock display rules](#clock-display) and visual reference.

#### Analog hands

The three Analog hands use the same direct planar RGB565A8 encoding. Preserve their source vectors, dimensions, pivot points, rotation, smooth alpha edges and Day/Night styles, without cropping the hands or changing their geometry. The Analog generator owns these outputs; no separate decoder, cache or drawing task is introduced.

### Planned file structure

Paths are relative to the repository root. This is the approved structural allocation, not a work list or an instruction to create empty files. Optional entries are created only for a concrete need. Existing application and development files remain subject to the [file-creation rules](../AGENTS.md#file-creation-and-permission); the exact-path permission requirement is unchanged. Pure calculation files are kept separate because their date/astronomy logic can be tested without the board. UI files group related behavior instead of assigning one file to every screen.

```text
docs/
  index.html                            Browser-installation page
  alarm-design.html                     Approved interactive visual reference
  build.md                              Environment, build and installation guide
assets/
  analog/
    hour_hand.svg                       Hour hand source
    minute_hand.svg                     Minute hand source
    second_hand.svg                     Second hand source
    center_cap.svg                      Analog center cap source
  sounds/
    originals/                          Original Sound Machine recordings
    clock/                              Prepared playback recordings
  icons/
    clear-day.svg / clear-night.svg     Clear weather artwork
    cloudy.svg / fog.svg                Cloud and fog artwork
    partlycloudy-day.svg / partlycloudy-night.svg  Partly cloudy artwork
    rain.svg / showers-day.svg / showers-night.svg  Rain and shower artwork
    rain-snow.svg / snow.svg            Rain & Snow and Snow artwork
    sleet.svg                           Documentation-only ice-pellet reference
    thunderstorm.svg / thundersnow.svg  Electrical-weather artwork
    freezing-rain.svg                   Freezing Rain artwork
    hail.svg / ice.svg                  Hail and Generic Ice artwork
    wind.svg / tornado.svg              Wind and Funnel/Tornado artwork
CMakeLists.txt                          ESP-IDF project definition
sdkconfig.defaults                      Board, memory, task and LVGL defaults
partitions.csv                          Application, settings and sound-storage partitions
dependencies.lock                       Managed dependency versions
main/
  main.c                                Startup, task creation, clock-control and UI integration
  app_runtime.h                         Copied status for display
  app_ui.h                              Sole public UI interface
  bsp_shims.h                           Public BSP backlight helper declarations
  idf_component.yml                     Managed component requirements
  device_config.c / .h                  Settings defaults, validation and NVS saves
  connectivity.c / .h                   Wi-Fi connections/scans and network time
  weather_client.c / .h                 Weather worker/schedules/retention; Radar images; NWS events/records
  forecast_representative.c / .h        Representative forecast calculations
  location_lookup.c / .h                Geocoding requests and lookup results
  location_scoring.c / .h               Location query parsing, ranking and resolved labels
  timezone_table.c / .h                 Timezone mapping data
  weather_icon_names.c / .h             Provider facts, picture/icon selection and captions
  message_service.c / .h                Message records, hub and browser operations
  web/
    messages.html                       House Messages browser rendering and controls
  CMakeLists.txt                        Direct firmware source list
  clock_time.c / .h                     Time trust, timezone and RTC synchronization
  ds3231.c / .h                         DS3231 register access on the board I2C bus
  alarm.c / .h                          Alarm, one-time target, snooze and missed notices; NVS records
  alarm_schedule.c / .h                 Recurring/one-time occurrence and DST calculations
  audio.c / .h                          Alarm/weather/preview and Sound Machine playback, timer and volume
  night.c / .h                          Day/Night appearance, fallback and backlight
  solar_times.c / .h                    Sunrise/sunset and unavailable-event calculations
  ui/
    ui.c                                Navigation, alarm preemption and input routing
    ui_private.h                        Declarations shared only by these UI files
    clock_views.c                       Clock, ringing and clock-status rendering
    weather_views.c                     Forecast, Radar, Alerts, message and status rendering
    settings.c                          Settings editors, drafts and save/lookup feedback
    controls.c                          Optional shared LVGL helpers; not currently needed
  assets/
    weather_icons.c / .h                Icon-name lookup and original-canvas crop metadata
    wac_icons.c / .h                    Shared UI and weather icon assets
    wac_fonts.c / .h                    Generated only for needed added glyphs
scripts/
  activate-idf.sh                       Activate the installed ESP-IDF environment
  common.sh                             Shared shell paths/environment checks
  setup.sh                              Existing build/flash entry point
  set-target.sh                         Select ESP32-P4
  build.sh                              Firmware compilation
  build-release.sh                      Build/copy local browser-installation images
  flash.sh                              Authorized board flashing
  monitor.sh                            Interactive serial monitoring
  capture-serial-log.sh                 Bounded serial-capture wrapper
  backup-device.sh                      Explicit device-data backup
  restore-device.sh                     Explicit device-data restoration
  test-weather-parity.sh                Native weather/location fixture checks
  generate-analog-parity-assets.sh      Analog artwork generation
  generate-screen-parity-assets.sh      Screen/weather artwork generation
  generate-custom-fonts.sh              Required custom glyph generation
tools/
  capture_serial.py                     Serial capture/reconnect implementation
  generate_lvgl_image.py                PNG decoding and LVGL image conversion
  generate_timezone_table.py            Timezone mapping generation
  rasterize_svg.py                      Vector-art rasterization
  render_analog_stage_svg.py            Analog stage/hand generation
  render_edge_indicator.py              Clock-edge artwork generation
  render_screen_stage_svg.py            Screen/utility drawing and weather SVG loading
tests/
  README.md                             Test commands and instructions
  CMakeLists.txt                        Native test source/build definitions
  test_support.c / .h                   Shared hardware substitutions only when used
  test_device_config.c                  Settings validation and save-failure checks
  test_time.c                           Time trust, timezone and RTC correction checks
  test_connectivity.c                   Wi-Fi/scan concurrency and network-time completion checks
  test_weather_service.c                Weather decoding, NWS owner and independent request schedules
  test_forecast_representative.c        Daily-picture calculation and fixture checks
  test_location_scoring.c               Qualified location query and ranking checks
  test_message_ack.py                   Message acknowledgment and shared-store handoff checks
  test_alarm.c                          Alarm/snooze, dismissal and restart-recovery checks
  test_alarm_schedule.c                 Recurring occurrences and DST boundary checks
  test_audio.c                          Alarm/weather/preview priority and Sound Machine playback checks
  test_night.c                          Day/Night, fallback and brightness checks
  test_solar_times.c                    Solar-event and unavailable-event checks
  test_ui.c                             Navigation, draft and alarm-preemption checks
  data/
    README.md                           Fixture descriptions and limits
    forecast-representative-cases.json  Forecast calculation inputs and expected results
    location-resolution-cases.json      Location parsing and ranking inputs
    weather-open-meteo.json             Sanitized Open-Meteo response
    weather-visual-crossing.json        Sanitized Visual Crossing response
    radar-six-frame/
      README.md                         Captured Radar inputs, references and verification limits
      provider-metadata.json            RainViewer timeline for the captured frames
      expected-frame-0.png              First composed 800×800 reference image
      expected-frame-1.png              Second composed reference image
      expected-frame-2.png              Third composed reference image
      expected-frame-3.png              Fourth composed reference image
      expected-frame-4.png              Fifth composed reference image
      expected-frame-5.png              Newest composed reference image
      frames/
        manifest.json                   Capture geometry, relative tile paths and checksums
        usgs-background-800.png         Captured USGS map background
        frame-0-1792413000-z7-34-53-512-smooth.png  First frame, western tile
        frame-0-1792413000-z7-35-53-512-smooth.png  First frame, eastern tile
        frame-1-1792413600-z7-34-53-512-smooth.png  Second frame, western tile
        frame-1-1792413600-z7-35-53-512-smooth.png  Second frame, eastern tile
        frame-2-1792414200-z7-34-53-512-smooth.png  Third frame, western tile
        frame-2-1792414200-z7-35-53-512-smooth.png  Third frame, eastern tile
        frame-3-1792414800-z7-34-53-512-smooth.png  Fourth frame, western tile
        frame-3-1792414800-z7-35-53-512-smooth.png  Fourth frame, eastern tile
        frame-4-1792415400-z7-34-53-512-smooth.png  Fifth frame, western tile
        frame-4-1792415400-z7-35-53-512-smooth.png  Fifth frame, eastern tile
        frame-5-1792416000-z7-34-53-512-smooth.png  Newest frame, western tile
        frame-5-1792416000-z7-35-53-512-smooth.png  Newest frame, eastern tile
```

The build/tool entries above describe existing maintained files, not permission to add other scripts or tools. The exact-path approval rule remains unchanged.

In this tree, `name.c / .h` denotes the two exact filenames with that stem. Concrete generated artwork/font files are listed directly in `main/CMakeLists.txt`. Files outside the approved paths require the permission specified in AGENTS.md; generation does not exempt maintained source files from that rule.

## Verification and limits

Weather/location checks compile the ESP32-P4 C calculations against the retained fixtures below. Use the existing ESP32-P4 build and test helpers; no other application implementation or equivalent test suite is maintained. Source artwork and recordings are in `assets/`; native fixtures are in `tests/data/`.

Existing fixtures for native C checks:

- Forecast: [forecast-representative-cases.json](../tests/data/forecast-representative-cases.json).
- Location: [location-resolution-cases.json](../tests/data/location-resolution-cases.json).

Use focused tests for settings validation/save errors, DST and catch-up boundaries, dismissal/snooze/missed-notice restart behavior, RTC status/correction, solar/fallback/brightness decisions, preview preemption, and draft/navigation behavior. Include qualified location searches, rejection of unusable timezones, Shared-mode identity validation and background message-dismissal success/failure. Test ordinary interrupted-save outcomes against the accepted partial-save behavior and operating-record failures against their accepted in-memory/restart behavior.

Build with the preserved ESP-IDF 5.5.5 environment. Hardware is required to establish touch behavior, rendering, RTC operation, audible sound, brightness and physical power-cut results. Task timing, stack/memory use and asset sizes are implementation checks, not claims established by this document.

This specification is not evidence of passing firmware tests.

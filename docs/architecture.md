# Weather alarm clock architecture and product specification

Revised October 15, 2026. This is the main authoritative specification for approved product behavior, defaults, accepted tradeoffs, module/resource/task ownership, integration and source placement. Detailed weather condition mappings and their decision status are maintained in [weather.md](weather.md). Neither document is an implementation sequence or evidence of completed firmware. Undecided behavior is not implicitly approved.

Build the weather alarm clock for the Waveshare ESP32-P4-WIFI6-Touch-LCD-3.4C using the existing ESP-IDF application. Only ESP32-P4 is supported. There is no requirement to maintain Raspberry Pi behavior, another firmware implementation, or backward compatibility with their settings or interfaces.

Keep the existing weather, storm, location selection, Wi-Fi and House Messages capabilities, including the browser messaging page. No multiple-alarm feature, ambient-light sensor, battery-voltage monitor, outage log or separate diagnostics screen is introduced.

[AGENTS.md](../AGENTS.md) defines working rules. [README.md](../README.md) provides the user-facing project overview, features, hardware, setup and user guide. Development status and verification records belong in [docs/work.md](work.md). Detailed build and test instructions remain in [targets/esp32-p4/README.md](../targets/esp32-p4/README.md) and [targets/esp32-p4/tests/README.md](../targets/esp32-p4/tests/README.md).

- [Product behavior and defaults](#product-behavior-and-defaults)
  - [Editing and saving user settings](#editing-and-saving-user-settings)
  - [Alarm and snooze](#alarm-and-snooze)
  - [Sound Machine recording decisions](#sound-machine-recording-decisions)
  - [Sound Machine controls](#sound-machine-controls)
  - [Sound Machine playback](#sound-machine-playback)
  - [UI and visual reference](#ui-and-visual-reference)
  - [Night Shift](#night-shift)
  - [Time and RTC](#time-and-rtc)
  - [Weather](#weather)
    - [Location resolution](#location-resolution)
    - [Weather icons](#weather-icons)
    - [Radar source and palette](#radar-source-and-palette)
  - [House Messages](#house-messages)
- [Implementation architecture](#implementation-architecture)
- [Verification and limits](#verification-and-limits)

## Product behavior and defaults

### Editing and saving user settings

Use ordinary saves with ESP-IDF storage.

Cancel discards unsaved edits. Nested Done returns to the parent draft without saving. Sound Machine is the immediate-control exception described in its [playback rules](#sound-machine-playback); its Back and Done controls navigate without undoing applied choices. Display Settings has independent Cancel/Save controls. Save applies Home Screen, Time Format, Leading Zero and Day/Night brightness without restarting, then returns to Settings. Display Cancel and Back discard its edits and return to Settings. Apply the chosen Home Screen when returning to the clock, while preserving later manual clock-face navigation.

Alarm Save applies without restart. The On/Off switch in Alarm Settings applies and saves immediately, writing only the enabled value. The Alarm Settings switch does not apply pending time, repeat, sound, volume or snooze edits; those still require Save. A successful switch change remains applied after Cancel or Back, while those other edits are discarded. Failed switch saves retain the applied On/Off state and pending edits for retry. Device Setup and Wi-Fi share pending edits for Save & Restart. Weather retains its independent save and restart, including location, coordinates, timezone and units. Its Back and Cancel return to Device Setup without discarding pending device edits. A Weather Save must not apply those unsubmitted device edits.

Apply only changes made in that editor, preserving unrelated applied choices. Display saves cover only its five existing fields; Device saves cover the remaining Device Setup choices. Keep the existing settings owner, stored keys and ordinary save facilities. Validation errors remain in the editor; save failure retains the draft and does not automatically restart.

A submitted Save continues if its editor is hidden, closed or replaced. A successful Device or Weather Save still completes its required restart. Results specific to the old editor must not reopen it or change a newer editor; the general saved/restarting notice may appear on the current screen. Preserve the existing restart delay, including when ringing hides the editor. Failed saves never trigger that restart.

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

Turning Alarm Off stops current ringing and cancels a pending snooze, including
one restored before usable time is available. Preserve the saved hour/minute,
repeat days and any pending missed-alarm notice. Remember the canceled occurrence
using the ordinary dismissal record; returned storage failures follow the same
in-memory action and restart limitations as Dismiss below.

Switching Alarm from Off to On, or changing its hour/minute or repeat days while
it remains On, arms the next scheduled occurrence while running. An already-past
scheduled time does not cause retroactive ringing or a new missed notice. An
occurrence due exactly when the change is observed may ring. If usable time is
not yet available, make that arming decision when it becomes available. Preserve
an existing captured ringing or snoozed alarm and pending missed notice. Changes
only to sound, volume or snooze duration do not rearm the schedule. This runtime
rule does not replace normal restart recovery below; no separate arming history
is stored.

Classic Beep, Classic Bell and Rising Tones share sound definitions between preview and ringing. Ringing increases volume gradually. Preview stops after approximately five seconds or cancellation; ringing takes priority.

Classic Bell replaces Gentle Chime, rather than adding a fourth choice. The
user selected the slower B bell demo on October 12, 2026: bright alternating
metallic bells with light hammer clatter, about 21 strikes per second in total
and a second bell pitched 6.1% higher. The selected listening reference is
[slower B](../.reviews/alarm-sound-demos/analog-bell-demo-b-slow.wav); it is
evaluation evidence, not an audio asset to embed or play in the application.
Generate the sound live in the existing audio owner, using the same definition
for preview and ringing. Preserve the other two sounds, the default Classic
Beep choice, existing volume controls and ringing progression. Keep calculation
bounded and efficient enough to feed the existing 20ms chunks without audible
cutouts. The user accepted the replacement through listening and declined further
normal-path timing and alarm retests. Unmeasured timing remains an accepted
verification limit, not unfinished replacement work; results and their limits
belong in [work.md](work.md#sprint-classic-bell-replacement--approved-october-12-2026).
Do not add a playback worker, recording, sound framework or new settings/storage
mechanism for this replacement.

Use one fixed 30-minute catch-up window for ordinary alarms and snooze. Evaluate recovery once usable time is available from the RTC or network. An eligible unhandled alarm within the window rings; an older missed occurrence produces a missed notice.

- Save the snooze deadline when Snooze is pressed, and update it on subsequent snoozes. Clear it when dismissed or finished.
- If the snooze deadline is still ahead, resume waiting until that deadline.
- If the snooze deadline has passed by 30 minutes or less, ring when usable time becomes available.
- If it is more than 30 minutes overdue, discard the snooze.

Use the same overdue-deadline check for ordinary alarm recovery and snooze recovery; do not introduce separate configurable policies.

Remember which scheduled ringing was dismissed so, after its dismissal record is successfully saved, a restart does not replay it through the 30-minute catch-up rule. This refers to the individual occurrence of the one recurring alarm (for example, Monday at 7:00 AM), not multiple alarm schedules. Dismissal does not disable future scheduled alarms. Store only the information needed for this behavior, not an alarm-history log.

Keep one pending missed-alarm notice across restarts, including the missed alarm's date/time for display. Clear it when the user dismisses the notice or the next alarm actually rings. Dismissing the notice does not disable or change the recurring alarm schedule. This is a pending notice, not a list or history of missed alarms.

Use ordinary storage operations and error reporting for the snooze deadline, dismissal state, and pending missed-alarm notice. Do not introduce backup snapshots, a journal, migration, or a separate recovery coordinator for this feature. These operating-state writes do not authorize background rewriting of user settings. Operating-state persistence not explicitly approved in this document remains undecided.

Apply Snooze, Dismiss and missed-notice transitions in memory even if their operating-record write returns an error. Snooze defers the sound and Dismiss stops it without waiting for a successful save. Log alarm operating-record read/write failures without replacing the on-screen alarm state with storage captions, and retain live transitions for the current run. A later restart may recover an older stored deadline, dismissal or missed notice; an alarm may ring again or a notice may reappear. This is an accepted limitation of failed operating-record saves. Settings Save follows the separate [editing and saving rules](#editing-and-saving-user-settings).

### Sound Machine recording decisions

Prepare all nine recordings from their original files with the selected
two-second crossfade and a 160 kbps average-bit-rate target, in mono. Apply this
to the lower-rate originals too, because every recording needs the selected
loop smoothing. Retain the original source files for later re-encoding or
reverting. Keep both sets in the project: originals in
`shared/assets/sounds/originals/` and the selected playback files in
`shared/assets/sounds/clock/`. Use only the playback set when packaging clock
recordings. Preparation results belong in
[work.md](work.md#sound-machine-recording-conversion--october-13-2026).
Keep the retained original and prepared MP3 files unchanged for the approved
[recorded-sound playback gain](#recorded-sound-playback-gain); apply that gain
on the clock rather than re-encoding or replacing the recordings.

The recordings are intended for repeating playback. Blend each ending into its
beginning with the selected constant-power curve, gradually fading one out as
the other fades in. The overlap shortens each repeat interval by two seconds.
Prepare the blend in the recording, so playback does not need to mix two copies.
The player must remove MP3 encoder delay and end padding rather than play that
extra space, and keep audio buffered across each repeat to avoid adding a pause.
Package the selected playback files in the existing storage partition. USB and
prepared browser installations include its generated image with the application.
This records the audio-asset and loop decisions. The approved controls are
defined below; implementation and verification status belong in
[work.md](work.md#prepared-sound-machine-sprint).

### Sound Machine controls

The user approved the Sound Machine design for the official
[visual reference](../mockups/alarm-design.html) on October 13, 2026. This approval
promotes the screens and controls; it is not evidence of completed firmware.

Settings replaces its redundant Alarm On/Off row with Sound Machine, showing the
chosen sound as subtext. Alarm Settings retains its immediate On/Off switch.
The Sound Machine screen contains Sound, Sleep Timer and Volume in that order,
with playback status below Volume, a Play/Stop button and Back. Volume is separate
from alarm volume. The status supports Stopped, timed playback with minutes left,
and continuous playback.

Choose one sound from Rain, Thunder, Ocean, Wind, Forest, Campfire, Stream, Night,
Singing Bowl, White Noise, Pink Noise and Brown Noise. The sound selector uses
one scrollable column of full-width choices with a clearly visible scrollbar on
the right; scroll by dragging the list or scrollbar. The Sleep Timer selector
shows all five choices: Continuous, 15, 30, 60 and 120 minutes. Both selectors
have Back and Done. Their appearance and geometry are owned by the visual
reference, using the existing settings controls and Day/Night styles.

Both clock faces show a speaker shortcut in playing and stopped states, mirrored
from the RTC warning and centered vertically between the weekday and date.
Both icons stay clear of every weekday name without moving or shrinking the
weekday/date text.
Keep the existing alarm shortcut positions and tap areas. Both clock headers
use title case for weekday and month, with matching weekday-to-date spacing.
Tap-area outlines remain diagnostic previews and are not part of the device UI.

The interactive mockup illustrates choice changes, volume, Play/Stop and retained
playback status while navigating. It does not play Sound Machine audio or run a
countdown. The approved defaults and operating rules are defined below; preview
samples are illustrative. Canceling a tap after dragging outside a button remains
a recommendation, not an approved change to the clock's touch handling.

### Sound Machine playback

The user approved these operating rules on October 13, 2026. Implementation and
verification are recorded in
[work.md](work.md#prepared-sound-machine-sprint).

First-use defaults are Rain, 30% volume, a 30-minute sleep timer and Stopped.
Volume uses the illustrated 0–100% range independently of alarm volume. Remember
the chosen sound, volume and timer using ordinary configuration storage. Every
startup, including recovery from power loss, begins Stopped. Do not persist or
restore playback activity, a remaining duration or a timer deadline.

Sound and volume changes apply immediately during playback. Changing the sound
keeps the current timer deadline; changing the timer starts the newly selected
duration from that moment. Continuous removes the expiration. While Stopped,
these choices configure the next Play without starting playback. Back and Done
only navigate: applied choices remain and playback continues. Save only the
Sound Machine preferences intentionally changed by the user through existing
ordinary save/error handling; preserve unrelated applied settings and editor
drafts. Play, Stop, countdown updates and fades do not write user settings.

Play starts the chosen sound and a fresh full timer, or continuous playback.
The timer measures elapsed running time, independent of RTC/network corrections.
Stop cancels playback immediately and leaves the selected preferences intact.
For a timed session, fade volume over its final 30 seconds and stop at expiration.
The fade changes playback gain, not the user's remembered volume setting.
A subsequent Play starts a fresh full interval. This end-of-session fade is
separate from the prepared recording blends; do not add another repeat crossfade.

Play one Sound Machine sound at a time. Generate White, Pink and Brown Noise
live on the board; use the selected prepared MP3 files for the other nine sounds
under the [recording rules](#sound-machine-recording-decisions). Changing between
recordings and noise does not reset the remaining time.

Generate the three noises efficiently in short audio chunks, using a lightweight
pseudorandom source and small, fixed filter/integrator state. Keep that state
across chunks so boundaries do not introduce discontinuities. Prepare filter
coefficients outside the sample loop. Use efficient integer or single-precision
arithmetic. Do not allocate memory, perform file/network I/O, log, or wait on
shared locks for each sample. Generation must finish with headroom inside each
chunk's playback interval so it does not cause audible hiccups or leave the
speaker without samples. Verify worst-case generation/feed timing and sustained
playback on the board during representative normal weather, network, display and
touch activity; host timing or listening to a short sample does not establish
that result. Retain the existing single audio worker rather than adding a noise
worker to compensate for inefficient generation.

Use the White, Pink and Brown equations from soundmachine.app's
[`noiseBuffer` source](https://soundmachine.app/playback.js), with no additional
generator clipping or limits on their internal state. Retain the board's existing
uniform pseudorandom input, 22,050 Hz stream and continuous state across chunks;
the website's 32,000 Hz buffered mixer and looping machinery are not imported.
For these generated noises, use the fixed playback-output reductions below,
retaining the squared volume control and timer fade. Recording boosts do not apply.
No loudness normalization is added. The existing final 16-bit PCM conversion
guard remains outside the generator. Removing the former half-level White output
and Pink/Brown clipping raises peaks; unchanged output gain does not promise a
fixed peak ceiling or distortion-free playback at every level.

Ringing and alarm-sound previews take priority and stop Sound Machine, leaving
it Stopped. There is no automatic resume after preview completion, Snooze or
Dismiss; the user presses Play to start it again. Preserve existing ringing
volume progression, preview duration, alarm settings and alarm recovery. A
Sound Machine request cannot displace active ringing or an alarm-sound preview.

Use the existing audio owner for playback, noise generation, elapsed timer,
expiration gain and copied playing/remaining/error status. Keep configuration
preferences with `device_config` and navigation/rendering with the existing UI
owners. Retain the existing single speaker owner, lazy initialization and
returned-error policy. A failed Sound Machine playback attempt ends in Stopped
with an error; an explicit later Play may retry an open/read/decode/write attempt
when the retained speaker is usable. A failed speaker creation remains unavailable
until restart, as for current alarm audio. Do not add automatic retry or resume.

#### Recorded-sound playback gain

Apply these fixed playback boosts to the existing prepared mono MP3 recordings,
as approved on October 14, 2026:

| Recording | Additional playback gain |
|---|---:|
| Rain, Ocean, Wind, Forest, Stream | +5.49 dB |
| Campfire, Night | +15.49 dB |
| Thunder | +1.12 dB |
| Singing Bowl | +2.80 dB |

The user approved a further +10 dB for Campfire and Night after the initial
board comparison, producing the total +15.49 dB boost above. Campfire's large
crackles may exceed the available range at higher volume; testing this louder
output is an accepted tradeoff. Keep the existing final PCM conversion guard,
without adding a limiter, compressor or changes to the MP3 files.

These fixed values do not target equal LUFS or RMS levels. Fixed gain does not
avoid clipping when boosted peaks exceed the
existing output range. Apply these boosts only to recordings. The three noises
retain their source equations, with these approved reductions at playback output
relative to their former output gain: White Noise −12 dB; Pink/Brown Noise −6 dB.
Do not change their internal state or add generator clipping.

Fold the selected recording's fixed multiplier into the existing Sound Machine
volume, output-headroom and sleep-fade calculation once per audio chunk, before
the final 16-bit PCM conversion. Keep the existing conversion guard and codec
output setting. Use the existing audio owner and sample loop, with no additional
sample-processing pass, worker, module, setting or storage. Do not add automatic
loudness measurement, normalization, compression or limiting for this boost.
The volume control, final fade, gapless repeat handling, alarm priority and
playback error behavior otherwise retain their existing rules. Implementation
and verification status belong in
[work.md](work.md#recorded-sound-playback-gain--october-14-2026).

### UI and visual reference

Ringing preempts menus, notices and previews; preemption alone does not acknowledge a notice or apply a draft.

The single approved visual entry point is [mockups/alarm-design.html](../mockups/alarm-design.html). It contains the complete combined device preview and the House Messages browser preview, selected using the controls above the previews. Images, fonts and both previews are embedded in that file. It owns appearance and controls; this specification owns behavior, defaults and source placement. Preview selectors and sample values are not production features. The digital ringing layout is the chosen ringing presentation; carousel alternatives are not additional screens.

Settings pages use the reviewed lower Back/title placement. Messaging's distinct
Device ID guidance is vertically centered in the fourth-row space, with its
three controls kept in their standard positions; normal messages replace that
guidance in the same space. Guidance text sizes follow the approved visual reference.
Device Setup's Weather entry opens the Weather editor with the common single-line
heading placement. Its four rows are Weather Location, Provider, Units and API
Key. Tapping the whole Weather Location row opens its usual input/keyboard;
location, units, lookup and independent save/restart behavior remain.

Open-Meteo is the default provider and its API Key row reads Not required.
Visual Crossing requires a nonempty API key. Weather Save checks a newly
selected Visual Crossing provider or changed key with a forecast request before
saving; failed checks retain the editable draft without a restart. Saves using
the unchanged selected provider/key do not make another validation request.
Provider and key belong to the ordinary Weather save, alongside location and
units. Switching to Open-Meteo retains the saved key for later reuse; only the
API Key input's Clear control explicitly clears it. The key is always masked,
and Configured does not reveal it. Cancel discards unsaved provider/key changes;
alarm preemption preserves the existing Weather draft and keyboard.

Use LVGL for the keyboard, matching the approved mockup’s lowercase, uppercase, numbers and symbols arrangements, size and position. The temporary typing layout hides inactive controls and shows the active input above the keyboard. Show the enlarged character while a key is pressed. This replaces the earlier retained-key-map and larger centered-keyboard choices; caret and colon remain available on the illustrated symbol layouts.

Use the same raised active label/input placement for location, API key, room
name, device ID, network name and password. While editing location, keep the
exact muted note “Time zone is set automatically from this location.” between
the input and keyboard. Keep real field errors above the active label, using
the reference's single-line band; normal lookup notices use “Location not
found.” and “Lookup unavailable.” without changing lookup or retry behavior.

Alarm Time and Fixed Schedule start/end use the same time control. Each minute-arrow press adds or subtracts five minutes, advancing or reducing the hour when minutes cross an hour boundary. This also changes AM/PM across noon or midnight and wraps between 23:55 and 00:00. Opening or leaving a time editor does not round an existing saved minute value. These user-selected changes replace the reference’s previous one-minute step and independent minute wrapping; the separate hour and AM/PM buttons retain their behavior.

Volume and Day/Night Brightness sliders use an 8px visible track and a 36px circular handle, centered in the existing 48px-high touch area. Retain their Day/Night colors, values, increments and draft/save behavior. This user-approved styling replaces the former browser-dependent slider painting and the firmware’s thick track; the illustrated control positions and surrounding layout remain.

Open the existing Brightness editor from Display Settings. Its Back and Done return to Display Settings, retaining the Display draft. Menu placement does not change brightness ownership, defaults, limits or backlight behavior.

Leading Zero controls the 12-hour preference. In 24-hour mode, hours always use
two digits, and Display Settings shows Leading Zero On with its switch disabled.
Returning to 12-hour mode enables the switch and restores the previous 12-hour
choice, including when editing a draft. Saving 24-hour mode preserves that choice
rather than forcing the stored preference On.

Normal Digital Night time uses a 250px nominal display size in 24-hour mode and in 12-hour mode with Leading Zero Off. Keep the existing fitted size for 12-hour Leading Zero On. Each format's size stays steady as time advances; preserve proportional Montserrat Medium digits, the existing spacing, centering and AM/PM baseline and gap. Day and alarm-specific layouts retain their separate sizes.

The normal analog clock's next-alarm caption shows its complete text on one line,
including Today/Tomorrow and 12/24-hour times. Preserve its illustrated font,
centered position, icon and touch area.

After Save & Restart, retain normal startup navigation to the clock or required unfinished setup. Wi-Fi connection status belongs in Device Setup; the mockup's connection previews do not require a separate startup screen or a saved return-route flag.

### Night Shift

Night Shift modes remain Off, Always On, Fixed Schedule and Sunset to Sunrise. Fixed equal endpoints mean Night all day; a range across midnight is supported. Solar events are calculated locally with saved offsets. While running, retain the current effective appearance when required inputs/events become unavailable. Apply gradual backlight transitions, including startup.

Local solar calculation uses the saved coordinates and usable clock time, independently of weather downloads. Keep each event's full UTC date/time, using local calendar days rather than adding 24 hours across daylight-saving changes. Apply offsets as elapsed minutes to those events. Night begins at the shifted sunset and ends at the following shifted sunrise; if the offsets remove that interval by moving its start to or beyond its end, there is no Night interval for that pair. Missing required events use the existing appearance-retention/startup fallback rules.

The Sunrise/Sunset Offsets page shows tonight's start and tomorrow's end from the Night owner's copied raw events, formatted with the selected 12/24-hour preference. Draft offsets preview those displayed times without changing the running appearance or writing storage. Refresh the times in place; retain the approved unavailable layout when they cannot be calculated.

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

Network time is primary; trustworthy RTC time can start the clock while synchronization proceeds. The RTC stores UTC. An observed stop flag makes RTC time untrustworthy; failed access is a distinct connection warning. No compile-time substitute for the actual time: without usable time, show unavailable time and wait before triggering alarms. Keep the backup oscillator enabled and correct the RTC following successful network synchronization.

When the running clock uses trustworthy backup-clock time and has not yet accepted network synchronization in the current run, show the visual reference's muted clock icon and “Waiting for sync” notice on either clock face, in Day and Night. Clear it when network synchronization succeeds, independently of backup-clock correction. It may remain while offline; losing Wi-Fi after successful synchronization does not bring it back. The existing “Clock paused” warning takes priority. This notice does not make backup-clock time unusable or change alarm eligibility, weather downloads or Night Shift decisions.

Do not persist RTC warnings or their acknowledgments across restarts. Keep a detected warning in memory for the current run, including after network time corrects the RTC, until the user dismisses it. At each startup, check the RTC again and report problems detected then; do not restore an earlier warning from storage.

If a backup-clock Dismiss request cannot be submitted, keep the technical failure in logs rather than showing a diagnostic in the dialog. Preserve the existing warning, retry opportunity and acknowledgment behavior; do not treat a failed request as a successful dismissal.

Inspect the RTC for lost-time evidence before correcting it, distinguish lost time from failed access, and clear the hardware stop flag only after successful correction. If RTC initialization fails, end that attempt before reading its calendar; report failed access and retry through the existing correction schedule. Lost-time inspection waits until initialization succeeds and still precedes correction. Correcting the RTC must not depend on saving a warning.

Retain the inherited `Clock paused` warning with its existing 135-second threshold. It reports that the displayed minute has stopped changing while UI updates continue.

### Weather

#### Weather Contract

- Weather sources: Open-Meteo (default, no key) or Visual Crossing (API key required)
- Refresh interval: every 10 minutes
- Weather downloads require Wi-Fi and a saved location, independently of usable clock time or network-time synchronization. Failed weather downloads retry after 20 seconds, except for the confirmed daily-quota pause below. Network-time startup failures also retry after 20 seconds without preventing weather downloads; successful startup leaves synchronization and its transport retries running in the background.
- Until live weather data arrives, use the approved waiting presentation, with `--` for any visible unavailable measurements; do not present fallback pictures or preview sample values as actual weather on the clock or Forecast screens.
- On Analog and Digital, keep `Waiting for weather…` in the existing weather area during the initial download and failed retries before the first successful download. Hide weather values and artwork, and show no additional download-failure footer. Preserve the existing Night hiding of this weather presentation.
- On Forecast, show `Waiting for weather…` in the existing central waiting area before the first successful download, including failed retries. Show `--` for all five day labels and temperatures, hide the five weather icons, and retain the background and forecast frames. Keep this waiting presentation visible in both Day and Night using the existing colors. Show no additional failure notice at the top or bottom. Replace the waiting presentation with provider-dated forecasts when data arrives.
- On Conditions, show `Waiting for weather…` along the existing top curve before the first successful download, including failed retries. Preserve the compass, column icons and unavailable readings, and remove the separate bottom failure notice. Keep the existing heading size, colors and Day/Night visibility; automatic retries are unchanged.
- Keep routine Wi-Fi connection, setup/location reminders and weather-download progress off the normal clock, Forecast and Conditions screens. Setup pages retain their connection and validation feedback, and operational logs remain available.
- If a download fails after earlier weather was received:
  - continue serving the last known good payload when available
  - on Analog, Digital and Conditions, show `Weather updated <age> ago` using the actual elapsed age since the last successful download when reliable; otherwise show `Weather data stale`
  - age is measured from a download timestamp captured with usable network time; show `just now` below one minute, whole minutes below one hour, whole hours below one day, then whole days. Use the stale fallback if that timestamp is missing, current network time is unavailable, or the clock is earlier than the timestamp
  - place that clock-face notice between temperature and High/Low on Analog, and below current weather and above the forecast strip on Digital; remove the additional failure footer
  - use no ending period on these short weather notices, and preserve their existing Night hiding
  - retain the failure/stale notice during retries until a successful download clears it; Wi-Fi loss still marks retained weather stale
- On Forecast, a failed refresh with retained weather shows `Weather data stale` directly above the primary forecast's day heading (`Tomorrow` when applicable). Preserve the notice's existing font, colors and Night hiding, the rest of the forecast layout and automatic retries. This notice does not calculate elapsed age.
- On Conditions, keep the retained-weather age/stale notice below the three condition columns, preserving its existing position, font and colors. The separate clock/backup-clock warnings retain their wording, appearance and visibility rules; storm warnings are unchanged.
- Current condition rendering includes:
  - current temperature
  - high/low temperatures
  - icon selection from reported condition facts plus day/night state
- Icon names, provider mappings, artwork previews and provider limitations are maintained in [weather.md](weather.md). Unknown conditions retain valid temperatures and dates with no invented weather picture.

**Approved weather sprint scope, October 13, 2026:**
include the mapping and artwork revisions in [weather.md](weather.md), including
their current-condition selection and wording rules. The approved
[daily picture rules](weather.md#five-day-forecasts) are owned there as well.
Implementation and verification records belong in [work.md](work.md).
Visual Crossing provider-credit placement is deferred at the
user's direction; no placement or private-use exception is established by that
deferral.

#### Visual Crossing quota recovery

**Approved October 13, 2026.** This is the
exception to ordinary weather retry timing when Visual Crossing reports
confirmed exhaustion of its daily allowance.

- A `429` alone does not establish daily-quota exhaustion; it can also report
  a temporary concurrency or rate limit. Apply this policy only to confirmed
  daily-quota exhaustion.
- Pause normal ten-minute downloads and ordinary twenty-second failure retries
  for the affected key. Retain the last good weather and existing stale-data
  presentation. Keep the selected provider; switching remains a user choice.
- Use a valid provider-supplied `Retry-After` delay/date or absolute reset time
  to schedule the next checking request. Honor the indicated wait; when both
  give usable times, respect the later time.
- If no usable retry/reset information is supplied, make **one checking request
  one hour after exhaustion is confirmed**. If that request again reports daily
  exhaustion without usable timing, wait another hour before the next check.
- Resume normal refreshes after a successful weather download. A failed check
  does not restart the ordinary retry loop; continue the quota-recovery schedule,
  using any newly supplied retry/reset information.
- Keep the quota pause in memory. After a restart, allow one normal startup
  request. If that request confirms daily exhaustion, restore the provider wait
  or hourly checking schedule. Do not add persistent quota state. This permits
  one additional attempt per restart.

This replaces the earlier proposed twenty-four-hour fallback. It applies to
this clock's requests using the affected key, including Save validation; a
different draft key is not paused because the applied key exhausted its quota.
Use the existing weather request and scheduling owners rather than a separate
polling task. No limit on the number of clocks sharing a key is introduced.

The availability of reset metadata and the accounting of rejected requests
remain unverified. Hourly checking is the user's approved recovery policy;
it is not a claim about Visual Crossing's reset boundary or a provider-guaranteed
retry interval. See the [provider's error guidance](https://www.visualcrossing.com/resources/documentation/weather-api/http-error-codes/).

#### Forecast Contract

- A successful download selects the same five upcoming provider entries. Retained forecasts keep those entries and their original dates after midnight or a failed refresh; do not shift their values into newer days.
- Each day includes:
  - the provider's full calendar date, used for its weekday label
  - representative icon under the [daily forecast selection rules](weather.md#five-day-forecasts)
  - high
  - low
- Show `Tomorrow` for the first entry only when usable local time confirms its date is the next calendar date. Otherwise show that entry's weekday. A missing or invalid provider date shows `--` for the label without rejecting otherwise valid weather values.

#### Conditions Contract

- Inputs: provider current wind speed/direction, hourly pressure, wind/gusts,
  precipitation amount/probability, snowfall, condition facts and temperature,
  plus daily highs. Normalize both providers to km/h, hPa, mm and cm snowfall;
  temperatures retain the selected unit and imperial display uses mph/inHg/in.
  Missing measurements remain unavailable instead of becoming zero or supporting
  a dry/calm claim. Hourly local labels come from provider dates/times; their
  absolute timestamps identify the current interval and distinguish repeated hours.
- Pressure trend is the 3h change (falling/rising beyond 1.5 hPa, otherwise
  steady -- wide enough to ignore the ~1 hPa twice-daily atmospheric tide); before 03:00 local, the next 3h forecast change is used instead
- Alerts consider now through the next 3h:
  - `thunder`: reported thunder according to the [weather classification](weather.md#thunder-classification)
  - `gusts`: gusts >= 40 mph / 64 km/h, else `wind`: sustained >= 25 mph / 40 km/h
  - `precip`: >= 7.6 mm in an hour (`Heavy snow` if snowfall, else `Heavy rain`)
- Falling pressure is not a storm-alert trigger. The user removed it on
  October 15, 2026 because it was too sensitive. Pressure changes alone must
  not activate the clock's storm indicator or storm-first navigation. Keep
  pressure measurements/trend and any non-alert explanatory use; the thunder,
  wind/gust and heavy-precipitation thresholds above are unchanged.
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
- Times follow `timeFormat` (`6 PM` or `18:00`). After successful Display Save,
  retained Conditions headline/detail use the applied format by the next ordinary
  UI update, without another weather download or restart. Unsaved format edits do
  not change those captions. Weather data, wording and warning decisions remain
  unchanged.

These warning limits are fixed, not editable settings. Imperial wind limits use
the exact conversion of 1 mph to 1.609344 km/h; metric limits use the listed
km/h values. Preserve these unit-specific cutoffs in the conditions calculation.

#### Location resolution

These rules define the ESP32-P4 user-visible behavior for turning a text location into Open-Meteo latitude, longitude, and timezone values.

##### First-time weather location setup

Start with no saved weather location or coordinates. During first-time setup, use the existing Weather editor with an empty location field. The user must select and successfully save a valid location before setup can finish. Location lookup supplies the coordinates and timezone; do not supply a default location.

Reuse the existing editor and its validation/error handling. Do not add “Not configured” menu labels or missing-location presentations across the normal clock and weather screens. The separate first-use mockup is unfinished; use the editor's ordinary empty-field presentation and retain the required setup exit guard.

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

Use the [weather mapping reference](weather.md) for provider classifications, icon names and artwork, selection recommendations and the current firmware mapping. That document is the sole owner of these details. Follow the [behavior-change disclosure rule](../AGENTS.md) before implementing selection changes.

#### Radar source and palette

The user selected RainViewer for the clock's radar and the custom green/yellow/red
palette on October 13, 2026. This is a radar source choice; the forecast and
current-condition provider remain governed by the Weather Contract above.
The selected [northern Colorado comparison](../.reviews/rainviewer-palette-comparison-2026-10-13.png)
shows original colors on the left and the approved palette on the right, with
a background map and town labels. It is a palette reference, not an approved
clock screen layout or a location default.

Weather Radar replaces the Conditions screen on the working clock for this
pass, as selected on October 15, 2026. The weather row contains Forecast then
Radar normally, and Radar then Forecast during a storm alert, using the existing
left/right swipes and row order retained for the current visit. The storm
indicator remains active and its first weather destination becomes Radar.
Radar is not a settings/menu page and has no return button.

Keep Conditions available in the official mockup as a retained design reference.
Retain its existing implementation, required assets and useful tests in their
current source locations, as explicitly requested on October 15. Remove it from
user navigation and other user-accessible routes while Radar occupies that slot.
Do not create inactive Conditions display objects solely to keep the code, or
add a fallback route, settings switch, alternate build mode or backup copy.
The device consequently loses that page's wind, pressure and outlook display;
this is an approved reduction in visible information. Preserve current weather
data, calculations and storm detection used by the clock, Forecast and alert
indicator. Removing the Conditions page does not disable those shared functions.

Use a dark background map that blends into the surrounding clock surface,
with subdued but clearly visible roads and terrain, and legible town labels.
Keep the selected radar colors distinct. Fade the map and radar gradually into
that background, keeping enough definition that the map does not look overly
soft. On October 13, 2026 the user selected the refined design in the
[official visual reference](../mockups/alarm-design.html), using its Weather Radar
preview: curved heading and frame timestamp, charcoal map with clearer roads
and terrain, enlarged town labels, a 48-pixel eased edge fade on the 800-pixel
screen, and the selected palette with a strength legend and visible source
credits. Use this as the working radar screen design. The northern Colorado
frame is recorded reference data, not a location default or live radar.
The production background provider remains to be selected.
The October 15 follow-up capitalizes Radar in the screen heading and carries
over the tighter fade selected in the preview.

The first version uses a fixed map **50 miles across** (about 80.5 km), centered
on the saved weather location: 25 miles (about 40.2 km) from the center to the
map's circular edge before its visual fade. This is a ground-distance coverage
requirement, not a provider zoom-level number. Keep this coverage when units or
latitude change. Use a small marker for that location, automatic updates and a
visible frame timestamp. The marker represents the resolved weather location, not the
clock's precise physical position. Show only the latest image; no animation,
timeline, zoom or pan controls are introduced. Preserve the selected appearance,
town labels, legend and source credits. After a failed update, keep the last
image with its original timestamp and clearly mark it as old. If no image has
been received, show “Radar unavailable.” These first-version choices were
approved on October 15, 2026; they do not establish a completed implementation.

Check RainViewer for a newer frame every **five minutes** after the initial
attempt when Wi-Fi and the saved location are ready. Download replacement radar
imagery only when needed for a newer frame, the current location or a retry of
an incomplete image; an unchanged frame is not a download failure and does not
make its timestamp newer. The displayed time is the provider's frame timestamp,
not the time this clock checked for updates. This interval does not change
forecast/current-condition refresh, retry or quota behavior. The fixed coverage
and interval were selected on October 15, 2026. They are built-in values; no
radar settings or zoom slider are introduced for this pass.

Use green below 35 dBZ, yellow from 35 through 44 dBZ and red at 45 dBZ and
above. dBZ measures radar echo strength; these colors are not a direct rainfall
rate or a storm-warning classification. Preserve the selected gradients using
these color stops, interpolating each RGB component between stops within a band:

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

RainViewer currently supplies only [Universal Blue tiles and its color table](https://www.rainviewer.com/api/color-schemes.html).
Apply the custom mapping to the original, unsmoothed radar pixels before
resizing or blending with the background; preserve transparency. Some source
colors represent several echo strengths, so the mapping must not imply exact
strength within those ranges. The comparison used snow coloring off; a separate
snow presentation has not been selected.

The [public API](https://www.rainviewer.com/api.html) needs no key, permits custom
colors for personal use and requires visible RainViewer attribution. Its
[Weather Maps API](https://www.rainviewer.com/api/weather-maps-api.html) supplies
radar overlays, not a background map or town labels. The
[2026 service limits](https://www.rainviewer.com/api/transition-faq.html) provide
two hours of past frames at ten-minute intervals, a maximum zoom of 7 and
100 requests per IP per minute; future radar frames are unavailable. These
detailed limits take precedence over the main API page's older FAQ statements.

The background-map source remains undecided. This choice
does not introduce new radar settings, source paths or task ownership; those
depend on the eventual implementation design. Implementation status belongs in
[work.md](work.md#radar-source-and-palette-decision--october-13-2026).

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
- The browser's Active Messages list shows all active, unexpired messages, including messages targeted to an individual clock. Dismissal on a clock does not remove a message from this list. The browser's Deactivate action makes the message inactive for every clock. Its existing `deviceId=all&includeDismissed=true` request selects the browser list; normal clock polling and device views remain filtered to messages addressed to that device or to all clocks, including when a clock's own ID is `all`.

#### Shared-Clock Message Behavior

- Shared mode discovers a LAN message hub over UDP broadcast
- Discovery requests, hub replies and heartbeats encode accepted device IDs and room names as JSON strings. Retain the existing packet-size limit; an oversized or failed encoding is not sent as a truncated packet.
- One device becomes the hub if none is found
- Hub selection is deterministic by device ID sort order
- Shared clients proxy message API operations to the current hub, preserving the request path, query and body without repeating the query
- Pending local message changes survive Wi-Fi loss and discovery. The existing message worker sends them to the selected hub and retries failed transfers every 15 seconds; a changed hub address is used on the next attempt.
- A successful transfer completes the unchanged copy that was sent. Local changes arriving during transfer remain pending. Reconnection alone does not resend an already delivered copy. Retain the existing RAM records and merge rules for acknowledgments and deactivation; no additional message store or worker is needed.

## Implementation architecture

### Structure

Keep one ESP-IDF application within the existing `main` component, grouped by function. Weather, location, connectivity, timezone, messaging, clock features, board support and artwork belong to that application. Service owners live directly under `main/`; device UI files live under `main/ui/`, and generated artwork and fonts use `main/assets/`. No separate original/new application layers, new component hierarchy or generic application framework are needed.

Reuse and adapt useful rendering code in the single device UI. Change setup flow, navigation, touch handling and status presentation where required by the approved design. `main/ui/ui.c` implements `main/app_ui.h`, the sole public UI interface; change that interface and its callers together when needed. Upstream integration follows the working rules in [AGENTS.md](../AGENTS.md#current-checkout-and-retained-material).

Startup and inherited-screen cleanup covers the startup sequence, screen implementation, appearance ownership and retirement of superseded paths. Keep the working weather, location, House Messages/browser, settings and clock services while replacing the device screens according to the approved visual reference. Rendering consumes feature-owner decisions; it does not keep another implementation of those decisions. Remove each replaced path when its replacement is integrated and verified. Completion requires clear ownership, preserved approved behavior, removal of superseded code, affected native checks and a firmware build; physical results remain subject to hardware verification. Rearranging files or reducing line counts alone is insufficient. Current progress and remaining work belong in [docs/work.md](work.md); product and architecture requirements remain here.

### Ownership

UI screen assignments follow the [approved visual reference](#ui-and-visual-reference).

| Owner | Responsibility and authoritative state |
|---|---|
| `main.c` | Startup and application task creation. Its existing weather worker owns refresh/retry timing, the last successful weather payload and fetch/stale status. The UI heartbeat assembles copied owner snapshots; UI selects their presentation. Alarm, time-trust and appearance decisions remain with their feature owners. |
| `device_config.c/.h` | Applied user settings, defaults, validation, serialized settings writes and publication after successful saves. Settings UI owns drafts. |
| `connectivity.c/.h` | Network initialization, connection and scan state, and network-time synchronization transport/completion. Time trust and system timezone application belong to `clock_time`. |
| `weather_client.c/.h` | Serialized weather requests, provider decoding and in-memory key-specific quota admission deadlines. Refresh/recovery scheduling and the retained payload belong to the weather worker in `main.c`. |
| `forecast_representative.c/.h` | Representative forecast calculation; no live weather state. |
| `storm_conditions.c/.h` | Conditions and storm-summary calculation; no live weather state. |
| `location_lookup.c/.h` | Geocoding requests and lookup results. `device_config` owns saved location values. |
| `location_scoring.c/.h` | Query parsing, candidate ranking and label formatting; no saved location state. |
| `timezone_table.c/.h` | Timezone mapping data used for validation and conversion. `clock_time` applies the selected mapping to the system clock's local-time rules. |
| `weather_icon_names.c/.h` | Condition-to-icon mapping and rendering resources, including generated image assets; no weather state or refresh decisions. |
| `message_service.c/.h` | Message records, acknowledgments, shared-client cache, hub role/discovery, message network operations and browser delivery. UI owns device navigation. |
| `main/web/messages.html` | House Messages browser rendering and controls, outside the device LVGL views. `message_service` serves the page and owns message records and operations. |
| `clock_time.c/.h` | Time source/trust, system timezone application, RTC synchronization and in-memory RTC warning. Uses the system clock; connectivity supplies actual network synchronization completion. |
| `ds3231.c/.h` | RTC register access through the board's existing I2C bus. RTC trust/correction decisions belong to `clock_time`; board facilities own the bus. |
| `alarm.c/.h` | Live alarm state, snooze, dismissal, missed notice and their operating-record storage. Occurrence calculations belong to `alarm_schedule`. |
| `alarm_schedule.c/.h` | Recurring-occurrence and DST calculations; no live alarm state, storage or execution task. |
| `audio.c/.h` | Sound definitions, speaker/codec access, MP3 and generated-noise playback, Sound Machine timer/fade and status, volume progression, preview lifetime and ringing/preview/Sound Machine priority. Alarm owns whether ringing is required. |
| `night.c/.h` | Effective appearance, its remembered value and backlight transitions. Owns the private adjacent-day solar cache and publishes raw events for Settings. Saved preferences belong to `device_config`; solar calculations belong to `solar_times`. |
| `solar_times.c/.h` | Solar-event calculations and unavailable-event results. `night` decides their effect on appearance. |
| `ui/ui.c` | UI lifetime, active view and navigation, including return destinations, gestures and first-use entry; overlays, alarm preemption, input routing and presentation selection. Touch actions go to the correct feature owner; feature decisions remain with those owners. |
| `ui/clock_views.c` | Rendering and object handles for analog/digital faces, unavailable time, ringing/snoozed clock surfaces, next alarm, missed alarm, clock status and backup-clock indicator/notice. Active-view and navigation state belong to `ui.c`. |
| `ui/weather_views.c` | Rendering and private object handles for Forecast, Radar replacing Conditions, and storm treatment; retained Conditions rendering code without an active user route; messages and empty/important styling; weather status and warnings. Conditions also remains available in the mockup under the radar decision above. Consumes supplied snapshots and presentation inputs without retaining another weather/message model or active-view state. Navigation, retained display snapshots, displayed-message identity and dismissal completion decisions belong to `ui.c`; message operations belong to `message_service`. Clock-edge objects remain with `clock_views.c`. |
| `ui/settings.c` | Rendering and controls for Settings entry; Device Setup; Display settings and Messaging; Weather; Wi-Fi list, credentials and progress; Alarm, Time, Repeat, Sound and Snooze editors; Night Shift mode, fixed/solar schedule, offsets and brightness. Owns drafts, nested editing, immediate Sound Machine controls, validation/save/lookup presentation and submission of user changes. Applied settings belong to `device_config`. |
| `ui/controls.c` | Shared LVGL rows/buttons, styling, time input and keyboard helpers; no saved settings, active-view state, feature decisions/state or navigation authority. |

A copied value is a snapshot for a consumer, not another authority. Runtime fields about time and appearance come from their owners. Screens do not calculate alarm eligibility, infer RTC health, or drive the backlight. Feature-owner headers declare their own data; `app_runtime.h` holds data used for display, including values required by the approved weather age/stale presentation. Intermediate forecast calculations stay with `weather_client`; the display consumes the selected icon. The weather snapshot retains both time-format versions of the visible Conditions captions, produced from the same downloaded data by the existing calculation. The view selects the applied format; it does not retain another weather model or recalculate weather decisions. There is no general `wac_types.h` container. DS3231 access reuses the board bus and existing driver facilities without creating a second bus or an application fork of downloaded drivers.

Scan failures, validation errors, keyboards, previews, day/night styles and waiting states are states of the assigned views. They do not require separate files or managers. Keep only one set of each live UI object's handles and one active-view owner. Clock object handles remain private to `clock_views.c`; its private interface receives display inputs and callbacks from `ui.c` without exporting the shell's state or retaining another service/configuration model. Forecast, Conditions, message and weather-status object handles remain private to `weather_views.c`; the shell supplies current drawing inputs and visibility, without directly manipulating those objects. `ui/ui_private.h` contains declarations shared only by these UI files, not a second state model.

### Execution

The existing weather request gate uses a static binary semaphore to serialize
whole requests and their quota-state updates. It does not retain a temporary
SDK mutex priority boost across the remaining HTTP and decoding work. Current
request callers are background workers; preserve serialization without forcing
task priorities or adding a second request owner.

Initialize cJSON's allocation hooks once in `main.c`, before application JSON
users start. Allocate JSON nodes and strings in the board's PSRAM so downloaded
responses do not consume the internal memory needed by tasks and drivers.
Existing cJSON allocation failures still return through the current caller;
there is no new parser, pool or storage mechanism. Weather transport scratch
ends before decoding, so request buffers and Conditions formatting do not occupy
the same task stack together. Verify both weather-worker and Settings activation
paths on hardware rather than relying on native stack behavior.

Use ESP-IDF's existing memory-placement settings to keep larger software
allocations and TLS working data in PSRAM, preserving internal memory for task
stacks and hardware transfers. The ordinary allocation preference is 2,048
bytes, keeping the MP3 input and smaller filesystem buffers internal-preferred;
the SDK may still fall back when its preferred region cannot satisfy a request.
TLS selects external software allocation while its hardware transfer descriptors
remain internal. Keep these choices in `sdkconfig.defaults`, without a custom
allocator or driver fork. They preserve save behavior and weather validation
and retries; physical verification and its limits belong in the work record.

Retain the existing network/message workers, LVGL task, and UI heartbeat. Add two long-lived tasks:

- A clock-control task, started by `main.c`, runs time maintenance, alarm evaluation, and Night Shift updates by ordinary calls to their owners. It never waits for the display lock, network requests, or audio streaming. Evaluate due alarms before other optional work. RTC calls use finite timeouts; brightness fades advance in steps, not a sleep loop.
- The existing audio worker owns all playback. Ringing takes priority over alarm previews; both stop Sound Machine under its [operating rules](#sound-machine-playback). Stop/snooze remains responsive during playback. The current alarm implementation uses a 6 KB stack; measure decoder stack use and adjust this same worker allocation only for a demonstrated need.

Create the existing audio worker on core 1 at its existing priority, keeping
its execution and lazy speaker completion handling separate from the drawing
work on core 0. Set the existing LVGL worker and UI heartbeat to core 0 through
the public BSP display configuration and ESP-IDF task creation facilities;
retain the other BSP defaults and task priorities. This prevents an unpinned
display task or the P4's floating-point pinning from placing drawing on the
audio core. Recording repeats close and reopen the same retained filename,
using SPIFFS's cached lookup rather than a backward seek through the partition;
retain decoder reset, exact sample counts and joined tail/head chunks. Failed
reopens follow the existing Stopped/error and explicit-retry rules.

Start audio after ordinary BSP display/bus initialization. Its worker reads the
alarm owner's copied phase and applied sound/volume between short PCM writes;
it does not wait for an alarm operating-record save or keep a second ringing
decision. One overwritten preview-command entry carries the latest copied
Start or Stop. Cancel replaces an older Start, including while speaker setup
is pending; ringing discards previews. Extend this bounded copied-command
facility only for current Sound Machine operations, keeping preview cancellation
scoped so ordinary editor navigation cannot stop Sound Machine. UI reads short
completion/error and Sound Machine playing/remaining copies; it does not keep a
second playback state or deadline. Readiness means requests can be submitted,
not that the speaker has been tested.

The existing public BSP owns speaker/codec wiring and the shared I2C bus. The
audio worker alone attempts lazy speaker creation once per run, retains the
handle and opens/closes playback sessions. A failed creation remains unavailable
until restart; returned open/write failures end that attempt without repeated
retry during the same ringing occurrence. A later explicit preview, Sound
Machine Play, or new/resumed ring can open a retained handle again. Do not add
another bus, copied codec setup, driver fork, reset service or playback persistence.

One generator supplies the three approved tone patterns for preview and ringing.
Use single-precision tone/envelope arithmetic and integer sample positions within
each pattern. This avoids software double arithmetic on the P4 and preserves
note boundaries during long ringing; it does not change the sound definitions.
Classic Bell keeps fixed worker-owned damped-oscillator state so strikes and
chunks retain earlier tails. Prepare its coefficients outside the sample loop;
reset its state for fresh playback and when switching to it from another sound.
Its internal normalization preserves the shared gain below and the other sounds.
Preview uses copied draft sound/volume and ends after approximately five seconds
of playback or cancellation. Ringing uses applied values and raises PCM gain
from 10% of the selected level to that level over 30 seconds; a new or resumed
ring starts that progression again. Preserve valid saved volume values, including
zero. Short 20ms PCM chunks permit checks between writes. The volume percentage
uses a squared fraction as a simple perceptual approximation: 50% produces
one-quarter of maximum signal amplitude and 0% is silent. The common 0.82 PCM
gain brings the loudest pattern to approximately full scale after the existing
codec's configured gain at 100% volume; lower percentages remain available.
The codec output control stays fixed at 100. Actual loudness and speaker
distortion require listening on the board. The installed public codec's fixed
write wait can still delay cancellation during a hardware fault;
source inspection does not establish physical stopping latency, synthesis time
or worst-case stack use. Completed observations, accepted verification limits
and remaining board checks belong in [work.md](work.md).

Initialize Night Shift's appearance before starting the clock task. Time maintenance can start before the display; signal backlight readiness only after board display initialization and the initial draw. The clock task then advances the configured brightness transition. The UI reads the owner's copied appearance without running its policy or touching the backlight.

Settings saves, Wi-Fi scans, and location lookup use short-lived background work, following the existing scan/lookup approach. A pending operation disables duplicate submission; completion is handed back as data, not a pointer to a screen that may have closed. Slow settings writes do not hold the lock used to read the current configuration. There is no permanent storage worker or general job system.

UI callbacks submit commands and render results; they do not perform network requests, flash writes, or RTC access. A small fixed command queue delivers alarm/notice actions to the clock task; audio has its own playback commands. Commands carry copied values with explicit lifetimes. A full queue returns failure rather than silently losing a Dismiss or Snooze. These queues are for those actual task boundaries, not an event bus.

Alarm actions reuse the clock task's two-entry notice queue. Each request copies
the displayed occurrence so a delayed tap cannot dismiss a different alarm.
The UI keeps one action pending across navigation; its completion remains with
`main.c` until the heartbeat obtains the display lock. Refresh the displayed
alarm state before handling that completion. The clock owner decides the action
and its storage result; `ui.c` decides whether returning to home or the snoozed
page still belongs to the user's current view. Log rejected alarm submissions
and actions that cannot be applied without replacing the on-screen alarm state
with a diagnostic. Preserve the actual alarm state, completion matching and
retry controls; do not show an unaccepted or unapplied action as successful.
Alarm operating-record storage feedback follows the [alarm contract](#alarm-and-snooze).
Log audio playback failures without showing an audio-failure caption on the
ringing screen. Preserve playback failure handling, retry behavior, Snooze,
Dismiss and alarm recovery. This does not change audio feedback inside the
preview or Sound Machine editors.

The Alarm editor previews its draft's next scheduled time through the existing
pure `alarm_schedule` calculation using supplied usable clock time. This is
display-only: it does not handle, arm or save an occurrence. Live recovery,
handled occurrences and snooze remain with `alarm`; the editor retains its
existing alarm-time formatter and caption layout.

Ringing hides the existing Settings root without closing it; the draft, active
input and keyboard retain their single owner. A later Settings swipe resumes
that editor. The explicit Alarm Settings shortcut resumes a held Alarm editor;
otherwise it closes any unrelated held editor without saving and starts the
Alarm editor. No second draft or saved return route is created.

For House Messages dismissal, UI submits copied displayed-message and device IDs to the existing message worker. `message_service` performs local acknowledgment or the shared-hub request and returns completion data. UI waits for successful acknowledgment before returning to the prior home view; failure retains the message and is reported. Completion must not override alarm preemption or navigate a view the user has already left. `ui.c` owns this navigation; `message_service` owns the acknowledgment and updated message snapshot. Disable duplicate dismissal while pending and report submission failure. No additional permanent worker or general job system is needed.

The existing UI heartbeat gathers owner snapshots before taking the display lock.
Schedule its updates at fixed one-second intervals, including its work in that
period. After an overrun, update current state without replaying missed frames.
Retain the UI task's minimum-stack observation after valid weather updates,
outside the display lock, for the measured stack-fault correction. Retire the
visual-only object/asset/hand-angle diagnostics and their renderer interface;
ordinary operational status and error logs remain.
It consumes a pending message-dismissal result only after acquiring that lock, so the message owner retains the result while the display is busy. On consuming an alarm completion, refresh only its short copied owner state under that lock so an earlier frame or result cannot replace a newly resumed ring. That read performs no I/O. Only UI code under the display lock changes LVGL objects. Keep state-copy locks short; never hold them during I/O. Hardware reads and small operating-record writes can take time; this design does not promise uninterrupted real-time execution during flash operations. Measure interaction and alarm latency during implementation rather than adding speculative workers now.

### Configuration and storage integration

Use the existing NVS partition and ordinary typed NVS operations. Settings stay with `device_config`; alarm operating data stays with `alarm`; the remembered appearance stays with `night`. Owners use distinct names/keys and call NVS directly. There is no shared persistence wrapper, whole-settings JSON codec, snapshot selector, migration path, journal, or cross-service partition-recovery protocol.

Extend the existing configuration structure for the approved settings. Use one Night Shift mode value, not both a mode and an independent enable flag. Derive readiness from the settings needed by the feature without a separate setup-version gate or location-confirmed flag. `device_config` validates saved location values against the [location output rules](#output-rules) and Shared-mode identity against the [message contract](#message-contract); the settings UI enforces the [first-time setup decision](#first-time-weather-location-setup) using the existing editor. Initialize application settings according to the compatibility scope in AGENTS.md; no migration from other firmware is required.

Use explicit update operations for the actual Save actions. The settings UI owns its current draft and the values with which editing began; nested editors share that draft. `device_config` applies only the submitted changes to its latest configuration. Comparing the known fields is sufficient for applying edits; no reusable session manager, revision system, or public generic patch format is required. Each explicit Display, Device, Weather or Alarm Save then writes all entries belonging to that group, including unchanged values, using ordinary individual writes. The immediate alarm toggle writes only the enabled entry. Do not compare against running settings to skip storage writes. This accepts extra writes on infrequent explicit saves and keeps retries straightforward; it does not rewrite other groups or add background saving. Alarm preemption does not make a draft authoritative. Closing/canceling an editor must not publish it.

Serialize settings writers. Validate before writing, check NVS return values, and publish a new in-memory configuration after a successful save. Use a short lock for copying the published configuration, distinct from the slow save operation. On failure, follow the [editing and saving rules](#editing-and-saving-user-settings) and keep the previously applied running configuration; do not report rollback. Some stored keys may already have changed and will be read on the next normal startup, as allowed under [Editing and saving user settings](#editing-and-saving-user-settings).

Alarm data needs only the current relevant occurrence, snooze, dismissal and one pending missed notice, not a history log. Use a small explicit representation for those fields. Failed operating-record writes follow the [alarm and snooze recovery rule](#alarm-and-snooze). Use one last-effective-appearance value in the Night Shift owner. Warning persistence is governed by [Time and RTC](#time-and-rtc) and introduces no record. These operating records are not part of settings Save and do not rewrite preferences.

Initialize NVS once in startup. A reported initialization failure must be visible and must not trigger a blanket erase/retry. Stop storage-dependent startup on that failure rather than adding an automatic repair service. Normal missing settings receive defaults; an actual read error is reported rather than treated as a missing key.

Apply configured Wi-Fi credentials through RAM driver storage in common preparation for both scanning and connecting. `device_config` is the application credential authority; connectivity must not create a second persistent credential copy. Its preparation, initialization and scans share one operation lock so overlapping callers cannot prepare or change the driver together. Event callbacks do not take this lock; existing disconnect/reconnect handling remains responsive during those operations. Check support through the existing ESP-Hosted setup during implementation.

### Existing-code changes that matter

- `main.c`: connect owner startup/tasks, remove the unconditional normal-operation backlight-on override, and stop treating an RTC-initialized date as proof of network synchronization. Use the BSP's display/touch startup and default transfer waits, with no copied startup helper or application touch-timeout override. Install the small returned-read-error callback under the initial display lock; clear failed-read points and cancel an interrupted press before normal polling continues. Board facilities and the existing driver retain ownership of the shared I2C bus and recovery. The callback cannot recover a read that never returns.
- `connectivity.c/.h`: preserve networking and scanning; deliver actual synchronization completion to the time owner and apply the credential rule above. Inspect both the caller and `connectivity_sync_time()` shortcut so RTC startup does not suppress network synchronization.
- `clock_time.c/.h`: apply the selected system timezone using the existing mapping; move timezone application out of connectivity and UI paths so this owner controls it.
- `location_lookup.c/.h` and `location_scoring.c/.h`: preserve the query qualifier and enforce the location search/output rules above.
- `message_service.c/.h` and `main/web/messages.html`: provide the background dismissal path and preserve the browser page's approved appearance and controls. Keep message records, cache, hub operations and browser delivery with the existing service.
- `app_runtime.h`: extend the existing display snapshot only as needed; remove competing time/appearance decisions from consumers. Update `main.c` consumers to read published configuration copies instead of a concurrently mutated global.
- `device_config.c/.h`: extend the single model and ordinary save operations. Do not carry over original setup migration or compatibility gates. During configuration implementation, remove the persistent diagnostic boot counter, its storage operations and startup-log field. No approved product behavior requires it.
- `main/CMakeLists.txt`: list actual application sources directly, including UI files and generated artwork/font sources. It is the sole firmware source list; generators update asset contents without maintaining another manifest or relying on a directory-scan fallback. Add or remove filenames here when the asset set changes.
- Existing asset/font scripts and `main/assets/`: reuse their conversion approach and existing assets. Keep the existing PNG decoder in `tools/generate_lvgl_image.py`. Add only artwork or glyphs required by the approved UI, not a new asset pipeline.
- `sdkconfig.defaults`: disable the BSP's optional abort-on-error setting so speaker-creation failure can return to the audio owner without resetting the clock. This global BSP option also makes its checked display/touch, bus and backlight failures return to their existing callers; healthy paths and transfer waits remain unchanged. It does not remove unconditional assertions or every board-library error check, and returned codec status does not verify every underlying register operation.
- `sdkconfig.defaults`: retain the board's 32 MB flash and enable ESP-IDF's extended cache-address setting for application content beyond 16 MiB. The flash-size helper alone does not enable those CPU reads. The SDK requires its experimental-feature gate for this setting; verify matching bootloader/application images on the actual flash chip. No custom flash driver or changed partition layout is needed.

During UI integration, remove the old benchmark/parity renderers, their alternate startup paths, and files, assets and build options used only by those modes. Measure the actual application with existing ESP-IDF/LVGL timing and memory facilities when needed; do not retain separate experimental UIs. This retires the old layer-by-layer and alternative-renderer experiments without changing approved product behavior. Existing messaging, including its browser page and message storage, retains its own owner; no broad storage cleanup is implied.

### Planned file structure

Paths are relative to the repository root. This is the structural allocation, not an instruction to create empty files. Existing files that still serve the application or its development remain unless explicitly moved into this functional grouping. Pure calculation files are kept separate because their date/astronomy logic can be tested without the board. UI files group related behavior instead of assigning one file to every screen.

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
    clock_time.c / .h                   Time trust, timezone and RTC synchronization
    ds3231.c / .h                       DS3231 register access on the board I2C bus
    alarm.c / .h                        Alarm, snooze and missed notices; NVS records
    alarm_schedule.c / .h               Recurring alarm and DST calculations
    audio.c / .h                        Alarm/preview sounds, volume and playback
    night.c / .h                        Day/Night appearance, fallback and backlight
    solar_times.c / .h                  Sunrise/sunset and unavailable-event calculations
    ui/
      ui.c                              Navigation, alarm preemption and input routing
      ui_private.h                      Declarations shared only by these UI files
      clock_views.c                     Clock, ringing and clock-status rendering
      weather_views.c                   Weather, message and indicator rendering
      settings.c                        Settings editors, drafts and save/lookup feedback
      controls.c                        Shared LVGL controls and input helpers
    assets/
      wac_icons.c / .h                  Generated only for needed added icons
      wac_fonts.c / .h                  Generated only for needed added glyphs
  tests/
    README.md                           Test commands and instructions
    CMakeLists.txt                      Native test source/build definitions
    test_support.c / .h                 Shared hardware substitutions only when used
    test_device_config.c                Settings validation and save-failure checks
    test_time.c                         Time trust, timezone and RTC correction checks
    test_connectivity.c                 Wi-Fi/scan concurrency and network-time completion checks
    test_weather_service.c              Weather decoding and independent download/time retry checks
    test_alarm.c                        Alarm/snooze, dismissal and restart-recovery checks
    test_alarm_schedule.c               Recurring occurrences and DST boundary checks
    test_audio.c                        Sound preview and ringing-priority checks
    test_night.c                        Day/Night, fallback and brightness checks
    test_solar_times.c                  Solar-event and unavailable-event checks
    test_ui.c                           Navigation, draft and alarm-preemption checks
```

In this tree, `name.c / .h` denotes the two exact filenames with that stem. No source files are created by this architecture revision. Concrete asset inputs can use the existing asset tools; their required filenames will be identified from the selected graphics, rather than inventing assets now.

## Verification and limits

Weather/location checks compile the ESP32-P4 C calculations against the retained fixtures below. Use the existing ESP32-P4 build and test helpers; no other application implementation or equivalent test suite is maintained. Reuse needed assets and fixtures in their existing locations; a `shared/` path does not impose another supported platform.

Existing fixtures for native C checks:

- Forecast: [forecast-representative-cases.json](../shared/test-data/forecast-representative-cases.json).
- Conditions: [storm-conditions-cases.json](../shared/test-data/storm-conditions-cases.json).
- Location: [location-resolution-cases.json](../shared/test-data/location-resolution-cases.json).

During implementation, use focused tests for settings validation/save errors, DST and catch-up boundaries, dismissal/snooze/missed-notice restart behavior, RTC status/correction, solar/fallback/brightness decisions, preview preemption, and draft/navigation behavior. Include qualified location searches, rejection of unusable timezones, Shared-mode identity validation and background message-dismissal success/failure. Test ordinary interrupted-save outcomes against the accepted partial-save behavior and operating-record failures against their accepted in-memory/restart behavior.

Build with the preserved ESP-IDF 5.5.5 environment. Hardware is required to establish touch behavior, rendering, RTC operation, audible sound, brightness and physical power-cut results. Task timing, stack/memory use and asset sizes are implementation checks, not claims established by this document.

Current implementation status and verification records are reported in [docs/work.md](work.md). This specification is not evidence of passing firmware tests.

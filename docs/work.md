# ESP32 Weather Alarm Clock development work

Local commit checkpoint (October 12, 2026): the user authorizes committing the
reviewed application and documentation changes through the Weather menu and
Classic Bell work. This supersedes earlier slice-specific statements that no
commit was authorized. The completed review and fresh checks below cover this
checkpoint; deferred work and accepted testing limits remain recorded. Local
review evidence and editor files stay outside the commit. No push is authorized.

Completed adversarial review (October 12, 2026; report only): two Optimizer and
two independent Skeptic passes found no new actionable finding in the full
branch and pending application changes. Fresh native configure/build and all
33 checks pass in 15.66 seconds; weather fixtures, messaging checks, firmware
build and syntax/whitespace checks also pass. Source and board state are
unchanged; no fixes, flash or PR comments. Cached Git references were used
after automatic approval review rejected the remote fetch. Known deferred
issues and accepted Bell testing limits remain as recorded. No new work-list
item is added. See the [local review report](../.reviews/main/adversarial-weather-bell-2026-10-12/summary.md)
for scope, reviewer verification and evidence limits.

## Sprint: Weather menu interface and shared keyboard spacing — approved October 12, 2026

Status: completed in the existing Settings renderer and tests, with the requested
Day checks and Night appearance confirmed by the user. Parent-owned
fresh native configure/build and all 33 checks pass in 14.20 seconds. Independent
source/simplicity review found no actionable defect. ESP-IDF 5.5.5 build passes
(image `0xf622c0`, 23% application partition free). Authorized flash verifies
all three written images without writing the NVS partition. The 45-second
startup capture matches ELF `1f6719148`, reports display/touch initialization
and live Open-Meteo weather, and contains one boot with no panic, assertion or
watchdog error. The final normal image has no Bell timing/automatic-playback
code. After the requested pause, the user confirms all three prompted board
checks pass in Day appearance: the four Weather rows are readable; Open-Meteo's
Not required row leaves the keyboard closed; location typing, the timezone
note and keyboard fit; the Visual Crossing key is masked and shows Entered
(not saved), Save remains blocked without restart, and Cancel/reopening restores
Open-Meteo; Room Name, Device ID, Network Name and Password typing also fit and
work. These are user-observed Day results. The user subsequently confirms Night
mode looks good too, closing the requested appearance check. They did not
separately repeat each functional step in Night. Classic Bell sounds fine in
their listening check; the user accepts the replacement and declines additional
alarm retesting. Both bounded sprints are complete at this accepted checkpoint.
No further board test or capture is started; the normal firmware keeps running.
Disposable native build directories and task-owned serial captures are cleaned
up; useful logs remain local.
The user authorizes firmware implementation, testing and flashing. This chat
owns firmware and board access. Preserve the Classic Bell replacement, remove
its remaining disposable timing counters before the final normal build, and
retain the existing alarm evidence without adding replacement-sound retests.
Local evidence is in
[the closure record](../.reviews/main/weather-menu-spacing-2026-10-12/closure.md).

Implementation uses one temporary provider choice and a 128-character key draft
with the existing editor, plus its shared sixth textarea slot. There is no
provider configuration or credential persistence. The key summary reads
“Entered (not saved)” and never displays the key. The timezone note uses existing
22px Montserrat with slightly tighter spacing to fit the exact sentence on one
line; it ends at y382 before the keyboard at y390. Weather's two-line notices
end at y211 before the first row at y212. Existing custom font supports the
pending-message ellipsis; the small save-failure notice uses a straight
apostrophe with the same words. No font asset/configuration change was needed.
All six raised input/label layouts, keyboard pages, whole-row entry, disabled
Open-Meteo key row, unsupported-provider Save blocking/reset, retained Device
draft, alarm hiding/resumption and the existing save/failure/restart cases pass.

Objective: implement the current official mockup's four-row Weather menu and
shared raised typing layout. Permitted implementation files are existing
`targets/esp32-p4/main/ui/settings.c` and, only if its existing navigation needs
adjustment, `ui.c`/`ui_private.h`. Extend existing `tests/test_ui.c` checks;
update this work record, architecture and relevant user/test guidance. Remove
Bell probes only from existing `main/audio.c`. No new source path, provider
adapter, task, queue or credential storage is planned.

Disclosed temporary provider behavior: Open-Meteo remains the working provider.
Selecting Visual Crossing changes only the Weather editor's unsaved presentation
and enables its password-style API Key field. Show that Visual Crossing is not
available yet and block Weather Save while selected. Provider/key drafts are
discarded on leaving the Weather editor; neither becomes applied configuration
or a stored credential. Switching back to Open-Meteo permits the existing
location/units save without a key. Backend implementation is deferred.

Acceptance: Day/Night Weather shows Location, Provider, Units and API Key in
the reference's order and row/footer positions. The whole location row opens
editing. All six text fields use the shared 12px-raised label/input placement;
keyboard geometry/pages remain unchanged. Location's exact automatic-timezone
note and real errors fit their reference bands. Verify unsupported-provider
Save blocking, draft discard, existing lookup/timezone/units and Save isolation,
Back/Done/Cancel and alarm interruption/resume. Run affected checks and the full
native suite, compile firmware, independently review simplicity and ownership,
then flash without erasing settings and match bounded startup evidence to the
tested image. Physical confirmations cover only the new controls/spacing and
remain open until observed. No commit, push or merge is authorized.

Keyboard spacing mockup follow-up (October 12, 2026): the shared active input
and label move up 12px across all six text editors. Weather's timezone guidance
is enlarged to nearly the input's full width and centered between the field
and keyboard. Weather field errors keep their wording and fit on one line above
the raised label. Browser checks confirm the common placement for all six
fields, Day/Night guidance layout and clearance for the missing-location and
missing-API-key messages. This records the earlier mockup-only checkpoint;
its interface/spacing firmware is now implemented above. Screenshots and measurements
are local under `.reviews/weather-settings-mockup-2026-10-12/`.

## Sprint: Classic Bell replacement — approved October 12, 2026

Status: completed and accepted by the user on October 12, 2026. The user confirms
Classic Bell sounds fine and declines additional alarm retesting because this
slice changes the sound, not the alarm controls. Existing alarm/recovery results
are retained; no further alarm tests are required for this replacement.
The middle enum/label/description, live generator, browser preview and affected
tests are updated in existing files. Nine focused audio cases pass in 0.45
seconds. The full native suite passes all 33 checks in 13.82 seconds; ESP-IDF
5.5.5 build passes (image `0xf61e50`, 23% application partition free). Independent
source/necessity review found no actionable code issue. Temporary counters in
existing audio.c measure generation, feed gaps and stack reserve. Early automatic
previews played over the Analog clock, unlike the normal sound editor or ringing
screen. They measured about 1.6ms average generation in one run, but individual
wall-clock delays reached 21ms and, in a second run, 159–237ms. These measurements
include task interruptions and do not establish the cause or a passing result.
The user reported Bell playback at restart; that was the disclosed diagnostic
preview. Automatic playback is removed and the manual-only measurement image
is loaded: all three written images verify, the 35-second capture matches ELF
`a18bee8e1`, and display/touch and live weather initialize without a panic.
The capture includes ordinary playback activity; its sound/volume and listening
result have not been identified, so it is not a Bell acceptance result.
The remaining counters are removed and the exact clean audio source hash is
verified. All
nine counter-free audio checks pass in 0.70 seconds; the full 33-check suite and
firmware build also pass in the Weather sprint above. That normal combined image
is flashed and identity-verified. The user now reports Classic Bell sounds fine.
This is a basic listening acceptance, without a separately measured normal-path
timing result or scheduled Bell ring. Those additional checks are removed from
this sprint's work list at the user's request, not reported as having passed.
Earlier Classic Beep ring/recovery results remain valid evidence for the
unchanged alarm behavior; do not relabel them as Bell measurements.

Behavior change: replace the existing Gentle Chime melody and its name with
Classic Bell, using the user's latest selection, **slower B**. This changes the
second sound choice; it does not add a choice or change the default sound.
The approved sound and preserved playback behavior belong to
[Alarm and snooze](architecture.md#alarm-and-snooze), and audio ownership remains
under [Ownership](architecture.md#ownership). No unrelated
behavior change is proposed; disclose any newly necessary change before coding.

Work and ownership:

1. In existing `main/audio.c`, replace the chime calculation with efficient live
   bell generation. Use single-precision arithmetic and integer sample timing,
   with bounded work and storage. Move repeated coefficient calculations outside
   the sample loop where useful; choose further optimization from measurements.
   Preserve overlapping bell tails across strikes/chunks and reset playback
   state for a fresh preview or new/resumed ring. Normalize the bell internally
   as needed for headroom, preserving the shared volume curve and other sounds.
   Keep speaker access, commands, cancellation and errors with the existing worker.
2. Update existing configuration identifiers, Settings labels/descriptions,
   affected tests, the official mockup's sound choice/preview and user-facing
   sound list. Remove the superseded chime definition and its obsolete waveform
   expectations. Keep unrelated concurrent changes and historical evidence.
3. Verify the sound and playback controls in existing tests, build with ESP-IDF
   5.5.5, then flash and verify the resulting images and running identity. Preserve
   NVS settings; record and restore any temporary alarm choices used for testing.
4. Diagnostic measurements and their limits are recorded above. Disposable
   counters/automatic previews are removed and the final normal image is verified.
   The user accepts the sound after listening and declines further alarm retests.
   Keep unmeasured normal-path timing as an evidence limit, not pending work.

Acceptance: Classic Bell matches the selected character/cadence and appears in
the second sound slot; Classic Beep and Rising Tones retain their waveforms.
Check full-volume bounds, zero/arbitrary volume, five-second preview, cancellation,
new/resumed-ring progression, priority and long-running sample timing. All
affected checks and the normal native suite/build pass. The user's final listening
acceptance closes this replacement; additional alarm retesting is declined.
The diagnostic measurements retain their stated limits. This bounded session does not
reopen the previously deferred wider outage or twelve-minute alarm tests.

The YAGNI choice is replacement inside existing owners: it preserves the alarm
feature set while changing the selected sound, and avoids an additional worker,
queue, persistent field, recorded PCM asset or generic synthesis system. Sound
requirements stay in the architecture, execution/evidence here, UI wording in
the existing views and mockup, and generation/playback in `audio.c`. Use approved
existing source paths only. No push, merge or unrelated cleanup is authorized.

## Existing development and verification record

Weather provider mockup update (October 12, 2026): the official Weather screen
under Device Setup now shows Weather Location, Provider, Units and API Key.
The location row opens the existing editor; its automatic-timezone guidance
appears below the location input and above the keyboard. Open-Meteo shows
“Not required” for the key; Visual Crossing enables the key editor. The preview
simulates key presence without storing or checking a real key. Only the two
lookup failure messages were shortened, and the Weather notice area is centered.
Browser checks covered both providers and units, required-key guidance, draft
retention/discard, location editing, sample lookup/save and Day/Night layout.
All embedded scripts parse. These are mockup checks, not firmware
or hardware verification. The interface/spacing firmware is now implemented in
the current sprint above; Visual Crossing downloads and credential persistence
remain deferred. This earlier mockup task changed no firmware; concurrent work
was preserved. Screenshots are local under
`.reviews/weather-settings-mockup-2026-10-12/`.

Startup and inherited-screen cleanup is the current priority under [AGENTS.md](../AGENTS.md#current-checkout-and-retained-material). Its scope and completion requirements are in the [architecture](architecture.md#structure). Continue this work before unrelated feature expansion; it is not an unspecified later sprint.

Messaging guidance centering (October 12, 2026): the user confirms the Weather
heading and larger guidance look good, and requests the guidance sit higher,
centered where the fourth entry box would be. The existing note moves up 19px;
its two-line text is vertically centered in that row. Font, wording, color,
controls, error/typing layout and save behavior remain. The mockup, architecture
and existing test guide reflect this placement. Fresh native configure/build and
all 33 checks pass in 14.89 seconds, including actual vertical centering, control/
Done clearance and keyboard hide/restore in both appearances. The independent
spacing review passes; its feedback about stale work-list wording is corrected.
All 11 mockup scripts parse and its actual Messaging renderer uses the revised
position. ESP-IDF 5.5.5 build passes (image `0xf61e60`, 23% application partition
free). Authorized flash verifies all three written images without writing NVS
settings. A 45-second capture matches tested ELF `e530affc2`, reports Display
ready/touch detected and live weather, and contains one boot with no panic,
assertion or watchdog error. The user confirms the guidance is centered and looks
good, closing the requested placement and Done-clearance check. The appearance
was not separately stated; the short capture does not establish prolonged stability.
The preceding Weather-title/font checks are closed as reported, without
separately establishing both appearances. The disposable native build directory
and serial capture are cleaned up. Concurrent Weather mockup/documentation work
is preserved; it is separate from this one-coordinate firmware change. No commit
occurred. See
[local closure evidence](../.reviews/main/messaging-guidance-center-2026-10-12/closure.md).

Weather heading and Messaging guidance follow-up (October 12, 2026): the user
confirms the loaded lower Back/title placement, nearby text clearance, Back
navigation and Weather entry work. Messaging guidance hides while typing and
returns when the keyboard closes. They requested the editor heading also read
Weather and the guidance match the larger automatic-timezone note. The existing
renderer now uses the common single-line Weather heading and the existing 26px
guidance font. No save behavior, new source path or font asset is introduced.
Intervening edits to the official mockup are preserved; its Weather heading was
already renamed, and only the Messaging guidance size changes in this follow-up.
Fresh native configure/build and all 33 checks pass in 16.30 seconds. Actual LVGL
checks confirm the guidance wraps to two lines and clears the controls/Done in
both appearances, with typing restoration unchanged. Weather header, navigation,
pending/saved/first-use views, save scope and retained alarm/weather checks pass.
All 11 mockup scripts parse and execution checks pass. Independent review found
no remaining issue. ESP-IDF 5.5.5 build passes (image `0xf61e60`, 23% application
partition free). The authorized flash verifies all three written images without
writing NVS settings. A 45-second startup capture matches tested ELF `ef45f42df`,
reports Display ready/touch detected and live weather, and contains one boot with
no panic/assertion/watchdog error. The disposable native build directory and
serial capture are cleaned up; logs remain local. Earlier header/typing checks
are closed as observed, without separately establishing both appearances. The
user confirms the new Weather title and larger guidance look good; their request
to center that guidance higher is addressed above. No commit occurred. See
[local closure evidence](../.reviews/main/settings-header-followup-2026-10-12/closure.md).

Settings-header firmware adjustment (approved October 12, 2026): the user
accepted the official preview and authorized implementation, testing and flash.
The existing settings renderer now uses the lower shared Back/title placement
and Messaging guidance band. Messaging's normal notices replace the guidance in
that same band; typing retains its existing temporary layout. Device Setup's
Weather entry opens the unchanged Weather Location editor. The official mockup,
architecture, user routes and test guidance reflect the entry name. No source
path, helper, state, storage mechanism or save policy was added or changed.

ESP-IDF 5.5.5 firmware build passes (image `0xf61e60`, 23% application partition
free). Fresh native configure/build and all 33 checks pass in 14.44 seconds,
including Day/Night header geometry, actual Weather/Back pointer navigation,
Messaging guidance clearance, typing restoration and preserved Weather Save
scope. All 11 mockup scripts parse; execution checks of the actual shared header,
Device Setup and Messaging functions pass. Independent scope and necessity
review found no remaining issue. The application edit is five line replacements
in one existing renderer; seven existing source/document paths changed, with
the other 213 starting files unchanged.

The authorized flash completed and verified all three written images without
writing the NVS settings partition. A 45-second capture, using one normal
restart, matches the tested ELF prefix `93a0ebba3`, reaches Display ready/touch
detected and receives live weather. It contains one boot and no panic,
assertion or watchdog error. The user subsequently confirms lower Back/title
placement, nearby text clearance, Back navigation, the Weather entry and Messaging
guidance/typing restoration. Their header naming and text-size feedback is
addressed in the follow-up above. Both appearances pass native checks; the user
did not separately state the appearance tested, and the short startup capture
does not establish prolonged stability. No commit occurred. See
[local closure evidence](../.reviews/main/settings-header-firmware-2026-10-12/closure.md).

Accepted settings-header mockup adjustment (October 12, 2026):
the official device preview moves the shared Back button down 16px and adds the
same 16px to every page title, including Weather Location's existing special
position. Messaging retains its three controls and shows the existing Device ID
guidance as plain muted text centered in the unused fourth-row space, hidden
during typing. No footer, row geometry, palette, font or interaction changed.
The user subsequently approved the appearance and authorized the firmware
implementation recorded above.

All 11 embedded scripts parse, and parent execution checks confirm shared Back
at y=104, normal titles at y=128, Weather Location at y=110, Messaging controls
at 212/320/428, guidance at 574 and Done at 664. Independent read-only review
confirms coverage; the lower titles reduce spacing above Alarm's next-time
caption and several other short subtitles. This is a source spacing observation,
not a confirmed glyph collision. Physical review of the implementation should
include Messaging, Alarm, Wi-Fi, Alarm Sound, Fixed Schedule and Sunrise/Sunset.
The file was queued to open in Codex; rendered appearance was not inspected.
No browser-file access workaround, firmware build, flash or commit occurred.
See [local evidence](../.reviews/main/settings-header-mockup-2026-10-12/review.md).

Leading Zero control correction (authorized October 12, 2026): Display Settings
now shows On and disables the existing switch in 24-hour mode. Returning to
12-hour mode restores its prior draft preference; saving 24-hour mode preserves
the underlying 12-hour choice. The official mockup matches using its existing
disabled appearance. No extra state, setting, source path, storage mechanism or
clock-formatting change was introduced. This changes the existing control's
presentation and permitted interaction, as approved; alarm/weather behavior and
restart recovery remain.

Fresh native configure/build and all 33 checks pass in 14.89 seconds. Both saved
preferences and both appearances cover disabled pointer/direct-event handling,
draft restoration, Cancel/Back, Save in both formats and ordinary configuration
reload. Existing save-scope/retry and alarm recovery checks remain passing. All
11 mockup inline scripts parse; parent checks of the actual renderer/handler pass
both preference directions. Independent source and necessity review found no
remaining issue. ESP-IDF 5.5.5 build passes (image `0xf61e60`, 23% application
partition free). The authorized flash completed and verified all three written
images without writing/erasing the NVS settings partition. A 45-second startup
capture matches the tested ELF prefix `4c6bbbf98`, reaches Display ready/touch
detected and downloads live weather, with no panic/assertion/watchdog error in
that short window. The real Conditions headline `Rain likely by 3 PM` allowed
the previously pending format check. The user confirms Leading Zero On/disabled
in 24-hour mode, restored Off and On choices in 12-hour drafts, and Conditions
changing promptly between `3 PM` and `15:00` after Save in both directions.
Leading Zero Off and the original Analog/12-Hour/Day 100%/Night 25% baseline are
restored. These close the prompted basic control and Conditions-caption checks;
both appearances, offline transport, precise rendering and prolonged stability
were not separately established. Architecture, user guide and test guidance are
updated. No
commit occurred. See [local evidence](../.reviews/main/leading-zero-24h-2026-10-12/closure.md).

Authorized firmware load and combined board checks (October 12, 2026): loaded
the current tested image `0xf61e60`, including the analog-caption correction,
Conditions format selection, independent Display Save and menu changes. All
three written images verified; the ordinary firmware writes did not erase or
write the NVS settings partition. The sandbox initially denied USB access;
the same authorized flash completed with device access enabled.

A 45-second startup capture, using one normal restart to collect the complete
boot, matches the tested ELF hash prefix `f29ebb376`. Startup reports Display
ready with touch detected and a successful live-weather download. No panic,
assertion or watchdog error appears in that capture; this short observation
does not establish prolonged stability or physical touch. That first capture
reported `Calm and dry`, so the time-bearing caption check remained pending then.
The subsequent Leading Zero session above supplies and confirms the timed check.
The user confirms `Tomorrow · 7:00 AM` is one complete readable line, its tap
opens Alarm Settings, and clock/swipes respond after Cancel. This closes the
reported analog wrapping case; the appearance was not separately stated.
The recorded Display baseline is Analog, 12-Hour, Leading Zero Off, Day 100%,
Night 25%. The user confirms Brightness Back retains the draft, Display Cancel
discards all temporary choices, and Display Save applies Digital/24-hour/Day 75%
live without restarting. Original values were restored with Save, also without
restart. Night brightness stayed at 25%; changing its applied level and behavior
in the other appearance were not separately checked. The temporary Leading Zero
On selection used 24-hour mode, so it does not verify the switch's visible effect
in 12-hour mode. The user observed 24-hour leading zeros with the setting Off;
this agrees with the existing 12-hour-only preference and formatting code. The
subsequent approved control correction and confirmed checks are recorded above.
No saved
choices were changed by the diagnostics and no
commit occurred. See [board evidence and current checks](../.reviews/main/analog-alarm-caption-2026-10-12/hardware-test.md).

Completed analog next-alarm caption correction (reported and authorized October
12, 2026): removed the normal analog row's unused horizontal padding so full
Today/Tomorrow captions fit on one line at the existing font size. The centered
360×72 touch area, icon, gap, colors, wording and Alarm Settings action remain.
Digital and missed rows retain their padding; longer error text can still wrap.
This is a presentation correction to existing behavior. Alarm scheduling,
operating records and restart recovery are unchanged; no source path or mechanism
was added.

Before the fix, the actual LVGL check reproduced the user's `Tomorrow · 7:00 AM`
notice at two lines (60px high rather than 30px). After the fix, all 33 native
checks pass in 14.51 seconds, including longer 12-hour, leading-zero and 24-hour
captions in both appearances, complete text width, containment/centering,
unchanged font/touch geometry and pointer access. The ESP-IDF 5.5.5 firmware build
passes (image `0xf61e60`, 23% application partition free). Independent source and
necessity review found no remaining issue in this bounded correction. Relevant
architecture and test guidance are updated. Physical fit was pending at the
implementation checkpoint; the subsequent board confirmation is recorded above.
No flash or commit occurred during implementation. See [local evidence](../.reviews/main/analog-alarm-caption-2026-10-12/closure.md).

Completed Conditions time-format correction (authorized October 12, 2026):
the existing weather decoder prepares both formats of the visible headline and
detail from the same downloaded data. The view selects the saved Display format
on ordinary updates and successful Save completion, including stale/offline
retained weather and a download begun with the earlier format. Unsaved changes
and Cancel do not alter the captions. The original complete Conditions output,
wording, forecasts/provider dates, measurements, warnings, requests/retries and
alarm recovery remain. The approved timing is in the
[Conditions contract](architecture.md#conditions-contract).

The application change is 42 added/13 removed lines (net +29) in five existing
files. It adds only a format flag and two alternate captions to the existing
weather snapshot, reusing the same calculation and output storage. No source
path, task, queue, owner, request, stored record or provider-response cache was
added. Target ELF measurements show weather snapshots grow from 1,104 to 1,272
bytes (+168 including padding); the 784-byte Conditions result is unchanged.
There is no second full Conditions result on the weather task's stack. This
does not establish actual runtime stack headroom or calculation latency.

Parent native configure/build and all 33 checks pass in 14.03 seconds. The
retained forecast/location/30 Conditions fixtures pass. The ESP-IDF 5.5.5 build
passes (image `0xf61de0`, 23% application partition free; 256 image bytes larger
than the preceding Display-save build). Actual decoder checks use matching
current-hour data and independently expected captions, both formats from one
request, unchanged normal output and failed-refresh retention. LVGL checks cover
Save/Cancel in both directions, Day/Night, stale and late earlier-format payloads,
missing/detail states, preserved data/warnings and alarm interruption/Snooze/newer
destinations. Existing alarm recovery checks remain passing. Independent source
and necessity review found no remaining defect. The test fixture wording was
clarified to identify controlled HTTP responses; no live-provider or board result
is claimed. Architecture, user instructions and test guide are updated. No flash
or commit occurred. See [local closure evidence](../.reviews/main/conditions-time-format-2026-10-12/closure.md).

Completed Settings navigation and independent Display Save (authorized October
12, 2026): firmware now opens Display Settings from Settings, and Weather
Location from Device Setup. The latter orders Wi-Fi, Weather Location, Night
Shift and Messaging, matching the [approved visual reference](../mockups/alarm-design.html).
Display Cancel/Back discards its separate draft; Save applies its five existing
choices without restarting and returns to Settings. Brightness Back/Done retains
the Display draft. Device Save writes its remaining ten entries. Weather retains
its independent lookup/save/restart; Back/Cancel restores pending Device edits,
and Weather Save cannot apply those edits. This is the approved save/navigation
change under the [editing and saving rules](architecture.md#editing-and-saving-user-settings),
not an expansion to other services or settings.

The existing settings owner, short-lived worker, typed keys and save/copy locks
are reused. One held Device draft preserves the parent during independent
Weather editing; one private UI callback applies the published home/format while
preserving newer menus, weather/message destinations and alarm/Snooze flags.
Review caught and closed a delayed-save case that retained the old home after an
alarm replaced Display Settings. Later manual navigation remains intact. No new
maintained source path, owner, task, queue, stored record or persistence framework
was added.

Parent verification passes all 33 native checks in 13.53 seconds and the
ESP-IDF 5.5.5 firmware build (image `0xf61ce0`, 23% application partition free).
The first native configure could not link against the selected Mac SDK; explicitly
selecting the installed Command Line Tools SDK resolved that environment issue
without a source change. Checks cover both appearances and menu/footer geometry,
separate five/ten-key save scopes, latest unrelated settings, ordinary partial-save
failures/retries, Weather parent-draft retention, first-use guards, live home/format
updates, manual navigation and alarm interruption/replacement. Independent source
and necessity review found no remaining required finding within the bounded slice.
Earlier browser checks of the approved reference passed; this firmware step did
not edit it. No flash or commit occurred. Physical rendering, touch, live backlight
application and restart behavior remain pending. See [local closure evidence](../.reviews/main/display-save-2026-10-12/closure.md).

Conditions timing decision resolved October 12, 2026: the user approved applying
the saved 12/24-hour format to retained Conditions descriptions by the next
ordinary display update, without another download or restart. The bounded
caption-variant correction is completed and verified above. The [earlier independent review](../.reviews/main/display-save-2026-10-12/independent-review.md)
records the original inconsistency; it is evidence of that earlier checkpoint.

Completed Brightness menu move (authorized October 12, 2026): the existing Brightness entry and draft summary now open from Display Settings in the [approved visual reference](../mockups/alarm-design.html), and the single retained editor's Back/Done return to Display. Time Format and Leading Zero follow Brightness; Night Shift retains its three rows and blank fourth slot. Drafts, parent Cancel, Save & Restart, alarm interruption/resume, slider appearance and brightness defaults/limits/storage/backlight behavior remain. No maintained source path, second editor, return-route flag, module, task, queue or persistence mechanism was added.

Fresh native configure/build and all 33 checks pass in 13.33 seconds. The ESP-IDF 5.5.5 build passes (image `0xf61950`, 23% application partition free). Updated existing checks cover both appearances and equal row geometry, the empty Night Shift slot, actual pointer entry, Back/Done and refreshed edited summaries, Cancel, normal saves/failures/restart, and retained slider objects/draft values through an alarm. An initial test-only compile issue used a helper unavailable in the UI-shell target; the test now inspects the existing LVGL children without a new helper or production change. This firmware slice did not edit the mockup; its earlier browser checks preceded implementation. A concurrent reference update changes other menu/save controls outside this bounded move, while Brightness placement and return behavior remain as approved. No flash or commit occurred. Physical layout and touch remain pending. See [local closure evidence](../.reviews/main/brightness-menu-2026-10-12/closure.md).

Documentation cleanup (October 12, 2026): removed duplicated development and verification history from the root and target READMEs after confirming that the useful records are already retained here. A follow-up removes the root README's remaining Development Notes section and unfinished wiring-guide note; concise documentation links remain. Source ownership and test instructions stay in the architecture and target guides, and the wiring-guide task is retained below. The root README keeps feature descriptions and user instructions; the target README keeps environment/build/flash guidance. The two AGENTS.md passages that requested README implementation status now place that record here. Firmware and product behavior are unchanged.

Initial Messaging presentation (authorized October 12, 2026; guidance placement superseded by the later header/font/centering work above): the existing settings renderer now matches the latest [official visual reference](../mockups/alarm-design.html). The Device Setup entry and one-line heading read Messaging, replacing the earlier Messaging Setup name/two-line treatment. Room Name and Device ID use the first two label-over-value rows; House Messages keeps its Single/Shared control in the third row, with the fourth slot blank. Tapping a whole row opens the retained textarea/keyboard, and closing restores the row with its draft text. Required distinct-ID guidance remains above the rows and Shared validation directs the user to Open Messaging. Existing lengths, keyboard pages, Done/Back/Cancel, Save & Restart and alarm interruption/resume remain. No maintained source path, module, task, queue, saved record or navigation framework was added.

Parent verification passes all 33 native checks in 13.60 seconds and the ESP-IDF 5.5.5 build (image `0xf61950`, 23% application partition free). Source/LVGL checks cover both appearances, exact bordered-row geometry, heading and guidance fit, blank slot, pointer access through labels/values/chevrons/padding, typing restoration, validation and preserved saves/alarm interruption. Independent comparison caught and closed a one-pixel border-coordinate mismatch before final verification. These checks establish software objects and events; physical rendering/touch remain pending. No flash or commit occurred. See [local closure evidence](../.reviews/main/messaging-2026-10-12/closure.md).
Messaging Setup mockup follow-up (authorized October 12, 2026): updated the
[official visual reference](../mockups/alarm-design.html) for the requested
menu naming and row presentation. Browser checks pass for Day/Night appearance,
standard row geometry, two-line heading alignment matching Weather Location,
Room Name and Device ID keyboard editing, House Messages selection, nested Done
and parent Cancel. The existing temporary typing layout
and House Messages browser preview are preserved. Firmware source is unchanged;
no firmware build, flash or physical-board verification was performed for this
mockup-only step. The later naming correction and completed firmware slice are recorded above.

Completed F1/F2 corrections (authorized October 12, 2026): successful Device/Weather Save now completes its existing 900ms restart even when alarm navigation has replaced the original editor. The old editor's detailed feedback stays separate from the generic restart notice; failed saves and Alarm Save remain unchanged. Enabled hour/minute/repeat-day edits reuse the alarm owner's runtime arming step with three remembered calendar values. Captured ringing/snooze, existing missed notices and normal restart recovery are preserved under the [approved alarm rules](architecture.md#alarm-and-snooze). No new maintained source path, task, queue or stored record was added. All 33 native checks pass in 13.62 seconds, including supported pointer interruption/replacement, hidden ringing, failure and exact restart-delay cases, plus calendar edits and unchanged recovery. The ESP-IDF 5.5.5 build passes (image `0xf61660`, 23% application partition free). Independent source/necessity review found no credible defect. These corrections have not been flashed or physically tested; the earlier audio/recovery observations below remain evidence for the preceding image. See [local closure evidence](../.reviews/main/f1-f2-2026-10-12/closure.md).

Earlier audio checkpoint (October 11, 2026, before the Classic Bell replacement): playback and sound preview are implemented; hardware acceptance remains open. The debugger-confirmed audio stack overflow is corrected with the existing worker’s 6 KB allocation. The measured generation bottleneck is corrected in the existing generator using single-precision arithmetic and integer sample timing, preserving sound patterns, preview duration and ringing ramp. All three measured sounds have no generation/feed chunk over 20 ms; Gentle Chime averages 3.49 ms, down from 86.7 ms, and the user hears no cutouts in the retest. Temporary timing code is removed. All 30 counter-free native checks and the firmware build pass; that image is flashed with all three images verified. Longer ringing under load remains unverified.

The user requests maximum output after codec amplification and accepts a simple perceptual volume approximation. The common PCM gain is now 0.82; independent source/equation checks put the loudest pattern near 66.56% of PCM full scale and approximately 99.59% after the configured codec gain. The slider scales the squared percentage fraction, so 50% gives one-quarter of maximum signal and zero stays silent. Tone patterns, ramp, controls, codec settings and saved preferences remain. Nine audio cases pass at this scale/curve, including the original-waveform comparison and full-volume preview/ring clipping checks. Final full native verification passes 30/30 in 12.83 seconds, and the IDF 5.5.5 build passes (image `0xf61560`, 23% free). The final image is loaded, all three images verified and the running identity matched; Off/07:00 weekdays/Classic Beep/100%/ten-minute settings are reread unchanged. The user confirms all three 100% previews sound good and are definitely louder, with no distortion or cutouts; Classic Beep is noticeably quieter at 50%. These prompted preview checks pass. At that preview checkpoint, the user chose Alarm Off, 07:00 weekdays, Classic Beep, 100% volume and ten-minute snooze; the later restored baseline is recorded below.

Current board pass: short physical power-cut checks pass for ordinary ringing catch-up, preserving a future snooze deadline, resuming after an overdue snooze deadline and remembering Dismiss. The user also confirms Off cancels snooze; the cleared state is independently read. The final cold restart stays silent with Alarm On and shows tomorrow’s 8:40 PM alarm. The user restores Off/07:00 Monday–Friday/Classic Beep/75%/ten-minute snooze and confirms normal Save. The 12-minute sustained-ringing test and the longer-than-30-minute outage/missed-notice test are deferred by the user; their results remain unverified. No source change or reflash was needed.

On the previously loaded image, all three previews stopped after about five seconds. Stop Preview, choosing another sound, and leaving the picker also stopped playback promptly. At the original output level before the generation correction, the user reported clear output without crackling or distortion, disliked the sound designs—especially Gentle Chime—and found the volume low even at 100%. The final corrected/higher-output previews now sound good to the user. These are listening observations, not measured loudness; no replacement tones were approved at that earlier checkpoint. Classic Bell is approved in the current sprint above. The first preview reboot was not captured in the serial log. A later debugger capture did identify the task stack overflow. The earlier temporary scheduled-ring/ramp/Snooze trial ended at a reboot and blue screen. On the corrected image, the user reports gradual gain increase for the 7:05 PM alarm and an ongoing Snooze countdown; its resumed ring and Dismiss now pass the prompted physical check. The user then reported intermittent cutouts in both ringing and previews; the subsequent measured correction closes the tested preview issue.

The original saved alarm settings were On, 07:00, weekdays, Classic Beep, 50% volume and ten-minute snooze. The failed trial’s temporary saved settings were read as On, 18:40, Every Day, Classic Beep, 25% and ten-minute snooze. The subsequent ringing trial used the user-reported 7:05 PM time; 100% volume and five-minute snooze were requested but have not been independently reread. The user subsequently confirms restoration and normal Save/clock/touch. A later readback finds Off/07:00/weekdays/100%/ten-minute snooze; the user explicitly chooses to keep those current values after testing. A separate runtime issue is confirmed: changing an enabled alarm's time or repeat days can make the running alarm owner process a past occurrence, as seen after creating an Every Day 18:40 schedule on Sunday. The approved F2 correction is complete in the October 12 slice above. Normal restart recovery is preserved; its durable state still does not distinguish a newly saved schedule from an older one. See [local closure evidence](../.reviews/main/audio-2026-10-11/closure.md).

Previous development checkpoint: local Sunset-to-Sunrise Night Shift is implemented, verified in development and loaded for board testing. All 18 native checks and the ESP-IDF 5.5.5 firmware build pass, with 24% application partition free. Independent source review found no actionable defect. The authorized flash verified all three images and matched the running identity; a 35-second startup capture shows display/touch initialization, advancing analog-hand updates and live weather without a panic or touch-read failure. The user confirms the populated Solar page is readable and unclipped, the unsaved offset preview/Cancel works, and Save/restart retains Solar mode and both At offsets with the expected afternoon Day appearance. The user subsequently reports Night Shift activation at the appropriate time on October 11; exact transition timing, sunrise, configured fades and offline behavior remain to be checked. This build also includes the preceding verified weather/message cleanup. Saved preferences were preserved during flashing, and neither slice has been committed. The previous menu/screen checkpoint is committed as `0eb8b6f`.

Last reconciled: October 12, 2026, after F1/F2 development verification and reconciliation of the earlier audio/power-cut observations. Corrected-image flash and identity verification are complete; the observed stack-headroom check and three-sound preview retest pass; the 7:05 PM gain-rise/countdown and resumed-ring/Dismiss observations pass, the later restored Off/07:00 weekdays/Classic Beep/75%/ten-minute alarm preferences are user-confirmed, and tested preview cutouts and higher-output/50% control checks pass. Earlier completed-slice entries record their status at their own checkpoints; the current status is above. Solar board observations include populated-menu, offset-preview/Cancel and Save/restart checks; Night Shift activation at the appropriate sunset time is now user-confirmed; sunrise, precise timing and fade checks remain pending.

Deferred by the user on October 11, 2026: the inherited Shared Device ID query issue from the [local adversarial review](../.reviews/main/adversarial-2026-10-11/summary.md). An accepted ID containing `&` can be shortened during message polling; ordinary IDs are unaffected, and no board failure was observed. Revisit this separately after moving forward with the sprint. No fix or restriction on allowed ID characters is approved or implemented.

Completed audio playback and sound preview (authorized October 11, 2026): the
approved `audio.c/.h` now owns one playback worker, the retained public BSP
speaker handle, shared tone generation, gradual ringing gain and preview
lifetime/error publication. The worker reads copied alarm/configuration state
between 20ms PCM chunks, so live Snooze/Dismiss cancels playback independently
of a pending operating-record save. One overwritten copied Preview/Stop entry
keeps the latest request; no ring-command history, extra bus, persistence,
assets, driver fork or repair/retry manager was added. Classic Beep, Gentle Chime
and Rising Tones use the reference's frequencies, intervals and envelopes for
both preview and ringing. Preview uses unsaved sound/volume for approximately
five seconds after setup; new/resumed ringing raises gain from 10% of the saved
level to that level over 30 seconds. Valid stored 0/7/37 volume values remain.

The existing Sound Picker button/caption changes in place between Preview Sound
and Stop Preview; selection, navigation, closing, hiding and ringing cancel.
The outlined Stop mark uses the existing 32px LVGL slot without another asset.
Drafts, layouts, palettes, Save behavior and the previously approved screens
remain. Returned audio errors reuse the caption or ringing footer, after action
and storage error priority. Review caught and closed cancellation during slow
speaker setup, setup consuming preview time, stale audio status after a tap and
ring-masked status skipping the definite Stop on hide. No unsubmitted edit is
saved or made authoritative.

The disclosed existing-failure change disables the BSP's optional abort-on-error
setting. Speaker creation can return failure without that whole-clock abort;
the global setting also changes checked display/touch/bus/backlight failure
paths, whose callers were reviewed. Healthy paths and standard transfer/touch
waits are unchanged. Unconditional board assertions/checks and hidden codec
register failures remain; this is not complete hardware fault recovery. One
failed BSP speaker creation stays unavailable until restart, avoiding repeated
partial allocations. A returned failure on a retained handle ends that playback
attempt, allowing a later explicit preview/new/resumed ring to try that handle.

Parent verification passes all 30 native checks and the final firmware build.
The new approved `test_audio.c` executes the actual private worker/generator with
platform substitutions in nine fresh-process scenarios; pointer UI checks cover
preview/cancellation/error/geometry and draft retention. Independent review
passes 13 focused cases. An initial UI-shell run reached its 15-second native
limit while builds competed for CPU; quiet final runs passed without changing
that limit. The three new maintained sources were already in the approved tree;
all other edits use existing paths. Documentation is reconciled. See the closure
above for logs, scope and hash evidence.

Broader audio board verification remains open. The final generator and selected output/curve are loaded and identity-verified; all three 100% previews pass without distortion or cutouts, and Classic Beep at 50% is quieter. Observed stack reserve is about 2 KB, not a worst-case guarantee. The earlier 7:05 PM gain-rise/countdown and resumed-ring/Dismiss checks pass. Still check scheduled time accuracy, prompt initial Snooze stopping/exact duration, ringing priority over previews/menus/messages and prolonged ringing during weather, touch and RTC activity. The short power-cut/restart checks above pass; the greater-than-30-minute missed-notice case, power loss during a save, attached-clock behavior, sunrise and detailed Solar transition/fade checks remain in the combined board pass; closed basic menu checks need not be repeated.

The BSP write path waits up to 1000 ms, and source estimates about 70 ms of queued sound; neither establishes the physical stopping delay. The stopped-state measurements identify insufficient generation speed, especially for Gentle Chime. The replacement preserves the sound definitions while using the P4’s faster single-precision arithmetic; timing measurements and listening confirm the tested preview correction. The enabled-schedule time/repeat issue above is separate from audio playback and the stack correction.

Completed alarm presentation/actions (authorized October 11, 2026): the existing
clock renderer now draws the approved digital Ringing and Snoozed pages,
captured scheduled date/time, deadline countdown, home next/off/snoozed and
missed rows, and saved-alarm controls on unavailable time in both appearances.
Actions reuse the existing two-entry clock/notice queue with copied occurrence
identity and one pending completion retained until the display is available.
The heartbeat refreshes the owner's current copied state before handling that
completion; it does not retain an older action snapshot. Queue/full/action/save
errors reuse the existing caption/footer, and failed record writes preserve
applied live actions. Ringing hides the single Settings root without saving or
closing its draft/input; later Settings navigation resumes it. Explicit Alarm
navigation closes an unrelated held editor without saving. Message and notice
completions cannot override alarm preemption or a newer user destination.

The Alarm editor now previews draft time/day changes through the existing pure
calendar calculator using usable owner time, without arming, handling or saving
an occurrence. It refreshes the existing caption in place and retains its
formatter, Save/error priority and Cancel behavior. Exact equality moves the
preview to the following occurrence; intended dates/minutes survive daylight-
saving changes. The existing 28px font supplement supplies the middle dot with
the same ordinary Montserrat glyphs and metrics. Five needed Lucide alarm/off/
close icons and colon/hyphen in the existing 96px countdown font use the existing
generators and grouped asset files. Original icon bytes and six other font
fragments are unchanged. No new source path, task, queue, store, migration,
manager or asset pipeline was added by this presentation slice.

Native checks caught and closed an unreachable unavailable-time Alarm Settings
button beneath the swipe layer. Source review simplified completion delivery
and corrected its runtime-before-navigation order. Parent verification passes
all 21 native targets, including real LVGL pointer/draft/preemption checks,
fifteen explicit editor calendar/format cases and actual command helpers;
the final firmware build passes. Independent review also passes three focused
targets. Hardware pixels/touch, actual alarm timing, stack/latency, real NVS/power
cuts and audio remain unverified. Speaker playback and sound preview are integrated in the subsequent audio slice above. Combined board checks should cover Ringing/Snoozed controls,
menu/input interruption and resume, missed/unavailable rows, Alarm preview and
explicit saves with the scheduling/recovery and audio checks once ready; do not
repeat the already successful basic menu checks. Evidence is linked above.

Completed alarm scheduling/recovery backend (authorized October 11, 2026): the
pure calendar calculator keeps recurring dates and the approved skipped/repeated
time behavior; the live alarm owner handles due/snoozed state, dismissal and one
missed notice with a separate ordinary 56-byte NVS record. The existing clock task
evaluates alarms before optional RTC/backlight work and consumes copied state for
change logs. Off cancels current ringing/snooze through the existing Dismiss path,
even without usable time. Runtime Off-to-On skips a strictly past occurrence in
memory, preserves an existing missed notice and arms the next occurrence;
already-enabled startup retains normal recovery. No arming history, settings
rewrite, task, queue, migration, journal or generic framework was added. Review
caught and closed lost snooze recovery during resumed ringing and misleading next
alarm reporting after a backward correction. Parent verification passed all 20
native targets, including 36 independently specified calendar cases and operating
record failure/concurrency checks, and the firmware build. The only new maintained
sources are the already-approved alarm/schedule pairs and their two tests; no UI,
asset or mockup changed in this slice. Ringing presentation/actions are integrated in the subsequent presentation slice above; audio,
physical timing, real NVS/power cuts and attached-RTC recovery remain unverified. See the closure
evidence above; this does not establish a complete audible alarm.

Brightness follow-up: the user could not see the screen or recover through Settings during the 20% brightness test. The debugger confirmed Day mode at 20%; temporary RAM-only Day/Night targets of 50% brought the application and BSP backlight values to 50%. No stored preference or firmware changed during recovery. The user confirms the screen is visible and Settings respond at the recovered brightness. A read-only storage check before the subsequent authorized firmware load found Day 30%, Night 26% and Always On Night; these stored choices were preserved. This confirms that new choices reached storage, without establishing who saved them or physical usability at the current 26% Night brightness. The previous “barely visible at 20%” observation no longer establishes a usable minimum. Pause the 20% checks; a 30% minimum is proposed, with its selection and implementation still pending. The temporary 50% recovery is superseded by the saved choices; the permitted minimum remains unchanged. See [recovery evidence](../.reviews/main/brightness-recovery-2026-10-11/summary.md).

Approved Digital Night size follow-up: implemented the user's 250px nominal target for 24-hour and 12-hour Leading Zero Off; 12-hour Leading Zero On retains its previous fitted size. The official mockup uses the same selected sizes. Existing proportional font, spacing, AM/PM treatment and Day layout are preserved. Actual LVGL checks cover all 4,320 hour/minute/format combinations and verify steady size, transformed round-screen fit, baseline/gap and restored Day positions. All 17 native checks passed, the ESP-IDF 5.5.5 build passed with 24% application partition free, and independent review found no actionable defect. The authorized load verified all three written images and the running ELF identity. A 35-second restarted capture shows display/touch initialization and live-weather retrieval without a panic or touch-read failure. The absent-backup-clock, gesture-configuration and Wi-Fi coprocessor-version warnings remain. The user subsequently confirms that both 12-hour Leading Zero Off and 24-hour fit fully and are readable in the prompted Night check. This closes that physical fit/readability check, without establishing every displayed time, exact mockup rendering, the retained padded format or long-term stability. No new source path or asset was added. See [follow-up evidence](../.reviews/main/night-digital-250-2026-10-11/closure.md).

Forecast centering (approved and implemented October 11, 2026): after the user's left-offset observation, the read-only check found both firmware and reference centered the four small daily forecasts at x385. The user approved moving all four 15px right. Existing firmware constants and the single mockup coordinate list now give screen centers x230, 340, 460 and 570, centered at x400. Spacing, vertical positions, sizes, fonts, artwork and weather behavior are preserved. Parent source-coordinate and edited-function syntax checks pass; all 17 fresh native checks passed and the ESP-IDF 5.5.5 build passed with 24% application partition free. No source path, asset, layout mechanism or state was added. The authorized load verified all three written images and matched the running ELF identity. A 35-second restarted capture shows display/touch initialization and live-weather retrieval without a panic or touch-read failure; the existing absent-backup-clock, gesture and Wi-Fi coprocessor warnings remain. The user confirms the four small daily forecast columns look centered, with all labels and icons visible. This closes the prompted centering/clipping check; appearance was not specified, so exact pixels, both appearances and longer operation remain unverified. See the [initial finding](../.reviews/main/forecast-alignment-2026-10-11.md) and [implementation evidence](../.reviews/main/forecast-centering-2026-10-11/closure.md).

Completed bounded weather/message ownership cleanup (approved October 11, 2026): moved existing Forecast, Conditions, message and weather-status drawing into the already-approved `main/ui/weather_views.c`. Its object handles and curved-text drawing buffers are private; drawing calls consume current inputs without retaining another weather/message snapshot or active-view state. `ui.c` retains the weather/config/status copies used during navigation, view and return destinations, swipe ordering, displayed-message ID, held-message and acknowledgment state, and completion decisions. Clock drawing remains with `clock_views.c`; both maintained build lists include the renderer. Source comparison preserves the layout constants, fonts, artwork, text, styling, provider dates, warning placement, layer order and startup yields. All 17 native checks pass, including real LVGL pointer/message/warning/appearance transitions; independent review found no actionable defect. The ESP-IDF 5.5.5 firmware build passes with 24% application partition free. This establishes private rendering ownership, with a small private interface as its integration cost; no task, queue, service, persistence mechanism, asset or mockup change was added. Physical rendering/touch remains unverified for this extraction. See [local closure evidence](../.reviews/main/weather-message-cleanup-2026-10-11/closure.md).

Completed local Solar Night Shift (approved October 11, 2026): the user chose local calculation after considering provider times. The pure calculator uses the NOAA/Meeus equations; `night` keeps the three adjacent local days, decides appearance using saved offsets and publishes raw tonight/tomorrow events through the existing display snapshot. Settings formats them and previews draft offsets in the approved layout, refreshing labels without rebuilding an open editor. Existing modes, brightness, fade and appearance-record behavior are preserved. No new task, queue, persistence mechanism, provider request, asset or mockup change was added; the three new source paths were already approved in the architecture. All 18 native checks pass, including seven independent dated reference cases, boundary/calendar/fallback/storage checks and actual LVGL menu/runtime checks. The ESP-IDF 5.5.5 image is `0xf4ae60`, with 24% application partition free. Source/necessity review found no actionable defect. The seven reference comparisons differ by at most 28 seconds; this does not establish observed horizon times or high-latitude season-transition accuracy. The subsequent authorized load verified the images and running identity, with the startup observation recorded above. Physical solar/menu checks remain pending; no commit was performed. See [local closure evidence](../.reviews/main/solar-night-shift-2026-10-11/closure.md).

Solar board checks, October 11, 2026: the user reports Leander, TX, Night Starts 7:04 PM and Night Ends 7:31 AM. Both rows say At sunset/At sunrise, and all labels fit and remain readable. The user confirms that selecting Before previews a 15-minute-earlier start (the prompt specified 6:49 PM), then Cancel restores the original At setting and time. This closes the prompted populated-page and draft-preview/Cancel checks in the observed appearance; it does not establish the other palette, saved offsets or actual transitions. A separate city-center reference check gives today's sunset 7:04 PM and tomorrow's sunrise 7:32 AM, a one-minute sunrise difference. No defect is established; exact saved coordinates were not read. See [board and reference evidence](../.reviews/main/solar-night-shift-2026-10-11/closure.md#board-menu-observations).

The subsequent Save/restart check passed: after temporarily setting both brightness levels to 50%, saving Sunset to Sunrise mode and restarting, the user confirms afternoon Day appearance, retained Solar mode and both At offsets. Before the test, the user recorded Day brightness 100% and Night brightness 25%; restore those recorded choices after the temporary test, subject to their physical visibility. This verifies the prompted zero-offset save, not persistence of changed nonzero offsets. With both test levels at 50%, an appearance change alone will not verify a brightness fade. Automatic sunset/sunrise changes and offline operation remain pending.

Implemented firmware changes from the screen walkthrough (approved October 10–11, 2026):

- Analog weather waiting: the single reference message replaces placeholder weather artwork and temperature/High-Low text until a payload exists. Hands/date remain; Night continues hiding weather.
- Digital Day AM/PM: raised 11 pixels, preserving fonts, horizontal placement and the existing Night fitting.
- Shared Digital/Conditions horizontal dividers: the existing 1px lines use reference Day blue `#93d1ff`/Night rose `#e1b6b6` at 35% opacity. Geometry and Conditions column borders remain; Digital Night still hides its lines. Low-brightness visibility needs a board check.
- Digital Day temperature: moved from x125 to x135. The description/High-Low group stays at x500, as selected.
- High/Low labels: the existing shared formatter adds a space after `H:` and `L:` on both clock faces; forecast slash pairs remain unchanged.
- Digital weather waiting: hidden weather rows/pictures are replaced by the reference message until a payload exists. Time/date remain, and Night continues hiding weather.
- Forecast weather waiting: all five fallback pictures are invisible; the reference “Updating…” appears in both appearances. Their layout space is retained so unavailable `--` dates/temperatures do not move. Normal artwork returns with the payload; provider-date/Tomorrow rules remain.
- Forecast data warnings: the existing status label uses the approved position and 22px muted styling, “Weather data stale” after a failed refresh with retained weather and “Weather update failed.” before the first payload. Success clears it; Night hiding, retries and separate progress captions remain.
- Conditions data warnings: the same status label uses this screen's approved position and styling with the same two messages. Navigation immediately moves it to the destination's position. Compass, metric icons, unavailable readings, current palette/geometry and storm behavior remain as selected.
- Backup-clock Dismiss corners: changed only the existing button radius from 24px to 32px; size, position, colors, label, tap area and dismissal ownership remain.
- Clock Waiting for sync: the approved muted clock icon/notice uses the existing [time-source rules](architecture.md#time-and-rtc) on both faces/appearances. “Clock paused” retains priority and its threshold. Accepted network time clears the notice independently of backup correction; unusable time keeps its separate screen. No alarm, weather or Night Shift gate was added.

All eleven changes passed source/object/transition checks and the firmware build. The slice reuses the existing UI, status, font and icon files, adding only the required clock graphic and one 32px ellipsis glyph. Existing font/icon content is preserved byte-for-byte. No new maintained source path, task, queue, store or service owner was added. The board currently has no attached backup-clock module, so actual backup-time startup remains unverified. Firmware load/startup and physical retest outcomes are recorded in the [local slice evidence](../.reviews/main/approved-screen-changes-2026-10-11/closure.md).

Physical check: the user confirms that Digital Day AM/PM, temperature and spaced High/Low labels fit and remain readable, and that the thin dividers on Digital and Conditions are visible and readable after swiping through Forecast. Brightness was not specified. This closes the prompted fit/readability check; exact mockup comparison, low-brightness visibility, Analog/Night presentation and exceptional states remain separate checks. No firmware change or reload was needed to record this result.

Resolved without a firmware change:

- Analog quarter-hour tick color — proposed pure-white Day marks disapproved October 10, 2026. Keep the firmware's bluish-white `#f4f8ff` marks at 12, 3, 6 and 9. The official mockup retains that color; other marks and Night colors remain unchanged. All Day samples now use the existing drawn layout, as described in the approved High/Low spacing item. Browser display was not rechecked.
- Digital Day description/High-Low position — proposed move 10 pixels right disapproved October 11, 2026. Keep the firmware's x500 start, 270px text width, font sizes and wrapping. The official mockup now uses x500 for both labels. This decision is separate from the approved temperature move and label spacing.
- Digital Night equal-width digits — disapproved October 11, 2026. Keep the firmware's proportional Montserrat Medium digits. The official mockup now uses the font's normal digits instead of its equal-width alternatives; the existing measured Night fitting adapts to their widths. Day typography, centering, AM/PM styling and fitting rules remain unchanged. The separate Night digit-spacing decision is recorded below. Source/script checks pass; browser display was not rechecked. No firmware change is queued for this item.
- Digital Night digit spacing — disapproved October 11, 2026. Keep the firmware's spacing of -2px at its 144px base font, scaled with the fitted Night text. The official mockup now uses the same ratio, `calc(-2em / 144)`, instead of `-.012em`. Preserve proportional Montserrat Medium digits, Day spacing, AM/PM styling, centering and measured fitting. Source/script checks pass; browser display was not rechecked. No firmware change is queued for this item.
- Conditions weather waiting — disapproved October 11, 2026. Keep the current curved “Checking the weather...” heading, compass, direction letters, metric icons and unavailable `--` readings. The official mockup now uses that composition in Day and Night, retaining the current speed/unit row and column geometry while hiding the wind arrow, direction, gust and condition-change text until data arrives. Reuse the existing Conditions drawing and assets; no separate waiting screen or firmware change is queued. The already-approved horizontal-divider style and other Conditions decisions remain separate. All ten executable mockup scripts parse, eight waiting-state checks pass, and 24 normal Conditions render comparisons remain unchanged. Browser display was not rechecked.
- Conditions wind readout placement — disapproved October 11, 2026. Keep the firmware's centered speed/unit row and the grouped direction/gust lines beneath it. The official mockup now follows that layout, using the current “gusts to” wording and retaining its existing sample values. The separate color decision is recorded below. No firmware change is queued for this item. All ten executable mockup scripts parse; 24 normal-state checks confirm only the wind readout changed, and eight waiting outputs remain unchanged. Browser display was not rechecked. Horizontal-divider geometry and column separators are resolved below; warning presentation is approved above.
- Conditions horizontal-divider geometry — disapproved October 11, 2026. Keep the existing 440px row with two equally growing lines and 14px gaps beside the status text. The official mockup now fits its line lengths around the wording, with the lines at y500, instead of using two fixed 95px lines and a 240px text area at y499. The previously approved shared colors and 35% opacity are retained; this rejection queues no firmware geometry change. All ten executable mockup scripts parse; 24 normal-state checks confirm only the divider changed, and eight waiting outputs remain unchanged. Browser display was not rechecked.
- Conditions column separators — disapproved October 11, 2026. Keep the existing borders following their column contents, usually 85px tall, with the Pressure and Rain contents inset by their 1px left borders. The official mockup now uses those insets, the current separator opacity and content heights instead of fixed 120px separators. Its longer “Rain by 4 PM” sample uses the firmware's smaller 18px value font to fit, giving that separator a 79px height; other samples retain 24px values and 85px height. Pending weather retains its preceding composition. The separate palette choice is resolved below; this rejection queues no firmware change. All ten executable mockup scripts parse; 24 normal-state checks pass with content outside the columns unchanged, and eight waiting outputs remain unchanged. Browser display was not rechecked.

- Conditions color scheme — disapproved October 11, 2026. Keep the current firmware colors, including warning highlights on the affected measurements. The official mockup now follows the Day/Night headline, detail, compass, wind readout, column and separator colors, including the current unit opacity. Its illustrative storm sample highlights Wind and Pressure; the Rain timing alone does not represent a heavy-rain warning. Existing embedded icon transparency supplies the required runtime colors without additional asset files. Preserve the separately approved horizontal-divider style and data-warning messages, previous geometry and waiting composition. No firmware change is queued for this item. All ten executable mockup scripts parse; 32 Conditions output checks pass, including eight unchanged waiting outputs, with the approved notices and text geometry preserved. Browser display was not rechecked.

- Time unavailable text colors — disapproved October 11, 2026. Keep the firmware's primary color for “Time unavailable” and muted color for “Waiting for time sync,” in both Day and Night. The official mockup now uses those color roles; the large `--:--`, fonts, positions and other content remain unchanged. No firmware change is queued for this item. All ten executable mockup scripts parse and eight unavailable-screen output checks pass across both appearances and alarm samples, with changes confined to the two text-color classes. Browser display was not rechecked.

- Clock paused warning icon — disapproved October 11, 2026. Keep the current font warning triangle beside “Clock paused.” The official mockup now embeds the existing LVGL 28px font glyph and uses its transparency for the current Day/Night notice color, with the native glyph dimensions and spacing; no separate asset file is added. Preserve the wording, notice position, 135-second detection threshold and the separate synchronization and backup-clock notices. No firmware change is queued for this item. All ten executable mockup scripts parse; 12 clock-notice output checks pass across both faces, both appearances and paused/estimated/current samples. The embedded image's alpha coverage matches the source glyph, and non-paused notice outputs remain unchanged. Browser display was not rechecked.

- Backup-clock warning icon — disapproved October 11, 2026. Keep the current filled font warning triangle in the tappable backup-clock indicator. The official mockup now embeds the existing LVGL 32px font glyph without scaling and uses its transparency for the current Day/Night warning color, centered with its native font dimensions. No separate asset file is added. Preserve visibility, button position and size, accessible labels, tap-to-open behavior and the notice/dismissal behavior; the separate 32px Dismiss-button corner change is approved above. No firmware change is queued for this item. All ten executable mockup scripts parse; 32 backup-indicator output checks pass across both faces, appearances, time states, visibility states and warning types. The embedded image's alpha coverage matches the source glyph. Browser display was not rechecked.

The one-at-a-time screen decision walkthrough and its approved firmware implementation are complete. Disapproved visual choices have been reconciled in the official mockup. Physical visual verification remains separate; this does not complete unfinished alarm screens or the broader screen cleanup.

Earlier board investigation: before the settings-menu rework, the centered keyboard fit and typing worked but it covered the input box. That layout is now superseded by the mockup’s temporary typing composition, which passed the user’s visible-input and typing checks. Earlier non-resetting captures established an endless touchscreen I2C read while the display lock was held, both after an analog freeze and on Digital unavailable time despite usable network time. The finite-timeout correction and brightness change passed native/build checks and were loaded with all three written images verified. The user subsequently reported that the corrected clock was still running the next morning. This is a successful overnight smoke observation, not proof of the initiating cause or every recovery path. Occasional plain-blue flashes remain unresolved; source review identifies display-memory underrun as a lead, without a matching event log. The guarded diagnostic baseline subsequently captured two failed touch reads in one episode, shortly after an absent backup-clock request; that proximity is a lead, not causal proof. Wi-Fi connection and live weather retrieval are confirmed; RTC hardware, broader rendering, brightness transitions, network recovery and power-cut behavior remain unverified. See the [earlier board investigation](../.reviews/main/board-crash-2026-10-09/summary.md), [touch correction review](../.reviews/main/touch-freeze-fix-2026-10-09/independent-review.md) and [blue-flash source review](../.reviews/main/blue-flash-2026-10-10/source-review.md).

After the touch correction was loaded, the user initially saw no output. A non-resetting capture showed Always On Night with target and application/BSP cached brightness at the saved 15%, backlight readiness true, the LVGL display worker waiting normally and the clock task in its normal delay. A temporary RAM-only Night target of 80% reached the backlight, and the user confirmed visible output. At the user's request, the temporary target was then set to 50%; the user confirmed visible output, advancing time and responsive touch. The user subsequently confirmed visibility at 40% and 30%, with 20% barely visible. Diagnostics did not write user preferences. After loading the brightness change, a read-only check found a valid saved Night choice of 25% applied; the later full backup captured Day 80%/Night 30%, Night Off and Digital at that later time. These checks do not establish who made each intervening settings change. See the [brightness closure evidence](../.reviews/main/brightness-minimum-2026-10-10/closure.md). Solar calculations remain unimplemented; earlier diagnostics showed Sunset to Sunrise retaining Day despite usable network time.

The authorized comparison reused the clean original checkout at `d9d621a`, built with the same ESP-IDF 5.5.5 and managed dependency versions. A private full-device backup preserves our loaded firmware and settings; all three original images were flashed and verified. The user confirms original setup entry and both clock faces work. A bounded, non-resetting log confirms live weather and a plausible system time before Wi-Fi on captured restarts; this is not proof of fresh network synchronization. Original code accepts that existing time and otherwise displays its build timestamp plus elapsed UI ticks. It has no application DS3231 access. At that stage, our shared-bus RTC requests were one credible comparison variable, not yet a proven trigger; an RTC timeout in an earlier freeze log followed the first display-lock failure. The user also reports that original Wi-Fi setup is more usable. Source comparison confirms omitted signal strength and the explicit Open label despite retained scan data, and missing connection status required by the mockup. Nearby-list code remains, but its physical presentation/results need verification. See the [local comparison evidence](../.reviews/main/original-firmware-comparison-2026-10-10/status.md). That initial original run is complete; the following controlled phase measured failures in our application.

The controlled comparison retained wiring, saved settings, startup RTC access, the 100 ms touch guard and restart/capture method. All three initial enabled runs and all three exact-image reversal runs reached network time and recorded a 16-byte RTC success in 52–55 microseconds, followed by two failed touch reads. All three omitted runs reached network time without touch errors. This establishes post-network RTC access as a trigger on this board, without proving the driver's precise interrupt sequence. The small correction in existing `clock_time.c` stops calendar access when RTC initialization fails, reports that error and retains the same retries; successful RTC operation and lost-time inspection before correction remain. D2/D3/D4 passed with real network time, expected one startup plus three correction control reads and zero touch errors. D1 lacked synchronization during its 90-second window and is not counted as a matched trial. All 15 native checks and firmware compilation pass.

The manual `start_display` helper and finite-touch-timeout setting are removed, returning both startup paths to the BSP. The small callback still cancels interrupted presses after returned errors and is installed under the initial display lock. This returns touch reads to the BSP default unlimited wait; the callback cannot recover a call that never returns. Temporary completed-read and completed-UI-update counters verified actual progress in three restarted instrumented runs; E1 lasted 180 seconds, E2 lasted 90 seconds and E3 lasted 660 seconds, recording 13,000 touch reads and 570 completed UI updates with advancing UTC. Weather refreshed normally after ten minutes and both counters continued afterward. Final source has no temporary diagnostics, correction omission or diagnostic timer substitutes; the RTC register driver is byte-identical to its pre-investigation version. Independent review caught a missing generated forwarding header hidden by a reused native build folder; it was corrected, and fresh configure/build plus the parent's rerun passed all 15 checks. Final source tests and firmware compilation also pass, with 25% application partition free. All three final images were loaded and hash-verified. Its 180-second startup capture shows actual network time/live weather without touch errors; two non-resetting live-state samples show the touch timer advancing 109,154 milliseconds, the displayed minute/analog hand advancing and normal I2C DONE states. Each sample was resumed; the debugger/captures are stopped and disposable native builds/unused omission symbols removed. No user preferences were intentionally changed in these correction phases. The user subsequently reports that everything seems to be working. A detailed per-control retest, attached-RTC operation and longer-term stability remain unverified. See [local evidence](../.reviews/main/freeze-cause-2026-10-10/status.md).

Completed so far:

- Closed the basic settings-menu board-test pass. The user confirms controls and typing, nearby/manual Wi-Fi pages, five-minute time steps and hour wrapping, all four initial keyboard pages, selected marks and all three sliders. Normal Night Alarm, Device Setup/Display/Device/Night Shift, time/offset and Wi-Fi pages are readable and unclipped in the prompted checks; Night Brightness tapping/dragging works. Device Setup Save & Restart returns to the clock with working Settings. Restored the original mode and pre-test appearance using existing controls: Off once establishes remembered Day, then Sunset to Sunrise retains Day while solar events remain unavailable. The user verifies the final Mode and Day menus. No firmware change, rebuild, flash or commit was needed for these feedback records. Current work/status summaries are consolidated; the full test history stays in the [local report](../.reviews/main/settings-menu-2026-10-10/final.md). Exact pixels, every exceptional state and broader persistence/hardware behavior are not established by this basic pass.

- Completed the approved Volume and Day/Night Brightness slider styling in the existing shared helper. The bar is thinner and the handle distinct in size, with the same center, surrounding layout and comfortable touch area; colors, actual values/increments, draft-only edits and Save/Cancel remain. The mockup now specifies rail/handle painting and updates its filled portion with the existing input handler. Parent fresh verification passed all 17 native checks, including top/bottom edge taps, held-pointer dragging and in-place percentage changes across all three sliders and both appearances. Existing arbitrary-volume and settings-preservation checks remain passing. The actual mockup script parses and its fill update passes direct checks. ESP-IDF 5.5.5 build passes; `0xf46cd0`, 24% application partition free. All three flashed images are hash-verified; a startup observation is recorded in the local evidence. The user confirms clear circular handles and normal tapping/dragging with percentage updates on all three controls. The production change uses existing LVGL styling/click-area facilities and adds no source file, object wrapper, handler, state or storage machinery. Architecture, current work, test and status guidance are updated. See [local closure evidence](../.reviews/main/settings-menu-2026-10-10/final.md).

- Completed the requested hour carry/borrow in the existing shared time handler and both mockup handlers. Minute arrows still use five-minute steps, now moving the hour and AM/PM across boundaries, including midnight. Separate hour/AM-PM buttons, arbitrary saved minutes and Save/Cancel ownership remain. Parent verification passed all 17 native checks with actual pointer cases in all three editors and both time formats, including concrete noon/midnight moves, reverse moves, saved59 and Alarm Save preserving unrelated preferences. Both actual mockup-handler checks and the ESP-IDF 5.5.5 build pass; image size is `0xf46cd0`, with 24% application partition free. Loaded all three images with hash verification and recorded one announced 35-second restarted startup with display/touch/live weather and no logged errors. The user subsequently confirms hour wrapping in both Alarm Time directions and now confirms both Fixed Schedule editors fit and respond, with five-minute steps and hour wrapping. Noon/midnight AM/PM crossings were not separately reported. No new source file, production helper, state or storage mechanism was added. Updated the architecture, work/status and test guidance. See the follow-up evidence in the [local closure record](../.reviews/main/settings-menu-2026-10-10/final.md).

- Completed the user-requested five-minute minute-arrow change in the shared Alarm Time/Night start/end control and both mockup handlers. Saved arbitrary minute values remain intact when opened; each actual input adds/subtracts five, with hour and AM/PM behavior unchanged. A fresh parent build passed all 17 native checks, including real pointer wrapping from 00 and saved59, Done/Cancel/no writes and Alarm Save preserving unrelated preferences. Both actual mockup handlers pass direct stepping checks. The ESP-IDF 5.5.5 image remains `0xf46c50`, 24% free. Loaded all three images with hash verification and observed clean display/touch/live-weather startup for 35 seconds. The first passive capture missed startup; the recorded second capture used one announced deliberate restart. The user subsequently confirmed Alarm Time’s five-minute step in both directions. Night endpoints were not separately reported. The later hour carry/borrow request is a separate follow-up. No new source file, helper, state or storage mechanism was added. See the follow-up evidence in the [local closure record](../.reviews/main/settings-menu-2026-10-10/final.md).

- Completed Alarm, Sound/Snooze and Sunrise/Sunset menu presentation, closing the authorized settings rework in source. Alarm uses measured switch/value rows and the reference footer; pending/failed Save retains the editor and actual draft. Sound/Snooze uses the measured choices, descriptions, checks, volume and disabled Preview. User volume changes follow the reference's 10–100% choices in five-point steps; opening the editor preserves already-valid stored values, including 0/7/37, until actual input. Solar shows the applied location and truthful unavailable events rather than sample times. Nested Done/Cancel, existing group saves, immediate alarm toggle and Alarm Save without restart remain. Independent checks caught and closed a missing weekday dash and one-pixel icon gap; closing review removed unused export options/control constants and unreachable layout conditions. Parent full-suite verification passed all 17 native checks; the ESP-IDF 5.5.5 image is `0xf46c50`, with 24% application partition free. Clock/forecast/conditions drawing remains unchanged. All six font and 17 icon declarations have current consumers at the four previously approved asset paths. Audio preview, next occurrence and solar calculations remain unfinished feature work, with truthful disabled/unavailable presentation. The subsequent authorized load verified all three images and a 45-second display/touch/weather startup observation. Precise physical compliance remains unverified. See [local closure evidence](../.reviews/main/settings-menu-2026-10-10/final.md).

- Completed the shared Alarm/Night arrow time picker, full Repeat-day picker/shortcuts, Night main/mode/fixed schedule, direction/magnitude offsets and Brightness. Time fields wrap independently; 12-hour changes preserve AM/PM; Repeat blocks Done with zero days; signed offsets retain ±120/five-minute rules and start at 15 when a direction is chosen from zero. All nested controls remain drafts, with real stored values verified through the existing Save paths. Removed superseded rollers. Independent review caught and closed the Repeat full-caption mismatch and sparse numeric-font baseline mismatch; native object checks also caught and closed slider geometry lost during style removal. Required true-size 104/96 glyphs and exact 32px marks use the existing approved grouped assets/generators. Parent verification passed all 17 native checks and ESP-IDF 5.5.5 compilation, with 24% application partition free. No firmware loaded; physical rendering and touch remain unverified. The browser-native range track is platform-dependent; LVGL matches its explicit 48px box and accent colors without a claim of identical native-browser raster appearance. See [local controls evidence](../.reviews/main/settings-menu-2026-10-10/controls.md).

- Completed nearby networks and actual scan/loading/empty/error layouts, scrolling all ten retained results inside the three-row region, real signal strength and Open/Secured captions, Manual Network and Refresh. Device Setup uses the connectivity owner’s actual Connecting/Retrying/Connected status for the applied network, without changing connection or retry policy. Weather lookup/error/saved and credentials save/error/restart compositions now use the reference’s measured layout and truthful completed values. Storage-only Weather saves do not falsely announce a lookup; failed storage retains resolved values for retry. The saved timezone identifier uses the reference’s fallback presentation; friendly generic timezone names remain a visual gap. Parent verification passed all 17 native checks and the ESP-IDF 5.5.5 build, with 24% application partition free. No firmware loaded; real scan timing, touch, and raster compliance remain unverified. See [local network evidence](../.reviews/main/settings-menu-2026-10-10/network.md).

- Completed Device, Weather Location and Network Details normal layouts and whole temporary typing composition, matching the chosen four-page keyboard. The active field stays above the keyboard; Close/Done restores controls and unsaved text. Kept password masking, configured input lengths, first-use guards, draft/save ownership and resolved-location retry. Missing required values open their field; Network Details requires a nonblank network name while Device Setup still permits no profile. Independent review caught and closed long-error overlap using concise mockup typing messages and full-message restoration. Exact curved Shift/Delete paths use two 24×24 alpha masks in the already-approved icon assets through existing artwork tools; the subtitle font gained the one required curly apostrophe. Parent verification passed all 15 native checks and the ESP-IDF 5.5.5 build, with 24% application partition free. No firmware loaded and no physical compliance claim. Clock/forecast/conditions are unchanged. See [local typing evidence](../.reviews/main/settings-menu-2026-10-10/typing.md).

- Completed the settings-menu foundation: Settings Entry, Device Setup and Display now use the mockup’s measured heading, row order, title/value and subtitle hierarchy, switches, footer controls and Day/Night palette. Live appearance changes recolor existing objects, preserving editor drafts, focus, cursor and keyboard state. Added only the two missing subtitle glyphs at the already-approved `assets/wac_fonts.c/.h` paths, using the existing generator and bundled font fallback. Actual saved alarm/repeat captions and truthful applied-network status replace generic subtitles. Independent review and parent verification passed all 15 native checks and the ESP-IDF 5.5.5 build, with 25% application partition free. Source review corrected switch-knob border normalization. Save/Cancel, nested Done, first-use exits and existing worker/retry behavior remain. `ui.c` changes only settings appearance handoff; clock, forecast and conditions rendering are unchanged. This was the initial slice; later entries above close the remaining menu presentation. Hardware appearance and interaction are unverified. See [local slice evidence](../.reviews/main/settings-menu-2026-10-10/foundation.md).

- Consolidated the application into the functional source layout, keeping existing services and one public device UI interface.
- Corrected competing Wi-Fi startup/scan operations and repeated indicator pulse restarts; removed the extra message-completion copy and obsolete serial-capture benchmark requirement; changed the backup helper's archive destination. Unused keyboards and repeated presentation updates were also removed.
- Moved the existing Off/Always On/Fixed Schedule decision into `main/night.c`. The screen consumes its supplied appearance instead of calculating another result. Native checks cover schedule boundaries, unavailable time, local timezone rules and the supplied screen colors.
- Removed unused screen handles and update paths, ignored screen-update arguments and their caller work, the unread message-service settings copy and the unreachable message-worker synchronization branch. Active screen updates, the paused-clock warning and real hub synchronization remain intact. Native and message checks and the ESP32-P4 firmware build passed.
- Completed F8: verified that the House Messages browser page's appearance and controls already match the approved reference source. Removed the unnecessary adaptation task; the architecture now requires preserving the approved browser appearance and controls. Live browser/network operation remains unverified.
- Fixed F1: the decorative full-screen Night tint ignores touches, preserving its appearance while allowing message dismissal and swipes home. The existing message checks now run in Day and Night appearance; all 11 native tests and the firmware build passed. Physical touch remains unverified.
- Fixed F2: pending local message changes survive Wi-Fi reconnection and failed transfer, with 15-second retries through the existing worker. A successful transfer stops retries without replay on an unchanged reconnect; changes arriving during transfer stay pending. Retained RAM records, merge behavior and the existing store limits remain. Native message checks, all 11 native tests and the firmware build passed. Live behavior between clocks remains unverified.
- Fixed F3: forecast provider dates stay with their temperatures and icons through midnight, unavailable/corrected time and failed refreshes. The `Tomorrow` caption is used only when confirmed by local calendar time. Weather downloads proceed independently of network-time startup/completion; the existing worker preserves ten-minute successful refreshes and separate twenty-second weather/startup retries. The approved weather-service test executes actual decoding and worker/publication code; independent review caught and closed an overlapping retry deadline. All 14 native tests, retained weather/location/conditions checks and the ESP-IDF 5.5.5 firmware build passed. The F3 scope retained existing service owners and added no production files or tasks. Weather age/stale styling remains unfinished below; live network and board behavior remain unverified.
- Fixed F4: the existing discovery sender uses the existing JSON formatter so accepted quotation marks and backslashes retain their meaning. The same five packet fields, size limit and discovery behavior remain. Actual sender checks cover round-trip values, 319-byte acceptance/320-byte rejection, allocation/encoding failure and send-error cleanup; existing acknowledgement and transfer checks and the ESP-IDF 5.5.5 firmware build passed. Independent review verified the bounded change and improved the size-boundary test. No source paths, services or name restrictions were added. Live discovery remains unverified.
- Completed F5: removed the unsupported Raspberry Pi application, root setup/package commands, duplicate JavaScript calculations/checks, coupled version checker and unused Pi configuration/sample background. Retained ESP32-P4 tools, browser-installation support, canonical weather icons and all three native JSON fixture sets in their existing paths. Updated current maintenance, setup and release instructions; edited C calculations, tests and asset tools retain the same behavior. No replacement wrapper, version framework or source paths were added. All 14 native tests, retained weather/location/conditions and message checks, and the ESP-IDF 5.5.5 firmware build passed. This retires unsupported development behavior and preserves the clock features; hardware remains unverified.
- Completed F6: removed three unused runtime connection/fetch flags, their update arguments and the settings copy done only to fill one of them. Forecast entries retain provider date, temperatures and selected icon; unused stored code/day/thundersnow/summary fields and forecast-summary formatting are removed. Icon-selection calculations, current-condition fallback and displayed summary, status text, weather timestamps/stale handling and required startup/worker/heartbeat settings copies remain. Existing tests retain icon/pairing, refresh/retry and retained-payload coverage; the separate calculation test still checks representative codes directly. All 14 native tests, weather/location/conditions and message checks, and the ESP-IDF 5.5.5 firmware build passed. Independent review verified the scope and preserved behavior. Architecture and test instructions are updated; no new source files or mechanisms were added. Hardware remains unverified.
- Completed F7: removed the unused custom storm-warning limits, their absent-value initializer and the test adapter's override parsing. The conditions calculator retains exact imperial/metric default limits, warning types, outlook, units and time formatting. Replaced only the two legacy gust-override fixtures with default-boundary cases; the other 28 cases remain unchanged. All 30 conditions cases, retained forecast/location checks, all 14 native tests and the ESP-IDF 5.5.5 firmware build passed. Independent review verified the preserved defaults and unchanged cases. Updated the architecture, fixture guide and test instructions; no source files or mechanisms were added. This does not implement or change the planned audible wake-up alarm. Hardware remains unverified.
- Corrected the House Messages browser list after checking the original Raspberry Pi implementation: it now shows all active, unexpired messages, including individual targets, using the existing page and Deactivate controls. Shared clients forward the complete request without repeating its query. The original review's classification of targeted browser management as an optional new feature was incorrect. Normal clock filtering, dismissal, ordering and RAM retention remain; checks also cover a clock with ID `all`. Expanded existing message checks, all 14 native tests and the ESP-IDF 5.5.5 firmware build passed. No source paths, service, task or storage mechanism were added. See the [local closure evidence](../.reviews/main/message-browser-parity-2026-10-09/closure.md); live browser/network and board behavior remain unverified.
- Completed the requested optional cleanup. The same 37 artwork sources are listed directly in `main/CMakeLists.txt`; removed the generated manifest, updater and five updater calls. Trimmed unread message snapshot fields, kept synchronous acknowledgment private to the worker, and removed redundant/unreachable record-copy code. Removed the unused weather-image wrapper, temperature pivot constants and old-LVGL screen branch, while retaining live drawing margins and message priority/sender data. The edge renderer reuses the rasterizer's existing blur helper; both edge PNGs and a blurred SVG result are byte-identical. Existing message checks, all 14 native tests and the ESP-IDF 5.5.5 firmware build passed. Updated source-list and verification documentation; no source files, tasks, stores or product behavior were added. See [local cleanup evidence](../.reviews/main/optional-cleanup-2026-10-09/closure.md). Hardware and full asset-regeneration scripts remain unverified.
- Completed Night Shift startup and brightness ownership. Startup calculates appearance when possible, otherwise uses the last usable saved Day/Night value or Day. The existing clock-control task maintains appearance and advances configured brightness after display initialization and the initial draw; the UI reads the published value. Removed the normal startup override to 100%, while preserving the storage-error screen's visible backlight. One separate typed operating record saves only effective appearance changes; failed saves are reported and still allow live appearance and brightness to change. Solar calculations remain pending, with fallback/retention used in that mode. Fresh independent checks passed all 14 native tests and the ESP-IDF 5.5.5 firmware build. Independent review caught and closed a startup error-handling issue; memory-error checks confirm the display owner remains usable after record I/O failures. Existing source and test files suffice; no new task, queue or persistence framework was added. See [local closure evidence](../.reviews/main/night-startup-2026-10-09/closure.md). Physical brightness, fade smoothness, power-cut behavior and task timing remain unverified.
- Completed the bounded clock-screen slice. The approved `ui/clock_views.c` owns clock drawing objects and updates; `ui.c` retains navigation, stopped-minute detection and backup-notice actions/lifetime. Reused existing artwork/fonts for centered and fitted digital Night time, one unavailable-time surface for either face and the face-specific paused-clock notice. Retained normal Day/analog geometry, formats, usable RTC time, provider-date labels, warning dismissal, indicators and gestures. Removed the superseded clock objects/functions and hidden unavailable-label path from the shell; initial clock labels no longer show sample dates/weather. Integration checks caught and closed duplicate edge layers and a leftover weather status on unavailable home. Fresh parent verification passed all 14 native checks and the ESP-IDF 5.5.5 firmware build; independent source review found no remaining actionable issue. Updated both build lists and relevant guides; the only new maintained source is the already-approved `clock_views.c`. No new task, queue, store, font or artwork pipeline was added. See [local closure evidence](../.reviews/main/clock-screen-2026-10-09/closure.md). Physical rendering, scaled-font sharpness and touch remain unverified; alarm-specific copy/controls and ringing/snooze/missed surfaces remain unfinished.

- Corrected the shared settings keyboard position after the board user reported no visible keyboard for Wi-Fi credentials and weather location. LVGL creates the keyboard bottom-centered; adding the requested position without changing that alignment placed it at `(120,1000)` on the 800×800 screen. The existing keyboard now explicitly uses top-left alignment, resolving to the intended `(60,480)` position with its original 680×280 size. Removed the unsuccessful focus-only change and retained original click activation, layouts, character previews, masking, drafts and saves. Tests now verify resolved screen coordinates and real pointer key presses/releases, all five text fields, hiding/reopening, and the empty first-use location field through a runtime update. The previous position fails the new bounds test; all 14 native checks and the ESP-IDF 5.5.5 firmware build pass after correction. Independent review found no actionable issue. See the [local positioning evidence](../.reviews/main/text-input-position-2026-10-09/summary.md); the [earlier attempt](../.reviews/main/text-input-2026-10-09/summary.md) is retained as failed-attempt evidence. Loaded that firmware, verified all three written images, and confirmed display/touch startup. The user confirms that the keyboard appears, but its lower corners are clipped by the round panel; centering is the follow-up below. No source files, state or mechanisms were added.


- Centered the unchanged 680×280 settings keyboard at `(60,260)` after the user confirmed visibility but reported clipped lower corners on the round panel. The new corner test fails at `(60,759)` with the lower position and passes after centering; all 14 native checks and the ESP-IDF 5.5.5 firmware build pass. Independent review verified round-panel fit and the one-call production change. Existing layouts, previews, masking, click activation, drafts and saves remain. Updated the architecture's position decision, README input guide and test guide; no new source files or mechanisms were added. See [local round-panel evidence](../.reviews/main/text-input-round-fit-2026-10-09/summary.md). Loaded this firmware, verified all three written images and confirmed matching display/touch startup. The user physically confirms fit and typing, but cannot see the entered text because the keyboard covers its field. Usable entry is unfinished; the user requested discussion of the whole typing layout before further changes. This centered-layout decision is superseded by the later user choice to follow the mockup’s keyboard and complete temporary typing composition, implemented above.

- Completed the initial touchscreen-read containment in existing `main.c` (superseded by the October 10 cause correction above). The startup helper reuses public BSP panel creation and the existing LVGL adapter/GT911 driver, setting a 100 ms timeout for individual I2C operations before touch polling starts. Failed reads clear cached points and cancel an interrupted press, preventing a stale touch or unintended click; successful and held touches retain their existing handling. Both startup paths use the helper. No maintained source file, task, queue, driver fork or bus reset was added. Fresh parent verification passed all 15 native checks and the ESP-IDF 5.5.5 build, with 25% of the application partition free; all three firmware images were flashed and hash-verified. Independent review found no actionable issue. See the [local review](../.reviews/main/touch-freeze-fix-2026-10-09/independent-review.md) and its adjacent test, build, flash and brightness logs. Native hardware substitutes check setup and returned-error handling; they do not reproduce the real interrupt fault or establish physical recovery. The subsequent user smoke retest confirms advancing time and responsive touch at 50%; the initiating cause, physical recovery from a repeated touch-read fault and longer-term stability remain unverified.

- Completed the requested detailed, read-only all-screen mockup comparison. The [local report and per-screen tables](../.reviews/main/mockup-compliance-2026-10-09/report.md) cover both appearances, all 22 settings routes, editing/loading/error/progress states, clock notices, forecast, conditions, messages, indicators, browser and unfinished alarm surfaces. Most normal clock geometry agrees; small corrections include the Day AM/PM baseline, Night digit widths and specific styles/positions. The largest gaps are settings hierarchy, switches/pickers, Night styling and the whole typing state, with further weather waiting/error and conditions differences. Browser source matches its approved preview apart from simulated data. The report recorded a keyboard-map/centered-geometry conflict; the subsequent user choice and current settings rework resolve it in favor of the illustrated keyboard. No firmware source changed or tests/builds reran for this audit. Source comparison is complete; physical pixel compliance is unverified. Earlier functional completion did not establish visual completion.

- Completed the user-approved brightness change after board observations: 20% was barely visible and lower values were too hard to read. Both controls and configuration validation use 20–100%, with Day/Night defaults of 80%/30%. Successfully loaded values below 20% are raised to 20% only in memory; valid saved choices, other settings, normal explicit saves and read-error handling are preserved. The mockup uses the same range and default, with colors and geometry unchanged. Parent verification passed all 15 native checks, the actual mockup helper checks and the ESP-IDF 5.5.5 build; the application partition remains 25% free. Independent review found no actionable issue. All three loaded images were verified, and the user reported continued operation the next morning. No maintained source paths, tasks, queues, stores or migration framework were added. The user separately reported that Night clock lettering looks too white and explicitly requested no lettering or menu-color changes; this slice leaves them unchanged. See [local closure evidence](../.reviews/main/brightness-minimum-2026-10-10/closure.md).

The approved fixes from the YAGNI/MECE review are closed. F8's unnecessary browser-adaptation task was corrected earlier. Approved unfinished development and declined choices are listed below.

Approved development still unfinished:

- Backup-clock wiring guide: document the supported DS3231 module's connection to the Waveshare board, including the shared I2C bus and coin-cell placement. The root README retains hardware selection and user-facing battery limitations; writing and verifying the missing wiring instructions remain pending.
- Settings navigation and Display Save board verification: implementation above was flashed October 12; visual/control checks are in progress. Check both appearances, Settings/Device row order, the four Display rows and three Night Shift rows, remaining Brightness Done/summary and visible 12-hour clock-padding checks, both brightness levels/appearances, Weather Back/Cancel retaining pending Device edits, and alarm restoration. The basic Brightness Back, Display Cancel/live Save and baseline restoration pass is closed above; do not repeat it merely to extend the checklist. The basic real time-bearing Conditions Save check is closed in the Leading Zero session above (`3 PM` ↔ `15:00`); do not repeat it routinely. Offline/stale operation, format Cancel and other physical captions remain unverified. The reported analog `Tomorrow · 7:00 AM` one-line notice and Alarm Settings tap are confirmed above; both appearances and other captions were not separately reported. Retained Device/Weather restart behavior also needs a board retest. Exact panel alignment and physical touch remain unverified.
- Messaging board verification: the user confirms guidance readability, keyboard hide/restore and the later vertical-centering/Done-clearance adjustment. Whole-row access and House Messages controls passed earlier basic checks; do not repeat them just to extend the list. Separately stated Day/Night confirmation of the current placement and untested error states remain open.
- Board retest: the controlled RTC-trigger investigation and bounded correction are complete above. The user reports that everything seems to be working after BSP startup restoration. Detailed swipes/Settings/keyboard checks and longer operation remain to be recorded; the general observation and unattended polling do not establish every physical interaction. External RTC operation, precise driver interrupt ordering, broader rendering, brightness transitions, network recovery and power cuts remain unverified. Blue flashes remain a separate unresolved observation. Retire retained original/reference/fallback evaluation material once it no longer serves the current physical comparison or recovery checkpoint.
- Settings board comparison: the basic pass is closed. Prompted control/typing checks passed, including all four keyboard pages in the initial Device-field test, time stepping/hour wrapping, preference selections and sliders. Night checks passed for normal Alarm, Display, Device, Night Shift, offset and Wi-Fi pages, visible inputs and readable controls. The Wi-Fi question included scrolling when more than three networks appear; the reply did not separately state the row count. Device Setup Save & Restart passed, and the original Sunset to Sunrise mode and Day appearance are restored after the temporary Always On test. Remaining physical verification covers precise mockup positions/typography/colors, separately unreported noon/midnight AM/PM crossings, all Night keyboard pages, validation/error/scan-empty/pending states, actual connection-phase captions, the immediate Settings-entry Alarm save and broader feature-specific saving/persistence. Do not repeat successful basic checks merely to extend the test list. Keep existing discovery and settings-save ownership. See the [local closure evidence](../.reviews/main/settings-menu-2026-10-10/final.md) for exact prompted observations and limits.

- Night Shift board verification: local solar scheduling and calculated Settings times pass development checks above. The populated-page, unsaved preview/Cancel and Solar Save/restart checks are closed as reported, including the expected afternoon Day appearance and retained At offsets. Physical transitions, configured fades and offline operation remain pending; temporary 50% brightness levels must be restored to the user's recorded Day 100%/Night 25% after testing. Equal test levels do not verify a fade. Trustworthy offline startup still requires an attached backup clock. Earlier board diagnostics showing Solar retaining Day preceded this implementation; no lost-settings repair is implied.
- Screens: settings presentation is implemented and checked in source; physical compliance remains pending. The Weather saved screen uses the real timezone identifier through the reference fallback rather than the browser’s friendly generic name. The approved thin slider proportions are now explicit in both mockup and firmware; the user confirms clear handles and normal interaction on the board, while exact browser/board pixel equality remains unverified. The eleven approved normal-clock, weather-waiting and Forecast/Conditions warning changes are implemented above; the disapproved visuals remain as selected. Clock drawing ownership is established, but precise physical compliance remains pending; the bounded weather/message drawing ownership cleanup is complete above. Alarm-related clock surfaces are implemented in the current development slice; physical verification remains pending. The matching browser appearance needs no replacement. Before calling a screen complete, compare the exact approved positions, controls, typography, colors and relevant states, and state physical verification limits.
- Retirement: remove each superseded rendering path, unused asset and obsolete build option as its replacement is integrated and verified. File moves alone do not complete this work.
- Alarm board verification: occurrence handling/recovery, presentation/actions, draft next-time preview, speaker playback and sound preview are complete in development above. Prompted previews, resumed ringing/Dismiss and short power-cut recovery checks have passed within the recorded limits. Prolonged ringing under load, precise timing, longer outage/fault behavior and attached-RTC operation remain pending; combine the remaining checks later as the user requested. F1/F2 corrections pass development checks and were included in the October 12 firmware load; their board retest remains pending.

The requested optional cleanup items are closed, including the proxy-query simplification completed with the browser correction. The approved feature work above remains unfinished.

PNG-decoder replacement was declined; retain the existing implementation under the [asset-tool decision](architecture.md#existing-code-changes-that-matter).

Paused-clock warning removal was declined; retain the existing diagnostic under the [time decision](architecture.md#time-and-rtc). Broad removal of weather outputs used by tests is not recommended where those values serve approved behavior or useful calculation checks.

The preceding freeze correction is committed and the user reports continued operation. The keyboard decision and settings-menu basic board pass are closed. The eleven approved screen changes are implemented, checked and loaded; Digital Day label fit/readability and Digital/Conditions divider visibility/readability are confirmed by the user. Continue the remaining physical checks without repeating those or the successful basic menu checks. Detailed visual/failure-state and physical alarm checks remain unfinished. The bounded weather/message ownership cleanup, local solar scheduling and alarm scheduling/recovery backend are complete in development. Alarm presentation/actions and the draft next-time preview are complete in development; speaker playback and sound preview are also complete. The next checkpoint is the deferred combined board verification. Keep service decisions with their current owners and navigation in `ui.c`. The audit's optional suggestions, reference ambiguities and unmeasured risks are not automatically approved changes.

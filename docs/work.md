# ESP32 Weather Alarm Clock development work

## Sprint: Visual Crossing and approved weather mappings — authorized October 13, 2026

Status: implemented and verified for this bounded sprint. Native checks, firmware
build, verified flash, Visual Crossing activation/units and final physical
navigation pass. The supplied key is preloaded in board settings and verified.
Addressing, weather-memory and UI-task corrections have the evidence below.
Long-duration stability, rare live conditions and real quota exhaustion remain
unverified; the complete call chain behind the earlier UI crash was not captured.
Checkpoint review: the October 13 adversarial review completed all four
Optimizer/Skeptic passes with no new confirmed actionable finding or required
simplification. Fresh firmware, all 38 native tests, fixture/message checks and
offline actual-response decoding pass. The earlier Shared Device ID issue stays
deferred; the stated hardware/provider limits remain. Local reports are retained
under `.reviews/main/adversarial-visual-crossing-2026-10-13/`. The user authorized
a local checkpoint commit containing reviewed firmware/settings changes and
approved documentation, mockups and prepared assets. Radar/Sound Machine
firmware remains pending. No push or new flash is part of this checkpoint.

Started by Development Orchestration
following the user's Weather Provider Accuracy request. Implementation, sparing
live checks, flashing and bounded board tests are authorized. The reviewed instant-save Alarm change and
unrelated Radar/Sound Machine work are preserved. Product decisions remain in
[architecture](architecture.md#weather) and [weather mappings](weather.md);
this work record tracks delivery and evidence.

1. Shared facts, mappings and daily selection. Permitted files: existing
   `main/weather_icon_names.*`, `forecast_representative.*`, `storm_conditions.*`,
   their existing native tests and affected fixture data. Check all provider
   families/compound facts, current-only exclusions, the approved daily gates,
   duplicates, missing data and unchanged warning limits. Disclosed existing
   behavior changes: corrected pictures/captions and Unknown/no-picture fallback;
   paired 50%/1.5 mm Rain/Showers selection; two distinct Hail reports; strict
   mixed majority. Other family checks and approved cold guesses remain.
2. Requests and decoding. Permitted files: existing `main/weather_client.*`,
   `main/app_runtime.h`, `tests/test_weather_service.c`, existing support/build
   files when needed. Check downloaded real snapshots before live calls, units,
   fractional values, observation/epoch order, usable daily pairs, optional data,
   sanitized errors, serialized requests and key-specific quota admission/recovery.
   Confirmed daily exhaustion is the approved exception to normal retry timing;
   generic 429 responses retain ordinary retry behavior.
3. Settings and worker integration. Permitted files: existing
   `main/device_config.*`, `main/ui/settings.c`, `main/main.c`, affected existing
   tests and UI rendering/assets/build files. Persist provider/key in ordinary
   Weather saves, mask/retain/explicitly clear keys, validate selected credentials
   before publication, preserve independent drafts/restarts and wire the current
   Weather controls. New provider behavior expands the available choice; no menu
   redesign, provider framework, permanent worker, queue or extra store is planned.
4. Closure and board pass. Independently review correctness and necessity,
   run affected checks plus the regression suite, compile ESP-IDF5.5.5, inspect
   payload/heap/stack/asset sizes, then flash and verify images/boot. Check real
   provider/key switching, units and weather rendering with bounded user
   observations. Restore temporary choices, keep normal firmware operating and
   record genuine limits. Existing Alarm/Bell/Radar deferrals remain separate.

Original live-call plan: six authenticated Visual Crossing and two Open-Meteo
requests for deliberate schema/activation/board verification, including test
restarts and periodic requests during that bounded window. Start from the two
provided real snapshots; distinguish them from constructed error/rare-condition
cases. Track counts and queryCost when returned. Do not deliberately exhaust
quota. No key, key-bearing URL or credential-bearing payload enters reports or
repository fixtures. The protected temporary handoff will be removed when no
longer needed. Local evidence: `.reviews/main/visual-crossing-sprint-2026-10-13/`.

Delivered in the existing owners: shared provider facts/current captions,
approved daily selection, normalized requests/measurements, key-specific RAM
quota admission, seven-entry ordinary Weather saves and activation checks in
the existing save task. No provider framework, worker, queue, new store or
maintained source path was added. Fog updates the existing canonical asset;
Rain & Snow reuses the existing image; five new pictures add 940,000 image
bytes in the existing `wac_icons` file. Its previous UI image content is preserved.
The official Weather prototype now illustrates saved/changed masked keys,
explicit Clear, retained keys and validation without making real requests.

Independent review found and closed a credible future-hour conversion error:
the real Visual Crossing response omits per-day/hour UTC offsets, so applying
today's root offset across a DST change shifts future local samples. Decoding
now preserves provider local date/time and keeps epochs separately. Spring/fall
window and repeated-hour tests cover this correction. A duplicate Fog image was
also removed in favor of updating its existing asset. Current snow classification
uses reported evidence; the approved daily cold guesses remain as a preservation
tradeoff, not a demonstrated accuracy improvement.

Parent checks: all 38 native cases passed across the full run and isolated UI
rerun; forecast 53/53, Conditions 40/40, location 3/3 and the existing messaging
checks passed. The first full run's UI shell reached its 15-second limit during
concurrent firmware compilation; alone it passed in 11.91 seconds and under
the unchanged CTest limit in 11.11 seconds. No source or timeout change was needed.
All 14 embedded mockup scripts parse. ESP-IDF 5.5.5 firmware builds successfully:
17,089,376 bytes before the addressing correction, with 19% of the application partition free. Added/renamed source
path check and `git diff --check` pass. No commit or remote action was performed.

One deliberate live Visual Crossing US request returned HTTP 200 and queryCost 1;
its sanitized 46,205-byte full response was then decoded by the actual C source
without another request. Current observation, five provider dates/highs/lows,
all 168 local hours and fractional values pass. The decoder cap is 192 KiB.
Constructed rare IDs, errors, quota timing and DST cases remain distinct from
this ordinary live capture. Physical activation, unit changes,
navigation and decode-time heap/stack results are recorded below; real daily-quota
response/reset metadata and rare live IDs remain unverified.

Board load follow-up: all three images passed esptool write verification, but
normal boot rejected an application segment beyond 16 MiB. Its reported bytes
exactly match the same address wrapped into the first 16 MiB; the local image
checksum/validation hash is valid. The application never starts, so no additional
provider requests occur. The GD flash reports manufacturer `c8`, device `4019`,
32 MB; the P4 is revision 1.3. Enable ESP-IDF's existing extended cache-address
option and its experimental-feature gate in `sdkconfig.defaults`, preserving
assets, DIO, flash size and partitions. Rebuild and verify real boot before
proceeding. The experimental label concerns varying chip support, so the build
alone is insufficient. The requested key has not yet been written. Disposable
debugger/capture processes were stopped; the board remains in the USB loader
during this correction. No settings partition was erased or replaced.
The corrected matching build is 17,089,056 application bytes (still 19% free),
with a 23,488-byte bootloader. Reconfiguration/build succeeds. An independent
SDK/default review found no unrelated feature enabled; a complete earlier
ignored configuration was not retained, so no exhaustive old/new generated
configuration comparison is claimed. DIO/80MHz flash and HEX/200MHz PSRAM remain.
The supplied key is absent from the firmware image, sprint source, fixtures and
local reports. Actual extended-address boot and provisioning verification are
still pending.

The extended-address correction then passed real boot, including valid segment
loads above 16 MiB and entry into `app_main`. At the storage-ready breakpoint,
ordinary NVS calls wrote only `weather_key`, committed it and privately matched
readback. A subsequent normal initialization reloaded the supplied key; provider
remained Open-Meteo and every recorded nonsecret preference matched. Scratch RAM
was zeroed/freed. No provisioning firmware or settings-partition replacement was
used; the key is absent from application/source/report files.

Normal decoding revealed a second required hardware correction: the weather task
had only 636 bytes minimum stack headroom and overflowed during formatting;
internal RAM was nearly exhausted while more than 12 MB PSRAM remained free.
Target compiler frames explain the overlap: worker 1,952 bytes, request 3,152,
decode 528 and Conditions 1,440 before nested library calls. Release transport
scratch before decoding, retain the existing task size pending measurements, and
set cJSON's existing PSRAM allocator hooks once in startup for all application
JSON users. No new memory owner, worker or retry policy is introduced.
Five automatic Open-Meteo attempts occurred in the panic/restart loop, exceeding
the original two-call plan; they are recorded in the local budget. Visual Crossing
still has only the one host request. The board is held in the USB loader and the
saved key/preferences are preserved while this correction is tested.
The memory correction's compiled request frame falls from 3,152 to 400 bytes;
the transport helper uses 1,408 bytes and returns before decoding. The known
Conditions chain drops from 7,120 to 4,368 bytes before deeper library calls.
No stack-size change is made. Fresh native weather/maximum-key bounds checks,
offline actual-response decoding and firmware build pass; independent source
review found no remaining defect before the next board run. JSON hooks are set
once in startup, with SDK-compatible freeing and existing checked allocation
failure cleanup. The rebuilt application is 17,089,440 bytes, still 19% free.
One controlled Open-Meteo startup and one eventual restoration are the remaining
planned Open-Meteo checks after this incident; do not repeat failed live attempts.

Corrected board run: matching images passed write verification, normal boot reads
above 16 MiB and Open-Meteo returns HTTP 200 with an 11,090-byte payload. At decode,
minimum free stack is 1,980 bytes, internal free RAM 88,855 bytes, largest internal
block 31,744 bytes, and PSRAM free 12,542,560 bytes. Cleanup minimum stack is
1,772 bytes and internal free RAM 106,491 bytes. The twenty-second post-success
capture contains no panic/restart. These are one observed worker request, not
worst-case timing or every fault; Settings activation memory and physical
touch/appearance remain pending. The user's existing choices remain recorded
privately for restoration, and the requested saved key survives initialization
with Open-Meteo still selected. No key is embedded in any flashed image.
The Weather test prompt now asks the user to choose Visual Crossing, verify
Configured, Save once, and inspect restart/touch/current/Forecast/Conditions.
API quota/reset and rare provider conditions remain controlled/native evidence.

Visual Crossing activation passes on the board: the user confirms Configured,
successful Save/restart, working clock/touch and weather on Clock, Forecast and
Conditions, with Visual Crossing/Configured retained. Actual validation and
startup requests both return HTTP 200, 46,224 bytes, queryCost 1 and result 0.
The Settings save task's minimum free stack is 3,484 bytes and internal RAM stays
above 83,903 bytes; the weather task's minimum is 1,724 bytes and internal RAM
above 106,047 bytes. This activation phase used two requests, plus the single
earlier host query. The final prompt changes units to Metric then back
to Imperial using the same saved key, leaving Visual Crossing selected.
This tests unchanged-key saves without extra activation calls and both real unit
responses; confirmation and request counts remain pending. No alarm tests are
reopened. The protected temporary credential/snapshot handoff has been removed.

The user confirms Metric and restored Imperial/Configured fit and work. Serial
capture nevertheless found two stack-protection panics in `rwd_ui_heartbeat`
between these otherwise successful checks. The board was held in the USB loader
to stop further requests. Weather and Settings memory remain healthy; the second
fault crosses the UI task's stack bound while formatting text. This locates the
failure but does not establish the complete triggering call chain. Target frames show heartbeat 688 bytes,
copied config 624 bytes and newlib formatted output 1,152 bytes plus wrappers;
one fault enters a 48-byte formatter frame just below its stack bound. Increase
only the existing heartbeat to 6 KiB and record its minimum headroom after valid
weather updates, logging only when that minimum falls. This uses 2 KiB of measured available internal RAM,
preserving its local config copy and rendering, with no new state owner/task.
Making config static was considered but not implemented; it would remove the
local copy rather than establish the complete crash cause. The correction keeps
the existing local-copy ownership and gives the task more working space.
The six planned Visual Crossing requests are used, including the unexpected
restart requests. One controlled startup after the correction is now necessary;
record it as a seventh request rather than hiding it or repeating a crash loop.
Do not repeat the completed unit saves or broaden Alarm/Bell tests.
The final heartbeat patch and firmware build pass; the application is
17,089,488 bytes with 19% partition free. It preserves the local configuration
copy and all screen/Save code. The watermark is sampled after rendering outside
the display lock, with only a task-local previously reported minimum. Source
review and the final authorized board load pass. Matching images were verified;
normal startup returned Visual Crossing HTTP 200, 46,083 bytes, queryCost 1 and
result 0. Weather minimum free stack is 1,724 bytes and internal free RAM is
105,099 bytes after cleanup. UI minimum free stack reached 3,120 bytes in the
bounded capture, with no panic or unexpected restart. The subsequent user check
confirms the clock keeps advancing, Forecast/Conditions/Weather navigation and
touch work, and no unexpected restart occurs. This short observation is not a
worst-case stack bound or an overnight stability test.
The completed run totals seven Visual Crossing and six Open-Meteo requests,
including unexpected restart attempts. The serial capture ended normally.

The target compiler verifies that adding provider/key fields enlarged
`device_config_t` from 488 to 624 bytes. The otherwise unchanged heartbeat keeps
that whole copy locally, so its memory demand rose by 136 bytes. That is a
measured contributor, not proof that those bytes alone caused the overflow.
The missing full backtrace and limited observed stack paths remain explicit
limits on the causal explanation. No further source change is made for this
explanation. The board is left with Visual Crossing, Imperial and the configured
key; no additional unit/provider saves or Alarm/Bell tests are requested.

Closure: final physical navigation passed on October 13. Source/native/build/
flash and the scoped board acceptance checks are complete; no additional required
implementation item was found in the independent correctness/necessity review.
The key exists in board NVS only. Temporary credential handoff, serial/debugger
processes and the task-owned native build directory are cleaned up; useful logs,
fixtures and review evidence remain local. No commit, push or PR comment was made.
Retain the stated limits on crash causation, long-duration operation, rare live
weather pictures and actual provider quota/reset responses. The earlier deferred
Shared Device ID issue and separate Radar/Sound Machine work are unchanged.

## Radar source and palette decision — October 13, 2026

Recorded the user's RainViewer and green/yellow/red choice in the
[radar specification](architecture.md#radar-source-and-palette), including the
exact gradient stops from the selected comparison. Retained that comparison
as local visual evidence. Provider documentation and the recorded palette
were checked; firmware, the official mockup and application assets were not
changed. No software tests, firmware build or board checks were run for this
documentation decision.

Radar is not implemented. Its remaining design and implementation work is
listed under [current work](#current-work--reconciled-october-13-2026).

Follow-up: created a proposed round-screen radar preview with the selected
palette, a background map and enlarged town labels. The
[screen image](../.reviews/radar-screen-mockup-2026-10-13.png) uses the actual
northern Colorado sample from October 13 at 8:20 AM MDT, not current conditions.
The user clarified that radar is a peer of Forecast and Conditions. Revised
the preview to use a main weather screen with a curved heading and removed the
menu-style header and return arrow. The architecture records the navigation
choice; the exact screen order and layout remain unapproved.
The user then requested a darker map that blends into the clock background.
Revised the proposed preview with a charcoal basemap, a soft edge and muted,
legible labels. Rebuilt the same recorded radar frame as a separate layer,
using the approved color mapping and display opacity; all 34,691 visible source
pixels matched the provider's color table and retained their original alpha
before display opacity was applied. Background styling does not recolor radar.
Browser inspection at 736px and 320px confirmed readable labels, timestamp,
legend and credits. The preview script parses; the radar uses the same frame,
extent and palette as the selected comparison. This layout is a proposal and has not been approved or
added to the official mockup or firmware. No firmware or board checks were run.

Edge-blending follow-up: the user requested a more gradual transition into the
clock background. Updated the proposed preview to use one wider, eased opacity
fade for both basemap and radar, removing the radar layer's hard circular crop.
Town labels remain outside the fade. The default transition spans 120 pixels on
the 800-pixel clock, compared with the earlier background-only 64-pixel fade;
its exact width is still a mockup choice. Embedded map/radar image bytes, frame,
geographic extent and palette are unchanged. Safari inspection at 736px and
320px confirmed the softer perimeter and legible labels, heading, timestamp,
legend and credits. Script syntax passes. The host's optional fade-width control
was not exercised in the standalone inspection wrapper. Updated the existing
preview image; no official mockup, firmware source, build or board check changed.

Map-definition follow-up: the user found that fade too soft and the background
geometry too muted. Narrowed the proposed transition to 88 pixels and increased
only the basemap's brightness and contrast, making roads and terrain clearer.
The radar frame, palette and embedded source images are unchanged. Safari
inspection at 736px and 320px confirmed the clearer map, gradual perimeter and
readable labels. Script syntax passes. Updated the existing preview image;
the exact styling remains proposed, with no firmware or board checks.

Design selection and mockup promotion: the user approved trying this refined
design for the clock's radar screen. Integrated it into the existing
[official visual reference](../mockups/alarm-design.html), available through
the Radar preview beside Forecast and Conditions. The selected map, radar,
palette, fade, town labels, heading, timestamp, legend and credits are preserved;
the preview uses the clock mockup's embedded font and Day/Night appearance.
The toolbar's selector order does not settle the device's eventual swipe order.
The existing clock, settings, Sound Machine and House Messages content is
preserved. No new source file or firmware feature was added.

Safari checks passed for the Radar selector, Day/Night appearance, 736px and
320px layouts, and navigation to Forecast, Conditions, Sound Machine and the
House Messages browser preview. All eleven inline scripts parse across the
outer document and both previews. The embedded map and radar image data match
the selected design; reversing the six scoped integration edits reproduces all
previous device-preview content. No browser script error was observed; the
wrapper's favicon is blocked by its existing content security policy. Checks
and screenshots are local under `.reviews/radar-design-promotion-2026-10-13*`.
No firmware build, flash or hardware check was run: this slice only promotes
the selected design. Live radar implementation remains in the current work list.

October 15 follow-up: capitalized the heading to “Northern Colorado Radar” and
named the official mockup selector Weather Radar, directly beside Forecast and
Conditions. Carried over the preview's selected 48-pixel fade. The specification
now records the page after the Forecast/Conditions pair while preserving their
storm ordering. Embedded map/radar images and unrelated mockup content are
unchanged. No firmware source or assets changed.
All eleven inline scripts parse, the preview order check passes, and Safari
inspection confirms the capitalized heading and Weather Radar page. Evidence is
in `.reviews/radar-mockup-follow-up-2026-10-15.json` and its screenshot. No
firmware build, flash or hardware check was needed for this mockup-only edit.

First-version behavior approved October 15: recorded the fixed map centered on
the weather location, its location marker, automatic updates/frame timestamp,
retained old image after failed updates, unavailable state before any image,
and no animation in the [radar specification](architecture.md#radar-source-and-palette).
The marker and failure states are not yet drawn in the official mockup. The
subsequent coverage/interval selection is recorded in the specification and
prepared sprint below; its first step verifies the background source. Firmware
remains unimplemented. This follow-up changes documentation only.

## Prepared Weather Radar sprint — October 15, 2026

**Status: prepared locally and held unsent at the user's explicit request.**
Intended recipient: **Development Orchestration**
(`01a11e52-7ce0-78f3-a2e5-efcb90c7aa16`). No message has been sent and no
implementation, board load or remote action starts through this preparation.
Obtain the user's instruction to dispatch before sending the handoff. Preserve
the recipient's current notification review, pending firmware list, Sound
Machine work and other changes in this shared `main` checkout.

### Objective and boundaries

Deliver the first live Weather Radar page under the
[radar specification](architecture.md#radar-source-and-palette) and
[official visual reference](../mockups/alarm-design.html). The approved fixed
coverage and check interval replace the earlier open choices; the suggested
150-mile view and optional zoom slider are outside this pass. Product details
remain owned by the specification, including navigation, marker, latest-image
presentation, retained-old/unavailable states and appearance. Do not add radar
settings, animation, panning, persistent image storage or an extra supported
firmware/platform implementation.

This is a new feature. It must preserve existing weather refresh/retry/quota
rules, alarm and Night Shift behavior, message/settings navigation and audio
playback. It also includes the October 15 approved replacement of the device's
Conditions page with Radar; Conditions stays in the mockup and its implementation
and required assets are retained in the existing source tree. The loss of that
page's wind/pressure/outlook display is intentional. Keep shared current-weather
data and the remaining storm detection/indicators, with Radar taking the former Conditions
position in normal and alert navigation. Before each implementation slice,
disclose any additional existing
behavior change under AGENTS.md; this sprint does not silently approve unrelated
cleanup or changes to failure handling. No commit, push, merge, flash or release
packaging is authorized by this draft.

An additional existing-behavior change was approved October 15: remove falling
pressure as a storm-alert trigger because it is too sensitive. The current
calculator adds a pressure alert after a 3 hPa drop over three hours; remove
that qualifying alert in existing `main/storm_conditions.c/.h` and update the
affected existing fixtures/tests. Keep pressure measurements/trend and non-alert
descriptions where used. Pressure alone must not light the storm indicator or
put Radar first; the other qualifying alerts and their thresholds stay the same.
This is an approved policy change, not a new adjustable sensitivity setting.

### Bounded implementation slices

1. **Confirm the real 50-mile image and decoding path.** Start with the
   [USGSTopo raster service](https://basemap.nationalmap.gov/arcgis/rest/services/USGSTopo/MapServer)
   used for the selected sample, as a candidate rather than a newly approved
   production dependency. Confirm its endpoint, applicable credits, geographic
   coverage and legible town names at the actual clock image size. Its published
   service includes place names, roads and terrain and supports PNG export.
   Check a real 50-mile view at the configured weather location and the northern
   Colorado comparison area, using current provider documentation and limited
   public requests. Respect RainViewer's maximum zoom of 7: obtain supported
   tiles, crop/resample to the requested ground coverage and assess the resulting
   detail; do not silently widen the map or claim extra radar resolution.
   The existing sample is a visual reference, not proof of production scale,
   dynamic town labels or a working map pipeline. Select an existing ESP-IDF/LVGL
   or managed runtime image-decoding route that exposes unsmoothed radar pixels
   for the approved mapping. Current raster decoders are disabled; no custom
   PNG decoder or general mapping framework is planned. Establish bounded
   compressed/decompressed sizes and large-buffer allocation in PSRAM before
   proceeding. Record the concrete provider, decoder and owner choices in the
   architecture at the authorized implementation checkpoint. If matching the
   approved view requires a user account, narrower location support, materially
   different visuals or unlisted source paths, present that concrete choice
   before adopting it.
2. **Fetch, map and compose one current image.** Extend the existing
   `targets/esp32-p4/main/weather_client.c/.h` request/decoding owner and only
   needed existing configuration/dependency/build facilities. Fetch RainViewer's
   published frame list and newest usable frame; validate dimensions, lengths,
   returned errors and incomplete images with finite request bounds. Keep
   forecast-provider credentials and quota state separate from radar requests.
   Translate original unsmoothed colors/alpha under the specification before
   resizing or composition. Use one geographic transform for the background, radar and
   saved-location marker, including latitude's effect on ground distance. A
   provider's projected-meter bounds are not automatically ground-mile bounds.
   Confirm alignment and town readability with sourced imagery; don't bake the
   sample's seven town names into production. Render the background at its final
   display size; enlarge only the radar overlay as needed for the selected range,
   keeping background roads and labels sharp. Retain the current location's
   background in RAM and prepare replacements offscreen. Publish only a complete,
   matching composition; an empty valid echo layer means no detected echoes,
   not a failed download. Keep original frame time through retries and unchanged
   responses. Do not fetch the static background on every radar check when the
   current location/background remains usable.
3. **Integrate scheduling and resource ownership.** Reuse the weather worker in
   `main.c` with a separate radar deadline and short copied display status in
   `app_runtime.h`. Its wait must consider radar independently of forecast retry,
   time synchronization and provider quota deadlines; adding the radar wakeup
   must not trigger extra forecast requests. Use monotonic elapsed time for the
   check interval, independent of clock corrections. Give retained image buffers
   one owner and define their lifetime through the UI's safe image replacement;
   do not free a displayed buffer or publish a half-built one. Download/decode
   outside the display lock and outside clock/audio tasks. Keep the last valid
   same-location image on failure. After a location change, do not label an old
   location's image as the new location's radar. Handle offline/reconnect and
   failed allocation without a tight retry loop or changing forecast recovery.
   No new permanent task, queue, image database or multi-level cache is planned.
4. **Add the production page and finish its mockup states.** Reuse `ui/ui.c`,
   `ui/weather_views.c`, `ui/ui_private.h` and only needed public display inputs
   in `app_ui.h`. Replace the production Conditions page with Radar, keeping a
   two-page weather row under the specification, reverse swipes and the row order
   retained for the current visit. Keep the existing Conditions renderer,
   required assets/build entries and useful tests in their current source
   locations. Remove Conditions from user navigation and other user-accessible
   routes; do not create inactive live display objects solely for code retention.
   Keep shared weather data,
   storm calculations for the remaining triggers and the clock's alert indicator.
   Apply the approved pressure-alert removal in the existing calculation owner,
   not by hiding its output only in the UI. Do not add a Conditions enable
   setting, fallback route, alternate firmware mode or backup copy. Draw the
   selected heading, map, legible town labels,
   saved-location marker, frame timestamp, legend, credits and old/unavailable
   states in Day/Night. Reuse existing fonts, grouped artwork and LVGL drawing
   facilities. Complete the marker and failure-state reference in the existing
   official mockup; preserve its Conditions page and all other reference screens.
   Do not use its recorded data as live firmware input. Preserve alarm preemption
   and the existing menus. Keep
   UI navigation and LVGL handles with their current owners, not a second map UI.
5. **Review, native/build checks and a separately authorized board pass.** Extend
   existing `tests/test_weather_service.c`, `tests/test_ui.c` and test support/
   CMake only as needed. Check five-minute scheduling alongside forecast/time/
   quota deadlines, unchanged-frame checks without repeat image downloads,
   truncated/failed images, representative source colors/band boundaries/alpha,
   50-mile ground coverage/alignment, successful image replacement, location
   changes and retained-old/first-unavailable presentation. Keep useful tests
   for retained Conditions rendering and add production Forecast/Radar navigation
   checks that ensure Conditions is not user-accessible. Retain tests of shared
   weather data and the remaining storm detection. Update the existing
   pressure-only case in `shared/test-data/storm-conditions-cases.json` to expect
   no alert, and check that pressure plus another qualifying condition still
   alerts for that other condition. Verify that pressure alone cannot activate
   the indicator or storm-first ordering. Verify two-page navigation, Radar-first
   alert order, both appearances and alarm preemption, and confirm that Conditions
   remains selectable in the mockup while absent from the device UI. Distinguish
   captured real data from constructed failure inputs. Independently review
   coverage, correctness and whether each added mechanism is needed. Run the
   current native suite and firmware build using the existing guides, including:

   ```bash
   source targets/esp32-p4/scripts/activate-idf.sh
   cmake -S targets/esp32-p4/tests -B /tmp/esp32-p4-native-tests
   cmake --build /tmp/esp32-p4-native-tests
   ctest --test-dir /tmp/esp32-p4-native-tests --output-on-failure
   targets/esp32-p4/scripts/test-weather-parity.sh
   targets/esp32-p4/scripts/build.sh
   ```

   Inspect image/firmware sizes, peak internal/PSRAM use, worker/UI stack headroom
   and repeated replacement cleanup. The old 2–4 MiB radar estimate is a planning
   allowance, not measured free memory. Do not use `build-release.sh`, which also
   updates published firmware files. Seek separate flash authorization for the
   board pass. Then check actual source imagery/labels, range/marker alignment,
   frame timestamps over real checks, swipe/touch responsiveness and recovery
   from offline/failed fetches. Compare current Sound Machine playback before
   and during repeated radar fetch/decode/render, including a recording and
   generated noise; do not accept new audio cutouts. Exercise alarm preemption
   while radar work is pending. Restore temporary settings and record genuine
   board results and accepted limits in this work record. Update the user guide
   only when the feature is delivered.

### Source-path and readiness check

The intended work fits existing approved owners; enabling a bundled decoder or
adding a managed dependency uses the existing manifest, lock, configuration and
build files. Do not create `radar_client.c/.h`, `test_radar.c`, a helper tool or
another unlisted source path without exact-path permission. A separate module
is an option only if a concrete implementation need justifies it; preparing
this sprint grants no such permission.

Preparation checked the authoritative decisions, current worker/client/UI/test
paths, decoder configuration and build guide. A gpt-6-luna/xhigh read-only
review confirmed the existing-owner route and identified the provider/decoder
readiness checkpoint plus unmeasured resource/audio interaction. Parent source
inspection confirmed independent forecast timing/quota and the weather-row
snapshot behavior. No product behavior was reduced to avoid those checks; the
subsequent user-requested Conditions replacement is recorded below.
No firmware source, mockup, tests, build or board state changed during this
preparation. The sprint remains **UNSENT**.

Scope revised October 15 after the user's instruction to replace Conditions on
the working clock with Radar while retaining Conditions in the official mockup.
This supersedes the draft's earlier three-page row; it intentionally changes
which information is available on the device. Updated the specification,
implementation slice and verification criteria only. This revision does not
dispatch the sprint or start firmware work, and it does not remove Conditions
from the currently running board before the replacement is implemented.

The subsequent falling-pressure decision is included in this same unsent
sprint and in the authoritative alert rules. Firmware still contains its old
pressure trigger until the sprint is dispatched and implemented; no source,
build or board change was made while recording this decision.

Retention correction, October 15: the user explicitly requested keeping the
Conditions implementation while removing its user-interface access. Retain its
code, required assets and useful tests in place; this supersedes the earlier
instruction to retire the renderer. Conditions remains available in the mockup.
The specification, implementation slice and checks reflect this correction.
The sprint remains unsent; this correction changes documentation only.

## Radar and Sound Machine memory estimate — October 13, 2026

Read-only feasibility check: budget **2–4 MiB of additional RAM for radar**,
assuming the proposed roughly 534×534 map, one displayed frame and one
replacement, with large image buffers in PSRAM. One RGB565 map is 570,312
bytes; a cached background plus two completed maps is 1,710,936 bytes before
decoding and network scratch space. Budget **100–250 KiB for one streaming MP3
playback path**, including decoding, buffers and stack. The three live noise
generators need little state beyond playback buffers. These are planning
estimates, not measured allocations or approved implementation requirements.

The [October 12 boot capture](../.reviews/main/classic-bell-2026-10-12/measure2-startup.log)
reports 32 MB flash/PSRAM and a main 17,024 KiB PSRAM heap pool after code and
read-only data mapping, before application allocations. The current display
configuration uses three 800×800 RGB565 panel buffers (3,840,000 bytes in PSRAM)
and an 80,000-byte internal draw buffer. These are existing allocations, not
additional radar costs. No retained measurement establishes free RAM in the
running clock. Internal-memory headroom and uninterrupted sound during radar
downloads/decoding still require a board check. Espressif's
[MP3 decoder benchmark](https://components.espressif.com/components/espressif/esp_audio_codec/versions/2.6.2/readme?language=en)
reports 28 KB heap use at 44.1 kHz stereo, excluding stack, on ESP32-S3; it is
not a measurement of this ESP32-P4 application.

The nine original recordings total 8,336,084 bytes (7.950 MiB). A host-side
check using Espressif's existing SPIFFS image generator and the current firmware
settings successfully packed all nine originals into the 11 MiB file partition.
Blocks containing files occupy 9,146,368 bytes (8.723 MiB), including filesystem
overhead, leaving 2,387,968 bytes (2.277 MiB) in empty blocks. The generated image
was checked for the exact partition size and then removed; original file hashes
remained unchanged. This confirms image capacity, not a mounted filesystem or
board playback test.

[SPIFFS's approximately 75% utilization guidance](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32p4/api-reference/storage/spiffs.html)
is a reliability guideline, not a hard image-size limit. The original audio
payload is close to that guideline, so frequent file additions or replacements
would merit more headroom. Re-encoding is not required to pack this fixed sound
collection. Keeping radar caches in RAM avoids competing with recordings for
this file space.

All original recordings are stereo; eight use variable bit rates and Thunder is fixed
at 256 kbps. Their measured average rates and original file hashes are retained
in the [local analysis](../.reviews/radar-and-sound-memory-2026-10-13.json).
Estimated collection sizes are 5.30 MiB at 128 kbps, 4.08 MiB at 96 kbps and
2.82 MiB at 64 kbps, retaining any file already below the chosen target.
Lower bit rates are optional storage headroom, rather than a demonstrated
requirement for this collection. A listening comparison would be appropriate
before choosing compression; keep the already smaller singing-bowl file
unchanged. No re-encoding was performed during this feasibility check; the later
approved preparation is recorded below. The RAM budget
changes little because playback streams short chunks rather than loading entire
files.

Reading the Sound Machine Sounds chat confirmed the nine-recording/three-noise
design remains proposed. During this feasibility check no firmware source,
storage layout or audio file was changed; no build, flash, live heap measurement
or hardware check was run.

## Original firmware reference checkout removed — October 13, 2026

At the user's request, removed the unused `ESP32 Weather Alarm Clock Original`
checkout from Projects by moving it to macOS Trash. The active repository still
preserves `baseline-original-2026-10-07-d9d621a` at commit
`d9d621ac8e96e907bf962dfb8a31631ca4dba4e9`. The separate checkout was clean and
at that same commit before removal. No active build reference to the directory
was found. Its earlier build/comparison reports remain historical evidence.

After removal, verified the tag and all 18 MP3 hashes in the active project's
`shared/assets/sounds/originals/` and `shared/assets/sounds/clock/`. Those two
sets remain unchanged. No active firmware source, Git tag, build, flash or board
state was changed.

## Sound Machine recording conversion — October 13, 2026

Completed the approved recording preparation under the
[recording and loop decisions](architecture.md#sound-machine-recording-decisions).
The two retained audio sets are now inside the project:

- [Original recordings](../shared/assets/sounds/originals/): nine untouched MP3s
  for later re-encoding or reverting, totaling 8,336,084 bytes (7.950 MiB).
- [Clock recordings](../shared/assets/sounds/clock/): the nine final crossfaded
  MP3s for playback, totaling 5,653,510 bytes (5.392 MiB), 32.18% smaller.

All nine clock files were prepared directly from the original recordings, mixed
into mono and given the selected two-second constant-power tail/head blend.
Each was encoded once from temporary float PCM using LAME 4.0's 160 kbps average
preset and highest algorithm-quality setting, `-q 0`. Original sample rates
were retained: 24 kHz for Singing Bowl/Forest/Night and 44.1 kHz for the other
six. The preset's 0.95 gain reduction was compensated; there was no loudness
normalization. No earlier converted version was used as an encoding source.

All nine outputs decode fully to their original playable sample count minus
two seconds, in mono, with valid LAME delay/end-padding metadata and no
full-scale clipping. Independent macOS metadata checks agree with the playable
counts and padding values; read-only review confirmed the cyclic assembly and
frame/tag accounting. Repeat intervals are about 33 seconds for Campfire/Stream,
38 for Forest/Night/Rain/Wind, 43 for Ocean/Thunder and 47 for Singing Bowl.
The current 11 MiB SPIFFS image packs successfully with 5.082 MiB in empty
blocks after overhead. This is host verification, not board playback.

The user preferred the rain crossfade demonstration and said the finished
recording previews sound good. After comparing Singing Bowl with its original,
the user confirmed its natural strike-and-decay pattern and explicitly chose
to keep the crossfaded version unchanged. These listening decisions do not
verify the future ESP32 player's padding removal or buffering across repeats.
Final hashes, measured bit rates, exact sample counts, fade/wrap checks and
listening feedback remain in the [final validation record](../.reviews/sound-machine-crossfaded-2026-10-13.json).
Playback uses the prepared files without adding another crossfade; its approved
requirements belong in the linked architecture.

Cleanup at the user's request: moved both sets out of Downloads into the project
and verified all 18 original/final hashes after the move. Removed the 12 unused
listening WAVs and seven obsolete preview/conversion metadata or log files,
freeing 27.090 MiB. No alternate Sound Machine audio versions remain. The final
validation record is retained for integration; unused preview links and the
superseded conversion history have been removed from this work record. The
Sound Machine Sounds chat was notified of the corrected project paths.
No firmware source, maintained source path, build, flash or board playback was
changed or run by this asset preparation and cleanup.

## Daily Hail and Rain & Snow decision — October 13, 2026

Recorded the user's approval of the [additional daily picture rules](weather.md#additional-daily-pictures)
and current-only condition scope in the weather owner. The architecture points
to that owner, and the earlier integration analysis now marks its selection
proposal as superseded. Local links and preserved source files were checked.
Implementation remains deferred. No firmware, official mockup or canonical
artwork changes, software tests, builds, board work or weather API requests were
made.

## Visual Crossing sprint scope follow-up — October 13, 2026

The user approved including mapping/artwork revisions in the next weather sprint,
selected the simple quota behavior after restart, and deferred provider-credit
placement. These decisions are recorded with their [service owner](architecture.md#weather)
and [mapping owner](weather.md). At this checkpoint the extra daily picture rules
were proposed; their subsequent approval is recorded above. No implementation sprint was
started. Local documentation links and preserved source files were checked; no
firmware, official mockup or canonical artwork changes, software tests, builds,
board work or weather API requests were made.

## Visual Crossing quota recovery decision — October 13, 2026

Recorded the user's approved recovery policy in the
[weather service rules](architecture.md#visual-crossing-quota-recovery), replacing
the earlier proposed fallback. The weather mapping reference and local
integration analysis now point to that owner. Implementation remains deferred;
this decision does not authorize a new firmware sprint. Documentation links and
preserved source files were checked. No firmware, mockup or artwork changes,
software tests, builds, board checks or weather API requests were made.

## Daily forecast picture decision — October 13, 2026

The user approved reuse of the current daily forecast selection method for
both providers with a focused ordinary Rain/Showers qualification change. The
[selection policy](weather.md#five-day-forecasts) owns the approved details and
supersedes the earlier dominant-duration proposal. Documentation is updated;
implementation remains deferred. Local links and preserved source files were
checked. Firmware, official mockups and artwork were not changed, and no
additional provider requests, software tests or board checks were run for this
update.

## Documentation YAGNI and MECE audit — October 13, 2026

Completed a full review of AGENTS, architecture, weather and this work record.
Corrected the accepted Bell verification limit, separated complete weather
mappings from selection recommendations, identified Wind's new forecast branch,
retired completed sprint instructions and reconciled one [current work list](#current-work--reconciled-october-13-2026).
The earlier weather icon block is absent from the local architecture; it remains
in the last commit because the documentation changes are uncommitted.
The [audit report](../.reviews/docs-yagni-mece-2026-10-13.md) records findings,
ownership decisions and inspection limits. Firmware, official mockups and
canonical artwork were not changed. No firmware or board checks were run.

## Weather mapping reference — October 13, 2026

Status: documentation completed in [weather.md](weather.md), with the complete
provider mapping table, portable icon previews, source limitations and a separate
record of the unchanged current firmware mapping. The architecture, document
authority map and user guide link to that owner. Earlier local mapping reports
remain research evidence and point to the maintained reference.

Follow-up correction: removed misleading unresolved/pending mapping labels.
Generic Ice uses the white snowflake; Sleet is explicitly unused by these
providers. Shared pictures and complete compound/daily selection are defined
in the mapping owner. The remaining thunder code list was moved out of the
architecture, which now links to the weather classification. Independent
read-only inspection checked this ownership correction.

This is not a firmware implementation slice. Visual Crossing downloads,
credential persistence and revised icon/forecast rules remain unimplemented.
The new artwork is not integrated into canonical assets or official mockups.
Documentation links and provider-code coverage were checked; no firmware build,
software tests, authenticated provider request or hardware check was run for
this documentation update.

## Prepared Sound Machine sprint

Prepared and sent October 13, 2026. The user reviewed and approved the sprint,
including efficient, uninterrupted noise generation. **Dispatched to Development
Orchestration** (`01a11e52-7ce0-78f3-a2e5-efcb90c7aa16`); delivery was confirmed by
the task messaging tool. Implementation progress and evidence are owned by that
sprint. Dispatch is not implementation completion or board verification. Preserve
the existing weather work and its verification.

The [Sound Machine playback rules](architecture.md#sound-machine-playback),
[controls](architecture.md#sound-machine-controls),
[recording rules](architecture.md#sound-machine-recording-decisions) and
[official visual reference](../mockups/alarm-design.html) own the requirements.
The first-use volume is now 30%. All operating choices identified in the readiness
review are approved; decoder selection and buffer sizing are engineering work,
not further product decisions. This plan expands the existing audio and settings
implementation to deliver that approved behavior.

Implementation started in Development Orchestration on October 13 after the
originating human approval was verified. The first bounded slice adds ordinary
Sound Machine preference saves in existing configuration/test files; MP3 decoder
and packaging investigation runs alongside it. No new source path, board load,
commit or remote action was authorized by this implementation start. Detailed
local evidence stays under `.reviews/sound-machine-sprint-2026-10-13/`.

Coordination correction: the user's commit instruction preceded this sprint and
was completed as `246f4a3`. After context recovery it was mistakenly reused,
producing the additional partial commit `2ca209a` and an early stop. The user did
not request that second commit or a pause. The approved full sprint continues;
no further commit or board operation is authorized by this correction.

### Preference-storage checkpoint — October 13, 2026

Completed the preference-storage portion of slice 1 in existing
`device_config.c/.h` and `tests/test_device_config.c`. The three preferences use
typed NVS entries, approved defaults and validation, and the existing serialized
save/publication path. Saves preserve unrelated applied settings and pending
drafts; no playback state or deadline is stored. Ordinary partial-save behavior
remains unchanged. No new source file or storage mechanism was added.

Parent verification passed the configuration checks, the full 38-test native
suite (14.82 seconds), `git diff --check` and the ESP-IDF 5.5.5 firmware build.
The application image is `0x104c670` bytes, with 19% of its partition free.
These checks do not establish board behavior; this checkpoint was not flashed.
Logs remain under `.reviews/sound-machine-sprint-2026-10-13/`.

The read-only decoder investigation is complete. Its compatible-version and
recording-trim recommendations remain engineering evidence, not an installed
dependency or implemented playback path. Audio command ownership, recordings,
noise generation, timer/priority handling, production UI and board verification
remain unfinished. Next, integrate the compatible decoder and the existing audio
owner through the bounded slices below.

### Sound Machine source checkpoint — October 13, 2026

All five implementation slices are complete through software integration and
review. The existing audio worker now plays the nine recordings and generates
the three noises, owns the elapsed timer/fade and copied status, and preserves
alarm/preview priority and restart recovery. The existing settings and clock UI
owners implement the approved controls, immediate preference saves and shortcuts.
The replaced Settings-entry switch, retained Alarm Settings switch, title-case
clock headers and matching header spacing follow the approved reference.

Review corrected pending choices lost by closing/navigating during a save,
clock-shortcut Back navigation, stale playback status after preview preemption,
and editor controls/keyboard preservation while a Sound Machine save completes.
Only three desired preferences cross that existing save boundary; there is no
second applied configuration, countdown, audio worker or persistence mechanism.
Original artwork definitions and alarm tap areas remain intact. All changed
source paths already existed; no added or renamed source path required approval.

Parent final verification passed all 44 native checks in 16.11 seconds and the
ESP-IDF 5.5.5 firmware build. The application is `0x1067cd0` bytes, with 18% of
its partition free. Only the exact compatible decoder dependency was added;
unrelated locked components were retained. Existing main CMake removes that
package's redundant search flag that mishandles a project path containing spaces;
the managed package itself is unchanged. The generated 11 MiB recording image
contains exactly the nine prepared MP3s, independently reconstructed byte-for-byte
from SPIFFS data pages, with 5.082 MiB of empty blocks. Ordinary USB flash arguments
include it at `0x1410000`; NVS occupies separate sectors.

The audio worker remains at 6 KiB. Its compiled local frame is 2,608 bytes;
recording application buffers total 8,420 heap bytes, in addition to decoder,
stdio and codec storage. These are source/build facts, not measured runtime
reserve. Sound Machine's separate .66 maximum PCM gain retains the squared
volume curve and recorded relative levels while allowing for the codec gain;
the alarm's .82 gain is unchanged. Independent review and focused audio/UI
evidence are local under `.reviews/sound-machine-sprint-2026-10-13/`.

Board verification remains required: actual decoder output/counts and repeat
seams, all sounds/levels, maximum generation/refill/wrap timing, heap/stack reserve,
Stop/preemption, timer/fade, restart Stopped with retained choices, Day/Night
controls and sustained weather/network/display/touch activity. Host decoder
substitutes and short noise checks do not establish those results. This firmware
has not been flashed or committed; the prior coordination-error commit is
recorded above. The next step is a separately authorized flash and board pass.

The user also requested a format comparison. PCM would require 24.809 MiB;
diagnostic IMA ADPCM WAVs total 6.220 MiB versus the approved MP3s' 5.392 MiB.
ADPCM can use whole encoded blocks after a small loop-length adjustment during
preparation, but its quality still needs comparison. This is an evaluated option,
not an approved asset/format change. The current implementation uses MP3.

### Sound Machine board checks started — October 13, 2026

The user authorized flashing and beginning the board checks after the report-only
adversarial review. The reviewed production firmware, with no added measurement
probes, was flashed through the ordinary USB helper. Bootloader, partition table,
application and the nine-recording storage image each passed the flash tool's
written-data verification. No NVS erase was performed. Firmware/source hashes
still match the passing software checkpoint.

Saved settings were read from the board before flashing, excluding credentials
from reports: Digital, 12-Hour, Leading Zero Off, Day 100% / Night 25%, Night Shift
Off; Alarm Off, 07:00 Monday–Friday, Rising Tones, 75%, ten-minute snooze;
Visual Crossing with a configured key, Imperial, Leander. Sound Machine keys were
absent, so the approved first-use choices apply. These actual saved values are
the restoration baseline for any later test edits.

The user confirmed clock/weather/touch work, but reported Rain cutting out about
every two seconds and occasional plain blue flashes while changing Volume.
Untouched Rain and White Noise both cut out at 50%, so MP3/file decoding alone
cannot explain the common failure. Rain is quiet at 30% and 100%; White Noise is
much louder. Navigation/shortcut/Stop were not separately confirmed. These are
failed playback checks, not a completed board checkpoint.

Serial capture has no panic so far. The installed display driver describes blue
flashes as a possible external-memory underrun; the actual cause is not yet
established. This build keeps caches enabled during ordinary flash operations,
so cache disabling is not assumed. Temporary measurements in existing `audio.c`
will separate control copies, reads, decode/generation and speaker feeding;
report after playback stops, then remove probes after the investigation. No new
source path, audio worker or save mechanism is proposed. The level complaint is
being assessed separately against the full recordings and codec gain.

The temporary measurement build passes all 15 existing audio checks and the
ESP-IDF 5.5.5 build (`0x1068340`, 18% application partition free). Independent
inspection verified exact probe removal restores the reviewed audio source.
Counters report after Stop/cleanup; instrumented sound-change and alarm-preemption
timing is excluded because the report can delay opening the replacement sound.
These first probes locate the common cutouts, rather than establish complete
decoder repeat/sample or long-duration acceptance. The application-only diagnostic
flash passed written-data verification, preserving the already verified recording
image and NVS. Serial capture is active; untouched Rain/White Noise runs followed
by Stop and a Stopped-volume drag comparison are awaiting user observations.
This is a measurement load, not a claimed correction.

At priority 4, the measured Rain maximum preparation gap was 69.7 ms and write
interval 108.5 ms. White Noise reached 53.5 ms inside its fixed generation loop,
64.2 ms preparation and a 264.1 ms write interval. Stage times include time spent
waiting for CPU/other work; they do not prove those durations are computation.
The user narrowed White Noise cutouts to Analog display and saw no blue flash
during Stopped-volume dragging. The worker's observed lifetime stack reserve was
1,140 bytes, and recording cleanup released its buffers; this is bounded-run
evidence, not full resource acceptance.

A one-line priority comparison raises the existing audio worker from 4 to 7,
ahead of the LVGL drawing task at 6, with the same diagnostic probes, buffers,
sounds and volume. Actual IDF P4 floating-point use pins a task to its current
core; the ordinary unpinned creation does not guarantee migration during playback.
This comparison tests execution-time competition before adding buffering or
changing persistence. All 15 audio checks and firmware compilation pass. The
native hardware-call assertion follows the priority change; it does not prove
real scheduling. The application-only comparison load passed written-data
verification after stopping a serial capture that interfered with its first
attempt. Capture and flashing now have exclusive serial access. Retain the
priority change only after checking audio, clock, touch and display behavior;
blue-flash cause and louder recording correction remain open.

The priority-7 comparison failed: the user reports occasional cutouts despite
some improvement, occasional skipped Analog seconds, and blue flashes during
Volume changes. Its 15-minute capture expired before the user's run, so these
observations have no matching timing counters. The capture limit will be longer
for the next pass. Priority 4 and its native assertion are restored in source;
the next measurement load will restore that on the board. Priority 7 is not an
accepted fix.

Direct source inspection identified the actual triple-partial display path's
whole-frame C2M writeback and M2C invalidation: each addresses 1,280,000 bytes.
The installed SDK holds a critical section during each whole-range cache
operation with C2M chunking disabled. This is a credible shared-delay candidate,
not measured proof; full range does not mean every byte is physically dirty.
The next bounded measurement wraps the existing cache API in temporary code
within `audio.c` and its existing CMake link options, preserving real arguments
and results. It will record duration/direction/core/caller and report after
Stop, with task affinities. No SDK/library edits, new source path, worker,
buffering scheme, cache setting or gain change is selected yet.

The cache-call measurement build passes the 15 existing audio checks and the
ESP-IDF 5.5.5 build (`0x1068bb0`, 18% application partition free), with audio
priority restored to 4. Compiled-image inspection confirms the temporary
wrapper and timer are in internal instruction memory, its counters/lock are in
internal data memory, and both the display helpers and I2S completion callbacks
use it. The real call runs outside the probe's short lock; durations include
preemption and do not directly measure time with interrupts disabled. Reports
are emitted only after Stop/cleanup. Removing the marked block and probe calls
restores the reviewed production audio hash. The application-only measurement
load passed written-data verification, preserving storage/NVS. Its one-hour
serial capture is active for the requested matched Analog Rain/White Noise
runs and a separate playing-volume drag. Observations and timing results are
pending; this is not a fix. Independent source review found no actionable
wrapper defect. I2S auto-clear also calls the same SDK cache API from its
completion interrupt, using a separate audio buffer, so a slow small-buffer
call could indicate waiting behind display work. That connection still needs
measurements; a maximum records elapsed call time, not the entire event sequence.

The first cache-probe runs failed: Rain cut out with each Analog second-hand
update; White Noise was similar, with occasional skipped hand positions. Volume
dragging glitched sound without a blue flash on that run. Stop rebooted the board
three times. Captured assertions identify a diagnostic-only defect: the probe
queried the 16-character heartbeat name, while FreeRTOS requires lookup names
shorter than its 16-byte task-name limit. Native substitutions and the source
review did not catch this SDK precondition. Audio and LVGL affinity reports
confirmed both on core 0, but the assertion prevented all cache timing reports.
The capture was stopped deliberately before reloading. The probe now uses the
SDK-truncated heartbeat name with a compile-time length guard. All 15 audio
checks and firmware compilation pass (`0x1068bc0`, 18% application partition
free); the native checks do not execute the firmware-only diagnostic branch.
The application-only replacement passed written-data verification. A fresh
one-hour capture is active, with one 30-second Analog/White Noise run and Stop
requested to check the diagnostic correction and collect the lost timings.
Physical Stop verification is pending. The correction does not address the
original cutouts, whose cause remains unconfirmed.

The corrected probe's White Noise run captured about 18 seconds of measurements;
the user confirms Stop no longer reboots and audio still glitches with the
Analog hand. Audio, LVGL, UI heartbeat and clock all report core-0 affinity.
Audio remains priority 4, while LVGL is 6. Maximum elapsed generation is 28.4 ms,
preparation 31.5 ms, write interval 188.1 ms and blocking write 178.9 ms. The
largest recorded C2M latency is 27.7 ms for a 512-byte call from I2S writing;
whole-frame M2C invalidation is at most 32 microseconds. These elapsed calls
include preemption and shared-lock waits; the maximum-only buckets do not
separately retain large-frame C2M costs or establish a cache-work cause.

A bounded comparison creates the same audio worker on core 1, retaining its
priority 4, stack, buffers, playback and settings. The SDK pins tasks after first
floating-point use, so unpinned creation did not keep audio mobile. Speaker/I2S
initialization occurs in this worker; its lazy completion interrupt allocation
also follows the calling core. Thus this compares audio execution and completion
handling away from drawing, rather than separating those two causes. Shared
memory and cache locks remain shared. Only existing audio task creation and its
native substitutes change. All 44 native checks pass in 17.81 seconds and the
firmware build passes (`0x1068bc0`, 18% application partition free). Parent
inspection preserved the ordinary task-create declaration used by other native
checks, verified the new SDK signature/core assertion, and confirmed removing
the probes leaves only this single audio task-creation change. The previous
capture was stopped before the application-only comparison load, which passed
written-data verification and retained recording storage/NVS. A fresh one-hour
capture is active for matched Analog/White Noise and Rain runs plus playing
Volume dragging. Physical results and measured core-1 affinity are pending.
This is a placement experiment, not an accepted correction. A read-only level
assessment continues separately; no recording gain or asset has changed.

The core-1 comparison improves the observed periodic audio failure: the user
hears no cutouts during the White Noise/Rain checks. Captured White Noise runs
30.9 seconds with maximum generation 473 microseconds, preparation 506
microseconds and write interval 23.4 ms; longest write is 23.2 ms. Audio affinity
is 1 and LVGL/UI heartbeat/clock remain 0. Rain includes a 71.5-second run with
normal writes at most 23.4 ms and cache calls at most 206 microseconds. Retain
this tested placement while remaining failures are investigated; this is not
complete Sound Machine acceptance. Occasional Analog hand skips remain, with
their introduction unresolved. Volume changes still cause a whole plain-blue
screen; their sound effect is uncertain. The capture has no DSI underrun error
or panic. Later source inspection found that the display's underrun interrupt
is masked below a configured line count; the absence of that log does not rule
out missing display data.

That Rain run crosses one repeat and records a 131.4 ms combined rewind/decoder
reset, producing a 155.3 ms write interval. This exceeds the source-derived
35 ms Rain DMA reserve even on core 1; an audible repeat gap was not separately
confirmed. Existing temporary probes now measure actual `fseek` and decoder
reset calls separately, preserving all calls, results, gains and sample counts.
The split measurement is being tested/built but is not loaded yet. A manual
unsaved Brightness drag while White Noise plays is also requested to distinguish
ordinary drawing from Sound Machine's saves/gain changes, without changing
settings. Blue-flash cause, repeat delay and recording loudness remain open.

The user reports no blue flash while dragging unsaved Day Brightness during
White Noise. This narrows the comparison but does not prove storage is the
cause. The next temporary firmware preserves Sound Machine validation,
serialized updates/publication, controls and playback while returning success
before NVS access for that group only. Its Sound/Volume/Timer test choices apply
in memory and deliberately do not survive restart; other save groups remain
ordinary. This limitation was disclosed before implementation. Restore normal
Sound Machine storage immediately after the comparison and before closure.
The same build splits actual file-seek and decoder-reset costs. All 16 focused
audio/configuration checks and firmware compilation pass (`0x1068c60`, 18%
application partition free). Native checks exercise ordinary storage, excluding
the firmware-only diagnostic bypass; the RAM comparison requires board evidence.
Removing that guarded block restores the exact prior configuration source hash,
and removing audio probes restores only the tested core-1 task-creation change.
The application-only comparison load passed written-data verification. The user
reports no blue screen with its temporary RAM-only saves, generally continuous
sound and occasional volume-slider lag. Rain still has an audible dropout about
every 38 seconds. The captured completed Rain run lasts 129.6 seconds and crosses
three repeats: backward file seek takes up to 132.1 ms, while decoder reset takes
22 microseconds. No separate completed White Noise timing report was captured.
This identifies file seeking as the repeat delay and connects the blue flash to
the storage path; it does not yet identify which storage operation causes it.
The temporary bypass was then removed, restoring the exact prior configuration
source. The currently loaded build saves Sound Machine preferences normally.

The next bounded correction retains each recording's filename and closes/reopens
that same stream at its exact sample boundary, using SPIFFS's cached filename
lookup instead of the slow backward seek. Decoder reset, joined tail/head chunks,
sample counts, cancellation and the existing explicit-retry error policy remain.
Native checks cover two repeats of every recording with simulated PCM, stream
open/close accounting, returned reopen/close failures and Stop during reopening;
they do not establish real decoded sound or hardware refill speed.

That build also includes a fixed one-second UI heartbeat, with work included in
the period and no replay of missed frames after an overrun. All 44 native checks
pass in 16.84 seconds and the ESP-IDF 5.5.5 build passes (`0x10694d0`, 18% app
partition free). The application-only flash passed written-data verification.
The new capture deliberately restarts once to record display initialization.
Temporary diagnostics time NVS flash operations without recording their contents
and change the display's missing-data fallback pixel from blue to black. A black
flash would identify this hardware fallback; this marker is not a proposed visual
fix and must be removed. Startup confirms the original pixel value is `0x3fff`,
the diagnostic value is zero, the interrupt-discard count is 800, and memory
read/write priorities are both zero. No memory priority has been changed.
Normal storage is restored. On October 14 the user reports no flash during
White Noise volume changes and no heard dropout during the roughly one-minute
Rain repeat check, but a blue screen during Rain volume changes. The hand was
not separately reported in that response; its earlier sound-off skips remain
open. The one-hour capture had already completed before these later tests, so
there are no matched repeat or flash-operation timings for those observations.

Further SDK review found a flaw in the temporary color diagnostic: DPI register
changes require the existing configuration-update helper. Reading back zero
without that apply step does not establish the active fallback color. The next
comparison calls that helper after setting the marker and prints its register
value and existing interrupt configuration at Stop. Persistent blue in the
earlier test does not rule out display starvation. No interrupt masks, memory
priorities, display speed or normal appearance are changed by this diagnostic.

Read-only level assessment recommends a Rain-only 1.7x playback multiplier as
the smallest clean increase, approximately 4.6 dB with about 1 dB predicted
codec headroom. It would preserve file bytes/blends/counts and all other sounds,
but still leave Rain far quieter than White Noise by RMS. This is an unimplemented
recommendation under the user's loudness request, not a new product requirement
or established acoustic result. A larger adjustment needs transient control and
listening; no normalization framework or re-encoding has been selected.

The previous UI heartbeat delayed one second after completing its work and
could wait another 250 ms for the display lock, allowing accumulated drift.
The loaded comparison includes the fixed-period correction described above.
The user still reports occasional two-second Analog hand jumps with sound
stopped. Actual sampling/drawing timing remains unmeasured; this remains open
independently of audio, and source review is checking the remaining delays and
the relationship between the update schedule and clock-second boundaries.

The corrected-marker comparison is built (`0x1069890`, 18% application
partition free) and its application-only written hash is verified. A fresh
two-hour capture starts without an extra reset. The matched Rain run lasts
112.7 seconds and crosses two repeats: same-file reopen takes at most 254
microseconds and total repeat reset 284 microseconds, compared with the earlier
132 ms backward seek. Audio write interval is at most 35.0 ms and the user hears
continuous sound. During volume changes the user now sees flashing described as
probably black, definitely not blue; Stop confirms marker zero and the unchanged
interrupt-discard setting. The display-starvation explanation is supported,
without using absent interrupt logs to rule it out. The same run records 154 NVS
reads (maximum 7.4 ms), 66 writes (maximum 2.6 ms) and no erase. These are elapsed
individual operations, not a complete save duration or exact per-flash timeline.
No cadence-anomaly or lock-failure line is captured, and the user sees no Analog
hand skips in the requested two-minute sound-off observation. This is bounded
hardware evidence, not a claim that skips can never recur.

Settings inspection confirms one active save and one retained latest desired
Sound Machine choice; there is no unbounded save queue. A later desired value
waits for the existing completion poll before the next save. The connection to
storage remains credible, but a growing queue has not been established. A
separate SDK check rejects enabling cache-safe display interrupts on the assumed
cache-disable theory: current instruction/constant-data PSRAM settings select a
flash path that does not disable those interrupts. No such option is changed.

The next bounded comparison requests a higher display memory-read QoS value
through the existing SDK helper in `main.c`, from zero to one, preserving that
port's write value and other read values. It affects all reads through the
display's DW_GDMA memory port, including descriptors. The SDK names it a priority
setting but does not establish exact arbitration guarantees; this is a trial,
not a proven correction. It adds less machinery than interrupt wrapping, while
possibly delaying other memory users. Retain normal saving and the current
audio/clock changes, compare Rain volume changes and two repeats, and check audio,
touch, clock cadence and normal weather. Revert if the flash persists or other
work worsens. The build passes (`0x1069970`, 18% app partition free), and its
application-only write is hash-verified. The immediate no-reset capture records
the requested read field changing from `0x0` to `0x10000`, with writes still zero,
and a successful ordinary Visual Crossing startup response (`200`, `queryCost=1`).
Its early three-second clock jump occurs with a one-second update interval and
short snapshot/lock waits, shortly after network connection; that is consistent
with startup time correction, not evidence of a three-second rendering stall.
No matched volume comparison result has been requested for this priority trial.

The user questioned treating the symptom and briefly offered final-volume saving
at the end of a touch as a less-responsive fallback. That comparison was prepared
and host-checked but never flashed. The user then canceled it: even one flash is
a defect and root-cause investigation should continue. Only those trial edits
were reversed in Settings and its tests; the pre-existing live volume, ordinary
saving and Sound Machine work are restored. The parent rebuilt the native suite
after restoration: all 44 checks pass in 16.69 seconds. No product requirement
was changed. Saving only at release is no longer the intended correction.

The read-priority trial is also removed from source. The next load restores its
original value; current board priority remains the disclosed trial until then.
The next diagnostic measures actual display frame-completion callback intervals
alongside the existing NVS operation timestamps, preserving the adapter's
callbacks and userdata and reporting only after Stop. It uses the existing
diagnostic block and public SDK registration path, not a copied driver or new
worker. This is intended to distinguish missing frame-feed progress from an
ordinary UI update or storage queue. Corrected marker, audio/core placement,
repeat handling and cadence remain as described above. The diagnostic build
passes (17,217,760 application bytes, 18% partition free; SHA-256
`9a675a19282f7f7c0f6e8781cd7db5565080913dc77f375b59a7a3a7506a0c82`).
Parent source/ELF checks verify the intended call interception, internal-memory
interrupt helper/state placement and removal of the read-priority setter.
The existing callback slots, user data and callback return behavior are retained.
This adds approximately 5.8 KiB of fixed internal diagnostic data, with no new
task, driver fork or source path. It records an initial frame interval baseline,
the longest gap, eight recent above-baseline gaps and 128 completed NVS/cache
check spans. These are software DMA rearm callback intervals, not physical panel
VSYNC measurements; overwritten or unfinished storage spans limit correlation.
All reports run after Stop. The application-only flash is hash-verified and its
immediate no-reset capture confirms normal startup, original display read/write
priorities of zero, diagnostic marker zero and a successful ordinary Visual
Crossing response (`200`, `queryCost=1`). Serial evidence is local under
`.reviews/sound-machine-sprint-2026-10-13/board/frame-trace-*`.

The matched 26.13-second Rain/volume run confirms continuous sound with dark
flashes. The callback probe installs successfully and records 2,806 frames.
Before storage activity, 1,223 intervals average 9,240 microseconds with a
9,277-microsecond maximum. The longest interval is 20,210 microseconds and
overlaps four individual NVS reads; the retained recent enlarged intervals also
overlap reads/writes. The run records 77 reads (maximum 8,420 microseconds),
33 writes (maximum 3,170 microseconds), no erase and 33 cache checks with no
actual mapped-flash invalidation. Cache synchronization maxima are 36
microseconds on core 0 and 29 on core 1. All measured NVS access is on core 1;
frame callbacks are on core 0. Audio first-fill internal memory is 39,863 bytes
free, largest block 23,552; after closing it is 77,195 bytes free. No panic or
unexpected restart is captured in this run. These timings narrow the storage
connection; they do not identify a hardware stall versus delayed CPU service or
timestamp the visible flash directly. The report retains eight of 43 above-
baseline intervals and 128 of 143 completed storage/check spans, so discarded
history remains an explicit limit.

Source inspection of the selected GD flash read path finds no deliberate sleep
or busy-wait for a previous write in its 32-byte read; a single host transaction
is enough. The next temporary diagnostic in existing `audio.c`/CMake times host
configuration after the SPI1 mutex, the host read and command-completion polling.
This will split the surprising 5–8 ms small-read elapsed time without changing
normal saving, driver behavior, priorities or display refresh speed. The build
passes (17,216,528 bytes, 18% partition free; SHA-256
`3cb865fd7e5d6487e96c1819191941b7090aea4d8157bd36e3d6634fc484d0b1`).
Parent verification confirms all three flash-host function pointers use the
timing wrappers and required helper/state placement in internal memory. The
production audio reconstruction is unchanged. All 15 affected audio checks pass
in 1.01 seconds after rebuilding; the preceding full 44-check pass remains.
The narrower diagnostic retains 16 storage spans rather than 128, releasing
4,480 bytes; its small read-stage/heap records add 772 bytes, for a net 3,708-byte
fixed-data reduction. It measures free/largest DMA-capable internal blocks at
first fill and after Stop, separately from total internal memory. The
application-only flash is hash-verified, with immediate no-reset capture confirming
normal boot, original zero display priorities, temporary black fallback marker
and a successful startup Visual Crossing response (`200`, `queryCost=1`).
The matched 27.63-second Rain-volume test again confirms dark flashes with
continuous sound. Frame callbacks average 9,239 microseconds before saving,
with a 9,261-microsecond baseline maximum and a 19,885-microsecond overall gap.
The longest 32-byte NVS read takes 8,461 microseconds: 8,113 elapse before host
configuration, whereas the actual host read takes 88 and command polling 16.
No read scope is skipped. Most of this measured delay precedes the hardware
read; it combines initial API work, allocation, mutex waiting and scheduling.
At first fill, DMA-capable internal free/largest is 2,331/608 bytes, despite
41,891 total internal free and a 31,744-byte largest internal block. After Stop,
DMA-capable free/largest is 39,683/28,672. The preceding five periodic weather
responses on this image succeed before audio; post-audio overlap is unverified.
Evidence remains local under the same board directory's
`read-stage-*` files. All temporary diagnostics must be removed for final closure.

Read-only UI review confirms successful ordinary volume changes neither rebuild
nor clear the Sound Machine page; they update small labels/notices. A volume
change also does not reopen the recording/codec or reset the timer. The existing
RAM bypass is a useful storage-path comparison, but its changed operation timing
and unavailable exact temporary patch limit an assertion of perfect isolation.
Probe bookkeeping itself shares a cross-core lock, so callback/cache timings
can be perturbed; it did not introduce the earlier uninstrumented flash.

The next precise lead is SDK temporary-buffer allocation: a non-main-DRAM read
destination, including a valid internal RTC-RAM task stack, requires a scratch
buffer. The SDK first walks internal heap blocks to find the largest free block,
even for a 32-byte read, while holding the heap's critical-section lock. The
8 KiB Save stack could occupy RTC RAM under the measured pressure, but its
placement is not yet observed. The next small diagnostic records destination
classification and the nested heap-walk duration within the existing NVS-read
scope. No production allocation, driver, save or display policy is changed.
The extended build passes (17,217,584 bytes, 18% partition free; SHA-256
`d9fe4d043c9da9c082ea96734582dc04343cc9788a91d3aad811ae2b968d9781`).
Parent source/ELF checks confirm the SDK temporary-buffer allocator reaches the
wrapped largest-block lookup, with required probe helpers/state internal and
original arguments/return values retained. The extension adds 320 fixed bytes;
production audio reconstruction remains unchanged. All 15 rebuilt audio checks
pass in 1.38 seconds. Application-only write verification and normal startup
pass, with zero display priorities, the temporary black marker and ordinary
Visual Crossing weather success (`200`, `queryCost=1`). The matched 22.11-second
Rain-volume run again has dark flashes and continuous audio. Its longest 32-byte
read takes 8,625 microseconds; the destination is `0x50109c3c`, confirmed RTC RAM
rather than main DRAM. One nested largest-block lookup takes 7,987 microseconds,
versus a 90-microsecond host read and 17-microsecond command poll. That confirms
the slow-read mechanism in the SDK's temporary-buffer path. It does not yet
prove the complete chain to visible display starvation. Frame baseline is
9,240 microseconds, maximum 20,348; first-fill DMA-capable free/largest is only
1,831/176 bytes, then 39,187/27,648 after Stop. The measured placement is consistent
with a late Save stack using RTC RAM; destination classification alone does not
establish every allocation's ownership. New evidence uses `heap-walk-*` in the
existing local board directory.

The next disclosed one-variable comparison changes the existing SDK internal
reserve from 32 KiB to 64 KiB in `sdkconfig.defaults` and the active generated
configuration. This aims to leave room for the late 8 KiB Save stack without
the RTC-buffer scratch/heap-walk path. It preserves immediate volume and ordinary
saving, tasks, codec buffers, priorities, display rate and driver code. The
tradeoff is changed ordinary allocation placement/fragmentation, potentially
using more PSRAM; the reserve is not a hard guarantee. Its added 32 KiB is a
comparison budget, not yet an accepted permanent requirement. Acceptance needs
main-DRAM NVS destinations, absent nested heap walks, shorter reads, no observed
display flashes, continuous audio and healthy weather/network behavior. Do not
select it merely because aggregate free memory improves. The firmware build
passes (17,217,584 bytes, 18% app partition free; SHA-256
`f23de22b65712077b74face502fcbea44654319da16ffa2a4145757cf4f501ec`).
The generated configuration confirms 65,536 reserved bytes and production audio
is unchanged. The earlier full 44-test pass and latest rebuilt 15 audio checks
are retained; native tests do not execute SDK memory placement. Application-only
write verification and normal boot pass; startup prints the 64 KiB reserve,
original zero display priorities and ordinary Visual Crossing success (`200`,
`queryCost=1`). The matched volume check and an ordinary weather refresh during
playback are requested and remain pending. Evidence uses `reserve-64-*` in the
existing board directory. No driver fork, allocator ABI override, extra storage
worker or new source file is added. The comparison and all diagnostic probes
remain provisional until their effects and normal-firmware behavior are verified.

The 64 KiB reserve comparison failed: the user still observes dark volume flashes
and continuous audio, then reports failed weather during the longer playback.
The first run lasts 123.64 seconds, with first-fill DMA free/largest 9,663/6,912
bytes. The largest block is still smaller than the 8,192-byte Save stack; NVS
destination `0x5010bc94` remains RTC RAM, with a 6,939-microsecond heap lookup in
an 8,588-microsecond read and a 19,182-microsecond maximum frame interval.
The subsequent 697-second playback crosses 18 repeats, but TLS requests fail
certificate-signature verification with `0x4290`: the SDK's RSA-public error
plus a big-integer allocation failure. This is not evidence of a bad provider
certificate or daily quota. CPU-0 idle watchdogs and, after Stop, an AES alignment
allocation failure are captured. The longer audio write interval reaches 120 ms;
the user's continuous-audio answer for the volume check does not verify every
part of the later weather run. Source and generated configuration revert to
the original 32 KiB reserve; the physical board remains on the failed trial until
the next load. Evidence uses `reserve-64-result.json` and adjacent logs.

The next disclosed comparison changes only the ordinary allocation preference
threshold from 16 KiB to 2 KiB at the original reserve. Allocations larger than
2,048 bytes prefer PSRAM; smaller ones remain internal-preferred, with the SDK's
existing fallback behavior. Concrete sizes justify this boundary: MP3 input
2,048 and PCM 1,764; compiled SPIFFS descriptors 240, cache 1,400 and work 512
bytes for five 256-byte pages. The 4,608-byte decoded buffer and larger ordinary
codec allocations instead prefer PSRAM. This targets software memory pressure
without intentionally moving the flash-read cache out of main RAM. It adds one
existing SDK configuration value, with no helper, ABI override, driver change or
new source path; volume/save behavior remains. More PSRAM/cache traffic and
possible allocation fallback require measurement. Parent ELF structure checks
independently verify those filesystem sizes. The firmware build passes
(17,217,584 bytes, 18% app partition free; SHA-256
`becaf5bd49ae8312b478cee3aaa469fb93b4b7ff921e4bf0b24bde6db4da3022`).
Generated configuration verifies the 2,048-byte threshold and original 32,768-byte
reserve. Production audio reconstruction remains unchanged; preceding native
evidence is retained because host tests do not exercise this SDK allocation
preference. The application-only load is hash-verified and the board boots with
the original 32 KiB reserve; its first Visual Crossing request returns HTTP 200.
The serial capture remains active (PID 99162). NVS placement/scan, visible
flash, audio and weather overlap checks remain pending. TLS allocation placement
remains unchanged; its separate SDK option is not part of this trial.

The 2 KiB comparison improves the measured volume-save path. The user reports
the short test seems to work correctly. Its 99.45-second capture shows the
longest NVS read at 809 microseconds, destination `0x4ff35944` in main DRAM,
and zero nested largest-free-block lookups. Display intervals average 9,239
microseconds and peak at 9,279 rather than the earlier roughly 20,000.
The subsequent 533.91-second Rain run crosses 14 repeats with a 9,292-microsecond
maximum frame interval; a 34,175-microsecond NVS erase also occurs without a
measured frame delay. The remaining probes and changed allocation preferences
still require removal and normal-firmware verification before closure.

Weather overlap remains unsuccessful. A request made while playback is Stopped
returns HTTP 200, but the following request during Rain fails to allocate AES
output transfer descriptors. A retry after Stop fails an AES start-alignment
buffer allocation. Neither reaches an HTTP quota response. First-fill DMA
free/largest values are 35,563/27,648 bytes, and closed values 41,091/27,648;
these are snapshots outside the failed handshake, not proof of adequate memory
at its peak. The raw AHB-descriptor capability totals of zero are not evidence
that no descriptor memory exists: the SDK translates that capability into DMA
when allocating, while these raw heap-statistics calls do not. Actual AES
allocations request DMA-capable internal memory. Exact peak pressure remains
unmeasured. Evidence uses `prefer-2k-result.json` and the active runtime log.

The user's question about excluding the 32 KiB RTC area was checked without
changing that setting. IDF's existing `ESP_SYSTEM_ALLOW_RTC_FAST_MEM_AS_HEAP`
switch controls whether it contributes to dynamic allocation. Disabling it
would remove the current 32,608-byte RTC heap and prevent RTC task-stack
placement, but would not add DMA capacity; ordinary buffers could instead
fall back to PSRAM or internal-only allocations could fail. This is a supported
option, not an implemented fix or proof that the weather failure is resolved.

The next disclosed comparison retains the 2 KiB ordinary preference and original
32 KiB reserve, changing only the SDK TLS allocation choice from Internal to
External. The compiled connection allocates 16,717-byte input and 4,429-byte
output buffers plus 948 bytes of initial handshake objects, before certificate,
key, cipher and allocator costs. Moving TLS software allocations to PSRAM
targets demonstrated hardware-buffer allocation failures without changing
content limits, certificate checks, crypto selection, audio ownership, settings
saves or weather retry timing. AES transfer descriptors still need internal DMA
memory; aligned data buffers follow their source memory placement. This is an
existing SDK option, not a custom allocator or new source file. More external
memory/cache traffic must be tested. Acceptance requires ordinary weather
success during Rain, stable live-volume display timing and responsive continuous
playback. Exact peak pressure and the final production configuration remain
unverified; the comparison must not be treated as a completed fix.

The external-TLS comparison builds successfully with ESP-IDF 5.5.5
(17,217,584 bytes, 18% application partition free; SHA-256
`ea3d23275dfe4e46a897ccf7495a7713a8b6d39d6215ae17a8bd76369948a8a2`).
Generated configuration confirms External TLS, the 2,048-byte ordinary
preference and 32,768-byte reserve. Parent ELF inspection confirms the TLS
allocation function requests PSRAM and byte-accessible memory. Production audio
and configuration-owner hashes remain unchanged. Native checks retain their
earlier recorded results because they do not execute this hardware allocation
choice. Application-only flashing is hash-verified; startup and the first
Visual Crossing request pass (HTTP 200). Capture PID 18202 remains active.
Playback/volume and ordinary weather-overlap checks are pending; evidence uses
`tls-external-*` in the existing board review directory.

The user confirms the two later software restarts on the 2 KiB-only comparison
were intentional. On the external-TLS comparison the short volume check looks
correct, and the 834.09-second Rain run crosses 21 repeats and the next ordinary
weather request. That request returns HTTP 200 with no recorded allocation
failure. Short-test display intervals peak at 9,278 microseconds, with a main
DRAM NVS destination and zero nested heap scans. The longer run has one
17,091-microsecond maximum frame interval near weather-request startup; no
separate visible result was supplied for that moment. Do not claim universally
uninterrupted rendering from these measurements. The board result and limits
are in `tls-external-result.json`.

Removed all temporary audio/cache/flash/HAL/heap/display measurements, their
linker wraps and temporary dependencies, the diagnostic display fallback-color
write and clock-cadence instrumentation. Retained only the established core-1
audio placement, same-file repeat reopening, fixed one-second heartbeat and the
two SDK memory-placement choices. RTC heap inclusion and the original 32 KiB
internal reserve remain unchanged. Parent hashes independently match production
audio and its normal CMake file; scoped independent review finds no consequential
defect or added owner/allocator/storage mechanism. The existing configuration
files own the settings, with rationale in the architecture.

After removal, all 44 native checks pass in 17.98 seconds and ESP-IDF 5.5.5
builds normal firmware (17,202,672 bytes, 18% application partition free;
SHA-256 `9745ce89cad0ddcdd67d9a4e9cc2bc82c1e2ad101a6fcab07c72d6f2e72f4df9`).
Normal firmware is now loaded with application flash hash verification, and
its first Visual Crossing request succeeds (HTTP 200). Serial capture PID 35796
remains active. The final Analog/audio/live-volume/weather-overlap check remains
pending. Diagnostic results do not substitute for that retest.
Rain loudness and broader Sound Machine sound/timer/fade/navigation/priority
checks remain separate; no gain or asset change is included in this correction.

On normal firmware the user reports Analog/Rain repeats, live volume changes,
hands and touch appear to work. The separate 30-second White Noise check was
questioned rather than performed; it remains unverified. The next ordinary
weather request returns HTTP 200 with no allocation failure, watchdog or
restart, but the user thinks Rain cuts out around that update. Weather/audio
overlap therefore remains open. A successful response is not evidence of
continuous speaker output. The 32 KiB RTC heap remains enabled; no exclusion
was approved or implemented during the user's questions.

Review found a remaining older weather resource-measurement helper, outside
the removed audio/main probes. Each download queries the largest free blocks
in Internal and PSRAM twice, totaling four heap walks. The SDK holds a critical
heap lock during each walk, delaying local interrupt service and potentially
other users of that heap. This is a credible candidate under the reported
cutout, not a measured cause: the scan's core/duration and queued audio at
that moment are unknown. The next disclosed cleanup removes only this helper,
its two calls and its otherwise-unused includes from `weather_client.c`.
Normal provider/status/result logging, download/decoding, retries, settings,
audio buffering and priority remain. Affected native weather checks, the
firmware build and another Rain/ordinary-weather overlap check are required;
the redundant short White Noise/navigation test is skipped for now.

This cleanup removes 25 source lines and adds no source path, task, queue or
allocator. Parent diff inspection verifies that the normal provider/status log
and all weather operations remain. The affected native weather-service check
passes (0.78 seconds), and the ESP-IDF 5.5.5 build passes (17,202,272 bytes,
18% application partition free; SHA-256
`b4a3451b5aabc48962d292253843ffe0462d7b1be2e1ed3d9c62c1e788af52a2`).
The preceding full 44-check result remains recorded separately; host checks
do not establish interrupt timing or continuous audio. Load and the matched
Rain/ordinary-weather check are pending, with evidence under `no-weather-scans-*`.

The no-weather-scans image is application-flash hash verified; startup weather
returns HTTP 200, with capture PID 46203 active. The user then reports that the
clock stops updating during weather sync and its second hand jumps several
seconds. Whether this refers to that image's startup or the preceding ordinary
refresh is being clarified, along with whether audio was playing. It is a
reported defect, not a passed overlap result.

Read-only tracing finds no direct wait for weather HTTP completion in the
heartbeat. Weather requests/decoding occur outside the runtime-state mutex;
publication copies the completed payload. Time-owner locks are short state
copies with RTC I/O outside them. The heartbeat nevertheless skips an update
if its once-per-second drawing-lock attempt fails after 250 milliseconds.
Consecutive missed attempts can produce a multi-second hand jump. This is a
concrete possible mechanism, not evidence that it caused the observed event.
Message snapshot retrieval also has an unbounded owner-lock wait, without a
demonstrated connection to this weather event. Do not label it the cause.

The next bounded measurement adds temporary tagged timing lines only to the
existing `main.c` heartbeat. It records scheduled-versus-actual wake, configuration
copy, owner-snapshot, drawing-lock outcome and UI-update durations, plus monotonic
interval and UTC steps. Log only exceptional updates after releasing the drawing
lock; there are no heap scans, driver wraps, new tasks or source paths. Preserve
scheduling, the lock timeout, rendering and product behavior. Exact removal
reconstructs the pre-measurement source hash recorded in `ui-wait-checkpoint.json`.
Preparation is not a board result; the measurement still needs a build and a
matched event. Remove it after diagnosis, and distinguish object-update timing
from physical frame completion.

The user confirms the hand stall was during an ordinary refresh, with sound
probably stopped. The no-weather-scans capture also records a successful second
ordinary request without speaker-open messages; no audible or display result
for that precise logged request is otherwise inferred. The 18 tagged heartbeat
timing lines build successfully. Their exact removal is verified against normal
`main.c` SHA-256 `4e96a4d2d42bb7df54bacfb47a3ea2283426dd59814a4c3e5964a881a93284a6`.
They do not change task priority, scheduling, lock timeout or UI operations.
Firmware loading and a matched ordinary-refresh clock observation remain pending.

The user authorizes unattended testing and iteration until the clock/audio
interference is explained, instead of repeated manual setup and ten-minute
waits. The standalone heartbeat trace is already flash-verified, with capture
PID 51982; it will be superseded by the automatic sequence before its matched
test. No source/priority/lock/rendering fix is inferred from its startup logs.

The automatic diagnostic uses the existing weather worker and audio Play/Stop
commands: four real weather attempts, with a temporary one-minute successful
refresh interval. The first two run Stopped; after the second successful attempt
and ten seconds of settling, Rain starts automatically. The next two test
playback overlap, and the fourth attempt submits Stop and returns to the
ordinary cadence. Existing failed-request retry timing remains. The diagnostic
opens Analog without saving over the Home Screen preference, and forces only
the audio worker's temporary local Sound Machine values to Rain/50%/Continuous;
stored sound, volume and timer remain unchanged. Alarm/preview priority remains.
These are disclosed test-only behavior changes, not approved product changes.

Minimal worker timing records will measure preparation, codec writes and feeding
gaps, printing after playback closes rather than during sample feeding. Heartbeat
timings distinguish missed scheduling and data/drawing waits. They can detect
late software feeding and missed clock updates without the user; they do not
record acoustic output or establish exact panel pixels. No new task, generic
test manager, SDK hook or source path is introduced. Strip all tagged diagnostic
lines after identifying and verifying the actual correction; exact removal
hashes remain the closure check. Current work is the unattended investigation;
manual board prompts are deferred while these measurements run.

Automatic diagnostic preparation is complete. All 17 affected audio,
weather-worker and alarm-command native checks pass in 1.67 seconds, and the
firmware build passes. Parent removal checks independently reconstruct normal
main/audio sources. The audio timing state occupies 104 fixed bytes and uses
no per-chunk logging, allocation or shared lock; only the worker updates it.
The automatic controls are hardware-build-only, so native production checks
retain their original behavior. Load and the bounded four-attempt run are
pending; evidence uses `automatic-*` in the existing board review directory.
The sequence opens Analog only for the diagnostic boot; preferences are retained.

The unattended run is flash-hash verified and completes all four attempts
(HTTP 200 each), with audio inactive for the first two and active for the next
two. It starts and stops itself as designed. The actual saved preferences
reported by the worker are Analog, Rain and 32% volume; the diagnostic plays
Rain at 50% locally and does not rewrite that saved 32% value.

This run reproduces a 1,073,834-microsecond speaker-write call during the third
weather request. The maximum write-start interval is 1,073,992 microseconds;
preparation peaks at 27,152 microseconds at another time, and the largest gap
from one write's end to the next start is 27,164 microseconds. Thus the largest
delay is inside the codec write, including possible task preemption, rather than
recording preparation. All 5,537 writes complete through the current API without
an audio error being published. Driver waiting versus delayed scheduling still
needs distinguishing; an API success does not prove uninterrupted speaker output.

The ordinary stopped/Rain heartbeat records show short snapshot/lock waits
(at most 72/18 microseconds in the exceptional reports), no drawing-lock failure
and roughly 247 milliseconds of screen-object work on each new payload. The
largest reported ordinary applied interval is 1.085 seconds; no multi-second
software-update stall is reproduced in this bounded run. The startup three-second
UTC step is distinct. These are software timings, not physical-frame captures.
An inherited UI diagnostic forces owning-screen layout and multiple object-bound
reports while holding the drawing lock on each new weather payload; its cost is
a separate measured-boundary candidate, not yet an established cause.
Audio write waiting and the reported physical clock stall remain under
investigation. Current evidence and timestamps are in `automatic-checkpoint.json`.

The next unattended comparison samples the persistent audio worker's task state
from the existing 250 ms clock-control loop only while a speaker write exceeds
100 ms. Fixed bounded records distinguish waiting in the driver from delayed
processor scheduling; no new task or priority change is introduced. It also
measures the inherited weather object-log/layout call separately from total UI
work. Existing `main.c`, `audio.c` and `ui.c` contain temporary tagged probes;
removal hashes are recorded in `automatic-state-checkpoint.json`. Software
measurements do not capture acoustic output or physical frames.

Installed codec/driver source has no codec-layer retry. Its 1,000 ms timeout
applies separately to each driver wait, so the successful 1.07-second call
does not prove a timeout. The current two-core scheduler time-slices equal
priority tasks; equal priority by itself is not an established cause. No driver,
managed dependency, task priority, speaker-retention or product behavior change
is justified by these source findings alone.

The task-state comparison completes four successful real requests and stops
without intervention. Weather runs on core 0 in this boot. Its second stopped
request delays the UI wake by 619 ticks; the fourth Rain request delays it by
827 ticks and produces a 2.008-second applied clock interval. The clock-control
sampler's largest interval is 1.213 seconds. Rain's largest write is 42.629 ms
and its largest start interval is 56.434 ms; no write exceeds the 100 ms sampling
threshold, so no task-state conclusion is available. This differs from weather
on core 1 in the first run, when audio stalled over a second and UI timing stayed
short. Weather placement correlates with which consumer stalls; the responsible
operation still needs measuring. Inherited weather object logs/layout take
90.3–91.1 ms per publication within 249–292 ms total UI work. Affected native
checks (17) and the UI-shell check pass; build and flash hash are verified.

The next diagnostic places only the existing weather worker on core 1 to
reproduce the audio delay while the existing clock-control observer can run
on core 0. This is temporary controlled test placement, not a production fix.
Saved settings, task count and priority remain unchanged. Separate clock-control
work timing will distinguish a slow control operation from a late task wake.

The core-1 comparison reproduces a 1.059-second codec write during connection
setup, before the first connected event. Five bounded samples show audio Ready,
CPU 1 executing weather, and weather's effective priority 18 despite its configured
priority 4. Four samples span the same long write. Clock-control work has no
>100 ms reports; its largest call interval is 417 ms. All four requests succeed.
This identifies scheduling starvation rather than an unavailable speaker buffer:
audio is runnable while a boosted weather worker occupies its core.

Parent and independent SDK review verify the FreeRTOS rule: inherited priority
is restored only after a task releases its last held mutex. The application
holds its weather request mutex across HTTP, decoding and cleanup, so a boost
from a short inner SDK mutex can persist through the remaining request. lwIP's
shared protection mutex and priority-18 TCP/IP task provide a credible donor;
the exact donating acquisition is unobserved. The measured high priority and
Ready audio establish the harmful behavior independently of that donor identity.

The bounded correction changes only the existing request serialization to a
static binary semaphore, made available once at initialization. Keep the same
whole-request scope and all admission/return paths; quota state and overlapping
validation requests remain serialized. This avoids counting a held mutex through
slow work, without resetting priorities, changing worker priorities or adding
workers/locks. Existing native support gains only the binary creation substitute
needed for the real overlap test; its pthread model does not verify RTOS
priority. Test the same controlled core-1 sequence before accepting the fix,
then core 0 to verify clock behavior. A future higher-priority caller would not
boost an active request; current weather/settings-save callers both use priority 4.
Preparation is not a passing result.

The request-gate correction passes all 44 native checks in 16.62 seconds and
the firmware build. Its controlled core-1 run completes four successful requests
with no >100 ms audio-write samples; the largest write is 47.626 ms. The earlier
1.059-second scheduling stall does not recur. Ordinary clock intervals remain
under 1.096 seconds in exceptional reports. One 83.903 ms feeding interval
occurs outside either Rain/weather request, with 55.745 ms preparation; it
cannot be presented as verified continuous output.

The next comparison places weather on core 0 and uses the public speaker
completion callback to count all-zero transmitted PCM blocks during established
Rain playback. The existing `audio.c` contains a temporary attach-before-enable
wrapper, linked by one removable line in existing `main/CMakeLists.txt`; BSP,
codec, PCM, buffers and priorities are unchanged. The BSP clears completed
buffers after the callback, so it can inspect their content before clearing.
Exclude startup priming and report only after close. This detects silent digital
output blocks, not acoustic quality. Registration failure must be reported and
cannot be counted as coverage. The callback adds a small SDK cache synchronization
per completion; treat it as a measurement limit and remove it after diagnosis.

The corrected core-0 run completes four HTTP-200 requests. All ordinary
reported clock samples retain one-second UTC steps, zero wake lateness and no
drawing-lock failures, with a largest reported interval of 1.087 seconds. The
startup UTC correction is separate. Speaker registration and enable both succeed;
coherent counters cover 19,890 completed 512-byte buffers after priming, with
zero all-zero blocks and no skipped buffers across two real weather requests
and approximately three Rain repeats. Largest preparation/write intervals are
27.120/48.024 ms; largest write-start interval is 53.312 ms. These counters
exclude wholly silent output in this run, not partial interruption, acoustic
quality or physical frame rendering. The corrected core-1 comparison removed
the earlier scheduling starvation; both placements now have bounded evidence.

The final unattended overlap run restores ordinary task placement and the
production ten-minute weather cadence. Startup weather runs Stopped; after it
succeeds and settles ten seconds, temporary local Rain/50%/Continuous starts.
The next ordinary attempt ends playback and the test. Preference writes remain
absent. This checks sustained repeats and the actual refresh spacing rather
than only the accelerated comparison. Remove probes and load normal firmware
after this result; no new permanent source path or test framework is introduced.

While the ten-minute board run continues on its verified diagnostic image,
normal source is restored and all five removal hashes match: main/UI/audio
return to their pre-measurement contents, CMake loses the temporary wrap, and
weather retains only the request-gate correction and prior diagnostic-log
cleanup. All 44 normal native checks pass in 16.73 seconds and IDF 5.5.5 builds
without the probes. The current diagnostic ELF is kept temporarily for decoding
a failure during this still-active board run; discard it after successful
closure. Normal loading waits for that run's result.

The ordinary ten-minute run completes both requests successfully and stops
automatically. The regular refresh keeps one-second clock steps with no reported
wake lateness or drawing-lock failure; the earlier scheduling stall does not
recur. The transmitted-PCM measurement nevertheless finds 13 wholly silent
512-byte buffers among 102,076 inspected buffers, with a longest group of three
(about 17 milliseconds). These occur away from the weather request. They keep
the audio investigation open; the clock/weather correction is not evidence that
every playback gap is closed.

The retained Rain recording has no silent 256-sample blocks when decoded with
mpg123, including after applying the diagnostic 50% gain. Its longest exact-zero
run is three samples at that gain. This excludes ordinary recording silence
as the explanation for whole silent buffers, subject to the different native
decoder. The next bounded measurement compares actual driver refills with the
silent output buffers and their nearby preparation/write times. It uses temporary
probes in existing `audio.c`, `main.c` and CMake only, with automatic Play/Stop
and no preference writes. Driver/SDK code, priorities and buffer settings remain
unchanged. Remove the probes after the cause and correction are verified.

The matched-refill diagnostic passes 17 affected native checks in 1.11 seconds
and the firmware build. Independent source review and parent ELF checks confirm
bounded interrupt-side reads, internal code/data placement and no interrupt-side
logging, allocation or waiting. Its six buffer records and first 16 silent-event
records occupy about 2.7 KiB. The observation includes partial-copy cache
synchronization and generation checks; stable record coverage alone does not
establish a completed full refill or physical transmitted bytes. Automatic
Rain/50%/Continuous and ordinary ten-minute weather spacing preserve stored
preferences. The image and removal hashes are in `refill-checkpoint.json`; board
results are pending.

The matched-refill run completes two HTTP-200 requests and records 11 wholly
silent buffers among 102,089, with all six DMA addresses covered and no skipped
sizes or unmatched syncs. Each silent event's last stable, nonzero task sync
belongs to the preceding buffer generation: that buffer was not fed during its
latest observed reuse cycle. Several read-stage intervals, including file read
and lookahead, cross the refill deadline. Other events have old read/decode
landmarks and require a more precise current-stage/task observation. Buffer
alignment is not established as the cause. The UI completes 614 updates, with
a largest interval of 1.345 seconds including startup; this run alone does not
establish visible-frame timing or every displayed second. The next small probe
adds current playback stage and interrupted-task identity to the existing silent
records, plus clock-second anomalies during active playback. No buffer, priority,
driver or persistence behavior changes are proposed from these data alone.

The task/stage run stops after five silent records among 23,174 completed
buffers. One record directly observes LVGL running on core 1 while
audio's last checkpoint is its gain loop; source confirms LVGL's configured
priority 6 exceeds audio's priority 4. Other records show audio decoding or
the idle task while the driver waits following missed feeding. All 154 submitted
clock updates during the run have no active-playback repeated/skipped UTC seconds,
with a largest interval of 1.083 seconds. This is software-update evidence, not
a physical video or acoustic capture.

Source inspection finds that the BSP's default LVGL task has no core affinity;
the earlier audio-on-core-1 correction therefore did not ensure drawing stayed
on core 0. The UI heartbeat is also unpinned and shares LVGL's inherited-priority
display mutex. The disclosed bounded correction selects core 0 for both existing
display tasks: use the public BSP configuration with the same rotation, triple
partial buffering, touch flags, adapter defaults and priorities, and pin the
existing heartbeat with ordinary task creation. Both startup paths use that
configuration. Audio remains on core 1; weather and clock placement remain.
No helper, task, driver fork, buffer enlargement or file cache is added. Test
the same automatic Rain workload and normal weather spacing before accepting
this placement correction; file-read spans can include preemption and do not
yet justify a separate streaming change.

The matched core-placement comparison completes on October 14. Automatic
Rain/50%/Continuous runs for 591.41 measured seconds, crossing about 15 repeats
and the next ordinary ten-minute weather request. Both startup and ordinary
requests return HTTP 200 on core 1. Of 101,845 inspected completed 512-byte PCM
buffers, none is wholly silent; all six DMA addresses are covered, with no
unmatched cache-sync observation, unexpected buffer size/address or dropped
record. All 613 submitted UI updates have a largest interval of 1.094 seconds,
including startup; active playback has no repeated or skipped submitted UTC
seconds. This closes the measured audio-feeding and clock/weather scheduling
failures for this workload. The callback inspects buffer memory rather than
speaker output and adds cache writeback; it cannot exclude every partial glitch,
repeated nonzero audio sample or physical display delay.

All temporary measurement and automatic playback/stop code is removed. Normal
source hashes match the removal checkpoint while retaining the binary weather
gate, core placement and earlier established fixes. The first native pass has
43 successes and a 15-second UI-shell timeout while a firmware build runs
concurrently; that unchanged UI check then passes alone in 11.74 seconds, and
the complete 44-check suite passes alone in 16.51 seconds. No timeout or test
expectation was relaxed. ESP-IDF 5.5.5 builds normal firmware (17,202,400 bytes,
18% application partition free; binary SHA-256
`d22814ffef16e3ca301674d0964a84c468d6ab081b8f35e878acb739d26033f5`).
The application load is hash-verified; its boot ELF identity matches the normal
build, display/touch initialization succeeds, and Visual Crossing returns HTTP
200 with live weather published. Startup remains Stopped, without diagnostic
preference changes. Physical acoustic output and touch presses were not retested
after that final normal load.

The retained board-debug changes, measured against the saved initial Sound
Machine software checkpoint, reduce runtime C source by three lines, add six
SDK configuration lines and add 55 native-test/helper lines: 58 net source/config
lines in total, excluding the feature implementation and documentation. No new
source path, application task, queue, allocator, driver fork or playback cache
is added. All diagnostic probes are removed, including 190 tagged lines in the
last measurement variant; task-owned temporary ELF/WAV outputs are retired.
The [local full accounting](../.reviews/sound-machine-sprint-2026-10-13/board/timing-closure.md)
records causes, retained changes, costs, rejected experiments and verification
limits. Rain loudness and the remaining whole-feature acceptance checks stay
separate. No commit or push was performed.

The earlier `frame-trace-runtime.log` capture, after its playback stopped,
records ordinary weather TLS failures allocating AES transfer descriptors/alignment buffers and a nonfatal
CPU-1 idle watchdog during a subsequent connection attempt. No HTTP quota
response is received. Lazy audio initialization/mounting retains resources;
the internal free total falls from 93,211 after startup weather to 77,195 after
audio closes, while closing returns 37,332 bytes of playback allocations.
Source review confirms that the reported 31,744-byte largest internal block
does not establish DMA availability: RTC RAM lacks DMA capability. Fixed
diagnostics also reduce headroom but cannot alone explain the within-image
drop. Attribution, recovery and normal-firmware weather/audio overlap remain
unverified. Do not change speaker retention, weather retry behavior or TLS
placement from these totals alone; reduce disposable history and measure the
appropriate memory capability first. The failed-request and watchdog evidence
is retained with the frame trace.

Independent SDK review rules out assuming that NVS occupies the PSRAM's external
bus: this P4 has independent flash and PSRAM interfaces. The selected flash path
also keeps cache/interrupt access enabled. The display driver nevertheless
restarts its DMA transfer in software after every frame. The new measurement
will check those intervals and whether mapped flash-cache invalidation actually
occurs; elapsed storage-call time alone cannot identify the blocking operation.
The user's acceptance remains no observed flash during normal saves, including
a single save, rather than merely fewer flashes or a different fallback color.

An independent review initially proposed a shared interrupt-dispatch lock between
display DMA and audio DMA. Parent source and compiled-code verification rejected
that claim: the audio driver's shared-interrupt flag is conditional and is not
enabled on P4. The audio allocation uses `0x80e` (or `0xc0e` with IRAM), without
the `0x100` shared flag; it does not use that shared handler. This lead is
withdrawn. A separate common cache-operation lock remains a possible coupling,
without evidence that it causes the flashes. The display's default shared
interrupt priority is inferred from source and remains unconfirmed on the
running board. Choose a subsequent measurement or intervention from the current
frame results rather than treating another priority change as an established fix.

Read-only review identifies two credible remaining paths: the heartbeat's
one-second schedule is not aligned with clock-second boundaries, so small
sampling variation can repeat one second and skip the next; a failed 250 ms
display-lock attempt also skips that update. Neither has been measured on this
board. The existing capture contains no display-lock-failure log. A temporary
anomaly-only timing probe is prepared in existing `main.c`: it records submitted
UTC steps, the sample's fractional second and age, snapshot/lock/object-update
times and schedule overruns. Logs run after releasing the display lock. These
are object-update measurements, not proof of visible-frame completion. No new
task, faster full-screen polling or source path is introduced. The firmware
build passes (`0x10697c0`, 18% application partition free), and rebuilt native
touch-recovery/alarm-command checks both pass; those checks do not execute the
heartbeat probe. This cadence-only image was not loaded. The next comparison
combines this measurement with the corrected color-marker apply step described
above. Measure the hand with playback stopped and collect matched Rain-volume
results before choosing a correction. Remove all temporary probes after diagnosis.

The user also reports a weather-update failure followed by recovery on October
14. A fresh passive serial capture is opened without resetting the board or
requesting extra weather. No earlier failed HTTP response was captured, so the
cause of that event is unknown. The later ordinary refresh and subsequent
comparison startup both return `200` with `queryCost=1`; there is no current
quota-failure evidence. Weather behavior remains unchanged.
Earlier successful Visual Crossing requests report `queryCost=1`; normal
ten-minute polling amounts to about 144 of these requests per day. That is not
account-wide usage evidence. The provider's current
[cost guidance](https://www.visualcrossing.com/resources/documentation/weather-api/what-is-the-querycost-parameter/)
confirms forecast cost one, while its
[error guidance](https://www.visualcrossing.com/resources/documentation/weather-api/http-error-codes/)
requires the response body to distinguish daily exhaustion from other `429`
limits. No key, account allowance or retry policy was changed for this report.

Full-file native decoding shows Rain's average signal energy about 26.65 dB below
generated White Noise at the same setting. Current asset hashes and recorded
peaks were independently matched. Thunder limits a safe shared gain increase;
raising it cannot materially close that level gap without clipping louder files.
This is signal/source evidence, not measured acoustic output. No gain, default,
curve, recording-level or alarm-audio change has been applied. A separate bounded
level adjustment remains to be proposed under the user's loudness request. The
previous absence of normalization is preparation history, not a requirement to
preserve accidental level differences. Preserve the chosen recordings/blends and
existing alarm behavior while distinguishing any proposed level adjustment.

All-sound, repeat, timing/memory, timer/fade, priority, restart and Day/Night
checks remain pending. Further general board testing waits for the cutout and
blue-flash investigation; flash verification does not establish those results.

Local logs, image hashes and the settings baseline are under
`.reviews/sound-machine-sprint-2026-10-13/board/`. No commit, browser-release fix,
push or publication was made. The adversarial report's Minor browser-preparation
omission does not affect this complete USB installation.

### Implementation slices

1. **Preferences and command ownership.** Extend existing
   `targets/esp32-p4/main/device_config.c/.h` for the three preferences and their
   validation/defaults/ordinary saves; extend `audio.c/.h` for copied Sound Machine
   commands and a small playback/status snapshot. Keep one audio worker and speaker
   owner. Use existing save/completion paths, preserving unrelated preferences and
   other pending editor drafts. Scope preview cancellation independently of
   Sound Machine Stop. Verify defaults, preference reload, user-applied writes,
   failed saves, pending Start/Stop and priority through existing
   `tests/test_device_config.c`, `test_audio.c` and support files.
2. **Recordings and generated sounds.** Use an appropriate managed MP3 decoder
   through existing `main/idf_component.yml`/dependency lock and build facilities.
   Verify its actual delay/padding behavior rather than assuming it trims files.
   Package only `shared/assets/sounds/clock/` through the existing CMake/SPIFFS
   facilities into the current 11 MiB storage partition; exclude originals and
   retain the BSP's non-formatting mount-failure behavior. Extend `audio.c/.h`
   for correctly rated mono recording playback, short buffered repeats and the
   three live noise generators under the architecture's efficiency rules.
   Preserve the existing alarm generator and format. Check bounded generation
   cost/state, continuous chunk boundaries, output levels and long-run stability.
   Measure noise generation and speaker-feed timing on the board, retaining
   headroom within each chunk interval; verify no generation-induced starvation
   or audible hiccups under representative normal clock activity. Temporary
   measurements must not add per-sample overhead or remain as production probes.
   Verify exact playable samples and repeats against the final manifest, output
   levels/noise character, cancellation during setup/read/decode, malformed or
   missing files, codec failures and release of decoder/buffer resources. Use
   existing tests and disposable diagnostics; do not introduce an alternate UI,
   a decoder service or a separate playback worker.
3. **Timer, live changes and audio priority.** Implement the approved elapsed-time
   and gain rules in the existing audio owner. Extend existing audio checks for
   all timer choices, full-interval Play, Stop, live sound/volume/timer changes,
   the end fade, Continuous, clock corrections, ringing/preview interruption and
   no automatic resume. Keep playback/deadlines out of persistent storage. Preserve
   alarm preview lifetime, ringing ramp, Snooze/Dismiss and lazy codec failure/retry
   behavior; a failed codec must not publish false Playing status. Current native
   codec substitutes assume 22.05 kHz/441-sample alarm writes: extend them for
   recording formats while retaining their precise alarm assertions.
4. **Production UI and clock shortcuts.** Reuse existing `ui/settings.c`,
   `ui/controls.c`, `ui/ui.c`, `ui/clock_views.c`, `ui/ui_private.h`, and only needed
   display/startup integration in `app_runtime.h`/`main.c`. Replace the redundant
   Settings-entry Alarm switch, retain the immediate Alarm Settings switch, and
   implement the approved screens/status/navigation. Render from owner copies;
   keep no second countdown or audio state. Reuse existing grouped icon/font assets
   and their tools for needed speaker artwork. Verify all choices, scrolling,
   immediate controls, saved captions, Back continuation, error presentation,
   both appearances, clock headers/indicators and the preserved alarm tap areas
   in existing `tests/test_ui.c`. Keep current touch behavior; drag-outside tap
   cancellation is outside this sprint.
5. **Review and closure.** Independently review feature coverage, correctness and
   necessity, then run affected checks and the full native suite and compile
   ESP-IDF 5.5.5. Inspect firmware/recording image sizes, actual heap allocation and
   decoder/worker stack needs before a separately authorized flash/board pass.
   On hardware, check all twelve sounds, recording seams, noise character and
   comfortable volume, sustained noise timing/headroom without hiccups,
   timer expiration/fade, Stop/preemption response, restart
   Stopped with remembered preferences, Day/Night controls and sustained playback
   during representative normal weather/network/display/touch activity. Use current
   applied preferences as the restoration baseline; restore any temporary changes.
   Keep substituted software checks distinct from actual decoder/board evidence.
   Record results and remaining limits here; update user-facing guides when the
   firmware feature is delivered. Do not commit, push, merge or flash without
   the applicable authorization.

### Scope and readiness

Existing approved source paths suffice for the planned work. Existing firmware
and test CMake lists, dependency manifest/lock, BSP storage/audio facilities and
configuration are the integration points; do not create an additional service,
wrapper, maintained test file or tool unless a concrete need is established and
any required exact-path permission is obtained. Runtime/resource measurements
may justify an adjustment to the existing worker or buffers, not speculative
extra machinery. Preserve unrelated ongoing weather work.

The final recording image was host-packed successfully with 5.082 MiB of empty
blocks after filesystem overhead; this does not establish mounted board playback.
Current playback generates only alarm/preview PCM and has no application MP3
path. Running free memory, decoder stack and uninterrupted loops remain firmware
and board verification. No resource blocker has been demonstrated, and no
partition expansion is planned.

Preparation validation: checked authoritative decisions, file ownership,
source-path permissions, existing audio/configuration/test/build facilities and
recording capacity evidence. An independent read-only readiness check identified
no remaining product choice. The subsequent approved dispatch is recorded above;
it is not implementation evidence. No firmware tests, build or hardware checks were run for preparation
or dispatch.

## Slice: Sound Machine mockup promotion — approved October 13, 2026

Status: integrated into the existing official `mockups/alarm-design.html`.
The [Sound Machine specification](architecture.md#sound-machine-controls) owns
approved controls and the limits of this visual approval. No firmware, new source
file, task, queue or storage mechanism is added by this slice.

The approved changes replace the mockup's redundant Settings-entry Alarm switch
with Sound Machine and chosen-sound subtext; add its controls, twelve-choice
scrolling sound selector and five-choice Sleep Timer; and add playing/stopped
speaker shortcuts to both clock faces. Both headers use title case and matching
day/date spacing. The speaker and RTC tap areas follow the approved between-row
placement while the existing alarm areas remain. Diagnostic outlines are not
promoted to the device UI. Existing shared settings components own the style.

Parent validation passed in the actual interactive mockup for both appearances:
all twelve sound choices and five timers, list dragging, scrollbar-thumb dragging
in both directions, scrollbar keyboard navigation, description fit, selected
state, volume independent of alarm volume, Play/Stop, timed/continuous status,
Back to the entry screen, clock shortcuts, RTC notice and the retained immediate
Alarm Settings switch. Measurements confirm the approved clock geometry,
unchanged alarm areas and four-pixel Analog gap; new settings controls fit the
round display. All ten inline scripts parse, including the House Messages
preview, and the live device preview has no browser script errors.

Five existing settings screens were compared in both appearances. Their content
is pixel-identical within the clock; the only Night screenshot differences are
10–54 edge pixels where the circular clip meets the browser's dotted background.
Independent read-only coverage review found no actionable issue. Screenshots and
checks are local under `.reviews/main/sound-machine-promotion-2026-10-13/`.
No firmware build, flash or hardware check was run for this mockup slice; it
changes no firmware source or assets. No commit, push or merge was performed.

## Slice: Immediate Alarm Settings On/Off — approved October 13, 2026

Status: implemented and checked in development. Alarm Settings now reuses the
existing immediate enabled-only save operation. Other alarm edits retain their
draft and ordinary Save/Cancel behavior; a successful toggle updates only the
enabled field in that editor's baseline and draft. The existing Settings-entry
switch remains in firmware until Sound Machine menu integration is built.
The Sound Machine promotion slice replaces that row in the visual reference.
The [save rules](architecture.md#editing-and-saving-user-settings) own the behavior.

The change was disclosed before implementation: the Alarm Settings switch
previously stayed draft-only until Save; it now applies without requiring Save,
and Cancel/Back cannot undo a successfully saved toggle. No other behavior,
source path, task, queue or storage mechanism is added. The official mockup and
user guide reflect this choice. Sound Machine controls were subsequently
approved for the visual reference in the promotion slice; their firmware
integration remains unimplemented.

Parent validation: fresh native configure/build, focused Settings check and all
33 native checks pass (full suite 15.30 seconds). Actual LVGL controls and
production configuration code check enabled-only writes, preservation of all
other draft fields, Cancel/Back, subsequent full Save, both appearances, hidden
completion, duplicate taps, task-start/commit failures, retry, an unfinished
zero-day draft and configuration reload. These use controlled save failures,
not physical power-cut observations. ESP-IDF 5.5.5 build passes (image
`0xf622c0`, 23% application partition free). All eleven embedded mockup scripts
pass syntax checks; execution of its actual toggle/Cancel branches verifies
draft preservation. Independent read-only review found no actionable issue.
Evidence is local under `.reviews/main/alarm-immediate-toggle-2026-10-13/`.

Remaining: authorized flash and physical check of the immediate switch from
the main clock's Alarm shortcut, Cancel with other pending edits, and ordinary
alarm operation. No flash, hardware test, commit, push or merge was performed
for this slice. Existing board firmware and prior verification remain as recorded.

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
The October 12 authorization covered implementation, testing and flashing
for this completed sprint. Bell probes were removed before the normal build;
the accepted alarm evidence was retained. Local evidence is in
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

The completed scope used the existing Settings renderer and checks. Weather
interface behavior belongs to the [visual and editing contracts](architecture.md#ui-and-visual-reference);
the temporary provider/key presentation is defined there. This sprint added
no Visual Crossing backend, persistent key, provider adapter, task or queue.

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

Completed scope: replaced Gentle Chime with the selected slower B Classic
Bell in the existing audio owner and updated its name, description and preview.
Classic Beep remains the default; the other sound and existing controls remain.
No additional worker, stored recording, settings owner or source path was needed.
The [sound definition](architecture.md#alarm-and-snooze) and
[audio ownership](architecture.md#ownership) remain in the architecture.
The performed checks, listening acceptance and accepted unmeasured limits are
recorded above; this completed plan does not add future Bell testing work.

## Existing development and verification record

These are dated checkpoint records. Retain their performed checks, observations
and limitations as evidence; use the [current work list](#current-work--reconciled-october-13-2026)
for ongoing actions rather than treating an older remaining-work statement as a
new instruction.

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

## Round enclosure STEP prototype — October 13, 2026

The round enclosure prototype is exported as a main body, magnetic rear
cover and two-solid STEP assembly, with separate STLs in the requested print
orientations. The installed envelope is approximately 140 mm wide, 70 mm deep
and 133.8 mm high. The user accepted cable fit and waived further cable
measurements. The [prototype notes and actual CAD views](../.reviews/enclosure-concept-2026-10-13/round-prototype.md)
identify the hardware inputs, construction and verification limits.

The current draft incorporates the user's selected lower RTC with the
centered cable above it. The PCB seat is now 8 mm below its initial position,
leaving 6.3 mm under the battery holder. The latest revision lowers the RTC
another 4 mm from the preceding draft. The cable opening is about 8 mm below
its initial position and remains
14 × 8 mm; its corners now have a 2 mm radius. The full measured plug
outline clears the rounded opening in the CAD check. The STEP assembly,
separate parts, print-oriented STLs and actual CAD preview are updated together.

The speaker post-to-arm junction matches the latest modified Classic case
while retaining the longer wall links. The current aligned STEP comparison
found zero added or missing material versus Classic within the speaker outline
over the full post depth. Checks against the preceding round draft show that
the latest body changes are confined to the RTC supports and the cover changes
to the cable opening. The user confirmed straight rearward cell removal;
the lower approach and a rearward corridor at holder height clear the body.

STEP reimport, solid validity and body/cover clearance checks passed. Both
printable meshes are closed and connected with consistent winding; independent
mechanical review found no current geometry defect. Physical speaker corner
fit, RTC cell removal, printed tolerances and magnet retention remain prototype
checks. Face-up printing still needs slicer inspection for internal supports.
No enclosure was printed, and no firmware source, build or board load was
changed by this CAD work.

October 14 follow-up: the user printed the case and reports good appearance,
a flat bottom and less resistance to tipping than the Classic case. The user
selected two Classic-style legs with the 8° backward tilt retained, and asked
for a mockup before STEP implementation. The [legs concept and prompt notes](../.reviews/enclosure-concept-2026-10-13/round-legs-concept.md)
show that proposal. Leg dimensions and stability are not yet verified. The
existing flat-bottom STEP/STL files remain the unchanged reference version;
no legs have been implemented in CAD. No firmware source or board state changed.

For comparison, the user also requested a [Classic-case visual concept](../.reviews/enclosure-concept-2026-10-13/classic-tilt-inset-back-concept.md)
with the 8° backward tilt, revised legs and a thinner, inset rear cover. That
mockup preserves the Classic's slimmer front rim and cylindrical profile.
Both case references remain unchanged; cover dimensions, hardware clearance
and any CAD implementation are still pending.

The subsequent [detailed Classic interior mockup](../.reviews/enclosure-concept-2026-10-13/classic-interior/layout-notes.md)
proposes a uniform 4.7 mm wall, a narrow rear seating lip and four local magnet
pads rather than the thick rear band. A 3 mm flat cover sits 1 mm inside the
rear rim. Speaker and RTC move 4 mm forward; the RTC remains horizontal with
its holder below and rearward cell access. The temporary solid model is valid
and shows no checked body/cover collisions, with 2 mm behind the speaker,
4.48 mm behind the RTC and approximately 6.1 mm beneath the holder. The rounded
14 × 8 mm passage clears the measured plug proxy and sits above the RTC.
Independent geometry and visual review agreed. Speaker corners and wiring are
schematic; printed fit, battery handling, magnet retention and leg stability
remain physical checks. All 17 checked Classic/Round reference STEP/STL files
were unchanged by hash comparison. No revised STEP/STL, maintained source file,
firmware build or board load was produced by this mockup work.

Pad revision, approved October 14: the mockup now uses 10 mm pads and 8 mm
wide wall connections with true 2 mm radius rounds at their inside corners.
It leaves 2.35 mm around each 5.3 mm pocket and retains the prior 6.5 mm pad
depth and 4.3 mm floor. Both existing detailed views were updated and an
enlarged pad section was added. The revised body is one valid solid; new checks
find no pad overlap with the speaker, RTC, mainboard or cover. Component
positions, cover and prior clearances are unchanged. Independent review agreed
with the geometry and views. STEP/STL export is still pending the user's next
instruction, and printed strength and retention remain unverified.

RTC tilt revision, approved October 14: the interior mockup now tilts the
RTC, mounting seats, posts and screws 8° with the case while keeping the
mounting row fixed. The battery exits rearward along the case axis. The
revised body and cover remain valid single solids; battery and power-cable
corridors show zero checked overlaps. The holder has approximately 7.38 mm
vertical floor clearance, the RTC has 4.46 mm to the cover, and the power
corridor stays at least 5.43 mm above the components measured perpendicular
to the PCB. The battery corridor clears the rear lip by 5.06 mm radially
and its nearest post by 1.59 mm. Independent transformation, geometry and
visual checks agreed. The cable corridor is a conservative clearance box;
actual bends, connectors and physical cell handling remain unverified.
All 17 reference STEP/STL hashes remain unchanged. No STEP/STL revision,
maintained source change, firmware build or board load was produced.

Front-down finish constraint: the user requires an even front appearance and
reports visible perimeter-versus-fill differences on a lightly textured
plate. A shallow front-ring print with perimeter-filled/concentric paths and
the actual Tinmorry PETG-GF is proposed before committing to a full front-down
print. This is an unperformed finish trial, not a confirmed printing result;
no coupon, profile, enclosure geometry or STEP/STL change was made.

The user then requested the 3 mm front-ring STL. The
[finish-test file and settings](../.reviews/enclosure-concept-2026-10-13/classic-interior/layout-notes.md#front-down-surface-finish)
are now available: 125 mm outside diameter, 115.6 mm inside diameter and
3 mm thickness. The exported STL is closed, consistently wound and one
connected mesh with verified dimensions; a delivery copy is in Downloads.
The saved X1 Carbon/Tinmorry 3MF was inspected read-only. The trial recommends
six walls, concentric bottom paths and both first-layer speeds at 30 mm/s,
with the selected spool's working temperature and calibrated flow retained.
Manufacturer guidance was verified separately. All 17 case STEP/STL
reference hashes remain unchanged; no existing profile, 3MF or maintained
source was edited. Slicing and physical surface evaluation remain pending.

Wall-count clarification: four walls are now recommended for the front-ring
trial, matching the saved case profile. Six were an optional way to occupy
more of the rim with wall paths, not a strength requirement. Concentric bottom
fill and both 30 mm/s first-layer speeds remain recommended. Instructions
were updated; the STL and all case geometry are unchanged. No print result
has been established.

Rear-leg correction, October 14: the user's marked screenshot prompted a
cover-removal check. The earlier mockup cleared the installed cover but had
two small inside leg corners behind the rim that intersected rearward removal.
The mockup now continues the circular clearance through the rearward legs.
The full-disc sweep and the actual cover at checked offsets have zero overlap;
the body and both feet remain valid connected solids. Both full 13 × 79.5 mm
sole footprints and the leg-to-shell joining volume are preserved, as checked
independently. Interior/back views and a new enlarged section show the fix.
The RTC, magnet pads, cover and battery/cable clearances remain unchanged.
No case STEP/STL or front-ring finish-test STL was changed. Printed fit and
leg strength remain unverified, and the ring-print result is still pending.

The user then requested a view with the cover installed. The
[installed-cover mockup](../.reviews/enclosure-concept-2026-10-13/classic-interior/Classic-Back-Cover-Installed-Mockup.png)
shows the assembled rear and an enlarged leg-to-rim junction using the same
corrected geometry. Rebuilt parameters/checks and independent visual review
agree with the preceding model. No geometry, STEP/STL, profile or firmware
change was made for this additional view.

Integral-leg rethink, October 14: the user rejected a separate printed stand
and requested a new one-piece mockup with the 8° tilt retained. The
[preceding curved-leg proposal](../.reviews/enclosure-concept-2026-10-13/classic-interior/layout-notes.md#earlier-curved-leg-proposal)
shows the back installed and removed, an enlarged connection and a side view.
The proposed legs widen from 13 mm to 16 mm while retaining the full 79.5 mm
sole length. Curved connections end 15.5 mm forward of the rear rim; lower
heels retain the rearward stance. Model checks find one valid connected body,
complete sole contact areas, clear cover removal and unchanged interior
hardware positions and clearances. Printed strength and tipping resistance
remain unverified. The user reports visible imperfections in the first ring's
concentric underside and is testing front-down printing on a more textured
plate. The second finish result is pending. No enclosure STEP/STL, ring STL,
profile, maintained source or firmware change was made for this mockup.

Straight-leg follow-up, October 14: the user rejected the curved full-depth
side panels as still the same basic leg structure and a printability concern.
The [preceding candidate](../.reviews/enclosure-concept-2026-10-13/classic-interior/layout-notes.md#earlier-short-straight-leg-proposal)
uses two short 12 × 14 mm rectangular legs above flat 16 × 79.5 × 6 mm feet.
The case and legs remain one piece at 8°. Actual shell connections span case
depth 20.70–36.90 mm, ahead of the rear rim. The installed-cover and side
views show this structure; blue lines identify downward faces needing print
supports in the front-down orientation. Model checks find one valid connected
body, complete sole contact areas, clear cover removal, approximately 7.01 mm
clearance to the rear shell band and unchanged interior checks. This is a
proposal, with slicing, printed joint strength and tipping resistance still
unverified. STEP/STL files, the ring test and firmware are unchanged.

Front-face constraint, October 14: the user requires no support contact on
the visible front face. A [printing-orientation diagram](../.reviews/enclosure-concept-2026-10-13/classic-interior/Classic-Front-Down-Print-Orientation.png)
shows the full front ring flat on the plate, with the entire model rotated
from its assembled pose. Solid checks find no material below that plane,
leg material starting 0.33457 mm behind it and a complete first 0.20 mm ring
layer. The 8° assembled tilt remains built into the case-to-foot relationship.
Support needs on leg/foot faces are distinct from the visible front ring.
No geometry, STEP/STL, slicer profile or firmware change was made; slicing
and the more-textured-plate test result remain pending.

Solid-leg follow-up, October 14: the user rejected the thin legs and requires
substantial integral legs, straight case intersections, a flat front on the
print bed and no awkward rear overhangs. The
[updated mockup](../.reviews/enclosure-concept-2026-10-13/classic-interior/layout-notes.md#current-solid-leg-proposal)
uses 16 mm wide solid legs with four straight junction lines across the full
case depth. Their fronts share the front ring's plane. Flat soles are now
80.681 mm long, extending the front toe by 1.18 mm to meet that plane; the
8° tilt and 90.42 mm overall stance width remain. Roots end flush with the
rear plane, and lower heels clear the rear shell by approximately 3.335 mm.
Parent and independent geometry checks agree on one valid connected body,
complete sole contact, zero volume below the print bed, no downward-facing
exterior faces above it and clear rear-cover removal. Shared hardware,
battery and cable dimensions and clearances are unchanged. Front/rear
views with the installed cover and a side/print-section detail were reviewed.
All 17 case references and both ring-test copies retain their hashes. This
is a mockup only: interior features still need supports; slicing, printed
strength, stability and physical fit remain unverified. STEP/STLs, profiles,
maintained source and firmware are unchanged. The textured-plate trial
result remains pending.

STEP/STL export, October 14: the current solid-leg Classic geometry is now
exported as an assembly, separate body/cover STEP parts and front-down body /
outer-face-down cover STLs. The
[files and checks](../.reviews/enclosure-concept-2026-10-13/classic-interior/layout-notes.md#step-and-stl-files)
preserve all saved mockup parameters and checks. STEP reimport yields a valid
single body, single cover and two-solid assembly, with negligible adaptive
volume differences and zero checked hardware/service interference. Both STLs
are closed, consistently wound, connected meshes with no boundary, nonmanifold
or degenerate elements; volume differences are below 0.03%. The body front ring
and both leg fronts contact Z = 0, with an approximately 125 × 139.24 mm print
footprint and 79.90 mm height. The cover prints 3 mm high with pockets up.
Independent export review agrees on the key dimensions, mount geometry and
print orientations. Five distinct delivery copies are in the user's Bambu Lab
clock directory; all 17 prior case references and both ring copies are preserved.
This export does not establish sliced supports, printed fit, joint strength,
tipping resistance, magnet retention or physical cell removal. The textured-plate
trial result remains pending. No maintained source, firmware build, board load
or printer profile was changed.

Naming follow-up, October 14: the user selected **Orbit Case**. The five newly
copied STEP/STL files in the Bambu Lab working directory now use the
`Orbit-Case` prefix. Hash checks confirm unchanged contents; original reference
files and project evidence filenames are preserved.

Flush-rear Orbit revision, October 14: the user selected rear foot ends
directly below the upper rear cabinet rim and a flat cut from the desk-contact
bottoms to preserve the original end thickness. Rear Y changes from −70 to
−58.706856 mm. Removing 1.587148 mm from both flat soles, then lowering the
whole assembly equally, retains the upper heel profile and 6.582315 mm rear
thickness. The soles are now 16 × 69.164901 mm; the 8° tilt and 90.42 mm
overall stance width remain. Hardware positions inside the case and all
checked battery, cable and cover clearances are preserved. Parent and
independent reopened-STEP comparisons find exactly the two requested cuts,
with zero unplanned added or missing volume. Valid body/cover solids,
two-solid assembly, complete front-bed contact, straight joins and closed,
connected STLs pass their checks. The
[updated files and preview](../.reviews/enclosure-concept-2026-10-13/classic-interior/layout-notes.md#current-solid-leg-proposal)
are saved, and all five Orbit files in the Bambu Lab working directory are
updated. Current project filenames now also use Orbit. Working Bambu exports
were replaced; the existing long-foot project exports and images remain
evaluation references for comparing stability while this prototype is tested.
No new backup copies were created. All 17 original case references and both
ring-test copies retain their hashes.
The body print envelope is approximately 125 × 137.64 × 68.49 mm. Slicing,
physical fit, printed strength, tipping resistance and the textured-plate trial
remain unverified. No firmware, maintained source or printer profile changed.

Drawing refresh, October 14: the complete current Orbit set now includes
exterior/side views, populated interior, installed cover and leg junction,
back/magnet attachment, pad dimensions, exposed mounts/rear clearance and
whole-body front-down print orientation. All eight sheets use the shorter
feet and current table height; six previously missing current sheets were
rerendered. Independent visual review checked dimensions, RTC tilt, mounting
labels, clearances and bed contact. The
[current drawing index](../.reviews/enclosure-concept-2026-10-13/classic-interior/layout-notes.md#current-drawings)
links the sheets, and all eight PNGs are copied to the Bambu working directory.
Geometry parameters/checks and both copies of all five STEP/STL files are
unchanged. Earlier drawings remain clearly labeled comparisons. Slicing and
physical fit, strength and stability remain unverified; no firmware or printer
profile changed.

## Review follow-up: visual diagnostics and browser recordings — October 14, 2026

The user approved removing the inherited visual diagnostics and correcting the
weather header, fixing the review's F1 browser-packaging omission, and retaining
the UI stack monitor. Removed object/asset geometry, startup-hand and initial
angle logs, their renderer declaration/calls, diagnostic-only UI fields and
weather-change bookkeeping. Actual drawing, hand calculations, valid/last-good
weather handling, navigation, warnings and alarm behavior remain. The UI
stack-monitor source in main.c is unchanged. The weather header now describes
the existing binary request gate.

The existing browser-preparation helper now copies the generated storage image
with bootloader, partition table and application. Its manifest includes that
image at the current storage offset. Prepared the four-image bundle locally
and updated its installer name/text and user guides to describe the actual
current application and recordings. The existing version value remains;
no version/release machinery, publication or flash was added. Architecture owns
the retained-monitor, retired-visual-log and recording-package decisions.

The first native pass reports 43/44 with the existing 15-second UI-shell timeout,
without an assertion failure. That unchanged test passes alone in 12.04 seconds;
the final full suite passes 44/44 in 29.61 seconds, including UI-shell at 12.20
seconds. No test timeout or assertion was relaxed. The computer was also running
other CPU-intensive processes; this observation does not prove the cause of the
timeout. Forecast 53/53, location 3/3, Conditions 40/40 and messaging checks pass.
ESP-IDF 5.5.5 builds the application at 17,200,320 bytes with 18% application
partition free. All four staged hashes and offsets match the generated USB
images and binary partition table, including the 11 MiB storage image built
from exactly the nine approved recording files. Browser flashing and physical
appearance after this cleanup remain untested; the board retains the previous
loaded firmware.

Independent review finds no credible defect or unnecessary new mechanism.
Removed 198 UI source lines; packaging adds one copy step and one manifest entry.
No new source file, task, queue, storage scheme or renderer abstraction is added.
The only new bundle file is the ordinary generated recordings image. Existing
services, stack monitor, audio and alarm-recovery source hashes are preserved.
See [local closure evidence](../.reviews/main/post-timing-review-cleanup-2026-10-14/closure.md).
F1 is closed for source and local package preparation; publication and board
tests are separate actions. Other Sound Machine acceptance checks remain open.

## Brown Noise cutout investigation — October 14, 2026

The generator correction below is superseded by the subsequently approved
[source-equation restoration](#noise-source-equation-restoration--october-14-2026).
Its waveform and timing measurements describe the former generator, not the
restored source equations.

The user reported brief cutouts while leaving Brown Noise untouched. Inspection
found that its generator limited the returned sample but allowed the stored
integrator to exceed that limit. Ordinary random excursions then held the output
constant while the hidden excess recovered. This is a proven waveform defect
and a plausible explanation for quiet intervals, not yet a confirmed explanation
of the user's listening result.

Bounded the stored value at the existing limit and returned it, replacing three
lines in the existing audio owner with no net firmware lines. Kept the random
source, filter coefficients, gain, sample rate, chunk size, retained state and
single audio worker. No task, buffer, priority, persistence or alarm-recovery
change was added. Brief repeated values at the hard limit remain possible; Brown
Noise itself does not require a flat waveform. The fix prevents stored overshoot
from prolonging those repetitions.

For each of two normal seeds, the existing native test generates two million
samples. The firmware seed's longest Brown flat run falls from 33.878 ms to
0.635 ms; the other seed falls from 39.365 ms to 0.454 ms. Stored state remains
within the output bound. Brown's total RMS falls by approximately 0.65–0.67 dB,
with approximately retained adjacent-sample energy; those calculations do not
establish speaker loudness. White and Pink results remain unchanged. Added 17
net lines to the existing test for the additional seed and regression checks;
the one-millisecond margin is a software guard, not an acoustic threshold.

An authorized automatic 90-second board run played Brown at temporary 50% volume
and Continuous without saving those choices. Across 4,501 chunks, maximum
generation/scaling was 284 microseconds, about 1.42% of each 20 ms playback
interval. Maximum blocking codec write was 24.022 ms and maximum write-start
interval 24.142 ms. The run included startup Wi-Fi and successful live weather;
no panic or watchdog was captured. It did not measure transmitted silence,
underflows, physical touch, an ordinary ten-minute refresh or audible output.
These results weigh against slow Brown generation as the reported cause, without
claiming every operating workload has been covered.

Removed all temporary automatic-play and timing blocks, restoring the recorded
normal audio/main source hashes exactly. Main and the retained UI stack monitor
are unchanged by this slice. All 44 normal native checks pass in 17.74 seconds;
ESP-IDF 5.5.5 builds at 17,200,320 bytes with 18% application partition free.
All four staged browser images match the generated USB images and offsets.
Loaded the normal application with written-hash verification and matching boot
ELF identity, followed by successful display/touch initialization and live
weather. This load also includes the preceding visual-log removal and packaging
cleanup; browser installation remains untested and unpublished.

Independent source/evidence review finds no consequential defect in this bounded
fix. On corrected normal firmware, the user reports possibly hearing one very
brief cutout. A subsequent automatic 720-second Brown/50%/Continuous run crosses
an ordinary weather refresh; both startup and ordinary requests succeed.
Generation/scaling reaches 308 microseconds, blocking write 23.908 ms and
write-start interval 24.060 ms. A temporary completion callback inspects 61,930
512-byte buffers over the final 719 seconds: no wholly silent or constant
buffers, all six addresses covered, and no bad-size or extra-address observations.
The longest zero run is two PCM samples (0.091 ms); longest equal run is 16
(0.726 ms). Coverage matches the expected mono sample count. The callback adds
interrupt work and an SDK cache writeback; this is instrumented buffer-memory
evidence, excluding the first second, rather than electrical or acoustic capture.
It weighs against sample starvation in that workload, without closing the user's
possible residual cutout. Temporary probes and link option are removed, and the
normal sources/image reproduce their earlier verified hashes. Final normal
firmware is restored with written-hash and boot-identity verification, successful
display/touch initialization and live weather. Task-owned diagnostic ELF files
are removed. Other Sound Machine acceptance checks remain open. See the
[local investigation and limits](../.reviews/main/brown-noise-2026-10-14/closure.md).

Front-finish feedback, October 14: the user reports that the textured plate
hides the extrusion lines but leaves a surface too rough for the desired
appearance. A finer satin build surface is suggested for another existing-ring
trial; a Bambu-compatible manufacturer example and its comparison photos are
linked in the [finish notes](../.reviews/enclosure-concept-2026-10-13/classic-interior/layout-notes.md#front-down-surface-finish).
The exact Tinmorry PETG-GF combination is untested and no plate purchase or
profile change is approved. This report establishes a surface result, not
full-case fit or stability. Geometry, STEP/STL files and drawings are unchanged.

## Noise source-equation restoration — October 14, 2026

The user identified soundmachine.app as the intended source and approved
restoring its White, Pink and Brown calculations, removing our generator
clipping and retaining the existing output gain. The proposed -20 LUFS
normalization was explicitly withdrawn. The website's current `noiseBuffer`
source was compared directly; the earlier implementation had halved White,
clipped Pink and changed Brown's coefficients as well as clipping it.

Restored the equations in the existing audio owner. Retained the uniform
pseudorandom input, single-precision implementation, 22,050 Hz/441-sample chunks,
continuous filter state, existing output gain/volume/fade and final PCM conversion
guard. The website creates 32,000 Hz buffers; its mixer, finite loops and sound
gains are outside this slice. Generated peaks increase as discussed with the
user. No audio worker, buffer, task priority, settings or alarm behavior changes.
The specification owns the approved generation rule and source reference.

All 44 native checks pass in 17.38 seconds. Frozen external-source samples cover
ordinary Pink/Brown excursions above the former limit and unscaled Brown state;
two million samples for each noise and seed check complete retained-state chunks,
finite values, mean, RMS and source-appropriate high-frequency separation.
The parent also executes the actual website JavaScript with the same input and
confirms the frozen samples. Observed raw peaks are White 1.000, Pink .932–.951
and Brown .973–.984; these sequences do not establish all future peaks or physical
distortion. Existing gain, playback, recording and alarm checks remain intact.

Independent review finds no credible consequential defect or unnecessary new
mechanism. Only five audio lines are replaced, with no net firmware lines; the
existing test adds 32 net lines. Source hashes confirm preservation outside the
audio/test/documentation scope. ESP-IDF 5.5.5 builds at 17,200,192 bytes, with 18%
application partition free; all four staged images match the generated images.
Authorized app-only flash verifies the written hash and boot ELF identity. Normal
display/touch initialization and live weather succeed, without a captured crash
during startup and its settling interval. No settings erase or test preference
write is performed. The user confirms all three restored noises are continuous
and clear in the prompted roughly 30-second-per-noise listening check at their
usual volume. The previous cutout is not reported in this check. This does not
establish long-run peak/distortion limits, physical stopping or worst-case board
timing under all activity; the former Brown measurements do not establish them.
See [local restoration evidence](../.reviews/main/noise-source-restoration-2026-10-14/closure.md).

## Recorded-sound playback gain — October 14, 2026

Status: implemented and verified in software by Development Orchestration on
October 14, 2026. Flashed under the user's subsequent explicit authorization;
startup verification and the requested board listening checks pass.

The user approved fixed on-clock gain for the nine existing mono recordings,
using one common boost for seven sounds and two exceptions set by peak headroom.
The authoritative values and implementation rules belong in the
[recorded-sound gain specification](architecture.md#recorded-sound-playback-gain).
Original and prepared MP3 files remain unchanged. The separate -20 LUFS exports
are listening previews, not selected production replacements or a normalization
requirement. Generated noise retains its restored equations and existing gain.

The existing audio owner's gain calculation now combines the fixed recording
boost once per chunk, before its existing sample loop and conversion guard.
Only this production calculation changes: seven lines replace four, for three
net firmware lines. The existing test adds 54 net lines. No new source file,
runtime task, processing pass, configuration setting or persistence is added.
The volume curve, timer fade, repeats, playback priority, error behavior and
codec output setting are preserved. The restored noise equations and gain,
alarm paths and all original/prepared MP3 bytes are unchanged.

All 15 audio checks pass, followed by all 44 native checks. Independent
dB-derived expectations cover all nine recording selections and unchanged noise
gain, zero/partial/full volume, timer bounds, and recording worker output through
live volume changes, fades and two repeats. These worker checks use simulated
decoder PCM; they do not execute the target decoder. An independent review finds
no credible defect or unnecessary mechanism within this slice. ESP-IDF 5.5.5
builds successfully at 17,200,336 bytes, with 18% application partition free
(144 bytes more than the noise-restoration build).

An independent complete host decode of all nine prepared MP3s matches the retained
sample counts and peaks; all 18 original/prepared file hashes match before and
after. The maximum-volume PCM conversion simulation reaches its guard zero
times. The smallest reconstructed peak margin is 3.614 dB at PCM output, or
0.115 dB after the estimated codec gain. This narrow latter margin is a computer
estimate, not measured analog clearance or proof of distortion-free board
playback. The host measurements alone do not verify actual target decoding or
physical output. The board and staged browser-install images remain at the
previous noise-restoration firmware at the software checkpoint. The subsequent
authorized application-only flash loads the verified recording-gain build with
a matching written hash and boot ELF identity (`e6601b3c4…`). Display/touch
initialization and the first live weather request succeed, with no captured
panic, abort or watchdog failure during startup and the settling interval.
Saved settings, bootloader, partition table and MP3 storage are not rewritten.
The staged browser-install images still contain the prior noise-restoration
build; no commit, push or merge occurred. The user confirms all nine recordings
sound good with no distortion or cutouts in the requested brief listening check
at their usual volume, including maximum-volume checks of Rain, Thunder, Campfire
and Singing Bowl. This closes the gain slice's requested listening checkpoint;
it does not measure acoustic levels or establish every long-run workload.
Subsequent listening feedback identifies a separate balance issue: Night and
Campfire sound soft relative to the other recordings, while White/Pink/Brown
Noise are still too loud. Clarity/cutout acceptance remains recorded; relative
loudness needs adjustment.
Complete-file host RMS measurements support the level difference, but are not
perceived-loudness targets. Night has substantial estimated peak clearance;
Campfire's isolated peaks leave only about 0.115 dB after estimated codec gain
at maximum volume in the first build.

The user subsequently approves trying the larger output boost for both Night
and Campfire, plus the proposed noise-output reductions. The current values and
accepted Campfire clipping tradeoff are in the
[gain rules](architecture.md#recorded-sound-playback-gain); the earlier values
and listening acceptance above describe the preceding build. The adjustment
changes only the existing gain selection, with three net firmware lines and
34 net test lines. MP3 files, generator equations, volume curve, fade, alarms,
codec setting, final PCM guard and playback ownership remain unchanged. No new
source path, setting, state, task or processing pass is added.

All 15 focused audio checks and all 44 native checks pass. Independent dB-derived
expectations cover all 12 choices and the existing volume/fade checks; constructed
loud Campfire samples verify positive/negative saturation without integer
wraparound while quieter samples retain their scaling. Read-only independent
review finds no actionable defect or unnecessary mechanism. ESP-IDF 5.5.5 builds
at 17,200,352 bytes, with 18% partition free, 16 bytes above the preceding build.
Full signed16 host decodes at maximum volume reach the final guard for 63 of
Campfire's 1,454,328 samples (about 0.0043%); the other eight recordings reach it
zero times. This numerical result does not determine whether the crackles sound
distorted or measure physical output. The ongoing listening experiment's
application-only comparison flash verifies its written hash and boot ELF identity
(`515463814…`). Display/touch initialization and live weather status 200 succeed,
with no captured panic, abort or watchdog failure through the post-weather
settling interval. No settings, MP3 storage, bootloader or partition table is
rewritten. The staged browser-install images remain at the earlier noise-source
restoration build. The user confirms the balance is better for Night, Campfire
and all three generated noises in the requested same-volume comparison, and
Campfire sounds acceptable at 100% in the requested high/lower-volume check.
This closes the balance-adjustment listening checkpoint; it does not establish
measured acoustic levels or absence of clipping. No commit, push or merge is
performed. Evidence belongs in
[the balance check](../.reviews/main/sound-level-balance-2026-10-14/closure.md).
See [relative-level evidence](../.reviews/main/recorded-playback-gain-2026-10-14/relative-levels.json).
See [slice verification](../.reviews/main/recorded-playback-gain-2026-10-14/closure.md)
and its linked peak evidence.

The earlier gain-only demos and computer peak checks are retained under
`.reviews/sound-machine-gain-demos-2026-10-14/`; they do not establish board
playback or analog-output behavior. The retained demo settings predate the final
common-gain simplification; follow the authoritative architecture values.

## Clock shortcut spacing — October 14, 2026

The user's board photo shows the speaker crowding the start of Wednesday on
the Analog clock. The backup-clock warning uses a mirrored fixed position and
has the matching risk on the right. The existing shortcut test uses Friday and
checks coordinates without measuring clearance from the longer weekday text.
The bounded correction moves both shortcuts outward equally, preserving the
weekday/date fonts and positions, vertical icon alignment, 88×80 tap-area sizes,
alarm shortcut and navigation. No new layout mechanism, source path or asset is
needed. Firmware and the official mockup now move both shortcuts outward by
24 pixels. The new text-bound assertion reproduces the overlap on the old layout
and passes for all 28 weekday/face/appearance combinations after the correction.
Wednesday measures 201 pixels wide, with minimum remaining icon clearances of
16 pixels for the speaker and 13 for the warning. Date labels remain clear;
warning and stopped/playing speaker icon taps reach their expected screens
without settings writes. Unavailable-time positions follow the same horizontal
shift, with their existing vertical placement preserved.

Parent verification passes all 44 native checks in 17.47 seconds. ESP-IDF 5.5.5
builds at 17,200,352 bytes, with 18% partition free and no size increase from
the audio-balance build. All 11 inline mockup script blocks parse and both source
positions match firmware; browser rendering was not checked. The production
change replaces two coordinate values, with no new source file or runtime
mechanism. The existing UI test adds 72 net lines to cover actual text bounds
and navigation. The subsequently authorized application-only flash verifies
written-data hash and boot ELF identity (`de829d797…`). Display/touch
initialization and live weather status 200 succeed, with no captured panic,
abort or watchdog failure through the post-weather settling interval. Saved
settings, MP3 storage, bootloader and partition table are not rewritten. The
official mockup's two saved positions were rechecked after the user's reminder
and match firmware for both faces and appearances. The user confirms the spacing
looks good and icon taps work in the requested Analog Wednesday check, with the
warning included when visible. This closes that basic physical checkpoint; it
does not establish every face/appearance or independently confirm that a warning
was present in the check. Staged browser-install images still hold the earlier
noise-source-restoration build. No commit, push or merge occurred. See
[local spacing evidence](../.reviews/main/clock-shortcut-spacing-2026-10-14/closure.md).

## Routine clock footer removal — October 14, 2026

The user approved removing routine Wi-Fi and weather-fetching messages from the
clock display while preserving meaningful weather and clock warnings. The
[earlier inventory](../.reviews/main/footer-status-inventory-2026-10-14/report.md)
records the inherited messages before this change; it is historical evidence.
The bounded edit removes their startup seed and weather-worker publications in
`main/main.c`, rather than adding a display filter. Setup feedback still comes
from connectivity/settings, and operational logs and all network/retry schedules
remain. The initial empty-weather presentation is unchanged.

The existing publication helper now retains warning text when a connection
update supplies no text. Losing Wi-Fi with retained weather marks it stale and
uses the existing weather-failure notice. A retry no longer replaces a warning
with Fetching live weather; only a successful download clears it. Existing
clock/Forecast/Conditions warning wording, positions, Night visibility, storm
warnings, Clock paused, Waiting for sync and backup-clock notices remain.
There is no new module, source path, task, queue or storage. The official mockup
already omits these routine footers, so no mockup edit is needed.

Verification passes all 44 native checks in 16.97 seconds. The existing worker
checks now cover quiet setup and connection waits, retained warnings during
requests/retries and Wi-Fi loss, successful clearing, retained weather and
unchanged scheduling. Existing LVGL checks retain weather/clock warning rendering
and navigation. These are controlled inputs, not live network observations.
ESP-IDF 5.5.5 builds successfully at 17,199,856 application bytes, with 18% of the
application partition free. An independent gpt-6.1-sol/high review found no
credible defect or needed structural addition and independently reran the worker
test successfully. Source inspection confirms the eight removed Wi-Fi/setup/fetch
text values are absent from the weather worker; only the two existing weather
failure texts remain. The source-path check and `git diff --check` pass. Build
and test logs are retained under `.reviews/main/quiet-clock-status-2026-10-14/`.
No new source file, commit, flash or published browser-image update is part of
this slice; physical rendering remains unverified on this build.

## Notification walkthrough — October 15, 2026

The approved firmware items below are now implemented and verified in the
[notification sprint](#sprint-approved-notification-changes--authorized-october-15-2026).
Their earlier pending statements record the decision-by-decision checkpoint.

The user requested a fresh comparison of implemented notices against the official
mockup, with keep/remove/change decisions made one at a time. Earlier proposed
alarm-failure pop-ups are not approved or implemented. Firmware changes are being
collected for later implementation; this walkthrough updates the mockup and
records the approved decisions.

Approved firmware changes:

- **First weather download on Analog and Digital — approved; firmware pending.**
  Apply the [waiting presentation](architecture.md#weather-contract) during failed
  attempts before any weather arrives, removing the additional failure footer.
  The official mockup now uses that waiting layout for both its waiting and
  failure preview states on the two clock faces. This approval did not change
  Forecast or Conditions. The current firmware source and loaded board
  have not received this change.
- **Failed refresh on Analog and Digital — approved; firmware pending.**
  Apply the [retained-weather notice rule](architecture.md#weather-contract),
  replacing the long bottom failure footer with the actual age or stale notice
  in the existing weather area. The official mockup's age-unavailable wording
  is now `Weather data stale`. Its age-known example and both notice positions
  are retained. Firmware still needs the age calculation, fallback selection,
  placement and removal of the additional footer; the current source and board
  have not received these changes. This approval did not change Forecast or
  Conditions.
- **Forecast before the first successful download — approved; firmware pending.**
  Apply the [Forecast waiting rule](architecture.md#weather-contract), replacing
  `Updating…` with `Waiting for weather…` and removing the separate first-download
  failure notice. Preserve the existing firmware's unknown provider dates and
  temperature placeholders, hidden icons, frames, retries and Day/Night waiting
  visibility. The official mockup now reflects this rule, including `--` instead
  of sample day names while waiting. The current source and board remain
  unchanged; retained Forecast weather and Conditions are separate decisions.
- **Forecast retained-weather notice placement — approved; firmware pending.**
  Move the [retained-weather notice](architecture.md#weather-contract) directly
  above the primary forecast's day heading, using the approved comparison's
  y120 instead of y48. The official mockup now reflects this placement. Keep the
  22px font, colors, existing Night hiding and all other forecast positions and
  behavior. This approval covers placement; the existing `Weather data stale`
  wording is unchanged, and the elapsed-age wording shown as an alternative in
  the comparison has not been separately approved for Forecast. The current
  firmware source and loaded board have not received the placement change.
- **Conditions before the first successful download — approved; firmware pending.**
  Apply the [Conditions waiting rule](architecture.md#weather-contract): change
  the curved heading to `Waiting for weather…` and remove the separate bottom
  failure notice. The official mockup now uses this presentation for both its
  waiting and first-download failure preview states. Preserve the existing
  compass, column icons, unavailable readings, heading size/colors, Day/Night
  heading visibility and retries. Retained Conditions weather remains a separate
  decision. The current firmware source and loaded board are unchanged.
- **Conditions retained-weather notice wording — approved; firmware pending.**
  Apply the [retained-weather age/fallback rule](architecture.md#weather-contract)
  below the three Conditions readings. The official mockup now uses the shared
  age-known example (`Weather updated 2h ago`) or `Weather data stale` when time
  is unreliable. Keep its existing position, 22px font, colors, Night hiding,
  retry behavior and clearing after a successful download; use no ending period.
  Actual age tracking and selection remain pending firmware work. No current
  firmware source or loaded-board behavior changed with this approval.
- **Backup-clock Dismiss failure diagnostic — approved; firmware pending.**
  Apply the [log-only failure rule](architecture.md#time-and-rtc): remove the
  on-screen `Dismiss failed (...)` diagnostic and retain the technical detail
  in logs. Preserve the warning, pending/acknowledgment handling and ability to
  retry; a failed submission must not dismiss the notice. The official mockup
  already has no diagnostic line, so no mockup edit is needed. The firmware
  source and loaded board remain unchanged.
- **Alarm action submission diagnostic — approved; firmware pending.**
  Apply the [alarm command feedback rule](architecture.md#execution):
  remove `Action not sent. Try again.` from the screen and retain the existing
  technical log. Preserve rejection handling, the actual alarm state, pending
  rules, retry controls and restart recovery. The official mockup already omits
  this diagnostic; no mockup edit is needed. Firmware implementation remains
  pending, with the source and loaded board unchanged. This decision does not
  cover execution, storage or audio errors.
- **Alarm action execution diagnostic — approved; firmware pending.**
  Apply the [alarm command feedback rule](architecture.md#execution): remove
  `Action not applied. Try again.` from the screen and retain the existing
  technical logs. Preserve completion matching, rejection handling, the actual
  alarm state, pending rules, retry controls and restart recovery. The official
  mockup already omits this diagnostic; no mockup edit is needed. Firmware
  implementation remains pending, with the source and loaded board unchanged.
  This decision does not cover storage or audio errors.
- **Alarm operating-record storage captions — approved; firmware pending.**
  Apply the [alarm storage feedback rule](architecture.md#alarm-and-snooze): remove both
  `Could not save alarm state.` and `Alarm storage error.` from the alarm
  surfaces and retain their technical details in logs. Preserve live alarm
  actions, read/write failure handling, stored records and restart recovery.
  Alarm On/Off and settings-save feedback are outside this decision. The
  official mockup already omits both captions; no mockup edit is needed.
  Firmware implementation remains pending, with the source and loaded board
  unchanged. Audio failures remain a separate item.
- **Alarm audio failure caption — approved; firmware pending.**
  Apply the [alarm audio feedback rule](architecture.md#execution): remove
  `Alarm audio unavailable.` from the ringing screen and log playback failures.
  Add the missing application logging for runtime playback failures; existing
  startup and lower-level driver logs do not cover every playback failure.
  Preserve audio failure/retry handling, live alarm state, Snooze, Dismiss and
  restart recovery. Preview and Sound Machine editor feedback is outside this
  decision. The official mockup already omits this caption; no mockup edit is
  needed. Firmware implementation remains pending, with the source and loaded
  board unchanged.

Approved unchanged notices:

- **Clock paused — approved to keep unchanged.** Preserve the existing
  [time warning](architecture.md#time-and-rtc). Its text, icon, position,
  appearance and behavior already match the official mockup; no mockup or
  firmware edit is needed for this decision.
- **Waiting for sync — approved to keep firmware unchanged.** Preserve the
  existing [backup-time notice](architecture.md#time-and-rtc). The official
  mockup's clock-notice icon now uses stroke 2, matching the firmware icon's
  source geometry; the override is limited to the clock-notice row. Other icons,
  wording, layout, colors and behavior are unchanged. No firmware work is
  required for this decision.
- **Time unavailable — approved to keep firmware unchanged.** Preserve the
  existing unavailable-time screen and behavior. The official mockup's `--:--`
  now uses the firmware font's glyph pixels at the firmware's spacing and
  vertical position; its Day/Night text color still follows the existing theme.
  The small mask is embedded in the existing mockup assets, with no new asset
  file. Wording, other controls, and firmware are unchanged. No firmware work is
  required for this decision.
- **Backup Clock Lost Time / Check Battery — approved to keep unchanged.**
  Preserve the existing [backup-clock warning](architecture.md#time-and-rtc)
  and its Dismiss behavior. The official mockup already represents this notice;
  no mockup or firmware edit is required for this decision.
- **Backup Clock Unavailable / Check Connection — approved to keep unchanged.**
  Preserve the existing [backup-clock warning](architecture.md#time-and-rtc)
  and its Dismiss behavior. No mockup or firmware edit is required for this
  decision.
- **Missed Alarm — approved to keep unchanged.** Preserve the existing
  [missed-alarm notice](architecture.md#alarm-and-snooze), its appearance and
  tap-to-dismiss behavior. Dismissing it clears the pending notice while
  leaving the recurring schedule enabled. No mockup or firmware edit is
  required for this decision.
- **Startup settings-storage failure — existing firmware preserved; mockup added.**
  The user approved adding this existing screen to the official mockup without
  changing firmware behavior. The `Startup Storage Failure` preview now shows
  its current title, detail, fixed startup colors and centered layout. Its
  error code is explicitly illustrative; no storage failure is being claimed
  on the board. This completes the reference addition, not a firmware change.

Mockup verification after the third approval: all ten executable scripts parse.
Eight Forecast weather-state/appearance outputs pass checks for the single
waiting message, ten unknown day/temperature labels, hidden icons, retained
frames and unchanged populated/stale presentations. Sixteen clock-face outputs
also pass; the earlier thirty-two home checks covered the approved age fallback
and notice positions, and those builders are unchanged by this Forecast edit.
The reference font measures the new waiting text at 350px within the existing
400px area. Each edit was limited to its approved builder or wording; the
separate browser messaging preview is unchanged. `git diff --check` passes.
These are source-level checks; browser rendering and board appearance were not
tested. No firmware source, build, flash or commit is part of this walkthrough.

Verification after the Forecast placement approval: all ten executable mockup
scripts parse, and eight Forecast state/appearance outputs pass the y120 notice
placement, retained data, first-download waiting and Night-hiding checks. The
decoded edit changes only the retained Forecast notice's y coordinate; all other
builders and the separate browser messaging preview are unchanged. The next,
unapproved Conditions first-download comparison uses native firmware font pixels
and source positions for its current panel and the unchanged official Conditions
builder for its reference panel. Both Day/Night outputs pass checks for the
curved heading, compass, three empty columns and Day-only failure footer. An
independent read-only source check confirmed that invalid weather retains the
Conditions construction layout, including `-- mph`; it does not take the later
populated-weather update path. `git diff --check` passes. These are source-based
reconstructions, not board captures; no new Conditions decision or firmware
implementation is recorded.

Verification after the Conditions waiting approval: all ten executable mockup
scripts parse, and sixteen Conditions weather-state/appearance/storm outputs
pass the single curved waiting message, preserved compass/icons/placeholders,
removed first-download footer and unchanged retained-weather notice checks.
The edit is confined to the Conditions builder; the separate browser messaging
preview is unchanged. The next retained-weather comparison passes both Day and
Night, using firmware font pixels and source positions with illustrative weather
values alongside the official builder. Its existing `Weather data stale` wording
and bottom position remain unchanged. The proposed elapsed-age wording for
Conditions is unapproved; no firmware implementation, build, board test, flash
or commit is included. `git diff --check` passes.

Verification after the Conditions retained-weather approval: all ten executable
mockup scripts parse, and thirty-two Conditions state/appearance/time-trust
outputs pass age selection, stale fallback, the unchanged position and Night
hiding, and the approved first-download waiting layout. Forecast remains at its
approved position with its existing stale wording. The next `Clock paused`
comparison shows notice details only; its four face/appearance combinations
pass. Independent source inspection confirms matching text, muted colors,
28px font, warning-icon pixels, 10px gap, row positions and Night visibility.
The existing keep decision is preserved; no new Clock-paused change is proposed.
These are source-based comparisons, not board captures. No firmware source,
build, flash or commit is included; `git diff --check` passes.

After the explicit `Clock paused` keep approval, that decision is recorded above
without changing the already-matching mockup or firmware. The next `Waiting for
sync` notice-detail comparison passes all four face/appearance combinations.
It uses the native 28px font and actual 28x28 clock alpha image alongside the
official reference's CSS and Lucide clock. Independent read-only inspection
confirms the RTC/network-time meaning, clearing and priority, and the matching
wording, positions and muted colors. The firmware clock outline uses stroke 2;
the reference uses 1.6. Keeping the firmware notice and matching the reference's
icon weight to it is a proposal awaiting approval. No official mockup, firmware
source, build, board test, flash or commit is changed by this next comparison.
Browser rendering remains unverified; `git diff --check` passes.

After the `Waiting for sync` approval, its keep decision and scoped mockup icon
update are recorded above. All ten executable official scripts parse, and four
clock-face/appearance checks preserve the sync notice text and row geometry.
The scoped stroke-2 override leaves other icons at their existing stroke weight
and the separate browser preview unchanged. The next `Time unavailable`
comparison passes four Day/Night and saved-Alarm On/Off variants, using native
font pixels for the current panel and the unchanged official screen markup.
It includes no missed-alarm notice; that existing variant is a separate item.
The current placeholder uses the tight digit font and -2px spacing, while the
reference uses ordinary browser text metrics and zero spacing. Keeping firmware
unchanged and aligning that reference placeholder is an unapproved proposal.
No firmware source, build, board test, flash or commit is included. Browser
rendering remains unverified; `git diff --check` passes.

After the `Time unavailable` keep approval, the mockup's placeholder uses the
native glyph mask (767 embedded PNG bytes), with the same 247px text width,
104px line box and placement as the firmware. All ten executable official
scripts parse, and eight Day/Night, Alarm On/Off and missed-notice variants
preserve captions and button positions. Other markup and the separate browser
preview are unchanged. The next `Backup Clock Lost Time / Check Battery.`
dialog-detail comparison passes both appearances with native font pixels for
the current panel and unchanged reference markup. Independent read-only review
confirms the wording, layout, colors, lost-time evidence and dismissal behavior;
it is not a battery-voltage measurement. Keeping this notice unchanged remains
a proposal awaiting the user's decision. No firmware source, build, board test,
flash or commit is included. `git diff --check` passes.

After approval to keep `Backup Clock Lost Time / Check Battery.` and its Dismiss
behavior, that unchanged decision is recorded above. The next `Backup Clock
Unavailable / Check Connection.` comparison passes both Day/Night appearances,
using the same source-verified dialog layout and native title/detail fonts.
The 422px title and 269px detail fit their existing 460px areas. Wording, colors
and Dismiss layout agree with the official reference. Keeping this connection
notice and its existing Dismiss behavior is a proposal awaiting approval.
The official mockup and firmware are unchanged; these are source-based dialog
details, not board captures. No build, board test, flash or commit is included.
`git diff --check` passes.

After approval to keep `Backup Clock Unavailable / Check Connection.` and its
Dismiss behavior, that unchanged decision is recorded above. The next comparison
shows the separate `Dismiss failed (ESP_ERR_TIMEOUT).` diagnostic using an
explicitly illustrative error, not an observed board failure. Both appearances
pass native text-fit and geometry checks; the official dialog has no error line.
Read-only review confirms that production errors here concern submitting the
Dismiss request, not RTC access or settings storage. The existing test injects
the timeout through a stub; it does not establish a production failure.
Removing the on-screen diagnostic, retaining technical detail in logs, and
preserving acknowledgment/pending/retry behavior is an unapproved proposal.
The official mockup and firmware remain unchanged. No build, board test, flash
or commit is included; `git diff --check` passes.

After approval to remove the backup-clock Dismiss failure diagnostic, its
log-only rule and pending firmware change are recorded above. The official
mockup already omits that diagnostic and remains unchanged. The next comparison
shows `Action not sent. Try again.` in the ringing screen's lower section,
alongside the official `Alarm ringing` footer. It uses native firmware font
pixels and source geometry; the rejected Snooze is illustrative, not a board
observation. Both Day/Night outputs pass content and placement checks, and all
ten official executable scripts still parse. Independent read-only review
confirms that the existing technical log, unchanged owner state and enabled
retry controls cover submission rejection; no ordinary UI trigger for a full
queue was established. Removing this alarm submission diagnostic from the
screen is a proposal awaiting the user's decision, separate from later action,
storage and audio errors. No firmware implementation, build, board test, flash
or commit is included. Browser rendering remains unverified; `git diff --check`
passes.

After approval to remove `Action not sent. Try again.` from the screen, its
log-only rule and pending firmware change are recorded above. The official
mockup already omits this diagnostic and is unchanged. The next comparison
shows the separate `Action not applied. Try again.` completion diagnostic
against the official `Alarm ringing` footer. Both Day/Night outputs, restored
preview state and footer placement pass checks. Native font measurements and
LVGL's wrapping rules put the 417px wording on two lines in the existing 400px
ringing footer; the comparison preserves that layout. Independent read-only
review confirms this concerns an accepted command rejected during execution,
including protection against an action for an occurrence no longer current.
The illustrated failure is not an observed board fault. Removing this visible
diagnostic while retaining logs, completion matching, actual alarm state and
retry controls is a proposal awaiting approval; storage and audio errors remain
separate items. No firmware source, build, board test, flash or commit is
included. Browser rendering remains unverified; `git diff --check` passes.

After approval to remove `Action not applied. Try again.` from the screen, its
log-only rule and pending firmware change are recorded above. The next
comparison covers alarm operating-record storage feedback: `Could not save
alarm state.` after a completed action and the broader `Alarm storage error.`
fallback. Removing only the first wording would expose the fallback after a
genuine save error; the fallback also covers loading and other operating-record
writes. Both are shown as selectable illustrative variants in the Snoozed
screen's lower section against the official `Repeating alarm stays on` caption.
Four wording/appearance outputs, restored preview state, native font fit and
source placement pass checks; the current official footer, font and background
assets match the comparison, and all ten official executable scripts parse.
Independent read-only review confirms that Snooze/Dismiss take effect before
their record is saved, while a failed save can leave older restart information.
No such storage failure has been established on the board. Keeping these
technical storage details in logs rather than on the alarm surfaces is an
unapproved proposal; its loss of on-screen warning about restart information
does not change the existing storage or recovery behavior. Alarm On/Off and
settings-save feedback, and audio failures, are outside this storage-notice
proposal. No firmware source, build, board test, flash or commit is included.
Browser rendering remains unverified; `git diff --check` passes.

After approval to remove both alarm operating-record storage captions, their
log-only rule and pending firmware change are recorded above. Live alarm
actions, storage behavior and restart recovery remain unchanged; settings-save
feedback is outside that decision. The next comparison shows `Alarm audio
unavailable.` against the official `Alarm ringing` footer. Both Day/Night
outputs, restored preview state and placement pass checks; the native 354px
wording fits the 400px footer. The current official footer, font and background
assets match the comparison, and all ten official executable scripts parse.
Independent read-only review confirms that the caption reports not-ready or
error audio status during ringing, below the existing action/storage errors
in display priority. A previous preview/Sound Machine error can briefly carry
over until a new playback attempt updates that shared status. The illustrated
failure is not an observed board fault. Keeping this audio warning and adding
its failure variant to the official mockup is an unapproved recommendation.
Application logging currently covers audio startup failure but does not cover
every runtime playback failure; removal cannot be described as simply keeping
complete existing runtime logs. No firmware source, official mockup, build,
board test, flash or commit is changed by this comparison. Browser rendering
remains unverified; `git diff --check` passes.

The approved removal of `Alarm audio unavailable.` and addition of playback-
failure logging supersede the preceding unapproved keep recommendation. The
pending firmware list and architecture now record that decision; playback
handling, alarm behavior and recovery are unchanged. The official mockup
already omits the caption and needs no edit for this approval.

The next comparison is `Missed Alarm`, using an explicitly illustrative
previous-day occurrence. Native font and icon pixels show the current notice
alongside the official `missedNotice()` builder, with matching row bounds,
font/icon sizes, colors and dismissal meaning. Four face/appearance outputs,
restored preview state and source bounds pass checks; the native 380px sample
fits its 428px text area. The reference builder/font match the current official
mockup, and all ten official executable scripts parse. Independent read-only
inspection confirms that an unhandled ordinary occurrence more than 30 minutes
overdue creates one retained missed notice; tapping it clears that notice while
preserving the recurring schedule. This is not a timeout for unanswered ringing
or an expired snooze. Keeping this notice and its behavior is an unapproved
recommendation. Firmware and official mockup are unchanged by this comparison;
no build, board test, flash or commit is included. Browser rendering and the
previously deferred long-outage board case remain unverified.
`git diff --check` passes.

The user approved keeping `Missed Alarm` and its dismissal behavior unchanged;
that decision is recorded above. The remaining items in the original clock-
notice inventory were checked without treating existing features as new
approval requests. Weather-warning and unread/important-message edge
indicators retain their approved purpose, placement and behavior. Weather-arc
alpha pixels and Day/Night colors match the official embedded artwork exactly;
the firmware's pulse is not animated in the static reference. The message
indicator has small Night-tint and pulse-preview differences, with no practical
problem established or firmware change recommended.

The existing startup `Settings storage unavailable` screen was absent from the
official mockup at the final inventory check. It explains startup being stopped
after a real settings-storage failure, as already required by the architecture.
The user subsequently approved the reference addition recorded below, without
removing or redesigning the screen. No additional firmware change is recommended
for these remaining items. This closes the remaining
clock/Forecast/Conditions notice inventory at the source-review level; it is
not a review of every editor or messaging-page failure message. Approved
firmware implementation remains pending. No additional mockup comparison,
source edit, build, board test, flash or commit is part of this final check.
`git diff --check` passes.

The authorized startup-screen reference addition is complete. The official
mockup now has a `Startup Storage Failure` preview entry, using the existing
preview routing and text styling. The illustrative `ESP_ERR_NVS_NO_FREE_PAGES`
sample uses the firmware's native four-line detail wrapping, 32px/24px fonts,
35px/27px line heights and centered positions. Its fixed Day colors remain
independent of saved appearance; Night and Time preview controls are hidden for
this startup state and restored when leaving it. The screen has no clock art,
buttons or new behavior. No asset, source file, preview framework or firmware
mechanism was added.

All ten executable official scripts parse. Four Night/time combinations pass
the screen-content and navigation checks using the actual new builder, render
function and preview-click branch with a substituted document. Native source
geometry and font layout were also inspected. Reversing the five scoped
mockup edits exactly restores the prior decoded preview; the separate browser
messaging preview and outer document are unchanged. Independent
gpt-6.1-sol/high source review found no issue and confirmed wording, layout,
colors and reachability. `git diff --check` passes. Browser rendering and board
pixels were not tested. Firmware source, firmware behavior and loaded board are
unchanged by this documentation-only addition; no firmware build, flash or
commit is included.

## Sprint: approved notification changes — authorized October 15, 2026

Implement the approved [notification walkthrough](#notification-walkthrough--october-15-2026),
update its completion record and any affected instructions, verify the official
mockup, build, flash and test autonomously. The user authorizes this firmware
load and unattended testing. Preserve the dirty checkout and unrelated work;
no commit, push or merge is included.

The behavior changes are limited to the approved waiting/stale/age presentation,
Forecast notice placement and log-only backup/alarm diagnostics. Preserve
downloads, retries, alarm state and recovery, Dismiss/Snooze actions, Night
visibility, retained weather, and the existing notices/indicators selected to
remain. The existing startup storage-failure screen stays unchanged; its
reference addition is already complete. Separate Radar and weather-provider
work is not implementation scope for this notification sprint.

Bounded implementation and acceptance:

1. **Weather publication and presentation.** Permitted existing files:
   `main/main.c`, `main/app_runtime.h`, `main/ui/ui.c`,
   `main/ui/clock_views.c`, `main/ui/weather_views.c`, `main/ui/ui_private.h`,
   `tests/test_weather_service.c`, `tests/test_ui.c` and the existing test
   `CMakeLists.txt` extraction list, all under
   `targets/esp32-p4/`. Reuse the download timestamp and owner snapshots for
   actual age with a conservative stale fallback; remove obsolete failure
   footers/status text if no consumer remains. Verify waiting and retained-data
   states, both appearances, age/fallback selection, provider dates and unchanged
   retries, plus approved notice positions and absence of duplicate footers.
2. **Log-only diagnostics.** The same UI files plus existing `main/audio.c`
   and `tests/test_audio.c` may change. Preserve command matching, pending and
   retry behavior and alarm recovery while removing the approved diagnostic
   captions and unused rendering state. Add narrow playback-failure logging in
   the existing worker; verify accepted/rejected actions and error transitions.
3. **Integration and documentation.** Update `docs/work.md` and affected
   existing architecture/test instructions; verify `mockups/alarm-design.html`
   against the approved states, changing it only if an approved item remains
   missing. No new source path, module, task, queue, persistence mechanism or
   test framework is planned. Run affected native checks and the full existing
   suite, build with ESP-IDF 5.5.5 and independently review correctness and
   necessity before loading.
4. **Unattended board verification.** Identify the board, preserve saved
   preferences, load the verified application and inspect boot identity,
   runtime/log and display state through existing serial/debug facilities.
   Exercise supported states without requiring the user to set up or time the
   tests, restore any temporary changes, and retain evidence locally. Report
   physical/acoustic/visual claims only to the extent actually measured.

### Completion and verification

**Completed October 15.** Implemented the approved waiting/retained-weather
presentation, age/stale fallback, Forecast notice placement and log-only
backup/alarm diagnostics. The worker stamps the existing download time only
when network time is usable; the UI formats it without another clock or weather
owner. Removed the raw footer field, obsolete publication/text helpers, UI error
buffers and backup error label; reused the two clock waiting labels with one
rendered-visibility flag. No new source file, module, task, queue, persistence
mechanism or library was added. Alarm actions, storage/recovery behavior, retry
schedules and settings-save feedback are preserved.

All 44 native checks pass (18.03 seconds), including age boundaries/fallbacks,
waiting/stale transitions, Night visibility, positions, action failures and alarm
recovery. Existing weather/location/Conditions parity checks pass. ESP-IDF 5.5.5
build passes at 17,200,192 bytes with 18% of the application partition free.
Independent gpt-6.1-sol/high correctness and YAGNI/ownership review found no
credible defect or needed structural addition. Source-path and whitespace checks
pass. The existing official mockup already includes every approved change and
the startup storage-failure reference; independent gpt-6-luna/xhigh source review
and syntax checks of all eight executable inline scripts pass. No additional
mockup edit was necessary.

The authorized application-only flash verifies its written hash, and boot/running
identity matches ELF `8c63caa7f56b2f31cbd8b338e2cd44d42a383e4776bc4d92f91cdb7b4a011162`.
NVS and recorded-sound storage were not flashed. Autonomous board checks pass
26 states: eight first-download waiting states across Day/Night; twelve retained
weather age/fallback/Night states; and six backup/ringing presentation states.
A debugger-forced first-download timeout follows the actual twenty-second retry;
the next real Visual Crossing request returns HTTP 200 and restores live weather.
Other error cases change only temporary display snapshots, not service owners.
Applied-preference and alarm operating-record hashes match before/after the
completed sequences. Normal Digital Day, fresh weather and idle alarm/audio are
restored, and final UI clock samples advance three seconds. All task-owned serial
and debugger processes have ended.

These board checks inspect real LVGL objects, not physical pixels, touch feel or
sound quality. A full framebuffer read timed out and was abandoned after resuming
the board. An invalid post-download-to-empty Conditions probe was replaced by a
proper fresh-boot check; a button/child-label inspection was corrected. These test
method limits, raw evidence and closure are in the
[local sprint report](../.reviews/main/notification-sprint-2026-10-15/closure.md).
No new power-cut or listening test, browser-image update, commit, push or merge
was included. The existing browser bundle and unrelated work remain preserved.

## Checkpoint commit — authorized October 15, 2026

The user authorized a local commit after the notification sprint closed.
The checkpoint includes completed Sound Machine playback and its measured
memory/scheduling corrections, subsequent approved sound/icon changes,
the notification sprint, tests and accumulated approved documentation/mockups
since `2ca209a`. The latest source passes the software and autonomous board
checks recorded in the [notification closure](#completion-and-verification).

Include the previously prepared four-image browser installer bundle and its
recordings image. That bundle remains the October 14 preparation documented
above; its application is older than the October 15 board image and was not
regenerated for this commit. Local `.reviews/`, scratch output and editor
configuration remain uncommitted. No push, merge, tag or additional firmware
load is included.

## Current work — reconciled October 15, 2026

This is the current work list. Dated entries above retain the observations and
limits at their own checkpoints; their old remaining-work statements do not
reopen checks subsequently closed. Startup and inherited-screen cleanup remains
the priority under [AGENTS.md](../AGENTS.md#current-checkout-and-retained-material).
The Sound Machine investigation above includes the October 14 firmware and
unattended board comparisons; other checkpoints retain their stated limits.
The approved [routine footer removal](#routine-clock-footer-removal--october-14-2026)
is included in the completed, flashed and autonomously checked
[notification sprint](#sprint-approved-notification-changes--authorized-october-15-2026).

### Approved work still unfinished

- **Sound Machine:** mockup and playback decisions are approved. The
  [implementation sprint](#prepared-sound-machine-sprint) was sent to
  Development Orchestration, including efficient noise generation.
  [Preference storage](#preference-storage-checkpoint--october-13-2026) is implemented
  and the [full source checkpoint](#sound-machine-source-checkpoint--october-13-2026)
  passes software integration and review. The October 14 board investigation
  closes the measured volume-save scan, recording-repeat delay, TLS allocation
  failures and weather/drawing scheduling stalls within the recorded workloads.
  The final matched Rain run crosses repeats and an ordinary ten-minute weather
  refresh: zero wholly silent buffers among 101,845 inspected, and no repeated
  or skipped submitted clock seconds during playback. Retained changes use
  existing memory-placement settings, same-file reopening, a fixed heartbeat,
  one binary weather gate and explicit drawing-core-0/audio-core-1 placement.
  Temporary playback timing probes are removed; UI stack monitoring is retained.
  Normal firmware passes 44 native checks/build and is
  loaded with written-hash and boot-identity verification, working display/touch
  initialization and successful live weather. See the
  [full accounting and verification limits](../.reviews/sound-machine-sprint-2026-10-13/board/timing-closure.md).
  The subsequent approved [review follow-up](#review-follow-up-visual-diagnostics-and-browser-recordings--october-14-2026)
  removes inherited visual logging, corrects the gate comment and closes F1
  browser-image preparation with a verified four-image local bundle. That
  source/bundle passes software checks and independent review. Its application
  changes are included in the subsequent Brown Noise load; browser installation
  remains untested and unpublished. The UI stack monitor stays in place.
  The [Brown Noise investigation](#brown-noise-cutout-investigation--october-14-2026)
  records the retired generator's measured behavior; the user may still have
  heard one very brief cutout with that former generator. The approved
  [noise source-equation restoration](#noise-source-equation-restoration--october-14-2026)
  supersedes that generator correction, removes our clipping and preserves output
  gain. Its source verification, 44 native checks, independent review and build
  pass. The authorized app-only flash has verified written/boot identity and
  normal display/touch initialization and weather. The user confirms all three
  restored noises are continuous and clear in the prompted short listening check.
  The approved [recorded-sound gain slice](#recorded-sound-playback-gain--october-14-2026)
  now passes implementation review, 44 native checks, firmware build and complete
  host-decoded peak checks. Retained MP3 files and generated-noise output gain
  are unchanged. The subsequently authorized application-only flash verifies
  written/boot identity and normal display/touch initialization and weather.
  The user confirms all nine boosted recordings sound good with no distortion or
  cutouts in the requested brief check, including the four maximum-volume checks.
  Later listening identified Night/Campfire as soft and the generated noises as
  too loud. The approved
  output-only balance adjustment passes all 44 native checks, independent review
  and firmware build. Its application-only comparison flash verifies written/boot
  identity, display/touch initialization and weather. The user confirms better
  balance for all five adjusted sounds and acceptable Campfire at 100%, closing
  this balance listening checkpoint.
  These software/buffer observations do not establish every acoustic or physical
  rendering case. The remaining sound/timer/fade/navigation/priority
  checks remain open. These bounded results do not close the whole feature.
  The ADPCM comparison has not changed the approved MP3 format. Preserve the existing weather
  implementation work. Requirements belong in the
  [specification](architecture.md#sound-machine-playback).
- **Radar:** RainViewer and the custom palette are selected under the
  [radar specification](architecture.md#radar-source-and-palette). The refined
  screen design is selected and integrated into the official mockup's Weather Radar
  preview. Five-minute checks and fixed 50-mile coverage are approved. The
  [prepared radar sprint](#prepared-weather-radar-sprint--october-15-2026) is held
  unsent at the user's request. Its first step verifies the production background
  and labeled 50-mile view. The sprint now replaces the device Conditions page
  with Radar, retaining Conditions code/assets/tests and its mockup page while
  removing access through the device UI. It also removes falling
  pressure from storm-alert triggers while preserving shared weather data and
  the remaining alert behavior. No live radar firmware exists yet.
- **Immediate Alarm Settings On/Off:** development checks and firmware build pass
  in the October 13 slice above. It is included in the weather sprint's firmware
  loads, with its physical check still unreported. Its remaining checkpoint is the physical check
  of the shortcut, enabled-only save, Cancel with other draft edits and ordinary
  alarm operation. Keep the existing board firmware and evidence distinct from
  this development-only change.
- **Backup-clock wiring guide:** document the supported DS3231 module's connection
  to the Waveshare board, shared I2C bus and coin-cell placement. Hardware selection
  and user-facing battery limits remain in the root README.
- **Physical layout and settings behavior:** basic Day controls/typing, initial
  keyboard pages, Night settings readability, Weather's new four rows and
  Messaging guidance/Done clearance have passed within their recorded limits.
  The [clock shortcut spacing correction](#clock-shortcut-spacing--october-14-2026)
  passes native checks/build and its authorized app-only flash verifies
  written/boot identity, display/touch initialization and weather. The user confirms
  corrected spacing and icon taps in the prompted Analog Wednesday check.
  Remaining checks concern exact approved positions/typography/colors; separately
  unreported appearances and all Night keyboard pages; noon/midnight AM/PM
  crossings; validation/error/empty/pending and real connection-phase states;
  Wi-Fi scrolling beyond three networks; Brightness Done/summary, visible
  12-hour padding and both applied brightness levels; Weather Back/Cancel retaining
  Device edits and independent restart; stale/offline Conditions format Cancel
  and other captions; and alarm/editor restoration. Include the older Settings-entry
  immediate Alarm save, whose physical check is still unreported. Preserve passed
  Display Back/Cancel/live Save, analog caption/tap, Conditions `3 PM` / `15:00`
  updates, new Weather checks and Messaging observations; do not repeat them just
  to extend the list. Native checks do not establish exact panel pixels or touch.
- **Night Shift:** the populated Solar page, draft preview/Cancel, zero-offset
  Save/restart and sunset activation were observed. Remaining checks concern
  sunrise, precise transition timing, nonzero saved offsets, configured fades
  and offline operation. Day 100%/Night 25% was restored and subsequently confirmed;
  there is no outstanding temporary-50% restoration task. Record and restore the
  user's then-current preferences if later testing changes them. Equal brightness
  levels cannot establish a fade; trustworthy offline startup needs an attached RTC.
- **Alarm/recovery and audio:** prompted previews, resumed ringing/Dismiss, Off
  canceling Snooze and short power-cut recovery passed. F1/F2 corrections were
  included in the October 12 load but lack a separate board retest. Remaining
  observations concern precise scheduled time, initial Snooze stopping/duration,
  priority over previews/menus/messages and longer operation under weather/touch/RTC
  activity. Keep the 12-minute ring and greater-than-30-minute outage/missed-notice
  checks deferred as requested. The accepted Classic Bell listening result does
  not establish measured normal-path timing and does not reopen declined Bell tests.
- **Board reliability:** the controlled RTC-trigger correction and later overnight
  operation passed their bounded checks. External RTC operation, extended stability,
  network recovery, physical behavior during interrupted settings saves and other
  untested faults remain unverified. Blue flashes remain a separate observation.
  Successful short power-cut checks are recorded under Alarm above; they do not
  establish every power-loss scenario. Retain original/comparison material only
  while it serves an identified current evaluation or recovery need.

### Deferred or unapproved work
- Visual Crossing and mapping implementation is now authorized and underway in
  the [current weather sprint](#sprint-visual-crossing-and-approved-weather-mappings--authorized-october-13-2026).
  Provider-credit placement and real availability of rare provider IDs remain
  separate unresolved/verification items; implementation does not settle them.
- The accepted Shared Device ID query issue involving `&` remains deferred at
  the user's request; no allowed-character restriction or fix is approved.
- Raising the brightness minimum from 20% to 30% remains an unapproved proposal.
  The current range is owned by the architecture; do not infer a changed minimum
  from the earlier recovery observations.

The current implementation checkpoint is the authorized weather sprint above.
The reviewed immediate Alarm Settings switch remains preserved, with its board
status recorded in its own section. The broader combined board pass remains
deferred as recorded; the weather sprint does not reopen it. Completed cleanup
tasks stay closed.

PNG-decoder replacement was declined; retain the existing implementation under the [asset-tool decision](architecture.md#existing-code-changes-that-matter).

Paused-clock warning removal was declined; retain the existing diagnostic under the [time decision](architecture.md#time-and-rtc). Broad removal of weather outputs used by tests is not recommended where those values serve approved behavior or useful calculation checks.

Current work and deferred checks are listed above. Service decisions remain
with their architecture owners and navigation with `ui.c`; audit suggestions
and unmeasured risks do not automatically become approved changes.

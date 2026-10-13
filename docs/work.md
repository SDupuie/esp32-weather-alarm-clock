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
commit or remote action was authorized by this implementation start. The user
subsequently authorized a local commit at the preference-storage checkpoint below. Detailed
local evidence stays under `.reviews/sound-machine-sprint-2026-10-13/`.

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

## Current work — reconciled October 13, 2026

This is the current work list. Dated entries above retain the observations and
limits at their own checkpoints; their old remaining-work statements do not
reopen checks subsequently closed. Startup and inherited-screen cleanup remains
the priority under [AGENTS.md](../AGENTS.md#current-checkout-and-retained-material).
No new firmware or board work was performed during this documentation audit.

### Approved work still unfinished

- **Sound Machine:** mockup and playback decisions are approved. The
  [implementation sprint](#prepared-sound-machine-sprint) was sent to
  Development Orchestration, including efficient noise generation.
  [Preference storage](#preference-storage-checkpoint--october-13-2026) is implemented
  and verified in software; audio commands, playback, timer integration, controls
  and board verification remain pending. Preserve the existing weather
  implementation work. Requirements belong in the
  [specification](architecture.md#sound-machine-playback).
- **Radar:** RainViewer and the custom palette are selected under the
  [radar specification](architecture.md#radar-source-and-palette). The refined
  screen design is selected and integrated into the official mockup's Radar
  preview. Define the remaining map coverage and service behavior before a
  firmware implementation slice. No live radar firmware exists yet.
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

# Weather condition mappings

This document owns the detailed condition-to-icon mappings for Open-Meteo and Visual Crossing. The [architecture](architecture.md#weather) owns location, measurements, refreshes, [quota recovery](architecture.md#visual-crossing-quota-recovery), stale-data presentation and independent [NWS alerts](architecture.md#nws-watches-and-warnings).

The mappings and selection rules below are approved and implemented. Each provider is decoded into shared weather facts, which select our artwork and current-condition wording. The five-day forecast shows pictures without condition captions. Unknown inputs and deliberately unused pictures have defined treatments. Provider choices, units, screen behavior, credits and Radar belong to the [weather specification](architecture.md#weather). The [test instructions](../tests/README.md#weather-and-location) distinguish constructed provider cases from live responses.

## Reading the table

- Open-Meteo numbers are its numeric weather codes. Visual Crossing numbers mean `type_<number>` in `conditions`, requested with `lang=id`. Split and trim all returned IDs; a response can contain several.
- Visual Crossing's quoted icon names are `icon` values requested with `iconSet=icons2`. `preciptype` is an array: retain every value. Specific reported freezing, mixed or hail evidence takes precedence over a broader provider icon.
- **Direct** means the source explicitly identifies the condition family. **Shared artwork** uses the selected family's caption while reusing a picture. **Derived** combines evidence from the same short reporting period. **Approximation** uses the nearest existing picture with an accurate caption. **Unused** means neither provider supplies the precise condition needed to select that picture; it is a defined outcome, not an open mapping decision.
- All Visual Crossing native-ID assignments are conditional on the ID being returned. Its published dictionary is not a confirmed list of conditions emitted in current reports or forecasts. Availability of rare conditions still needs real-response verification. See the [provider's explanation](https://www.visualcrossing.com/resources/documentation/weather-api/weather-condition-fields/).
- Open-Meteo reports hail codes `96`/`99` only from models with an explicit hail forecast. The [provider's code table](https://open-meteo.com/en/docs#weather_variable_documentation) describes that source limitation; a model's ordinary thunder code does not establish hail.
- The 64px images in [weather-icons/](weather-icons/) preview the artwork on a dark background. They are documentation images, not runtime assets; vector sources and firmware storage are described under [Current firmware mapping](#current-firmware-mapping).

## Complete mapping table

The firmware handles the 29 Open-Meteo codes, 43 Visual Crossing native IDs and 16 `icons2` identifiers represented below. The table describes each input's treatment; when a record contains several conditions, the [selection rules](#current-conditions-and-compound-records) choose one picture and caption while retaining accompanying facts for calculation.

| Condition | Our icon | Open-Meteo | Visual Crossing | Treatment |
|---|---|---|---|---|
| Clear, day/night | ![Clear day](weather-icons/clear-day.png) ![Clear night](weather-icons/clear-night.png)<br>Clear day / night | `0` | `43`; `clear-day`, `clear-night` | Direct |
| Mostly Sunny / Mostly Clear | ![Mostly Sunny](weather-icons/partlycloudy-day.png) ![Mostly Clear](weather-icons/partlycloudy-night.png)<br>Partly cloudy day / night | `1`; retain the day/night caption distinction | No distinct native equivalent; `42` remains Partly Cloudy | Shared artwork; Open-Meteo distinction retained |
| Partly Cloudy, day/night | ![Partly Cloudy day](weather-icons/partlycloudy-day.png) ![Partly Cloudy night](weather-icons/partlycloudy-night.png)<br>Partly cloudy day / night | `2` | `42`; `partly-cloudy-day`, `partly-cloudy-night` | Direct |
| Cloudy | ![Cloudy](weather-icons/cloudy.png)<br>Cloudy | `3` | `41`; `cloudy` | Direct |
| Fog / Freezing Fog / Mist | ![Fog with two wavy lines](weather-icons/fog.png)<br>Fog | `45`, `48`; keep the freezing/rime distinction for `48` | `8`, `12`, `19`; `fog` | Shared artwork; two-wave Fog picture |
| Drizzle | ![Drizzle uses Rain](weather-icons/rain.png)<br>Rain | `51`, `53`, `55` | `2`, `3`, `4` | Shared artwork; current drizzle uses Rain |
| Rain / Drizzle and Rain | ![Rain](weather-icons/rain.png)<br>Rain | `61`, `63`, `65` | `5`, `6`, `21`, `25`, `26`; `rain` or rain type, unless explicitly refined to Showers | Direct / shared artwork; preserve intensity or combined wording when known |
| Showers, day/night | ![Showers day](weather-icons/showers-day.png) ![Showers night](weather-icons/showers-night.png)<br>Showers day / night | `80`, `81`, `82` | `24`; `showers-day`, `showers-night` | Direct; requires explicit shower evidence |
| Freezing Rain / Freezing Drizzle | ![Freezing Rain](weather-icons/freezing-rain.png)<br>Blue angled icicles | `56`, `57` are freezing drizzle; `66`, `67` are freezing rain | `9`, `10`, `11`, `13`, `14`; `preciptype` includes `freezingrain` | Shared artwork. IDs `9`–`11` combine drizzle/rain, so retain broad Freezing wording when ambiguous |
| Rain & Snow | ![Rain and Snow](weather-icons/rain-snow.png)<br>Rain-and-snow artwork | Derived from same-record liquid-rain and falling-snow evidence | `22`, `23`, `32`; otherwise same-record rain and snow evidence | Direct / derived; uses `rain-snow.svg` artwork |
| Snow | ![Snow](weather-icons/snow.png)<br>Snow | `71`, `73`, `75` | `31`, `34`, `35`; `snow` or snow type | Direct |
| Snow Showers | ![Snow Showers](weather-icons/snow.png)<br>Snow | `85`, `86` | `33`; `snow-showers-day`, `snow-showers-night` | Shared artwork; retain Snow Showers caption |
| Snow Grains | ![Snow Grains](weather-icons/snow.png)<br>Snow | `77` | No distinct native equivalent | Shared artwork; does not establish Sleet or falling snow for a compound condition |
| Blowing / Drifting Snow | ![Blowing Snow](weather-icons/snow.png)<br>Snow | No direct code | `1` | Shared artwork; retain Blowing Snow caption. Does not alone establish falling snow |
| Diamond Dust / Ice Crystals | ![Diamond Dust](weather-icons/snow.png)<br>Snow | No direct code | `39` | Shared artwork; retain Diamond Dust / Ice Crystals caption. Does not alone establish falling snow |
| Hail | ![Hail](weather-icons/hail.png)<br>Larger white pellets | `96`, `99` report hail with thunder | `16`, `40` | Direct when reported; within one record, Hail picture wins over Thunderstorm while retaining thunder |
| Thunderstorm | ![Thunderstorm](weather-icons/thunderstorm.png)<br>Thunderstorm | `95`, `97`; `96`, `99` also retain thunder when Hail supplies the picture | `37`, `38`; `thunder-rain`, `thunder-showers-day`, `thunder-showers-night` | Direct; recognize `97` as thunder. `38` does not imply rain |
| Lightning Without Thunder | ![Lightning](weather-icons/thunderstorm.png)<br>Thunderstorm artwork | No separate code | `18` | Approximation for current and forecast pictures; current caption Lightning. Do not invent thunder or rain |
| Thundersnow | ![Thundersnow](weather-icons/thundersnow.png)<br>Thundersnow | Derived from thunder and falling-snow evidence in the same interval | Same-record thunder and falling-snow evidence; no dedicated native ID | Derived; require same-period evidence |
| Sleet / Ice Pellets | ![Sleet](weather-icons/sleet.png)<br>Two rows of small white pellets | No direct ice-pellet code; `77` remains Snow Grains | No precise supported pairing; `17` and `preciptype=ice` map to Generic Ice | **Unused by these providers**; do not manufacture a Sleet classification |
| Generic Ice | ![Generic Ice](weather-icons/ice.png)<br>Large standalone white snowflake | No direct code | `17`; `preciptype` includes `ice` | Direct generic category; precise subtype deliberately unspecified. Current conditions only |
| Wind / Squalls | ![Wind](weather-icons/wind.png)<br>Three white gusts | No direct code; do not add a wind-speed threshold in this mapping | `36`; `wind` | Direct; retain Squalls caption for `36`. Current conditions only |
| Funnel Cloud / Tornado | ![Tornado](weather-icons/tornado.png)<br>Four gray funnel bands | No direct code | `15` | Direct combined category; keep the combined caption without claiming a confirmed tornado. Current conditions only |
| Dust Storm | ![Dust Storm uses Fog](weather-icons/fog.png)<br>Fog artwork | No direct code | `7` | Approximation; retain Dust Storm caption |
| Smoke / Haze | ![Smoke and Haze use Fog](weather-icons/fog.png)<br>Fog artwork | No direct code | `30` | Approximation; retain Smoke / Haze caption |
| Precipitation in Vicinity | Recognized accompanying weather or sky evidence; otherwise no picture | No direct code | `20` | No dedicated assignment; nearby precipitation does not establish a local type or amount |
| Sky coverage decreasing / increasing / unchanged | Recognized sky icon or actual cloud-coverage evidence; otherwise no picture | No direct code | `27`, `28`, `29` | No dedicated assignment; a trend does not establish cloud amount |
| Unknown / missing / new condition | Recognized accompanying evidence first; otherwise no picture | Unrecognized code; reported amounts can still establish rain, showers or snowfall | Unrecognized ID, icon or precipitation type; recognized accompanying fields can still supply facts | Defined fallback; current caption Unknown only when no recognized picture remains. Keep valid dates and temperatures |

Provider references: [Open-Meteo weather codes](https://open-meteo.com/en/docs#weather_variable_documentation), [Visual Crossing's English dictionary](https://docs.google.com/spreadsheets/d/1cc-jQIap7ZToVaEgiXEk_Aa6YVYjSObLV9PMe4oHrFg/edit#gid=1769797687), [Visual Crossing icon identifiers](https://www.visualcrossing.com/resources/documentation/weather-api/defining-icon-set-in-the-weather-api/), and [precipitation fields](https://www.visualcrossing.com/resources/documentation/weather-api/timeline-weather-api/). A dictionary entry is not proof of live availability.

## Selection and wording rules

Current conditions describe one reporting period. Daily forecasts summarize a daytime window using their own qualification rules. Decoding retains precipitation and thunder facts independently of the selected picture. Official watches and warnings have their own [NWS source and rules](architecture.md#nws-watches-and-warnings); pictures do not create alerts.

### Current conditions and compound records

Use each provider's reported condition evidence. Do not infer snow, sleet, freezing rain or hail from air temperature alone. The selected family supplies one current caption using the application's wording and distinctions shown in the table. Hail with thunder explicitly says Hail & Thunder; other losing families need not appear in the caption.

For conditions reported together in one current/hourly record, the picture order is **Funnel/Tornado → Hail → Freezing Rain → Thundersnow → Thunderstorm/Lightning → Rain & Snow → Snow → Rain/Showers → Generic Ice → Fog → Wind → sky**. This selects the picture for that record; it does not rank scientific severity or replace the daily selection method below. Generic Ice yields to more precise reported precipitation; Fog precedes Wind. Sleet is unused by these providers and has no selection branch.

Use Showers only when identified as showery; ordinary drizzle uses Rain. Generic Ice retains the Ice caption and does not manufacture Sleet, Hail, snow amounts or thunder. A broad provider icon must not override a more specific reported type.

For Visual Crossing, explicit Ice, Hail or Freezing evidence prevents a broad icon from adding rain or snow. A `type_38` dry-thunder report prevents the icon from adding rain, and `type_18` Lightning prevents it from inventing thunder. If no sky condition or recognized sky icon is available, reported cloud cover supplies Cloudy at 75% or more, Partly Cloudy at 35% or more, otherwise Clear.

Rain & Snow and Thundersnow are combined-period classifications. Require evidence in the same current/hourly record; a transition within that period is possible, so this does not claim continuous simultaneous precipitation. A day-wide list containing rain and snow, or thunder and snow at different hours, is insufficient.

For quantitative Rain & Snow, require positive `rain + showers` and positive `snowfall`. Total precipitation already includes snow water equivalent and cannot establish liquid rain by itself. Compare quantities only after respecting their units. Falling-snow evidence means a reported snowfall condition/type or positive same-period snowfall, rather than the Snow picture, blowing snow, snow grains, diamond dust or snow on the ground alone. See [Open-Meteo interval fields](https://open-meteo.com/en/docs#hourly-parameter-definition) and [Visual Crossing aggregation](https://www.visualcrossing.com/resources/documentation/weather-data/weather-data-documentation/).

For current Open-Meteo combinations, obtain supported current rain/showers/snowfall fields with the current condition data. Do not borrow the previous hourly record to relabel now or override an incompatible dry condition solely because an earlier interval was wet. If the needed evidence is absent, retain the supported ordinary condition.

Open-Meteo current day/night variants follow `is_day`. Visual Crossing first uses an explicit `-day` or `-night` icon; otherwise it compares the current record's time with that day's sunrise/sunset, using the daytime variant if no usable evidence is available. Five-day entries use daytime variants. Our captions are generated from decoded facts rather than provider free-text descriptions.

### Five-day forecasts

Both providers use [forecast_representative.c](../main/forecast_representative.c). Each entry keeps its provider date, high/low temperatures and daytime icon variant. Matching hourly records from **08:00 through 20:00 inclusive**, in the location's local timezone, supply 13 equally weighted samples when complete. Duplicate records for the same hour count once.

The calculation checks these families in order and uses the first qualifying result:

| Order | Picture family | Qualification over the daytime window |
|---|---|---|
| 1 | Hail | The [two-report rule](#additional-daily-pictures). |
| 2 | Thunderstorm / Lightning | At least two electrical-weather hours and either two wet hours, 0.5 mm precipitation or 35% maximum probability; or at least one electrical-weather hour, a wet hour or positive precipitation, and at least 60% maximum probability. |
| 3 | Freezing Rain | At least one freezing hour and either two wet hours, 1 mm precipitation or 60% maximum probability. |
| 4 | Snow / Rain & Snow | At least two snow-family hours, 0.5 cm snowfall, or a cold day with at least three wet hours and 60% maximum probability. The [mixed-snow rule](#additional-daily-pictures) selects Rain & Snow within this family. |
| 5 | Rain / Showers | The [paired probability and amount requirement](#ordinary-rainshowers-qualification). |
| 6 | Fog | At least three fog hours, at most one wet hour and average cloud cover of at least 70%. |
| 7 | Sky | Cloudy when average cloud cover is at least 70% or cloudy hours tie or outnumber both other sky groups; otherwise Partly Cloudy when average cover is at least 35% or partly cloudy hours tie or outnumber clear hours; otherwise Clear if a clear hour exists, or no picture. Each winning count must be nonzero. |

An electrical-weather hour reports thunder or Lightning. A snow-family hour reports a snow condition or positive snowfall; it need not establish falling snow for a compound picture. Wet-hour counts use reported liquid conditions or positive precipitation/rain/showers amounts; freezing rain needs a positive amount, while a condition explicitly decoded as freezing drizzle can count without one. Each sky sample counts first as Cloudy if its decoded sky is Cloudy or cover is at least 75%; otherwise Partly Cloudy if its decoded sky is Partly Cloudy or cover is at least 35%; otherwise Clear if its decoded sky is Clear or reported cover is below 35%. Missing cloud cover is excluded from the cloud-cover average.

The daily calculation also uses temperature-based Snow/Thundersnow guesses. A day is cold when the average of its unrounded provider high and low is at most **1°C or 34°F**. A qualifying thunder forecast uses Thundersnow if a thunder hour reports falling snow, or if thunder is present on a cold day. These daily guesses do not apply to current conditions and cannot create Hail or Rain & Snow or override qualifying Hail.

#### Ordinary Rain/Showers qualification

After the higher-priority family checks, select ordinary Rain/Showers only when **both** requirements are met:

| Measure over the daytime window | Required value |
|---|---|
| Highest hourly precipitation probability | **At least 50%** |
| Sum of hourly precipitation amounts | **At least 1.5 mm** (about 0.06 inches) |

The probability is the maximum hourly value in this window, not the probability of rain occurring at any time during the whole day. The amount is the sum over the window; the two measures need not come from the same hour. Use the returned fractional amounts before comparing with the threshold.

Three wet hours alone do not qualify ordinary Rain/Showers. Wet-hour counts serve the other family checks above. Choose Showers when its hourly count ties or exceeds Rain; the daily Showers count includes drizzle. If the paired requirement fails, continue to Fog and sky.

The 50% threshold applies only to this ordinary daily Rain/Showers check. Thunder, freezing and snow checks have their separate qualifications above; current-condition pictures use the reported condition evidence.

A brief event can determine the daily picture when the paired requirements are met; this does not require a majority of daytime hours. These thresholds are product rules, not a statistically validated forecast-accuracy claim.

#### Provider decoding

Each provider supplies shared facts to the same calculation. Visual Crossing conditions are not represented as invented Open-Meteo codes.

- Open-Meteo supplies hourly dates/times, numeric conditions, `precipitation_probability`, `precipitation`, `rain`, `showers`, `snowfall` and `cloud_cover`.
- Visual Crossing supplies each day's `datetime`, its `hours[].datetime`, `conditions` requested with `lang=id`, `preciptype`, `icon` requested with `iconSet=icons2`, `precipprob`, `precip`, `snow` and `cloudcover`. Decode all reported condition IDs and precipitation types. It has no separate numeric rain/showers amounts; derive those counters from reported types, conditions and explicit shower identifiers. Positive precipitation counts as a wet hour for the family checks.
- Precipitation is normalized to **millimeters** and snowfall to **centimeters**. In Visual Crossing US responses, precipitation inches are multiplied by 25.4 and snowfall inches by 2.54. Temperatures use the selected units, with high/low rounded for display. `snowdepth` is snow on the ground, not snowfall.
- When no matching daytime samples are available, the mapped daily record supplies the fallback for eligible families. Missing hourly data does not waive the Hail two-report or Rain & Snow majority requirements. Hail, Ice, Wind, Funnel/Tornado and derived mixed-precipitation facts are excluded from this fallback. Other reported evidence can still supply a picture; otherwise retain valid temperatures and dates without one. A day-wide union cannot establish same-period Rain & Snow or Thundersnow, although the daily cold-weather Thundersnow guess still applies when thunder is reported.

Provider condition decoding and representative daily selection are separate steps. Generic Ice, Wind and Funnel/Tornado have no daily picture or selection branch.

#### Additional daily pictures

These rules apply to both providers over the same daytime window. Hail qualifies independently; Rain & Snow refines a qualifying snow forecast.

| Picture | Daily treatment | Practical limit |
|---|---|---|
| ![Hail](weather-icons/hail.png) Hail | Select Hail when **at least two distinct daytime hourly forecasts explicitly report hail**. This is checked before Thunderstorm. **Thunder is not required**, and ordinary Rain/Showers probability and amount requirements do not apply. | One isolated hail report does not select the daily Hail picture, so meaningful hail confined to one hourly forecast can be omitted. Two reports do not imply two continuous hours of hail. |
| ![Rain and Snow](weather-icons/rain-snow.png) Rain & Snow | After the snow check qualifies, select Rain & Snow only when **more than half of the hours counted by that snow check have same-period liquid-rain and falling-snow evidence**. A tie retains Snow. | Rain at one hour and snow at another do not establish mixed precipitation. Temperature-only snow qualification cannot create the mixed picture. |

Count explicit Hail evidence from the provider mapping, independently of the hour's selected picture. Multiple hail identifiers in one hour or duplicate entries for the same hour count as one report. Open-Meteo `96`/`99` also preserve thunder; Visual Crossing Hail identifiers do not create thunder unless it is separately reported. Precipitation probability and total amount describe precipitation generally, not hail probability or hail amount. This picture rule is not a hail-warning threshold.

For Rain & Snow, count each snow-check hour once in the denominator and each same-period mixed hour once in the numerator. The majority is within the snow hours, not across the whole day. A cold-temperature snow guess with no reported mixed evidence remains Snow.

These are product rules, not measured provider confidence or a demonstrated forecast-accuracy improvement. Some distinct conditions share artwork, so the captionless forecast can show only that broad picture. Rare Visual Crossing forecast IDs still need real-response verification; constructed cases establish the implemented treatment, not provider availability.

## Current firmware mapping

Provider records are decoded into shared facts by
[weather_icon_names.c](../main/weather_icon_names.c), without
inventing Open-Meteo codes for Visual Crossing. Current pictures/captions follow
the table above; [forecast_representative.c](../main/forecast_representative.c)
selects daily pictures. [weather_client.c](../main/weather_client.c)
prepares provider records and publishes the selected results. The UI renders
copied results; service and state ownership remain in the [architecture](architecture.md#ownership).
Within one condition family, multiple Visual Crossing IDs can supply competing
captions. The decoder currently keeps the last applicable wording in returned
ID order; it does not perform a separate intensity ranking.

Rendering uses the selected icon name and the lookup in
[weather_icons.c](../main/assets/weather_icons.c); unknown names
return no picture. All 18 pictures use SVG sources in
[assets/icons/](../assets/icons/), including two-wave Fog and
Rain & Snow's `rain-snow.svg`. Freezing Rain, Hail, Generic Ice, Wind and Funnel/Tornado
reside in the combined `wac_icons` asset output; the other 13 have individual
compiled image files. The small-pellet Sleet drawing is retained separately as
[sleet.svg](../assets/icons/sleet.svg) for its documentation preview; it is
excluded from firmware generation and has no runtime image lookup.

The Sleet source contains vector paths with a transparent background. To export
its existing 64×64 documentation PNG, use CairoSVG with the preview's dark
background. From the project folder, with CairoSVG installed, run:

```bash
cairosvg assets/icons/sleet.svg --output-width 64 --output-height 64 --background '#08131d' -o docs/weather-icons/sleet.png
```

The reference export uses CairoSVG 2.8.2 and Cairo 1.18.4. These settings reproduce
the retained PNG byte for byte; the export command supplies the background
without adding one to the vector drawing.

Source dimensions, crop metadata and pixel encoding follow the
[artwork storage and placement rules](architecture.md#weather-icons).
[The build guide](build.md#regenerate-artwork-optional) documents
the generator command.

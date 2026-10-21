# ESP32-P4 weather, location and Radar test data

These JSON files provide inputs and expected results for the native C checks:

- `forecast-representative-cases.json`: representative daytime forecast icons.
- `location-resolution-cases.json`: location parsing and candidate ranking.
- `weather-open-meteo.json` and `weather-visual-crossing.json`: sanitized provider responses consumed by the native weather-service checks. Constructed cases in that test supplement these captures; they do not establish that rare conditions occur in provider traffic.

The checks compile the ESP32-P4 calculations and compare their results with these
expected values. The weather-service checks also decode the provider captures through the production client with substituted HTTP responses. See the [test instructions](../README.md#weather-and-location) and their [native suite commands](../README.md#settings-time-backup-clock-alarms-night-shift-connectivity-and-ui-controls).

[radar-six-frame/](radar-six-frame/README.md) contains captured Radar tiles,
timeline metadata, a map background and composed reference images for the
optional six-frame playback check. Its README describes the data and limits;
the [test instructions](../README.md) own the runnable command.

# ESP32-P4 weather and location test data

These JSON files provide inputs and expected results for the native C checks:

- `forecast-representative-cases.json`: representative daytime forecast icons.
- `location-resolution-cases.json`: location parsing and candidate ranking.
- `storm-conditions-cases.json`: 27 shared storm-alert cases covering current/upcoming wind, gusts, heavy precipitation, thunder, missing data and imperial/metric gust boundaries; pressure alone does not alert. The fixed rules are owned by [storm detection](../../docs/architecture.md#storm-detection).
- `weather-open-meteo.json` and `weather-visual-crossing.json`: sanitized provider responses consumed by the native weather-service checks. Constructed cases in that test supplement these captures; they do not establish that rare conditions occur in provider traffic.

The checks compile the ESP32-P4 calculations and compare their results with these
expected values. The weather-service checks also decode the provider captures through the production client with substituted HTTP responses. See the [test instructions](../../targets/esp32-p4/tests/README.md#weather-and-location) and their [native suite commands](../../targets/esp32-p4/tests/README.md#settings-time-backup-clock-alarms-night-shift-connectivity-and-ui-controls).

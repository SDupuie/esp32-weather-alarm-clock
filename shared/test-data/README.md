# ESP32-P4 weather and location test data

These JSON files provide inputs and expected results for the native C checks:

- `forecast-representative-cases.json`: representative daytime forecast icons.
- `location-resolution-cases.json`: location parsing and candidate ranking.
- `storm-conditions-cases.json`: weather outlook, measurements, fixed storm-warning limits and imperial/metric gust-boundary cases.

The checks compile the ESP32-P4 calculations and compare their results with these
expected values. See the [test instructions](../../targets/esp32-p4/tests/README.md#weather-and-location).

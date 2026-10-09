#pragma once

#include "cJSON.h"

/* Pure C calculation that picks a representative weather code for a forecast
 * day from Open-Meteo hourly data. It has no ESP-IDF dependency, so the native
 * C test can run it with the JSON cases in
 * shared/test-data/forecast-representative-cases.json. Those cases own the
 * test inputs and expected codes; this C function owns the calculation used
 * by the ESP32-P4 application.
 */

int forecast_pick_representative_code(
  const cJSON *hourly,
  const char *date,
  int daily_code,
  int daily_high,
  int daily_low,
  int snow_threshold
);

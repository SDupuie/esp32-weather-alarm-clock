#pragma once

#include <stdbool.h>

#include "cJSON.h"

/* Pure calculation for the clock's storm indicator. Inputs use normalized
 * condition_flags, precipitation mm and wind km/h. Look at the matched current
 * hour through the next three rows, inclusive. Epochs distinguish repeated DST
 * hours; local time is the fallback when the observation has no usable epoch.
 * Missing/unmatched data does not manufacture an alert. Fixed wind/gust limits
 * follow the selected display units; pressure is never an alert trigger. */
bool storm_conditions_alert(bool metric, const cJSON *current, const cJSON *hourly);

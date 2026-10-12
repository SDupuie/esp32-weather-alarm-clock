#pragma once

#include <stdbool.h>
#include <time.h>

typedef struct {
  bool available;
  time_t sunrise;
  time_t sunset;
} solar_times_t;

/* utc_noon is the UTC instant of noon on the requested local calendar day.
 * Select the solar noon nearest that anchor; longitude is positive east.
 * Results are UTC instants, including their day, without timezone or storage
 * access. Invalid coordinates or a day lacking either event yield unavailable
 * zeroed results. The caller decides how unavailable events affect appearance. */
void solar_times_calculate(time_t utc_noon, double latitude, double longitude,
                           solar_times_t *out);

#pragma once

#include "weather_icon_names.h"

/* One shared calculation over normalized hourly arrays. A daily condition is
 * only a fallback when there are no matching 08:00..20:00 samples. The daily
 * union cannot qualify Hail or a same-period compound. */
weather_picture_t forecast_pick_representative_picture(
  const cJSON *hourly, const char *date,
  const weather_condition_facts_t *daily_facts,
  double daily_high, double daily_low, int snow_threshold
);

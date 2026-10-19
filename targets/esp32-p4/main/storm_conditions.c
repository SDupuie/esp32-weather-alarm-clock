#include "storm_conditions.h"
#include "weather_icon_names.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define STORM_LOOKAHEAD_HOURS 3
#define KMH_PER_MPH 1.609344
#define HEAVY_PRECIP_MM_HR 7.6

static const cJSON *field(const cJSON *object, const char *key)
{
  return object != NULL ? cJSON_GetObjectItemCaseSensitive(object, key) : NULL;
}

static double number(const cJSON *item)
{
  return cJSON_IsNumber(item) && isfinite(item->valuedouble) ? item->valuedouble : NAN;
}

static double number_at(const cJSON *object, const char *key, int i)
{
  return number(cJSON_GetArrayItem(field(object, key), i));
}

/* A 15-minute observation such as 15:30 belongs to the 15:00 row. */
static int current_hour_index(const cJSON *current, const cJSON *hourly)
{
  const cJSON *times = field(hourly, "time");
  double epoch = number(field(current, "epoch"));
  if (isfinite(epoch)) {
    for (int i = 0; i < cJSON_GetArraySize(times); ++i) {
      double hour_epoch = number_at(hourly, "epoch", i);
      if (isfinite(hour_epoch) && epoch >= hour_epoch && epoch < hour_epoch + 3600) return i;
    }
    return -1;
  }

  const char *now = cJSON_GetStringValue(field(current, "time"));
  if (!cJSON_IsArray(times) || now == NULL || strlen(now) < 13) return -1;
  char wanted[20];
  snprintf(wanted, sizeof(wanted), "%.13s:00", now);
  for (int i = 0; i < cJSON_GetArraySize(times); ++i) {
    const char *time = cJSON_GetStringValue(cJSON_GetArrayItem(times, i));
    if (time != NULL && strcmp(time, wanted) == 0) return i;
  }
  return -1;
}

bool storm_conditions_alert(bool metric, const cJSON *current, const cJSON *hourly)
{
  int now = current_hour_index(current, hourly);
  if (now < 0) return false;
  double gust_limit = metric ? 64.0 : 40.0 * KMH_PER_MPH;
  double wind_limit = metric ? 40.0 : 25.0 * KMH_PER_MPH;
  double current_wind = number(field(current, "windspeed"));
  if (!isfinite(current_wind)) current_wind = number_at(hourly, "wind_speed_10m", now);
  if (current_wind >= wind_limit) return true;

  double current_flags = number(field(current, "condition_flags"));
  if (current_flags >= 0 && current_flags <= UINT32_MAX
      && ((uint32_t)current_flags & WEATHER_FACT_THUNDER)) return true;
  int last = cJSON_GetArraySize(field(hourly, "time")) - 1;
  int end = now + STORM_LOOKAHEAD_HOURS < last ? now + STORM_LOOKAHEAD_HOURS : last;
  for (int i = now; i <= end; ++i) {
    if ((weather_hourly_flags_at(hourly, i) & WEATHER_FACT_THUNDER)
        || number_at(hourly, "wind_gusts_10m", i) >= gust_limit
        || number_at(hourly, "wind_speed_10m", i) >= wind_limit
        || number_at(hourly, "precipitation", i) >= HEAVY_PRECIP_MM_HR) return true;
  }
  return false;
}

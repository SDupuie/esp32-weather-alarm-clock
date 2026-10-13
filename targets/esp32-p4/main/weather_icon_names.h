#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "cJSON.h"

/* Provider-independent evidence for one reporting period. Picture selection
 * never removes accompanying facts, such as thunder when Hail wins. */
typedef enum {
  WEATHER_SKY_UNKNOWN = 0, WEATHER_SKY_CLEAR, WEATHER_SKY_PARTLY, WEATHER_SKY_CLOUDY
} weather_sky_t;
typedef enum {
  WEATHER_PICTURE_NONE = 0, WEATHER_PICTURE_CLEAR, WEATHER_PICTURE_PARTLY,
  WEATHER_PICTURE_CLOUDY, WEATHER_PICTURE_FOG, WEATHER_PICTURE_RAIN,
  WEATHER_PICTURE_SHOWERS, WEATHER_PICTURE_FREEZING_RAIN, WEATHER_PICTURE_RAIN_SNOW,
  WEATHER_PICTURE_SNOW, WEATHER_PICTURE_HAIL, WEATHER_PICTURE_THUNDER,
  WEATHER_PICTURE_THUNDERSNOW, WEATHER_PICTURE_ICE, WEATHER_PICTURE_WIND,
  WEATHER_PICTURE_TORNADO
} weather_picture_t;

enum {
  WEATHER_FACT_THUNDER = 1u << 0, WEATHER_FACT_LIGHTNING = 1u << 1,
  WEATHER_FACT_HAIL = 1u << 2, WEATHER_FACT_FREEZING = 1u << 3,
  WEATHER_FACT_LIQUID = 1u << 4, WEATHER_FACT_FALLING_SNOW = 1u << 5,
  WEATHER_FACT_SNOW = 1u << 6, WEATHER_FACT_SHOWERS = 1u << 7,
  WEATHER_FACT_MIXED = 1u << 8, WEATHER_FACT_ICE = 1u << 9,
  WEATHER_FACT_FOG = 1u << 10, WEATHER_FACT_WIND = 1u << 11,
  WEATHER_FACT_TORNADO = 1u << 12, WEATHER_FACT_DRIZZLE = 1u << 13
};

typedef struct {
  uint32_t flags;
  weather_sky_t sky;
  bool mostly_clear;
  /* Static, application-owned wording, never provider prose. */
  const char *liquid_caption;
  const char *freezing_caption;
  const char *snow_caption;
  const char *fog_caption;
  const char *wind_caption;
  const char *mixed_caption;
  const char *hail_caption;
} weather_condition_facts_t;

/* Quantities use millimeters for liquid and centimeters for snowfall. NAN or
 * a missing JSON value means unavailable. Total precip is not liquid evidence.
 * Call per record; daily unions must not create same-period compounds. */
weather_condition_facts_t weather_facts_from_open_meteo(int code, double rain_mm,
  double showers_mm, double snowfall_cm);
weather_condition_facts_t weather_facts_from_visual_crossing(const cJSON *record,
  double snowfall_cm);
weather_picture_t weather_picture_for_facts(const weather_condition_facts_t *facts);
const char *weather_caption_for_facts(const weather_condition_facts_t *facts, bool is_day);
const char *weather_icon_name_for_picture(weather_picture_t picture, bool is_day);

/* Shared normalized hourly arrays: time (local ISO timestamp), epoch (instant
 * identity), condition_flags, sky, precipitation_probability (%), precipitation
 * (mm), rain/showers (mm), snowfall (cm), cloud_cover (%), pressure_msl (hPa),
 * wind_speed_10m/wind_gusts_10m (km/h), temperature_2m (selected display unit).
 * Keep unavailable quantities null rather than manufacturing zero. */
uint32_t weather_hourly_flags_at(const cJSON *hourly, int index);
weather_sky_t weather_hourly_sky_at(const cJSON *hourly, int index);

const char *weather_icon_fallback_name(void);
bool weather_icon_name_is_known(const char *icon_name);
const char *weather_icon_name_or_fallback(const char *icon_name);

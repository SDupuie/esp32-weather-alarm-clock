#include "weather_icons.h"

#include "weather_icon_clear_day.h"
#include "weather_icon_clear_night.h"
#include "weather_icon_cloudy.h"
#include "weather_icon_fog.h"
#include "weather_icon_partlycloudy_day.h"
#include "weather_icon_partlycloudy_night.h"
#include "weather_icon_rain.h"
#include "weather_icon_showers_day.h"
#include "weather_icon_showers_night.h"
#include "weather_icon_sleet.h"
#include "weather_icon_snow.h"
#include "weather_icon_thunderstorm.h"
#include "weather_icon_thundersnow.h"
#include "weather_icon_names.h"
#include "wac_icons.h"

#include <string.h>

const lv_image_dsc_t *weather_icon_image_for_name(const char *icon_name)
{
  const char *resolved_name = weather_icon_name_or_fallback(icon_name);

  if (strcmp(resolved_name, "clear-day") == 0) {
    return &weather_icon_clear_day;
  }

  if (strcmp(resolved_name, "clear-night") == 0) {
    return &weather_icon_clear_night;
  }

  if (strcmp(resolved_name, "cloudy") == 0) {
    return &weather_icon_cloudy;
  }

  if (strcmp(resolved_name, "fog") == 0) {
    return &weather_icon_fog;
  }

  if (strcmp(resolved_name, "partlycloudy-day") == 0) {
    return &weather_icon_partlycloudy_day;
  }

  if (strcmp(resolved_name, "partlycloudy-night") == 0) {
    return &weather_icon_partlycloudy_night;
  }

  if (strcmp(resolved_name, "rain") == 0) {
    return &weather_icon_rain;
  }

  if (strcmp(resolved_name, "showers-day") == 0) {
    return &weather_icon_showers_day;
  }

  if (strcmp(resolved_name, "showers-night") == 0) {
    return &weather_icon_showers_night;
  }

  if (strcmp(resolved_name, "rain-snow") == 0) {
    return &weather_icon_sleet;
  }

  if (strcmp(resolved_name, "snow") == 0) {
    return &weather_icon_snow;
  }

  if (strcmp(resolved_name, "thunderstorm") == 0) {
    return &weather_icon_thunderstorm;
  }

  if (strcmp(resolved_name, "thundersnow") == 0) {
    return &weather_icon_thundersnow;
  }

  if (strcmp(resolved_name, "freezing-rain") == 0) return &wac_weather_freezing_rain;
  if (strcmp(resolved_name, "hail") == 0) return &wac_weather_hail;
  if (strcmp(resolved_name, "ice") == 0) return &wac_weather_ice;
  if (strcmp(resolved_name, "wind") == 0) return &wac_weather_wind;
  if (strcmp(resolved_name, "tornado") == 0) return &wac_weather_tornado;
  return NULL;
}

const lv_image_dsc_t *weather_icon_image_for_snapshot(const app_weather_snapshot_t *weather)
{
  return weather && weather->valid ? weather_icon_image_for_name(weather->icon_name) : NULL;
}

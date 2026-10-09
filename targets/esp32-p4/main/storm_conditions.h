#pragma once

#include <stdbool.h>

#include "cJSON.h"

/* Pure C calculation for the Conditions view: wind, gusts, pressure trend,
 * precipitation, alert reasons, and their plain-language wording. It has no
 * ESP-IDF dependency. The native C test checks its output using the JSON
 * cases in shared/test-data/storm-conditions-cases.json; the C calculation
 * is the runtime behavior used by the ESP32-P4 application.
 */

#define STORM_TEXT_LEN 80
#define STORM_SHORT_TEXT_LEN 24
#define STORM_MAX_ALERTS 4

typedef enum {
  STORM_ALERT_THUNDER = 0,
  STORM_ALERT_GUSTS,
  STORM_ALERT_WIND,
  STORM_ALERT_PRESSURE,
  STORM_ALERT_PRECIP
} storm_alert_kind_t;

typedef enum {
  STORM_CHANGE_STEADY = 0,
  STORM_CHANGE_MAY_CHANGE,
  STORM_CHANGE_CHANGING
} storm_change_level_t;

typedef struct {
  storm_alert_kind_t kind;
  char label[STORM_SHORT_TEXT_LEN];
} storm_alert_t;

/* Display units and time format; weather-warning limits are fixed. */
typedef struct {
  bool metric;
  bool time_24h;
} storm_conditions_config_t;

typedef struct {
  /* Units */
  char wind_unit[8];
  char pressure_unit[8];
  char precip_unit[4];

  /* Summary: headline + detail curve along the top; the divider label sits
   * above the columns. */
  char headline[STORM_TEXT_LEN];
  bool has_detail;
  char detail[STORM_TEXT_LEN];
  bool alert;
  bool changing;
  storm_change_level_t change_level;
  char change_label[STORM_SHORT_TEXT_LEN];

  /* Wind (compass shows now; outlook is the column) */
  bool has_speed;
  int speed;
  bool has_gust;
  int gust;
  bool has_direction;
  double direction;
  char cardinal[4];
  char from[STORM_SHORT_TEXT_LEN];
  bool has_peak_gust;
  int peak_gust;
  char peak_gust_time[20];
  char outlook_status[16];
  char outlook_detail[STORM_TEXT_LEN];

  /* Pressure */
  bool has_pressure;
  double pressure_value;
  bool has_pressure_change;
  double pressure_change;
  char pressure_trend[8];
  char pressure_status[16];
  char pressure_meaning[40];

  /* Precipitation */
  char precip_label[8];
  bool has_precip_now;
  double precip_now;
  bool has_precip_outlook;
  double precip_outlook;
  int outlook_hours;
  bool has_chance;
  double chance;
  /* Space for the formatted chance value and its " chance" suffix. */
  char precip_status[40];
  bool has_precip_detail;
  char precip_detail[STORM_TEXT_LEN];

  int alert_count;
  storm_alert_t alerts[STORM_MAX_ALERTS];
} storm_conditions_t;

/* `current` is Open-Meteo's current_weather object; `hourly` and `daily` are
 * the response's hourly/daily objects (daily may be NULL). Returns false when
 * the current hour can't be found in the hourly data. */
bool storm_conditions_build(
  const storm_conditions_config_t *config,
  const cJSON *current,
  const cJSON *hourly,
  const cJSON *daily,
  storm_conditions_t *out
);

const char *storm_alert_kind_name(storm_alert_kind_t kind);
const char *storm_change_level_name(storm_change_level_t level);

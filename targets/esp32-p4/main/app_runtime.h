#pragma once

#include <stdbool.h>
#include <time.h>

#include "storm_conditions.h"
#include "clock_time.h"
#include "alarm.h"
#include "audio.h"
#include "solar_times.h"

#define APP_WEATHER_SUMMARY_LEN 32
#define APP_WEATHER_ICON_NAME_LEN 24
#define APP_FORECAST_DAYS 5
#define APP_MESSAGE_ID_LEN 40
#define APP_MESSAGE_TEXT_LEN 181
#define APP_MESSAGE_SENDER_LEN 41
#define APP_MESSAGE_META_LEN 80
#define APP_RADAR_WIDTH 800
#define APP_RADAR_HEIGHT 800

typedef struct {
  bool valid;
  bool stale;
  time_t frame_time; /* RainViewer's frame time, never the check/download time. */
  uint32_t generation;
  double latitude, longitude;
  /* Immutable BGRA pixels owned by weather_client. Reuse waits for the UI's
   * displayed-generation acknowledgement; snapshots never own or free them. */
  const uint8_t *pixels;
} app_radar_snapshot_t;

typedef struct {
  char date[11]; /* Provider calendar date: YYYY-MM-DD, or empty if absent. */
  int high;
  int low;
  char icon_name[APP_WEATHER_ICON_NAME_LEN];
} app_forecast_day_t;

typedef struct {
  bool valid;
  bool stale;
  time_t updated_at; /* Trusted download time, or zero if time was unavailable. */
  time_t observed_at; /* Provider observation, distinct from download time. */
  int temp;
  int high;
  int low;
  int code;
  bool is_day;
  bool thundersnow;
  char summary[APP_WEATHER_SUMMARY_LEN];
  char icon_name[APP_WEATHER_ICON_NAME_LEN];
  app_forecast_day_t forecast[APP_FORECAST_DAYS];
  /* Conditions view + storm alerts (storm_conditions.c); false when the
   * current hour couldn't be found in the hourly data. */
  bool has_conditions;
  storm_conditions_t conditions;
  /* Captions in conditions use this format; alternate captions use the other. */
  bool conditions_time_24h;
  char conditions_alternate_headline[STORM_TEXT_LEN];
  char conditions_alternate_detail[STORM_TEXT_LEN];
} app_weather_snapshot_t;

typedef struct {
  bool available;
  char id[APP_MESSAGE_ID_LEN];
  char text[APP_MESSAGE_TEXT_LEN];
  char meta[APP_MESSAGE_META_LEN];
} app_message_snapshot_t;

typedef struct {
  wac_time_snapshot_t clock;
  wac_alarm_snapshot_t alarm;
  wac_audio_snapshot_t audio;
  bool night_active;
  solar_times_t solar; /* Owner's raw sunset today and sunrise tomorrow. */
  uint32_t unread_message_count;
  bool has_important_message;
  app_weather_snapshot_t weather;
  app_radar_snapshot_t radar;
  app_message_snapshot_t message;
} app_runtime_state_t;

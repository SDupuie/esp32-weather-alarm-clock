#pragma once

#include <stdbool.h>
#include <time.h>

#include "clock_time.h"
#include "alarm.h"
#include "audio.h"
#include "device_config.h"
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

typedef enum {
  APP_RADAR_LATEST = 0,
  APP_RADAR_PLAY,
  APP_RADAR_PAUSE,
} app_radar_action_t;

typedef struct {
  bool valid;
  bool stale;
  time_t frame_time; /* RainViewer's frame time, never the check/download time. */
  uint32_t generation;
  double latitude, longitude;
  /* Immutable BGRA pixels owned by weather_client. Reuse waits for the UI's
   * displayed-generation acknowledgement; snapshots never own or free them. */
  const uint8_t *pixels;
  uint8_t history_count;
  bool history_loading;
  bool playing;
  bool paused; /* Explicitly held frame; latest still mode is not held. */
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
  device_config_weather_provider_t provider;
  time_t updated_at; /* Trusted download time, or zero if time was unavailable. */
  int temp;
  int high;
  int low;
  char summary[APP_WEATHER_SUMMARY_LEN];
  char icon_name[APP_WEATHER_ICON_NAME_LEN];
  app_forecast_day_t forecast[APP_FORECAST_DAYS];
} app_weather_snapshot_t;

typedef struct {
  bool available;
  char id[APP_MESSAGE_ID_LEN];
  char text[APP_MESSAGE_TEXT_LEN];
  char meta[APP_MESSAGE_META_LEN];
} app_message_snapshot_t;

typedef enum {
  APP_ALERT_CHECKING = 0,
  APP_ALERT_CURRENT,
  APP_ALERT_FAILED,
  APP_ALERT_UNSUPPORTED,
} app_alert_status_t;

/* Display copy only. The weather client owns events, ordering and dismissal;
 * full list/detail text is copied separately when those views need it. */
typedef struct {
  uint32_t revision;
  app_alert_status_t status;
  bool has_success;
  bool save_failed;
  time_t last_success;
  uint16_t warning_count, watch_count;
  uint64_t headline_id; /* Zero means every active headline is dismissed. */
  uint8_t headline_type; /* Index into the weather client's sole catalog. */
} app_alert_snapshot_t;

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
  app_alert_snapshot_t alerts;
  app_message_snapshot_t message;
} app_runtime_state_t;

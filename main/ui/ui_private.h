#pragma once

#include "device_config.h"
#include "app_runtime.h"
#include "app_ui.h"
#include "lvgl.h"

typedef enum {
  WAC_SETTINGS_NONE, WAC_SETTINGS_ENTRY, WAC_SETTINGS_DEVICE_SETUP,
  WAC_SETTINGS_DISPLAY, WAC_SETTINGS_DEVICE, WAC_SETTINGS_WEATHER,
  WAC_SETTINGS_WIFI, WAC_SETTINGS_CREDENTIALS, WAC_SETTINGS_ALARM,
  WAC_SETTINGS_ALARM_TIME, WAC_SETTINGS_REPEAT, WAC_SETTINGS_REPEAT_CUSTOM, WAC_SETTINGS_SOUND,
  WAC_SETTINGS_SOUND_PICKER, WAC_SETTINGS_SNOOZE,
  WAC_SETTINGS_NIGHT, WAC_SETTINGS_NIGHT_MODE, WAC_SETTINGS_NIGHT_FIXED,
  WAC_SETTINGS_NIGHT_START, WAC_SETTINGS_NIGHT_END,
  WAC_SETTINGS_NIGHT_SOLAR, WAC_SETTINGS_SUNSET_OFFSET,
  WAC_SETTINGS_SUNRISE_OFFSET, WAC_SETTINGS_BRIGHTNESS,
  WAC_SETTINGS_MACHINE, WAC_SETTINGS_MACHINE_SOUND, WAC_SETTINGS_MACHINE_TIMER,
  WAC_SETTINGS_PROVIDER, WAC_SETTINGS_ALERTS, WAC_SETTINGS_WATCH_SOUNDS,
  WAC_SETTINGS_WARNING_SOUNDS, WAC_SETTINGS_UPDATE_SOUNDS
} wac_settings_view_t;

/* UI navigation and every LVGL call run under the existing display lock. */
void wac_ui_settings_navigate(wac_settings_view_t view);
wac_settings_view_t wac_ui_settings_view(void);
void wac_ui_settings_close(void);
void wac_ui_apply_display(const device_config_t *config);
void wac_settings_begin(lv_obj_t *parent, const device_config_t *config);
void wac_settings_apply_night(bool active);
void wac_settings_update_solar(const solar_times_t *events);
void wac_settings_update_clock(const wac_time_snapshot_t *clock);
void wac_settings_update_alarm(const device_config_t *config, const wac_alarm_snapshot_t *alarm);
void wac_settings_update_audio(const wac_audio_snapshot_t *audio);
void wac_settings_start_machine(bool return_to_clock);
void wac_settings_set_alarm_return_to_clock(void);
void wac_settings_start_weather(const device_config_t *config);
void wac_settings_render(wac_settings_view_t view);
void wac_settings_close(void);
void wac_settings_set_visible(bool visible);
void wac_settings_poll(void);

/* Small, shared helpers already used by the shell and clock rendering. */
lv_color_t wac_ui_color_bg(void);
lv_color_t wac_ui_color_text_primary(void);
lv_color_t wac_ui_color_text_muted(void);
lv_color_t wac_ui_color_night_primary(void);
lv_color_t wac_ui_color_night_muted(void);
bool wac_ui_strings_equal_ignore_case(const char *a, const char *b);
void wac_ui_copy_uppercase_string(char *dest, size_t dest_size, const char *src);
void wac_ui_set_label_text(lv_obj_t *label, const char *text);
/* Immutable assets only: mutable Radar descriptors require explicit eviction. */
static inline bool wac_ui_set_image_asset(lv_obj_t *image, const lv_image_dsc_t *source)
{
  if (source != NULL && lv_image_get_src(image) == source) return false;
  lv_image_set_src(image, source);
  return true;
}
bool wac_ui_set_weather_image(lv_obj_t *image, const lv_image_dsc_t *source, int32_t size);
lv_obj_t *wac_ui_create_label(lv_obj_t *parent, const lv_font_t *font,
  lv_color_t color, lv_text_align_t align, const char *text);
lv_obj_t *wac_ui_create_weather_credit(lv_obj_t *parent);
void wac_ui_update_weather_credit(lv_obj_t *credit,
  const app_weather_snapshot_t *weather, bool visible, bool night);
void wac_ui_clear_container_chrome(lv_obj_t *obj);
void wac_ui_apply_image_night_tint(lv_obj_t *image, bool active, lv_color_t tint, lv_opa_t opa);
const char *wac_ui_forecast_weekday(const app_forecast_day_t *forecast);

/* Clock rendering owns its objects, never active-view or feature-owner state. */
void wac_clock_reset(void);
void wac_clock_build_analog(lv_obj_t *parent);
void wac_clock_build_digital(lv_obj_t *parent);
void wac_clock_build_unavailable(lv_obj_t *parent);
void wac_clock_set_visible(bool analog, bool digital, bool usable);
void wac_clock_apply_night(bool active);
void wac_clock_update_time(const device_config_t *config, const struct tm *local_time, bool night);
void wac_clock_update_weather(const app_weather_snapshot_t *weather);
void wac_clock_update_weather_notice(const char *notice);
void wac_clock_update_indicators(bool unread, bool important, bool night);
void wac_clock_build_alerts(lv_obj_t *parent, lv_event_cb_t open_cb,
  lv_event_cb_t dismiss_cb, lv_event_cb_t settings_cb);
void wac_clock_update_alerts(const app_alert_snapshot_t *alerts, bool home,
  bool analog, bool usable, bool night);
void wac_clock_build_status(lv_obj_t *parent);
void wac_clock_update_status(const char *text, bool analog, bool waiting_for_sync);
void wac_clock_build_backup(lv_obj_t *parent, lv_event_cb_t open_cb, lv_event_cb_t dismiss_cb);
void wac_clock_update_backup(bool home, bool analog, bool usable,
  wac_time_warning_t warning, bool open, bool dismiss_pending, bool night);

/* Navigation selects a surface; the clock renderer owns only its objects. */
typedef enum {
  WAC_CLOCK_ALARM_HIDDEN, WAC_CLOCK_ALARM_HOME,
  WAC_CLOCK_ALARM_RINGING, WAC_CLOCK_ALARM_SNOOZED, WAC_CLOCK_ALARM_DISMISSED
} wac_clock_alarm_surface_t;
void wac_clock_build_alarm(lv_obj_t *parent, lv_event_cb_t action_cb,
  lv_event_cb_t badge_cb, lv_event_cb_t open_alarm_cb, lv_event_cb_t slide_cb);
void wac_clock_cancel_alarm_slide(bool animate);
void wac_clock_update_alarm(const device_config_t *config,
  const wac_time_snapshot_t *clock, const wac_alarm_snapshot_t *alarm,
  wac_clock_alarm_surface_t surface, bool analog, bool night,
  bool caption_blocked, bool pending);

/* Weather/message rendering consumes inputs without retaining navigation
 * or another service/configuration snapshot. */
void wac_weather_reset(void);
void wac_weather_build_forecast(lv_obj_t *parent);
void wac_weather_set_radar_enabled(lv_obj_t *parent, bool enabled);
void wac_weather_update_radar(const app_radar_snapshot_t *radar,
  const device_config_t *config, bool night);
/* The shell's full-screen gesture layer routes taps; the drawn control is passive. */
bool wac_weather_radar_control_hit(int32_t x, int32_t y);
void wac_weather_build_message(lv_obj_t *parent);
void wac_weather_build_status(lv_obj_t *parent);
void wac_weather_build_alerts(lv_obj_t *parent, lv_event_cb_t back_cb, lv_event_cb_t row_cb);
uint64_t wac_weather_alert_event_id(lv_event_t *event);
void wac_weather_update_alerts(const app_alert_snapshot_t *snapshot,
  const device_config_t *config, bool list_visible, bool detail_visible,
  uint64_t detail_id, bool night);
void wac_weather_set_visible(bool forecast, bool radar, bool message);
void wac_weather_apply_night(const app_weather_snapshot_t *weather, bool important, bool active);
void wac_weather_update(const app_weather_snapshot_t *weather, const struct tm *local_calendar);
void wac_weather_update_message(const app_message_snapshot_t *snapshot);
void wac_weather_set_message_dismiss_text(const char *text);
void wac_weather_update_status(const app_weather_snapshot_t *weather,
  bool night, bool forecast_visible);

void wac_clock_build_machine(lv_obj_t *parent, lv_event_cb_t open_cb);
void wac_clock_update_machine(bool home, bool analog, bool usable, bool playing, bool night);

#pragma once

#include "device_config.h"
#include "app_runtime.h"
#include "lvgl.h"

typedef enum {
  WAC_SETTINGS_NONE, WAC_SETTINGS_ENTRY, WAC_SETTINGS_DEVICE_SETUP,
  WAC_SETTINGS_DISPLAY, WAC_SETTINGS_DEVICE, WAC_SETTINGS_WEATHER,
  WAC_SETTINGS_WIFI, WAC_SETTINGS_CREDENTIALS, WAC_SETTINGS_ALARM,
  WAC_SETTINGS_ALARM_TIME, WAC_SETTINGS_REPEAT, WAC_SETTINGS_SOUND,
  WAC_SETTINGS_SOUND_PICKER, WAC_SETTINGS_SNOOZE,
  WAC_SETTINGS_NIGHT, WAC_SETTINGS_NIGHT_MODE, WAC_SETTINGS_NIGHT_FIXED,
  WAC_SETTINGS_NIGHT_START, WAC_SETTINGS_NIGHT_END,
  WAC_SETTINGS_NIGHT_SOLAR, WAC_SETTINGS_SUNSET_OFFSET,
  WAC_SETTINGS_SUNRISE_OFFSET, WAC_SETTINGS_BRIGHTNESS
} wac_settings_view_t;

/* UI navigation and every LVGL call run under the existing display lock. */
void wac_ui_settings_navigate(wac_settings_view_t view);
wac_settings_view_t wac_ui_settings_view(void);
void wac_ui_settings_close(void);
void wac_settings_begin(lv_obj_t *parent, const device_config_t *config);
void wac_settings_start_weather(const device_config_t *config);
void wac_settings_render(wac_settings_view_t view);
void wac_settings_close(void);
void wac_settings_poll(void);

/* Small, shared helpers already used by the shell and clock rendering. */
lv_color_t wac_ui_color_bg(void);
lv_color_t wac_ui_color_text_primary(void);
lv_color_t wac_ui_color_text_muted(void);
lv_color_t wac_ui_color_night_primary(void);
lv_color_t wac_ui_color_night_muted(void);
lv_color_t wac_ui_color_night_alert(void);
bool wac_ui_strings_equal_ignore_case(const char *a, const char *b);
void wac_ui_copy_uppercase_string(char *dest, size_t dest_size, const char *src);
void wac_ui_set_label_text(lv_obj_t *label, const char *text);
lv_obj_t *wac_ui_create_label(lv_obj_t *parent, const lv_font_t *font,
  lv_color_t color, lv_text_align_t align, const char *text);
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
void wac_clock_log_weather(const app_weather_snapshot_t *weather);
void wac_clock_update_indicators(bool unread, bool important, bool weather_alert, bool night);
void wac_clock_build_status(lv_obj_t *parent);
void wac_clock_update_status(const char *text, bool analog);
void wac_clock_build_backup(lv_obj_t *parent, lv_event_cb_t open_cb, lv_event_cb_t dismiss_cb);
void wac_clock_update_backup(bool home, bool analog, bool usable,
  wac_time_warning_t warning, bool open, bool dismiss_pending, const char *error, bool night);

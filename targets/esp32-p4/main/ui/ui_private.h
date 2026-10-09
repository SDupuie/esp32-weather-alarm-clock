#pragma once

#include "device_config.h"
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

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#define DEVICE_CONFIG_STR_LEN 64
#define DEVICE_CONFIG_WIFI_LEN 33
#define DEVICE_CONFIG_WIFI_PASS_LEN 65
#define DEVICE_CONFIG_SHORT_STR_LEN 16
#define DEVICE_CONFIG_TIME_STR_LEN 6
#define DEVICE_CONFIG_COORD_LEN 16

typedef enum {
  DEVICE_CONFIG_ALARM_CLASSIC_BEEP = 0,
  DEVICE_CONFIG_ALARM_GENTLE_CHIME,
  DEVICE_CONFIG_ALARM_RISING_TONES,
} device_config_alarm_sound_t;

typedef enum {
  DEVICE_CONFIG_NIGHT_OFF = 0,
  DEVICE_CONFIG_NIGHT_ALWAYS_ON,
  DEVICE_CONFIG_NIGHT_FIXED,
  DEVICE_CONFIG_NIGHT_SOLAR,
} device_config_night_mode_t;

typedef struct {
  char device_id[DEVICE_CONFIG_STR_LEN];
  char room_name[DEVICE_CONFIG_STR_LEN];
  char location[DEVICE_CONFIG_STR_LEN];
  char timezone[DEVICE_CONFIG_STR_LEN];
  char wifi_ssid[DEVICE_CONFIG_WIFI_LEN];
  char wifi_password[DEVICE_CONFIG_WIFI_PASS_LEN];
  char message_sharing[DEVICE_CONFIG_SHORT_STR_LEN];
  char default_clock_face[DEVICE_CONFIG_SHORT_STR_LEN];
  char time_format[DEVICE_CONFIG_SHORT_STR_LEN];
  char units[DEVICE_CONFIG_SHORT_STR_LEN];
  char latitude[DEVICE_CONFIG_COORD_LEN];
  char longitude[DEVICE_CONFIG_COORD_LEN];
  char night_shift_start[DEVICE_CONFIG_TIME_STR_LEN];
  char night_shift_end[DEVICE_CONFIG_TIME_STR_LEN];
  bool leading_zero_12h;
  device_config_night_mode_t night_shift_mode;
  int16_t night_sunset_offset_minutes;
  int16_t night_sunrise_offset_minutes;
  uint8_t day_brightness;
  uint8_t night_brightness;
  bool alarm_enabled;
  uint8_t alarm_hour;
  uint8_t alarm_minute;
  /* Bit 0 is Sunday, bit 6 is Saturday. */
  uint8_t alarm_repeat_days;
  device_config_alarm_sound_t alarm_sound;
  uint8_t alarm_volume;
  uint8_t alarm_snooze_minutes;
  /* Derived consumer conveniences, ignored as submitted settings. */
  bool wifi_ready;
  bool location_ready;
} device_config_t;

void device_config_set_defaults(device_config_t *config);
/* Checks a complete settings value: location is either wholly unset or fully
 * resolved. Weather Save additionally requires the fully resolved case. */
esp_err_t device_config_validate(const device_config_t *config);

/* Startup only, after nvs_flash_init and before starting consumers.
 * Initializes missing keys; existing keys are not rewritten. An incomplete
 * location tuple from a partial save is retained with location_ready=false;
 * device/alarm saves can still apply while the location is being corrected.
 * A failed initialization does not publish its loaded/default values. */
esp_err_t device_config_init(void);
/* Copies applied settings under a short lock; performs no storage I/O. */
esp_err_t device_config_copy(device_config_t *config);

/* Call from background save work, never a display callback. Editors retain
 * baseline and draft; only fields changed between them are merged into the
 * latest applied settings. Each Save writes every entry in its own group,
 * including unchanged values; the immediate toggle writes only alarm_enabled.
 * Failure leaves the applied running values intact.
 * Ordinary storage may already contain some changes after a failed save. */
esp_err_t device_config_save_device(const device_config_t *baseline, const device_config_t *draft);
esp_err_t device_config_save_weather(const device_config_t *baseline, const device_config_t *draft);
esp_err_t device_config_save_alarm(const device_config_t *baseline, const device_config_t *draft);
esp_err_t device_config_set_alarm_enabled(bool enabled);

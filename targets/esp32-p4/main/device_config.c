#include "device_config.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"
#include "timezone_table.h"

/* Independent of the original application's rwd_cfg namespace. */
#define DEVICE_CONFIG_NAMESPACE "wac_cfg"

static SemaphoreHandle_t s_save_lock;
static SemaphoreHandle_t s_copy_lock;
static device_config_t s_applied;
static bool s_initialized;

static bool has_text(const char *text)
{
  for (; *text != '\0'; ++text) {
    if (!isspace((unsigned char) *text)) {
      return true;
    }
  }
  return false;
}

static bool strings_terminated(const device_config_t *config)
{
  if (config == NULL) {
    return false;
  }
#define CHECK_STRING(field) \
  if (memchr(config->field, '\0', sizeof(config->field)) == NULL) { return false; }
  CHECK_STRING(device_id)
  CHECK_STRING(room_name)
  CHECK_STRING(location)
  CHECK_STRING(timezone)
  CHECK_STRING(wifi_ssid)
  CHECK_STRING(wifi_password)
  CHECK_STRING(message_sharing)
  CHECK_STRING(default_clock_face)
  CHECK_STRING(time_format)
  CHECK_STRING(units)
  CHECK_STRING(weather_api_key)
  CHECK_STRING(latitude)
  CHECK_STRING(longitude)
  CHECK_STRING(night_shift_start)
  CHECK_STRING(night_shift_end)
#undef CHECK_STRING
  return true;
}

static bool coordinate_valid(const char *text, double limit)
{
  if (*text == '\0' || isspace((unsigned char) *text)) {
    return false;
  }
  errno = 0;
  char *end = NULL;
  double value = strtod(text, &end);
  return end != text && *end == '\0' && errno != ERANGE
    && isfinite(value) && value >= -limit && value <= limit;
}

static bool timezone_supported(const char *timezone)
{
  for (size_t i = 0; i < TIMEZONE_MAPPINGS_COUNT; ++i) {
    if (strcmp(timezone, TIMEZONE_MAPPINGS[i].iana_name) == 0
        && TIMEZONE_MAPPINGS[i].posix_name[0] != '\0') {
      return true;
    }
  }
  return false;
}

static bool hhmm_valid(const char *text)
{
  return strlen(text) == 5 && text[2] == ':'
    && text[0] >= '0' && text[0] <= '2'
    && text[1] >= '0' && text[1] <= '9'
    && (text[0] != '2' || text[1] <= '3')
    && text[3] >= '0' && text[3] <= '5'
    && text[4] >= '0' && text[4] <= '9';
}

static bool offset_valid(int16_t minutes)
{
  return minutes >= -120 && minutes <= 120 && minutes % 5 == 0;
}

static void derive_readiness(device_config_t *config)
{
  config->wifi_ready = config->wifi_ssid[0] != '\0';
  config->location_ready = has_text(config->location)
    && coordinate_valid(config->latitude, 90.0)
    && coordinate_valid(config->longitude, 180.0)
    && timezone_supported(config->timezone);
}

void device_config_set_defaults(device_config_t *config)
{
  if (config == NULL) {
    return;
  }
  memset(config, 0, sizeof(*config));
  strcpy(config->device_id, "clock-esp32-p4");
  strcpy(config->room_name, "Clock");
  strcpy(config->units, "imperial");
  strcpy(config->message_sharing, "single");
  strcpy(config->default_clock_face, "digital");
  strcpy(config->time_format, "12");
  strcpy(config->night_shift_start, "22:00");
  strcpy(config->night_shift_end, "06:00");
  config->night_shift_mode = DEVICE_CONFIG_NIGHT_OFF;
  config->day_brightness = 80;
  config->night_brightness = 30;
  config->alarm_hour = 7;
  config->alarm_repeat_days = 0x3e;
  config->alarm_sound = DEVICE_CONFIG_ALARM_CLASSIC_BEEP;
  config->alarm_volume = 50;
  config->alarm_snooze_minutes = 10;
}

static esp_err_t validate_settings(const device_config_t *config, bool allow_partial_location)
{
  if (!strings_terminated(config)
      || !has_text(config->device_id) || !has_text(config->room_name)
      || (strcmp(config->message_sharing, "single") != 0
          && strcmp(config->message_sharing, "shared") != 0)
      || (strcmp(config->message_sharing, "shared") == 0
          && strcmp(config->device_id, "clock-esp32-p4") == 0)
      || (strcmp(config->default_clock_face, "digital") != 0
          && strcmp(config->default_clock_face, "analog") != 0)
      || (strcmp(config->time_format, "12") != 0 && strcmp(config->time_format, "24") != 0)
      || (strcmp(config->units, "imperial") != 0 && strcmp(config->units, "metric") != 0)
      || (unsigned) config->weather_provider > DEVICE_CONFIG_WEATHER_VISUAL_CROSSING
      || (config->weather_provider == DEVICE_CONFIG_WEATHER_VISUAL_CROSSING
          && !has_text(config->weather_api_key))
      || !hhmm_valid(config->night_shift_start) || !hhmm_valid(config->night_shift_end)
      || (unsigned) config->night_shift_mode > DEVICE_CONFIG_NIGHT_SOLAR
      || !offset_valid(config->night_sunset_offset_minutes)
      || !offset_valid(config->night_sunrise_offset_minutes)
      || config->day_brightness < DEVICE_CONFIG_MIN_BRIGHTNESS || config->day_brightness > 100
      || config->night_brightness < DEVICE_CONFIG_MIN_BRIGHTNESS || config->night_brightness > 100
      || config->alarm_hour > 23 || config->alarm_minute > 59
      || config->alarm_repeat_days == 0 || (config->alarm_repeat_days & ~0x7f) != 0
      || (unsigned) config->alarm_sound > DEVICE_CONFIG_ALARM_RISING_TONES
      || config->alarm_volume > 100
      || (config->alarm_snooze_minutes != 5 && config->alarm_snooze_minutes != 10
          && config->alarm_snooze_minutes != 15)) {
    return ESP_ERR_INVALID_ARG;
  }

  /* Interrupted first-location saves can leave valid individual fields but
   * an incomplete tuple. Startup and unrelated saves retain those values;
   * only an explicit Weather Location save requires a complete tuple. */
  if ((config->location[0] != '\0' && !has_text(config->location))
      || (config->latitude[0] != '\0' && !coordinate_valid(config->latitude, 90.0))
      || (config->longitude[0] != '\0' && !coordinate_valid(config->longitude, 180.0))
      || (config->timezone[0] != '\0' && !timezone_supported(config->timezone))) {
    return ESP_ERR_INVALID_ARG;
  }
  bool location_empty = config->location[0] == '\0' && config->latitude[0] == '\0'
    && config->longitude[0] == '\0' && config->timezone[0] == '\0';
  if (!allow_partial_location && !location_empty && (!has_text(config->location)
      || !coordinate_valid(config->latitude, 90.0)
      || !coordinate_valid(config->longitude, 180.0)
      || !timezone_supported(config->timezone))) {
    return ESP_ERR_INVALID_ARG;
  }
  return ESP_OK;
}

esp_err_t device_config_validate(const device_config_t *config)
{
  return validate_settings(config, false);
}

static esp_err_t load_string(nvs_handle_t handle, const char *key,
                             char *value, size_t capacity, bool *seeded)
{
  size_t size = capacity;
  esp_err_t err = nvs_get_str(handle, key, value, &size);
  if (err == ESP_ERR_NVS_NOT_FOUND) {
    err = nvs_set_str(handle, key, value);
    if (err == ESP_OK) {
      *seeded = true;
    }
  }
  return err;
}

static esp_err_t load_u8(nvs_handle_t handle, const char *key, uint8_t *value, bool *seeded)
{
  esp_err_t err = nvs_get_u8(handle, key, value);
  if (err == ESP_ERR_NVS_NOT_FOUND) {
    err = nvs_set_u8(handle, key, *value);
    if (err == ESP_OK) {
      *seeded = true;
    }
  }
  return err;
}

static esp_err_t load_i16(nvs_handle_t handle, const char *key, int16_t *value, bool *seeded)
{
  esp_err_t err = nvs_get_i16(handle, key, value);
  if (err == ESP_ERR_NVS_NOT_FOUND) {
    err = nvs_set_i16(handle, key, *value);
    if (err == ESP_OK) {
      *seeded = true;
    }
  }
  return err;
}

static void publish(const device_config_t *config)
{
  xSemaphoreTake(s_copy_lock, portMAX_DELAY);
  s_applied = *config;
  s_initialized = true;
  xSemaphoreGive(s_copy_lock);
}

esp_err_t device_config_init(void)
{
  /* Startup creates these before any other task can call this owner. */
  if (s_save_lock == NULL) {
    s_save_lock = xSemaphoreCreateMutex();
  }
  if (s_copy_lock == NULL) {
    s_copy_lock = xSemaphoreCreateMutex();
  }
  if (s_save_lock == NULL || s_copy_lock == NULL) {
    return ESP_ERR_NO_MEM;
  }
  xSemaphoreTake(s_save_lock, portMAX_DELAY);
  device_config_t config;
  device_config_set_defaults(&config);
  nvs_handle_t handle = 0;
  esp_err_t err = nvs_open(DEVICE_CONFIG_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    xSemaphoreGive(s_save_lock);
    return err;
  }
  bool seeded = false;
  uint8_t value;
#define LOAD_STRING(key, field) \
  err = load_string(handle, key, config.field, sizeof(config.field), &seeded); \
  if (err != ESP_OK) { goto done; }
#define LOAD_U8(key, field) \
  err = load_u8(handle, key, &config.field, &seeded); \
  if (err != ESP_OK) { goto done; }
#define LOAD_I16(key, field) \
  err = load_i16(handle, key, &config.field, &seeded); \
  if (err != ESP_OK) { goto done; }
  LOAD_STRING("device_id", device_id)
  LOAD_STRING("room_name", room_name)
  LOAD_STRING("location", location)
  LOAD_STRING("timezone", timezone)
  LOAD_STRING("wifi_ssid", wifi_ssid)
  LOAD_STRING("wifi_pass", wifi_password)
  LOAD_STRING("msg_share", message_sharing)
  LOAD_STRING("clock_face", default_clock_face)
  LOAD_STRING("time_fmt", time_format)
  LOAD_STRING("units", units)
  LOAD_STRING("weather_key", weather_api_key)
  LOAD_STRING("lat", latitude)
  LOAD_STRING("lon", longitude)
  LOAD_STRING("night_st", night_shift_start)
  LOAD_STRING("night_end", night_shift_end)
  LOAD_I16("sunset_offset", night_sunset_offset_minutes)
  LOAD_I16("sunrise_offset", night_sunrise_offset_minutes)
  LOAD_U8("day_brightness", day_brightness)
  LOAD_U8("night_bright", night_brightness)
  LOAD_U8("alarm_hour", alarm_hour)
  LOAD_U8("alarm_minute", alarm_minute)
  LOAD_U8("alarm_days", alarm_repeat_days)
  LOAD_U8("alarm_volume", alarm_volume)
  LOAD_U8("alarm_snooze", alarm_snooze_minutes)
#undef LOAD_STRING
#undef LOAD_U8
#undef LOAD_I16
  value = config.leading_zero_12h;
  err = load_u8(handle, "lead_zero", &value, &seeded);
  if (err != ESP_OK) { goto done; }
  if (value > 1) { err = ESP_ERR_INVALID_ARG; goto done; }
  config.leading_zero_12h = value != 0;
  value = config.alarm_enabled;
  err = load_u8(handle, "alarm_enabled", &value, &seeded);
  if (err != ESP_OK) { goto done; }
  if (value > 1) { err = ESP_ERR_INVALID_ARG; goto done; }
  config.alarm_enabled = value != 0;
  value = config.night_shift_mode;
  err = load_u8(handle, "night_mode", &value, &seeded);
  if (err != ESP_OK) { goto done; }
  config.night_shift_mode = (device_config_night_mode_t) value;
  value = config.alarm_sound;
  err = load_u8(handle, "alarm_sound", &value, &seeded);
  if (err != ESP_OK) { goto done; }
  config.alarm_sound = (device_config_alarm_sound_t) value;
  value = config.weather_provider;
  err = load_u8(handle, "weather_src", &value, &seeded);
  if (err != ESP_OK) { goto done; }
  config.weather_provider = (device_config_weather_provider_t) value;

  /* Keep the display readable without rewriting stored brightness at startup. */
  if (config.day_brightness < DEVICE_CONFIG_MIN_BRIGHTNESS) {
    config.day_brightness = DEVICE_CONFIG_MIN_BRIGHTNESS;
  }
  if (config.night_brightness < DEVICE_CONFIG_MIN_BRIGHTNESS) {
    config.night_brightness = DEVICE_CONFIG_MIN_BRIGHTNESS;
  }
  err = validate_settings(&config, true);
  if (err == ESP_OK && seeded) {
    err = nvs_commit(handle);
  }
  if (err == ESP_OK) {
    derive_readiness(&config);
    publish(&config);
  }
done:
  nvs_close(handle);
  xSemaphoreGive(s_save_lock);
  return err;
}

esp_err_t device_config_copy(device_config_t *config)
{
  if (config == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  if (s_copy_lock == NULL) {
    return ESP_ERR_INVALID_STATE;
  }
  xSemaphoreTake(s_copy_lock, portMAX_DELAY);
  esp_err_t err = ESP_ERR_INVALID_STATE;
  if (s_initialized) {
    *config = s_applied;
    err = ESP_OK;
  }
  xSemaphoreGive(s_copy_lock);
  return err;
}

typedef enum {
  SAVE_DEVICE,
  SAVE_DISPLAY,
  SAVE_WEATHER,
  SAVE_ALARM,
  SAVE_ALARM_ENABLED,
} settings_save_t;

static esp_err_t write_settings(settings_save_t group, const device_config_t *config)
{
  nvs_handle_t handle = 0;
  esp_err_t err = nvs_open(DEVICE_CONFIG_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    return err;
  }
#define SAVE_STRING(key, field) \
  err = nvs_set_str(handle, key, config->field); \
  if (err != ESP_OK) { goto done; }
#define SAVE_NUMBER(type, key, field) \
  err = nvs_set_##type(handle, key, config->field); \
  if (err != ESP_OK) { goto done; }
  /* Each explicit Save writes its own entries, including unchanged values.
   * The merged config already preserves choices this editor did not change. */
  switch (group) {
    case SAVE_DEVICE:
      /* Disable Shared before restoring a default ID; install an individual
       * ID before enabling Shared. This also holds across a failed-save retry. */
      if (strcmp(config->message_sharing, "single") == 0) {
        SAVE_STRING("msg_share", message_sharing)
      }
      SAVE_STRING("device_id", device_id)
      SAVE_STRING("room_name", room_name)
      SAVE_STRING("wifi_ssid", wifi_ssid)
      SAVE_STRING("wifi_pass", wifi_password)
      if (strcmp(config->message_sharing, "shared") == 0) {
        SAVE_STRING("msg_share", message_sharing)
      }
      SAVE_STRING("night_st", night_shift_start)
      SAVE_STRING("night_end", night_shift_end)
      SAVE_NUMBER(u8, "night_mode", night_shift_mode)
      SAVE_NUMBER(i16, "sunset_offset", night_sunset_offset_minutes)
      SAVE_NUMBER(i16, "sunrise_offset", night_sunrise_offset_minutes)
      break;
    case SAVE_DISPLAY:
      SAVE_STRING("clock_face", default_clock_face)
      SAVE_STRING("time_fmt", time_format)
      SAVE_NUMBER(u8, "lead_zero", leading_zero_12h)
      SAVE_NUMBER(u8, "day_brightness", day_brightness)
      SAVE_NUMBER(u8, "night_bright", night_brightness)
      break;
    case SAVE_WEATHER:
      SAVE_STRING("location", location)
      SAVE_STRING("timezone", timezone)
      SAVE_STRING("units", units)
      SAVE_STRING("lat", latitude)
      SAVE_STRING("lon", longitude)
      /* Disable the keyed provider before an explicit key clear; install the
       * key before selecting it. Ordinary partial saves are still accepted. */
      if (config->weather_provider == DEVICE_CONFIG_WEATHER_OPEN_METEO) {
        SAVE_NUMBER(u8, "weather_src", weather_provider)
        SAVE_STRING("weather_key", weather_api_key)
      } else {
        SAVE_STRING("weather_key", weather_api_key)
        SAVE_NUMBER(u8, "weather_src", weather_provider)
      }
      break;
    case SAVE_ALARM:
      SAVE_NUMBER(u8, "alarm_enabled", alarm_enabled)
      SAVE_NUMBER(u8, "alarm_hour", alarm_hour)
      SAVE_NUMBER(u8, "alarm_minute", alarm_minute)
      SAVE_NUMBER(u8, "alarm_days", alarm_repeat_days)
      SAVE_NUMBER(u8, "alarm_sound", alarm_sound)
      SAVE_NUMBER(u8, "alarm_volume", alarm_volume)
      SAVE_NUMBER(u8, "alarm_snooze", alarm_snooze_minutes)
      break;
    case SAVE_ALARM_ENABLED:
      SAVE_NUMBER(u8, "alarm_enabled", alarm_enabled)
      break;
  }
#undef SAVE_STRING
#undef SAVE_NUMBER
  err = nvs_commit(handle);
done:
  nvs_close(handle);
  return err;
}

static esp_err_t begin_save(device_config_t *before)
{
  if (s_save_lock == NULL) {
    return ESP_ERR_INVALID_STATE;
  }
  xSemaphoreTake(s_save_lock, portMAX_DELAY);
  esp_err_t err = device_config_copy(before);
  if (err != ESP_OK) {
    xSemaphoreGive(s_save_lock);
  }
  return err;
}

static esp_err_t finish_save(device_config_t *after, settings_save_t group)
{
  esp_err_t err = validate_settings(after, group != SAVE_WEATHER);
  if (err == ESP_OK) {
    derive_readiness(after);
    err = write_settings(group, after);
  }
  if (err == ESP_OK) {
    publish(after);
  }
  xSemaphoreGive(s_save_lock);
  return err;
}

#define MERGE_STRING(field) \
  if (strcmp(baseline->field, draft->field) != 0) { \
    memcpy(after.field, draft->field, sizeof(after.field)); \
  }
#define MERGE_NUMBER(field) \
  if (baseline->field != draft->field) { after.field = draft->field; }

esp_err_t device_config_save_device(const device_config_t *baseline, const device_config_t *draft)
{
  if (!strings_terminated(baseline) || !strings_terminated(draft)) {
    return ESP_ERR_INVALID_ARG;
  }
  device_config_t before;
  esp_err_t err = begin_save(&before);
  if (err != ESP_OK) { return err; }
  device_config_t after = before;
  MERGE_STRING(device_id)
  MERGE_STRING(room_name)
  MERGE_STRING(wifi_ssid)
  MERGE_STRING(wifi_password)
  MERGE_STRING(message_sharing)
  MERGE_STRING(night_shift_start)
  MERGE_STRING(night_shift_end)
  MERGE_NUMBER(night_shift_mode)
  MERGE_NUMBER(night_sunset_offset_minutes)
  MERGE_NUMBER(night_sunrise_offset_minutes)
  return finish_save(&after, SAVE_DEVICE);
}

esp_err_t device_config_save_display(const device_config_t *baseline, const device_config_t *draft)
{
  if (!strings_terminated(baseline) || !strings_terminated(draft)) {
    return ESP_ERR_INVALID_ARG;
  }
  device_config_t before;
  esp_err_t err = begin_save(&before);
  if (err != ESP_OK) { return err; }
  device_config_t after = before;
  MERGE_STRING(default_clock_face)
  MERGE_STRING(time_format)
  MERGE_NUMBER(leading_zero_12h)
  MERGE_NUMBER(day_brightness)
  MERGE_NUMBER(night_brightness)
  return finish_save(&after, SAVE_DISPLAY);
}

esp_err_t device_config_save_weather(const device_config_t *baseline, const device_config_t *draft)
{
  if (!strings_terminated(baseline) || !strings_terminated(draft)) {
    return ESP_ERR_INVALID_ARG;
  }
  device_config_t before;
  esp_err_t err = begin_save(&before);
  if (err != ESP_OK) { return err; }
  device_config_t after = before;
  MERGE_STRING(location)
  MERGE_STRING(latitude)
  MERGE_STRING(longitude)
  MERGE_STRING(timezone)
  MERGE_STRING(units)
  MERGE_NUMBER(weather_provider)
  MERGE_STRING(weather_api_key)
  /* Unlike initial defaults, an explicitly saved location must be resolved. */
  if (!has_text(after.location)) {
    xSemaphoreGive(s_save_lock);
    return ESP_ERR_INVALID_ARG;
  }
  return finish_save(&after, SAVE_WEATHER);
}

esp_err_t device_config_save_alarm(const device_config_t *baseline, const device_config_t *draft)
{
  if (baseline == NULL || draft == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  device_config_t before;
  esp_err_t err = begin_save(&before);
  if (err != ESP_OK) { return err; }
  device_config_t after = before;
  MERGE_NUMBER(alarm_enabled)
  MERGE_NUMBER(alarm_hour)
  MERGE_NUMBER(alarm_minute)
  MERGE_NUMBER(alarm_repeat_days)
  MERGE_NUMBER(alarm_sound)
  MERGE_NUMBER(alarm_volume)
  MERGE_NUMBER(alarm_snooze_minutes)
  return finish_save(&after, SAVE_ALARM);
}
#undef MERGE_STRING
#undef MERGE_NUMBER

esp_err_t device_config_set_alarm_enabled(bool enabled)
{
  device_config_t before;
  esp_err_t err = begin_save(&before);
  if (err != ESP_OK) { return err; }
  device_config_t after = before;
  after.alarm_enabled = enabled;
  return finish_save(&after, SAVE_ALARM_ENABLED);
}

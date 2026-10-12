#include "night.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "bsp/display.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "nvs.h"

#define NIGHT_NAMESPACE "wac_night"
#define NIGHT_EFFECTIVE_KEY "effective"
#define FADE_POINT_US 50000
#define FADE_MAX_STEP 5

/* Only these copied values are shared. All other state belongs to the single
 * clock-control writer; no lock is held across NVS or backlight calls. */
static portMUX_TYPE s_copy_lock = portMUX_INITIALIZER_UNLOCKED;
static bool s_night;
static bool s_backlight_ready;
static bool s_initialized;
static bool s_fade_started;
static int s_brightness;
static int64_t s_fade_step_us;
static solar_times_t s_published_solar;
/* Private to the clock-control writer. Calendar noons, rather than 24-hour
 * increments, keep the adjacent local dates correct across clock changes. */
static bool s_solar_cached;
static time_t s_solar_noon;
static double s_solar_latitude, s_solar_longitude;
static char s_solar_timezone[DEVICE_CONFIG_STR_LEN];
static solar_times_t s_solar_days[3];

static bool parse_coordinate(const char *text, size_t size, double limit, double *out)
{
  if (memchr(text, '\0', size) == NULL || *text == '\0'
      || isspace((unsigned char)*text)) return false;
  errno = 0;
  char *end;
  double value = strtod(text, &end);
  if (end == text || *end != '\0' || errno == ERANGE
      || !isfinite(value) || value < -limit || value > limit) return false;
  *out = value;
  return true;
}

static void publish_solar(const solar_times_t *events)
{
  portENTER_CRITICAL(&s_copy_lock);
  s_published_solar = *events;
  portEXIT_CRITICAL(&s_copy_lock);
}

static void update_solar(const device_config_t *config, const wac_time_snapshot_t *clock)
{
  double latitude, longitude;
  struct tm local;
  if (clock == NULL || !clock->usable || !config->location_ready
      || memchr(config->timezone, '\0', sizeof(config->timezone)) == NULL
      || config->timezone[0] == '\0'
      || !parse_coordinate(config->latitude, sizeof(config->latitude), 90, &latitude)
      || !parse_coordinate(config->longitude, sizeof(config->longitude), 180, &longitude)
      || localtime_r(&clock->utc_now, &local) == NULL) {
    s_solar_cached = false;
    publish_solar(&(solar_times_t){0});
    return;
  }
  local.tm_hour = 12;
  local.tm_min = local.tm_sec = 0;
  local.tm_isdst = -1;
  time_t noon = mktime(&local);
  if (noon == (time_t)-1) {
    s_solar_cached = false;
    publish_solar(&(solar_times_t){0});
    return;
  }
  if (!s_solar_cached || noon != s_solar_noon || latitude != s_solar_latitude
      || longitude != s_solar_longitude || strcmp(config->timezone, s_solar_timezone)) {
    for (int i = 0; i < 3; ++i) {
      struct tm day = local;
      day.tm_mday += i - 1;
      day.tm_isdst = -1;
      time_t day_noon = mktime(&day);
      s_solar_days[i] = (solar_times_t){0};
      if (day_noon != (time_t)-1)
        solar_times_calculate(day_noon, latitude, longitude, &s_solar_days[i]);
    }
    s_solar_noon = noon;
    s_solar_latitude = latitude;
    s_solar_longitude = longitude;
    strcpy(s_solar_timezone, config->timezone);
    s_solar_cached = true;
  }
  solar_times_t events = {0};
  if (s_solar_days[1].available && s_solar_days[2].available) {
    events.available = true;
    events.sunset = s_solar_days[1].sunset;
    events.sunrise = s_solar_days[2].sunrise;
  }
  publish_solar(&events);
}

static bool solar_appearance(const device_config_t *config,
                             const wac_time_snapshot_t *clock, bool previous)
{
  if (!s_solar_cached) return previous;
  bool missing = false;
  for (int i = 0; i < 2; ++i) {
    time_t start = s_solar_days[i].sunset + config->night_sunset_offset_minutes * 60;
    time_t end = s_solar_days[i + 1].sunrise + config->night_sunrise_offset_minutes * 60;
    /* Either known boundary can rule an interval out without needing the
     * other event. This matters on the first/last day of a polar season. */
    if ((s_solar_days[i].available && clock->utc_now < start)
        || (s_solar_days[i + 1].available && clock->utc_now >= end)) continue;
    if (!s_solar_days[i].available || !s_solar_days[i + 1].available) {
      missing = true;
      continue;
    }
    /* A short night can be removed by the offsets; it must not become a
     * nearly full-day interval merely because the shifted endpoints cross. */
    if (start < end && clock->utc_now >= start && clock->utc_now < end) return true;
  }
  return missing ? previous : false;
}

static int parse_hhmm_minutes(const char *value)
{
  if (value == NULL || strlen(value) != 5 || value[2] != ':') return -1;
  if (!isdigit((unsigned char) value[0]) || !isdigit((unsigned char) value[1])
      || !isdigit((unsigned char) value[3]) || !isdigit((unsigned char) value[4]))
    return -1;
  int hours = (value[0] - '0') * 10 + (value[1] - '0');
  int minutes = (value[3] - '0') * 10 + (value[4] - '0');
  if (hours > 23 || minutes > 59) return -1;
  return hours * 60 + minutes;
}

static bool select_appearance(const device_config_t *config,
                              const wac_time_snapshot_t *clock, bool previous)
{
  if (config->night_shift_mode == DEVICE_CONFIG_NIGHT_OFF) return false;
  if (config->night_shift_mode == DEVICE_CONFIG_NIGHT_ALWAYS_ON) return true;
  if (config->night_shift_mode == DEVICE_CONFIG_NIGHT_SOLAR)
    return solar_appearance(config, clock, previous);
  if (clock == NULL || !clock->usable) return previous;

  struct tm local_time = {0};
  if (localtime_r(&clock->utc_now, &local_time) == NULL) return previous;
  int start = parse_hhmm_minutes(config->night_shift_start);
  int end = parse_hhmm_minutes(config->night_shift_end);
  int now_minutes = local_time.tm_hour * 60 + local_time.tm_min;
  if (start < 0 || end < 0) return false;
  if (start == end) return true;
  if (start < end) return now_minutes >= start && now_minutes < end;
  return now_minutes >= start || now_minutes < end;
}

static void publish_appearance(bool night)
{
  portENTER_CRITICAL(&s_copy_lock);
  s_night = night;
  portEXIT_CRITICAL(&s_copy_lock);
}

static esp_err_t save_appearance(bool night)
{
  nvs_handle_t handle;
  esp_err_t err = nvs_open(NIGHT_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK) return err;
  err = nvs_set_u8(handle, NIGHT_EFFECTIVE_KEY, night ? 1 : 0);
  if (err == ESP_OK) err = nvs_commit(handle);
  nvs_close(handle);
  return err;
}

esp_err_t wac_night_init(const device_config_t *config, const wac_time_snapshot_t *clock)
{
  if (config == NULL) return ESP_ERR_INVALID_ARG;

  s_solar_cached = false;
  memset(s_solar_days, 0, sizeof(s_solar_days));
  publish_solar(&(solar_times_t){0});
  update_solar(config, clock);

  bool saved_night = false;
  nvs_handle_t handle;
  esp_err_t err = nvs_open(NIGHT_NAMESPACE, NVS_READONLY, &handle);
  if (err == ESP_OK) {
    uint8_t value = 0;
    err = nvs_get_u8(handle, NIGHT_EFFECTIVE_KEY, &value);
    if (err == ESP_OK && value <= 1) saved_night = value == 1;
    nvs_close(handle);
  }
  /* A missing or unusable appearance is ordinary startup fallback, rather
   * than evidence of a storage connection/read failure. */
  if (err == ESP_ERR_NVS_NOT_FOUND || err == ESP_ERR_NVS_TYPE_MISMATCH) err = ESP_OK;

  bool selected = select_appearance(config, clock, saved_night);
  portENTER_CRITICAL(&s_copy_lock);
  s_night = selected;
  s_backlight_ready = false;
  portEXIT_CRITICAL(&s_copy_lock);
  s_fade_started = false;
  s_initialized = true;
  /* Do not overwrite a record whose actual read failed. Its error is returned.
   * The live selection still applies for this run. */
  if (err == ESP_OK && selected != saved_night) err = save_appearance(selected);
  return err;
}

bool wac_night_active(void)
{
  portENTER_CRITICAL(&s_copy_lock);
  bool night = s_night;
  portEXIT_CRITICAL(&s_copy_lock);
  return night;
}

void wac_night_solar_times(solar_times_t *out)
{
  if (out == NULL) return;
  portENTER_CRITICAL(&s_copy_lock);
  *out = s_published_solar;
  portEXIT_CRITICAL(&s_copy_lock);
}

void wac_night_start_backlight(void)
{
  portENTER_CRITICAL(&s_copy_lock);
  s_backlight_ready = true;
  portEXIT_CRITICAL(&s_copy_lock);
}

static esp_err_t advance_backlight(int target)
{
  portENTER_CRITICAL(&s_copy_lock);
  bool ready = s_backlight_ready;
  portEXIT_CRITICAL(&s_copy_lock);
  if (!ready) return ESP_OK;

  int64_t now = esp_timer_get_time();
  if (!s_fade_started) {
    s_brightness = bsp_display_brightness_get();
    s_fade_step_us = now;
    s_fade_started = true;
    return ESP_OK;
  }
  if (s_brightness == target) {
    s_fade_step_us = now;
    return ESP_OK;
  }
  int64_t elapsed_steps = (now - s_fade_step_us) / FADE_POINT_US;
  if (elapsed_steps <= 0) return ESP_OK;
  int step = elapsed_steps > FADE_MAX_STEP ? FADE_MAX_STEP : (int) elapsed_steps;
  int next = s_brightness < target ? s_brightness + step : s_brightness - step;
  if ((s_brightness < target && next > target)
      || (s_brightness > target && next < target)) next = target;
  esp_err_t err = bsp_display_brightness_set(next);
  /* BSP updates its cached value before checking LEDC errors. Keep our last
   * successful level so a failed step is not mistaken for completed hardware. */
  if (err == ESP_OK) s_brightness = next;
  if (err != ESP_OK || elapsed_steps > FADE_MAX_STEP || next == target) {
    s_fade_step_us = now;
  } else {
    s_fade_step_us += step * FADE_POINT_US;
  }
  return err;
}

esp_err_t wac_night_update(const device_config_t *config, const wac_time_snapshot_t *clock)
{
  if (config == NULL) return ESP_ERR_INVALID_ARG;
  if (!s_initialized) return ESP_ERR_INVALID_STATE;
  update_solar(config, clock);
  bool previous = wac_night_active();
  bool selected = select_appearance(config, clock, previous);
  esp_err_t err = ESP_OK;
  if (selected != previous) {
    publish_appearance(selected);
    err = save_appearance(selected);
  }
  int target = selected ? config->night_brightness : config->day_brightness;
  esp_err_t fade_err = advance_backlight(target);
  return err != ESP_OK ? err : fade_err;
}

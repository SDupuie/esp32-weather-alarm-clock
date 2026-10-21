#include "clock_time.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "ds3231.h"

#include "timezone_table.h"

esp_err_t wac_time_apply_timezone(const char *timezone)
{
  if (timezone == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  const char *rules = timezone[0] == '\0' ? "UTC0" : NULL;
  for (size_t i = 0; rules == NULL && i < TIMEZONE_MAPPINGS_COUNT; ++i) {
    if (strcmp(timezone, TIMEZONE_MAPPINGS[i].iana_name) == 0) {
      rules = TIMEZONE_MAPPINGS[i].posix_name;
    }
  }
  if (rules == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  if (setenv("TZ", rules, 1) != 0) {
    return ESP_FAIL;
  }
  tzset();
  return ESP_OK;
}

/* The clock task owns hardware work and retry state. The mutex protects only
 * the copied status and notice acknowledgment shared with the UI. */
static SemaphoreHandle_t state_lock;
static wac_time_snapshot_t state;
static unsigned observed_warnings, pending_warnings;
static uint32_t network_generation;
static unsigned correction_attempts;
static bool correction_pending;
static TickType_t last_attempt;

static void select_warning_locked(void)
{
  state.warning = (pending_warnings & (1u << WAC_TIME_WARNING_LOST_TIME))
    ? WAC_TIME_WARNING_LOST_TIME
    : (pending_warnings & (1u << WAC_TIME_WARNING_CONNECTION))
      ? WAC_TIME_WARNING_CONNECTION : WAC_TIME_WARNING_NONE;
}

static void observe_locked(wac_time_warning_t warning)
{
  /* A warning can be dismissed once per run. Later RTC correction neither
   * clears an undismissed warning nor reopens one the user already dismissed. */
  unsigned bit = 1u << warning;
  if (!(observed_warnings & bit)) pending_warnings |= bit;
  observed_warnings |= bit;
  select_warning_locked();
}

static void publish_rtc_result(esp_err_t error, bool stopped)
{
  xSemaphoreTake(state_lock, portMAX_DELAY);
  state.rtc_error = error;
  if (stopped || error == ESP_ERR_INVALID_RESPONSE)
    observe_locked(WAC_TIME_WARNING_LOST_TIME);
  if (error != ESP_OK && error != ESP_ERR_INVALID_RESPONSE &&
      error != ESP_ERR_INVALID_ARG)
    observe_locked(WAC_TIME_WARNING_CONNECTION);
  xSemaphoreGive(state_lock);
}

static bool network_date_valid(time_t utc)
{
  struct tm date;
  return gmtime_r(&utc, &date) && date.tm_year >= 100;
}

esp_err_t wac_time_init(const char *timezone)
{
  esp_err_t error = wac_time_apply_timezone(timezone);
  if (error != ESP_OK) return error;
  if (!state_lock) state_lock = xSemaphoreCreateMutex();
  if (!state_lock) return ESP_ERR_NO_MEM;
  xSemaphoreTake(state_lock, portMAX_DELAY);
  memset(&state, 0, sizeof(state));
  observed_warnings = pending_warnings = 0;
  xSemaphoreGive(state_lock);
  network_generation = correction_attempts = 0;
  correction_pending = false;
  last_attempt = 0;

  esp_err_t init_error = wac_ds3231_init();
  time_t utc = 0;
  bool stopped = false;
  error = init_error == ESP_OK ? wac_ds3231_read(&utc, &stopped) : init_error;
  publish_rtc_result(error, stopped);
  if (init_error == ESP_OK && error == ESP_OK && !stopped) {
    struct timeval clock = { .tv_sec = utc, .tv_usec = 0 };
    if (settimeofday(&clock, NULL) == 0) {
      xSemaphoreTake(state_lock, portMAX_DELAY);
      state.source = WAC_TIME_SOURCE_RTC;
      state.usable = true;
      xSemaphoreGive(state_lock);
    } else {
      /* Failure to seed the system clock must never make an uninitialized
       * system date usable. The RTC date itself was read successfully. */
      publish_rtc_result(ESP_FAIL, false);
    }
  }
  return ESP_OK;
}

void wac_time_poll(uint32_t actual_network_sync_generation)
{
  if (!state_lock) return;
  if (actual_network_sync_generation != network_generation) {
    network_generation = actual_network_sync_generation;
    time_t utc = time(NULL);
    if (actual_network_sync_generation != 0 && network_date_valid(utc)) {
      xSemaphoreTake(state_lock, portMAX_DELAY);
      state.source = WAC_TIME_SOURCE_NETWORK;
      state.usable = true;
      xSemaphoreGive(state_lock);
      correction_pending = true;
      correction_attempts = 0;
    }
  }
  if (!correction_pending) return;
  TickType_t now = xTaskGetTickCount();
  if (correction_attempts &&
      (TickType_t)(now - last_attempt) < pdMS_TO_TICKS(5000)) return;
  last_attempt = now;
  ++correction_attempts;

  esp_err_t init_error = wac_ds3231_init();
  time_t rtc_utc = 0;
  bool stopped = false;
  esp_err_t error = init_error == ESP_OK
    ? wac_ds3231_read(&rtc_utc, &stopped) : init_error;
  publish_rtc_result(error, stopped);
  /* A malformed date is lost-time evidence but successful register access.
   * Failed access must be retried before correction can erase that evidence. */
  if (init_error == ESP_OK &&
      (error == ESP_OK || error == ESP_ERR_INVALID_RESPONSE)) {
    error = wac_ds3231_write(time(NULL));
    publish_rtc_result(error, false);
  }
  if (error == ESP_OK || error == ESP_ERR_INVALID_ARG || correction_attempts >= 3) correction_pending = false;
}

void wac_time_snapshot(wac_time_snapshot_t *out)
{
  if (!out) return;
  if (!state_lock) {
    memset(out, 0, sizeof(*out));
    return;
  }
  xSemaphoreTake(state_lock, portMAX_DELAY);
  *out = state;
  xSemaphoreGive(state_lock);
  out->utc_now = out->usable ? time(NULL) : 0;
}

void wac_time_dismiss_warning(wac_time_warning_t warning)
{
  if (!state_lock || (warning != WAC_TIME_WARNING_LOST_TIME &&
                      warning != WAC_TIME_WARNING_CONNECTION)) return;
  xSemaphoreTake(state_lock, portMAX_DELAY);
  pending_warnings &= ~(1u << warning);
  select_warning_locked();
  xSemaphoreGive(state_lock);
}

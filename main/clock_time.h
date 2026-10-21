#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "esp_err.h"

typedef enum {
  WAC_TIME_SOURCE_UNAVAILABLE,
  WAC_TIME_SOURCE_RTC,
  WAC_TIME_SOURCE_NETWORK,
} wac_time_source_t;

typedef enum {
  WAC_TIME_WARNING_NONE,
  WAC_TIME_WARNING_LOST_TIME,
  WAC_TIME_WARNING_CONNECTION,
} wac_time_warning_t;

typedef struct {
  wac_time_source_t source;
  bool usable;
  time_t utc_now;
  wac_time_warning_t warning;
  esp_err_t rtc_error;
} wac_time_snapshot_t;

/* Empty timezone is permitted before first-use location setup and uses UTC. */
esp_err_t wac_time_apply_timezone(const char *timezone);
/* Initialize before starting tasks; poll runs in one clock-control task.
 * RTC failure is published in
 * the snapshot; it does not prevent that task from waiting for network time. */
esp_err_t wac_time_init(const char *timezone);
/* Generation starts at zero and changes only on actual SNTP completion. */
void wac_time_poll(uint32_t actual_network_sync_generation);
void wac_time_snapshot(wac_time_snapshot_t *out);
/* Dismiss the warning the user saw; leave newly observed other warnings. */
void wac_time_dismiss_warning(wac_time_warning_t warning);

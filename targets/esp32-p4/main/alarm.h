#pragma once

#include "alarm_schedule.h"
#include "clock_time.h"
#include "esp_err.h"

typedef enum {
  WAC_ALARM_IDLE,
  WAC_ALARM_RINGING,
  WAC_ALARM_SNOOZED,
} wac_alarm_phase_t;

typedef struct {
  wac_alarm_phase_t phase;
  wac_alarm_occurrence_t current;
  wac_alarm_occurrence_t next;
  time_t snooze_deadline;
  wac_alarm_occurrence_t missed;
  esp_err_t storage_error;
} wac_alarm_snapshot_t;

/* Startup only, before tasks. Loads operating data without rewriting settings.
 * A returned storage error still leaves the owner initialized. */
esp_err_t wac_alarm_init(void);
/* All mutations have one writer: the clock-control task, outside the display
 * lock. Off cancels captured ringing/snooze even without usable time; otherwise
 * a loaded snooze is evaluated only when clock->usable is true. Runtime Off->On
 * and enabled hour/minute/repeat-day edits skip past occurrences once time is
 * usable; already-On startup uses ordinary recovery. Runtime arming is not
 * persisted and leaves captured ringing/snooze intact. */
esp_err_t wac_alarm_update(const device_config_t *config, const wac_time_snapshot_t *clock);
esp_err_t wac_alarm_snooze(const device_config_t *config, const wac_time_snapshot_t *clock);
esp_err_t wac_alarm_dismiss(void);
esp_err_t wac_alarm_dismiss_missed(void);
/* Short copied read. Action transitions publish before any slow storage work;
 * their in-memory effect remains even when that save fails. */
void wac_alarm_copy(wac_alarm_snapshot_t *out);

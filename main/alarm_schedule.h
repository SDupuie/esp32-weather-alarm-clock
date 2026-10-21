#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "device_config.h"

typedef struct {
  bool valid;
  time_t utc;
  int32_t local_date; /* Intended schedule date, YYYYMMDD. */
  uint16_t minute;    /* Saved local hour * 60 + minute. */
} wac_alarm_occurrence_t;

typedef struct {
  wac_alarm_occurrence_t previous;
  wac_alarm_occurrence_t next;
} wac_alarm_schedule_t;

/* Uses the system timezone already applied by clock_time. For recurring days,
 * previous includes equality and next is strictly later. None (zero days) has
 * no previous and next is today or tomorrow, including equality.
 * Disabled/invalid schedules return empty
 * occurrences. Handling, recovery and backward-clock suppression belong to
 * alarm, not this stateless calculation. */
void wac_alarm_schedule_calculate(const device_config_t *config, time_t utc_now,
                                  wac_alarm_schedule_t *out);

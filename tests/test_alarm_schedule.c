#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alarm_schedule.h"

/* Expected times below are UTC, calculated from the stated POSIX offsets and
 * calendar dates. They do not call local conversion to create expectations. */
static time_t utc(const char *text)
{
  int year, month, day, hour, minute, second;
  assert(sscanf(text, "%d-%d-%d %d:%d:%d", &year, &month, &day,
    &hour, &minute, &second) == 6);
  struct tm date = {.tm_year = year - 1900, .tm_mon = month - 1,
    .tm_mday = day, .tm_hour = hour, .tm_min = minute, .tm_sec = second};
  return timegm(&date);
}

typedef struct {
  const char *zone;
  unsigned hour, minute, days;
  const char *now, *previous, *next;
  int previous_date, next_date;
} case_t;

static const case_t cases[] = {
  /* Daily: strict next, equality, and nonzero seconds. */
  {"UTC0", 7, 0, 127, "2026-10-12 06:59:59", "2026-10-11 07:00:00", "2026-10-12 07:00:00", 20261011, 20261012},
  {"UTC0", 7, 0, 127, "2026-10-12 07:00:00", "2026-10-12 07:00:00", "2026-10-13 07:00:00", 20261012, 20261013},
  {"UTC0", 7, 0, 127, "2026-10-12 07:00:59", "2026-10-12 07:00:00", "2026-10-13 07:00:00", 20261012, 20261013},
  /* Monday-Friday, Sunday bit 0, Saturday bit 6, and a sparse mixed mask. */
  {"UTC0", 7, 0, 62, "2026-10-11 12:00:00", "2026-10-09 07:00:00", "2026-10-12 07:00:00", 20261009, 20261012},
  {"UTC0", 7, 0, 1, "2026-10-11 07:00:00", "2026-10-11 07:00:00", "2026-10-18 07:00:00", 20261011, 20261018},
  {"UTC0", 7, 0, 64, "2026-10-11 07:00:00", "2026-10-10 07:00:00", "2026-10-17 07:00:00", 20261010, 20261017},
  {"UTC0", 23, 59, 10, "2026-10-13 00:00:00", "2026-10-12 23:59:00", "2026-10-14 23:59:00", 20261012, 20261014},
  /* Year boundary, leap day, century non-leap rule, and arbitrary minute. */
  {"UTC0", 0, 0, 127, "2026-01-01 00:00:00", "2026-01-01 00:00:00", "2026-01-02 00:00:00", 20260101, 20260102},
  {"UTC0", 0, 0, 1, "2026-01-01 00:00:00", "2025-12-28 00:00:00", "2026-01-04 00:00:00", 20251228, 20260104},
  {"UTC0", 7, 3, 127, "2024-02-29 07:02:59", "2024-02-28 07:03:00", "2024-02-29 07:03:00", 20240228, 20240229},
  {"UTC0", 7, 3, 127, "2024-03-01 00:00:00", "2024-02-29 07:03:00", "2024-03-01 07:03:00", 20240229, 20240301},
  {"UTC0", 7, 3, 127, "2000-03-01 00:00:00", "2000-02-29 07:03:00", "2000-03-01 07:03:00", 20000229, 20000301},
  {"UTC0", 7, 3, 127, "2100-03-01 00:00:00", "2100-02-28 07:03:00", "2100-03-01 07:03:00", 21000228, 21000301},
  /* US spring: saved 02:30 uses post-jump -04, ringing at 01:30 EST.
   * Check before, at, after, and the week around the transition. */
  {"EST5EDT,M3.2.0,M11.1.0", 2, 30, 1, "2026-03-08 06:29:59", "2026-03-01 07:30:00", "2026-03-08 06:30:00", 20260301, 20260308},
  {"EST5EDT,M3.2.0,M11.1.0", 2, 30, 1, "2026-03-08 06:30:00", "2026-03-08 06:30:00", "2026-03-15 06:30:00", 20260308, 20260315},
  {"EST5EDT,M3.2.0,M11.1.0", 2, 30, 1, "2026-03-08 07:30:00", "2026-03-08 06:30:00", "2026-03-15 06:30:00", 20260308, 20260315},
  {"EST5EDT,M3.2.0,M11.1.0", 2, 0, 1, "2026-03-08 06:00:00", "2026-03-08 06:00:00", "2026-03-15 06:00:00", 20260308, 20260315},
  {"EST5EDT,M3.2.0,M11.1.0", 3, 0, 1, "2026-03-08 07:00:00", "2026-03-08 07:00:00", "2026-03-15 07:00:00", 20260308, 20260315},
  {"EST5EDT,M3.2.0,M11.1.0", 7, 0, 127, "2026-03-08 10:59:59", "2026-03-07 12:00:00", "2026-03-08 11:00:00", 20260307, 20260308},
  /* US fall: 01:30's first occurrence is not due; second -05 is due. */
  {"EST5EDT,M3.2.0,M11.1.0", 1, 30, 1, "2026-11-01 05:30:00", "2026-10-25 05:30:00", "2026-11-01 06:30:00", 20261025, 20261101},
  {"EST5EDT,M3.2.0,M11.1.0", 1, 30, 1, "2026-11-01 06:29:59", "2026-10-25 05:30:00", "2026-11-01 06:30:00", 20261025, 20261101},
  {"EST5EDT,M3.2.0,M11.1.0", 1, 30, 1, "2026-11-01 06:30:00", "2026-11-01 06:30:00", "2026-11-08 06:30:00", 20261101, 20261108},
  {"EST5EDT,M3.2.0,M11.1.0", 1, 0, 1, "2026-11-01 05:00:00", "2026-10-25 05:00:00", "2026-11-01 06:00:00", 20261025, 20261101},
  {"EST5EDT,M3.2.0,M11.1.0", 2, 0, 1, "2026-11-01 07:00:00", "2026-11-01 07:00:00", "2026-11-08 07:00:00", 20261101, 20261108},
  {"CST6CDT,M3.2.0,M11.1.0", 7, 0, 62, "2026-11-02 12:59:59", "2026-10-30 12:00:00", "2026-11-02 13:00:00", 20261030, 20261102},
  /* Lord Howe: +10:30 to +11 is a thirty-minute jump, and autumn repeats
   * 01:30-01:59. The occurrence remains on its intended Australian date. */
  {"<+1030>-10:30<+11>-11,M10.1.0,M4.1.0", 2, 15, 1, "2026-10-03 15:14:59", "2026-09-26 15:45:00", "2026-10-03 15:15:00", 20260927, 20261004},
  {"<+1030>-10:30<+11>-11,M10.1.0,M4.1.0", 2, 15, 1, "2026-10-03 15:15:00", "2026-10-03 15:15:00", "2026-10-10 15:15:00", 20261004, 20261011},
  {"<+1030>-10:30<+11>-11,M10.1.0,M4.1.0", 1, 45, 1, "2026-04-04 14:45:00", "2026-03-28 14:45:00", "2026-04-04 15:15:00", 20260329, 20260405},
  {"<+1030>-10:30<+11>-11,M10.1.0,M4.1.0", 1, 45, 1, "2026-04-04 15:15:00", "2026-04-04 15:15:00", "2026-04-11 15:15:00", 20260405, 20260412},
  /* Europe has different transition dates and transition-hour rules. */
  {"CET-1CEST,M3.5.0,M10.5.0/3", 2, 30, 1, "2026-03-29 00:30:00", "2026-03-29 00:30:00", "2026-04-05 00:30:00", 20260329, 20260405},
  {"CET-1CEST,M3.5.0,M10.5.0/3", 2, 30, 1, "2026-10-25 00:30:00", "2026-10-18 00:30:00", "2026-10-25 01:30:00", 20261018, 20261025},
  /* Fixed fractional offsets and both sides of the international date line. */
  {"IST-5:30", 7, 0, 127, "2026-10-12 01:30:00", "2026-10-12 01:30:00", "2026-10-13 01:30:00", 20261012, 20261013},
  {"<+0545>-5:45", 0, 3, 127, "2026-10-11 18:17:59", "2026-10-10 18:18:00", "2026-10-11 18:18:00", 20261011, 20261012},
  {"<+14>-14", 0, 0, 127, "2025-12-31 10:00:00", "2025-12-31 10:00:00", "2026-01-01 10:00:00", 20260101, 20260102},
  {"<-12>12", 23, 59, 127, "2026-01-01 11:59:00", "2026-01-01 11:59:00", "2026-01-02 11:59:00", 20251231, 20260101},
  /* A midnight spring gap maps back into the previous actual date; the
   * intended Sunday date must survive instead of becoming Saturday. */
  {"STD0DST,M3.2.0/0,M11.1.0/0", 0, 30, 1, "2026-03-07 23:30:00", "2026-03-07 23:30:00", "2026-03-14 23:30:00", 20260308, 20260315},
};

static void expect(const wac_alarm_occurrence_t *actual, const char *timestamp,
                   int date, unsigned minute)
{
  assert(actual->valid);
  assert(actual->utc == utc(timestamp));
  assert(actual->local_date == date);
  assert(actual->minute == minute);
}

static void schedule_checks(void)
{
  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
    const case_t *c = &cases[i];
    assert(setenv("TZ", c->zone, 1) == 0);
    tzset();
    device_config_t config = {.alarm_enabled = true, .alarm_hour = c->hour,
      .alarm_minute = c->minute, .alarm_repeat_days = c->days};
    device_config_t original = config;
    wac_alarm_schedule_t result;
    wac_alarm_schedule_calculate(&config, utc(c->now), &result);
    if (!result.previous.valid || result.previous.utc != utc(c->previous)
        || !result.next.valid || result.next.utc != utc(c->next)) {
      fprintf(stderr, "Schedule case %zu failed (%s at %s): previous=%lld next=%lld\n",
        i, c->zone, c->now, (long long)result.previous.utc, (long long)result.next.utc);
    }
    expect(&result.previous, c->previous, c->previous_date, c->hour * 60 + c->minute);
    expect(&result.next, c->next, c->next_date, c->hour * 60 + c->minute);
    assert(memcmp(&original, &config, sizeof(config)) == 0);
    assert(strcmp(getenv("TZ"), c->zone) == 0);
  }
}

static void empty(const device_config_t *config)
{
  wac_alarm_schedule_t result = {
    .previous = {.valid = true, .utc = 1, .local_date = 1, .minute = 1},
    .next = {.valid = true, .utc = 1, .local_date = 1, .minute = 1},
  };
  wac_alarm_schedule_calculate(config, utc("2026-10-12 07:00:00"), &result);
  assert(!result.previous.valid && result.previous.utc == 0
    && result.previous.local_date == 0 && result.previous.minute == 0);
  assert(!result.next.valid && result.next.utc == 0
    && result.next.local_date == 0 && result.next.minute == 0);
}

static void invalid_checks(void)
{
  device_config_t config = {.alarm_enabled = false, .alarm_hour = 7,
    .alarm_repeat_days = 62};
  empty(&config); empty(NULL);
  config.alarm_enabled = true;
  config.alarm_repeat_days = 128; empty(&config);
  config.alarm_repeat_days = 255; empty(&config);
  config.alarm_repeat_days = 62; config.alarm_hour = 24; empty(&config);
  config.alarm_hour = 7; config.alarm_minute = 60; empty(&config);
  wac_alarm_schedule_calculate(&config, 0, NULL);
}

static void one_time_checks(void)
{
  const case_t once[] = {
    {"UTC0", 7, 0, 0, "2026-10-12 06:59:59", NULL, "2026-10-12 07:00:00", 0, 20261012},
    {"UTC0", 7, 0, 0, "2026-10-12 07:00:00", NULL, "2026-10-12 07:00:00", 0, 20261012},
    {"UTC0", 7, 0, 0, "2026-10-12 07:00:01", NULL, "2026-10-13 07:00:00", 0, 20261013},
    {"UTC0", 0, 0, 0, "2026-12-31 23:59:59", NULL, "2027-01-01 00:00:00", 0, 20270101},
    {"UTC0", 0, 0, 0, "2027-01-01 00:00:00", NULL, "2027-01-01 00:00:00", 0, 20270101},
    {"EST5EDT,M3.2.0,M11.1.0", 2, 30, 0, "2026-03-08 06:29:59", NULL, "2026-03-08 06:30:00", 0, 20260308},
    {"EST5EDT,M3.2.0,M11.1.0", 2, 30, 0, "2026-03-08 06:30:01", NULL, "2026-03-09 06:30:00", 0, 20260309},
    {"EST5EDT,M3.2.0,M11.1.0", 1, 30, 0, "2026-11-01 05:30:00", NULL, "2026-11-01 06:30:00", 0, 20261101},
    {"EST5EDT,M3.2.0,M11.1.0", 1, 30, 0, "2026-11-01 06:30:00", NULL, "2026-11-01 06:30:00", 0, 20261101},
    {"<+1030>-10:30<+11>-11,M10.1.0,M4.1.0", 2, 15, 0, "2026-10-03 15:14:59", NULL, "2026-10-03 15:15:00", 0, 20261004},
  };
  for (size_t i = 0; i < sizeof(once) / sizeof(once[0]); ++i) {
    const case_t *c = &once[i];
    assert(setenv("TZ", c->zone, 1) == 0); tzset();
    device_config_t config = {.alarm_enabled = true, .alarm_hour = c->hour,
      .alarm_minute = c->minute, .alarm_repeat_days = 0};
    wac_alarm_schedule_t result;
    wac_alarm_schedule_calculate(&config, utc(c->now), &result);
    assert(!result.previous.valid);
    expect(&result.next, c->next, c->next_date, c->hour * 60 + c->minute);
  }
}

int main(void)
{
  schedule_checks();
  one_time_checks();
  invalid_checks();
  printf("Alarm schedule: %zu independently specified UTC/date cases passed.\n",
    sizeof(cases) / sizeof(cases[0]));
  return 0;
}

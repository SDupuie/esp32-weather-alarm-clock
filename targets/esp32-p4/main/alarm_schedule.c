#include "alarm_schedule.h"

static int month_days(int year, int month)
{
  static const unsigned char days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return days[month] + (month == 1 && year % 4 == 0
    && (year % 100 != 0 || year % 400 == 0));
}

static void move_day(struct tm *date, int direction)
{
  date->tm_mday += direction;
  date->tm_wday = (date->tm_wday + direction + 7) % 7;
  if (date->tm_mday < 1) {
    if (--date->tm_mon < 0) {
      date->tm_mon = 11;
      --date->tm_year;
    }
    date->tm_mday = month_days(date->tm_year + 1900, date->tm_mon);
  } else if (date->tm_mday > month_days(date->tm_year + 1900, date->tm_mon)) {
    date->tm_mday = 1;
    if (++date->tm_mon > 11) {
      date->tm_mon = 0;
      ++date->tm_year;
    }
  }
}

static bool matches(const struct tm *actual, const struct tm *wanted)
{
  return actual->tm_year == wanted->tm_year && actual->tm_mon == wanted->tm_mon
    && actual->tm_mday == wanted->tm_mday && actual->tm_hour == wanted->tm_hour
    && actual->tm_min == wanted->tm_min && actual->tm_sec == 0;
}

static bool occurrence_utc(const struct tm *wanted, time_t *utc)
{
  time_t candidate[2];
  bool exact[2];
  for (int dst = 0; dst < 2; ++dst) {
    struct tm date = *wanted, actual;
    date.tm_isdst = dst;
    candidate[dst] = mktime(&date);
    if (!localtime_r(&candidate[dst], &actual)) return false;
    exact[dst] = matches(&actual, wanted);
  }
  if (exact[0] || exact[1]) {
    /* Both interpretations match only in a repeated interval. Its later UTC
     * value uses the offset after the transition, regardless of DST labels. */
    *utc = exact[0] && (!exact[1] || candidate[0] > candidate[1])
      ? candidate[0] : candidate[1];
  } else {
    /* A skipped wall time has no exact match. The larger offset after the
     * forward jump maps it to the earlier UTC candidate, before the jump.
     * This also handles jumps shorter than an hour. */
    *utc = candidate[0] < candidate[1] ? candidate[0] : candidate[1];
  }
  return true;
}

void wac_alarm_schedule_calculate(const device_config_t *config, time_t utc_now,
                                  wac_alarm_schedule_t *out)
{
  if (!out) return;
  *out = (wac_alarm_schedule_t){0};
  if (!config || !config->alarm_enabled || config->alarm_hour > 23
      || config->alarm_minute > 59 || !config->alarm_repeat_days
      || (config->alarm_repeat_days & 0x80)) return;

  struct tm date;
  if (!localtime_r(&utc_now, &date) || date.tm_year < -1899
      || date.tm_year > 8099) return;
  date.tm_hour = config->alarm_hour;
  date.tm_min = config->alarm_minute;
  date.tm_sec = 0;
  /* A weekly schedule needs seven days each way. One extra calendar day
   * covers a transition that moves a midnight occurrence across a date. */
  for (int i = 0; i < 8; ++i) move_day(&date, -1);
  for (int i = 0; i < 17; ++i, move_day(&date, 1)) {
    if (date.tm_year < -1899 || date.tm_year > 8099
        || !(config->alarm_repeat_days & (1u << date.tm_wday))) continue;
    time_t utc;
    if (!occurrence_utc(&date, &utc)) continue;
    wac_alarm_occurrence_t occurrence = {
      .valid = true, .utc = utc,
      .local_date = (date.tm_year + 1900) * 10000 + (date.tm_mon + 1) * 100 + date.tm_mday,
      .minute = config->alarm_hour * 60 + config->alarm_minute,
    };
    if (utc <= utc_now) {
      if (!out->previous.valid || utc > out->previous.utc) out->previous = occurrence;
    } else if (!out->next.valid || utc < out->next.utc) {
      out->next = occurrence;
    }
  }
}

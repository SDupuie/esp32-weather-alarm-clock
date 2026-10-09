#include <assert.h>
#include <stdio.h>
#include <time.h>

#include "wac/time.h"

int main(void)
{
  struct tm local;
  /* Known UTC dates, independent of the workstation's timezone. */
  time_t winter = 1705320000; /* 2024-01-15 12:00 UTC */
  time_t summer = 1721044800; /* 2024-07-15 12:00 UTC */
  assert(wac_time_apply_timezone("") == ESP_OK);
  localtime_r(&winter, &local);
  assert(local.tm_hour == 12);
  assert(wac_time_apply_timezone("America/New_York") == ESP_OK);
  localtime_r(&winter, &local);
  assert(local.tm_hour == 7 && local.tm_isdst == 0);
  localtime_r(&summer, &local);
  assert(local.tm_hour == 8 && local.tm_isdst > 0);
  assert(wac_time_apply_timezone("auto") == ESP_ERR_INVALID_ARG);
  assert(wac_time_apply_timezone(NULL) == ESP_ERR_INVALID_ARG);
  localtime_r(&summer, &local);
  assert(local.tm_hour == 8); /* A bad selection must not change clock rules. */
  puts("Timezone mapping checks passed (RTC and network time not simulated).");
  return 0;
}

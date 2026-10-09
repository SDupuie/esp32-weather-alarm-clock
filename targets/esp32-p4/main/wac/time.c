#include "time.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

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

#include "night.h"

#include <ctype.h>
#include <string.h>
#include <time.h>

static bool current_night;

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

bool wac_night_update(const device_config_t *config, const wac_time_snapshot_t *clock)
{
  if (config == NULL || config->night_shift_mode == DEVICE_CONFIG_NIGHT_OFF
      || config->night_shift_mode == DEVICE_CONFIG_NIGHT_SOLAR) {
    current_night = false;
  } else if (config->night_shift_mode == DEVICE_CONFIG_NIGHT_ALWAYS_ON) {
    current_night = true;
  } else if (clock != NULL && clock->usable) {
    struct tm local_time = {0};
    localtime_r(&clock->utc_now, &local_time);
    int start = parse_hhmm_minutes(config->night_shift_start);
    int end = parse_hhmm_minutes(config->night_shift_end);
    int now_minutes = local_time.tm_hour * 60 + local_time.tm_min;
    if (start < 0 || end < 0) current_night = false;
    else if (start == end) current_night = true;
    else if (start < end) current_night = now_minutes >= start && now_minutes < end;
    else current_night = now_minutes >= start || now_minutes < end;
  }
  return current_night;
}

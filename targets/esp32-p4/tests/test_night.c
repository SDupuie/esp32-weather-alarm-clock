#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "night.h"
#include "test_support.h"

static device_config_t config = {
  .night_shift_mode = DEVICE_CONFIG_NIGHT_FIXED,
  .night_shift_start = "22:00",
  .night_shift_end = "06:00",
};
static wac_time_snapshot_t clock_snapshot;

static bool at_utc(int year, int month, int day, int hour, int minute)
{
  struct tm utc = { .tm_year = year - 1900, .tm_mon = month - 1,
    .tm_mday = day, .tm_hour = hour, .tm_min = minute };
  clock_snapshot.usable = true;
  clock_snapshot.utc_now = timegm(&utc);
  return wac_night_update(&config, &clock_snapshot);
}

static bool at(int hour, int minute)
{
  return at_utc(2026, 10, 9, hour, minute);
}

static void fixed_schedule_checks(void)
{
  assert(!at(21, 59));
  assert(at(22, 0));
  assert(at(23, 59));
  assert(at(0, 0));
  assert(at(5, 59));
  assert(!at(6, 0));
  assert(!at(12, 0));

  snprintf(config.night_shift_start, sizeof(config.night_shift_start), "09:00");
  snprintf(config.night_shift_end, sizeof(config.night_shift_end), "17:00");
  assert(!at(8, 59));
  assert(at(9, 0));
  assert(at(16, 59));
  assert(!at(17, 0));
  assert(!at(23, 0));

  snprintf(config.night_shift_end, sizeof(config.night_shift_end), "09:00");
  assert(at(0, 0));
  assert(at(9, 0));
  assert(at(23, 59));
}

static void mode_and_unavailable_checks(void)
{
  clock_snapshot.usable = false;
  assert(wac_night_update(&config, &clock_snapshot));
  config.night_shift_mode = DEVICE_CONFIG_NIGHT_OFF;
  assert(!wac_night_update(&config, &clock_snapshot));
  config.night_shift_mode = DEVICE_CONFIG_NIGHT_FIXED;
  assert(!wac_night_update(&config, &clock_snapshot));
  config.night_shift_mode = DEVICE_CONFIG_NIGHT_ALWAYS_ON;
  assert(wac_night_update(&config, &clock_snapshot));
  config.night_shift_mode = DEVICE_CONFIG_NIGHT_FIXED;
  assert(wac_night_update(&config, &clock_snapshot));

  /* Unavailable time retains appearance before endpoint validation, as the
   * original renderer did. Corrupt input here is a controlled test example. */
  snprintf(config.night_shift_start, sizeof(config.night_shift_start), "25:00");
  assert(wac_night_update(&config, &clock_snapshot));
  assert(!at(12, 0));
  clock_snapshot.usable = false;
  assert(!wac_night_update(&config, &clock_snapshot));
  config.night_shift_mode = DEVICE_CONFIG_NIGHT_ALWAYS_ON;
  assert(wac_night_update(&config, &clock_snapshot));
  config.night_shift_mode = DEVICE_CONFIG_NIGHT_SOLAR;
  assert(!wac_night_update(&config, &clock_snapshot));
  assert(!at(23, 0));

  config.night_shift_mode = DEVICE_CONFIG_NIGHT_FIXED;
  snprintf(config.night_shift_start, sizeof(config.night_shift_start), "22:00");
  const char *invalid_endpoints[] = { "6:00", "06-00", "ab:cd", "06:60", "24:00", "" };
  for (size_t i = 0; i < sizeof(invalid_endpoints) / sizeof(invalid_endpoints[0]); ++i) {
    snprintf(config.night_shift_end, sizeof(config.night_shift_end), "%s", invalid_endpoints[i]);
    assert(!at(23, 0));
  }
  snprintf(config.night_shift_end, sizeof(config.night_shift_end), "06:00");
}

static void local_timezone_checks(void)
{
  /* Use the same system local-time rules selected by the clock owner. */
  assert(setenv("TZ", "CST6CDT,M3.2.0,M11.1.0", 1) == 0);
  tzset();
  assert(!at_utc(2026, 7, 10, 2, 59)); /* 21:59 on the preceding local day. */
  assert(at_utc(2026, 7, 10, 3, 0));
  assert(at_utc(2026, 7, 10, 10, 59));
  assert(!at_utc(2026, 7, 10, 11, 0));
  assert(!at_utc(2026, 1, 10, 3, 59));
  assert(at_utc(2026, 1, 10, 4, 0));
  assert(at_utc(2026, 1, 10, 11, 59));
  assert(!at_utc(2026, 1, 10, 12, 0));
}

int main(void)
{
  fake_nvs_reset();
  assert(setenv("TZ", "UTC0", 1) == 0);
  tzset();
  /* First call establishes that scheduled startup without time selects Day. */
  assert(!wac_night_update(&config, &clock_snapshot));
  fixed_schedule_checks();
  mode_and_unavailable_checks();
  local_timezone_checks();
  for (int op = 0; op < FAKE_OP_COUNT; ++op) assert(fake_nvs_calls(op) == 0);
  assert(fake_nvs_key_count() == 0);
  puts("Production Night Shift policy checks passed: current modes, fixed boundaries, retained appearance, local time, no storage writes.");
  return 0;
}

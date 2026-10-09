#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "test_support.h"
#include "clock_time.h"
#include "ds3231.h"

static time_t utc(int year, int month, int day, int hour, int minute, int second)
{
  struct tm date = { .tm_year = year - 1900, .tm_mon = month - 1,
    .tm_mday = day, .tm_hour = hour, .tm_min = minute, .tm_sec = second };
  return timegm(&date);
}
static uint8_t bcd(int value) { return (uint8_t)(value / 10 * 16 + value % 10); }
static void calendar(int year, int month, int day, int hour, int minute, int second)
{
  uint8_t *regs = fake_i2c_registers();
  regs[0] = bcd(second); regs[1] = bcd(minute); regs[2] = bcd(hour);
  regs[3] = 1; regs[4] = bcd(day); regs[5] = bcd(month); regs[6] = bcd(year - 2000);
}
static time_t normal_date(void) { return utc(2026, 10, 9, 12, 34, 56); }
static void reset(void)
{
  fake_nvs_reset(); fake_i2c_reset(); fake_clock_reset(0);
  calendar(2026, 10, 9, 12, 34, 56);
}
static wac_time_snapshot_t snapshot(void)
{
  wac_time_snapshot_t result;
  wac_time_snapshot(&result);
  return result;
}
static unsigned writes(uint8_t reg)
{
  unsigned count = 0;
  for (unsigned i = 0; i < fake_i2c_event_count(); ++i) {
    fake_i2c_event_t event = fake_i2c_event(i);
    if (event.write && event.reg == reg) ++count;
  }
  return count;
}
static void synchronize(uint32_t generation)
{
  fake_clock_set(utc(2026, 10, 9, 15, 0, 0));
  wac_time_poll(generation);
}
static void no_storage_activity(void)
{
  for (int op = 0; op < FAKE_OP_COUNT; ++op) assert(fake_nvs_calls(op) == 0);
  assert(fake_nvs_key_count() == 0);
}
static void timezone_checks(void)
{
  struct tm local;
  time_t winter = utc(2024, 1, 15, 12, 0, 0);
  time_t summer = utc(2024, 7, 15, 12, 0, 0);
  assert(wac_time_apply_timezone("") == ESP_OK);
  localtime_r(&winter, &local); assert(local.tm_hour == 12);
  assert(wac_time_apply_timezone("America/New_York") == ESP_OK);
  localtime_r(&winter, &local); assert(local.tm_hour == 7 && local.tm_isdst == 0);
  localtime_r(&summer, &local); assert(local.tm_hour == 8 && local.tm_isdst > 0);
  assert(wac_time_apply_timezone("auto") == ESP_ERR_INVALID_ARG);
  assert(wac_time_apply_timezone(NULL) == ESP_ERR_INVALID_ARG);
  localtime_r(&summer, &local); assert(local.tm_hour == 8);
}
static void startup_checks(void)
{
  /* First driver initialization: attachment failure leaves no device handle. */
  reset(); fake_i2c_fail_add(true);
  assert(wac_time_init("") == ESP_OK);
  assert(!snapshot().usable && snapshot().warning == WAC_TIME_WARNING_CONNECTION);
  fake_i2c_fail_add(false); synchronize(1);
  assert(snapshot().usable && writes(0) == 1); /* Retry attachment then inspect. */
  reset(); fake_i2c_registers()[0x0e] = 0x9d;
  assert(wac_time_init("America/New_York") == ESP_OK);
  wac_time_snapshot_t current = snapshot();
  assert(current.usable && current.source == WAC_TIME_SOURCE_RTC);
  assert(current.utc_now == normal_date() && current.warning == WAC_TIME_WARNING_NONE);
  assert(fake_i2c_registers()[0x0e] == 0x1d); /* Other control bits preserved. */
  assert(fake_clock_set_calls() == 1);
  struct tm local; localtime_r(&current.utc_now, &local); assert(local.tm_hour == 8);
  wac_time_poll(0); assert(writes(0) == 0); /* RTC is no SNTP completion. */
  fake_clock_advance(1000); assert(snapshot().utc_now == normal_date() + 1);

  reset(); fake_i2c_registers()[0x0f] = 0x80;
  assert(wac_time_init("") == ESP_OK); current = snapshot();
  assert(!current.usable && current.utc_now == 0 && current.source == WAC_TIME_SOURCE_UNAVAILABLE);
  assert(current.warning == WAC_TIME_WARNING_LOST_TIME && fake_clock_set_calls() == 0);
  assert(fake_i2c_registers()[0x0f] & 0x80);

  reset(); fake_i2c_registers()[1] = 0x6a; /* Constructed malformed BCD. */
  assert(wac_time_init("") == ESP_OK);
  assert(!snapshot().usable && snapshot().warning == WAC_TIME_WARNING_LOST_TIME);
  assert(snapshot().rtc_error == ESP_ERR_INVALID_RESPONSE);

  reset(); fake_i2c_fail(false, 0, 1);
  assert(wac_time_init("") == ESP_OK);
  assert(!snapshot().usable && snapshot().warning == WAC_TIME_WARNING_CONNECTION);
  assert(snapshot().rtc_error == ESP_ERR_TIMEOUT);

  reset(); fake_clock_fail_set(true);
  assert(wac_time_init("") == ESP_OK); assert(!snapshot().usable);
  reset(); fake_i2c_fail(false, 0x0e, 1);
  assert(wac_time_init("") == ESP_OK); assert(!snapshot().usable);
  assert(snapshot().warning == WAC_TIME_WARNING_CONNECTION);
  no_storage_activity();
}
static void correction_checks(void)
{
  reset(); assert(wac_time_init("") == ESP_OK);
  fake_i2c_clear_activity(); synchronize(1);
  assert(snapshot().source == WAC_TIME_SOURCE_NETWORK && snapshot().usable);
  assert(writes(0) == 1 && writes(0x0f) == 1);
  unsigned count = fake_i2c_event_count(); wac_time_poll(1);
  assert(fake_i2c_event_count() == count); /* No repeat writes per clock tick. */
  synchronize(2); assert(writes(0) == 2); /* Later network events re-correct. */

  reset(); fake_i2c_registers()[0x0f] = 0x8b;
  assert(wac_time_init("") == ESP_OK); fake_i2c_clear_activity(); synchronize(1);
  assert(snapshot().warning == WAC_TIME_WARNING_LOST_TIME && snapshot().usable);
  assert(fake_i2c_registers()[0x0f] == 0x0b); /* OSF cleared; 32kHz/alarm flags preserved. */
  bool inspected = false, written = false;
  for (unsigned i = 0; i < fake_i2c_event_count(); ++i) {
    fake_i2c_event_t event = fake_i2c_event(i);
    if (!event.write && event.reg == 0 && event.length == 16) inspected = true;
    if (event.write && event.reg == 0) { assert(inspected); written = true; }
    if (event.write && event.reg == 0x0f) assert(written);
  }
  wac_time_dismiss_warning(snapshot().warning); assert(snapshot().warning == WAC_TIME_WARNING_NONE);
  synchronize(2); assert(snapshot().warning == WAC_TIME_WARNING_NONE);
  assert(wac_time_init("") == ESP_OK); assert(snapshot().warning == WAC_TIME_WARNING_NONE);

  reset(); fake_i2c_registers()[0x0f] = 0x80; fake_i2c_fail(false, 0, 1);
  assert(wac_time_init("") == ESP_OK); assert(snapshot().warning == WAC_TIME_WARNING_CONNECTION);
  fake_i2c_clear_activity(); synchronize(1);
  assert(snapshot().warning == WAC_TIME_WARNING_LOST_TIME); /* Read again before correction. */
  assert(writes(0) == 1 && !(fake_i2c_registers()[0x0f] & 0x80));

  reset(); fake_i2c_registers()[4] = 0x32; fake_i2c_registers()[0x0f] = 0x80;
  assert(wac_time_init("") == ESP_OK); synchronize(1);
  assert(snapshot().usable && snapshot().warning == WAC_TIME_WARNING_LOST_TIME);
  assert(fake_i2c_registers()[4] == 0x09 && !(fake_i2c_registers()[0x0f] & 0x80));
  no_storage_activity();

  reset(); fake_i2c_registers()[0x0f] = 0x80;
  assert(wac_time_init("") == ESP_OK);
  wac_time_poll(0); assert(!snapshot().usable && writes(0) == 0);
  fake_clock_set(0); wac_time_poll(1); assert(!snapshot().usable && writes(0) == 0);
  fake_clock_set(utc(2100, 1, 1, 0, 0, 0)); wac_time_poll(2);
  assert(snapshot().usable && snapshot().source == WAC_TIME_SOURCE_NETWORK);
  assert(snapshot().rtc_error == ESP_ERR_INVALID_ARG && writes(0) == 0);
}
static void correction_failure_checks(void)
{
  reset(); fake_i2c_registers()[0x0f] = 0x80; assert(wac_time_init("") == ESP_OK);
  fake_i2c_clear_activity(); fake_i2c_fail(true, 0, 1); synchronize(1);
  assert(snapshot().usable && snapshot().source == WAC_TIME_SOURCE_NETWORK);
  assert(snapshot().rtc_error == ESP_ERR_TIMEOUT && snapshot().warning == WAC_TIME_WARNING_LOST_TIME);
  assert(writes(0x0f) == 0 && (fake_i2c_registers()[0x0f] & 0x80));
  unsigned count = fake_i2c_event_count(); wac_time_poll(1); assert(fake_i2c_event_count() == count);
  fake_clock_advance(5000); wac_time_poll(1);
  assert(snapshot().rtc_error == ESP_OK && !(fake_i2c_registers()[0x0f] & 0x80));

  reset(); fake_i2c_registers()[0x0f] = 0x80; assert(wac_time_init("") == ESP_OK);
  fake_i2c_clear_activity(); fake_i2c_fail(true, 0x0f, 1); synchronize(1);
  assert(snapshot().rtc_error == ESP_ERR_TIMEOUT && (fake_i2c_registers()[0x0f] & 0x80));
  assert(writes(0) == 1 && snapshot().usable);

  reset(); fake_i2c_registers()[0x0f] = 0x80; assert(wac_time_init("") == ESP_OK);
  fake_i2c_clear_activity(); fake_i2c_fail(false, 0x0f, 1); synchronize(1);
  assert(snapshot().rtc_error == ESP_ERR_TIMEOUT && writes(0x0f) == 0);
  assert(fake_i2c_registers()[0x0f] & 0x80); /* Failed status read cannot clear OSF. */

  reset(); assert(wac_time_init("") == ESP_OK);
  fake_i2c_clear_activity(); fake_i2c_fail(false, 0, 1); synchronize(1);
  assert(snapshot().usable && snapshot().warning == WAC_TIME_WARNING_CONNECTION);
  assert(writes(0) == 0); /* Never erase uninspected evidence after failed access. */
  for (int attempt = 0; attempt < 2; ++attempt) {
    fake_i2c_fail(false, 0, 1); fake_clock_advance(5000); wac_time_poll(1);
  }
  count = fake_i2c_event_count(); fake_clock_advance(60000); wac_time_poll(1);
  assert(fake_i2c_event_count() == count); /* Finite retry budget exhausted. */
  fake_i2c_clear_activity(); synchronize(2); assert(writes(0) == 1);
  wac_time_dismiss_warning(snapshot().warning); assert(snapshot().warning == WAC_TIME_WARNING_NONE);
  assert(wac_time_init("") == ESP_OK); assert(snapshot().warning == WAC_TIME_WARNING_NONE);

  reset(); fake_i2c_registers()[0x0f] = 0x80; assert(wac_time_init("") == ESP_OK);
  wac_time_dismiss_warning(snapshot().warning); assert(snapshot().warning == WAC_TIME_WARNING_NONE);
  assert(wac_time_init("") == ESP_OK); assert(snapshot().warning == WAC_TIME_WARNING_LOST_TIME);
  no_storage_activity();
}
static void dismissal_race_checks(void)
{
  /* The queued request carries the warning visible when the button was
   * pressed. Correction can discover a different warning before or after it. */
  reset(); fake_i2c_registers()[0x0f] = 0x80; fake_i2c_fail(false, 0, 1);
  assert(wac_time_init("") == ESP_OK);
  assert(snapshot().warning == WAC_TIME_WARNING_CONNECTION);
  wac_time_dismiss_warning(WAC_TIME_WARNING_CONNECTION); synchronize(1);
  assert(snapshot().warning == WAC_TIME_WARNING_LOST_TIME);
  wac_time_dismiss_warning(WAC_TIME_WARNING_CONNECTION);
  assert(snapshot().warning == WAC_TIME_WARNING_LOST_TIME);
  wac_time_dismiss_warning(WAC_TIME_WARNING_NONE);
  wac_time_dismiss_warning((wac_time_warning_t)-1);
  wac_time_dismiss_warning((wac_time_warning_t)100);
  assert(snapshot().warning == WAC_TIME_WARNING_LOST_TIME);
  wac_time_dismiss_warning(WAC_TIME_WARNING_LOST_TIME);
  assert(snapshot().warning == WAC_TIME_WARNING_NONE);

  reset(); fake_i2c_registers()[0x0f] = 0x80;
  assert(wac_time_init("") == ESP_OK);
  wac_time_dismiss_warning(WAC_TIME_WARNING_LOST_TIME);
  fake_i2c_clear_activity(); fake_i2c_fail(false, 0, 1); synchronize(1);
  assert(snapshot().warning == WAC_TIME_WARNING_CONNECTION);
  wac_time_dismiss_warning(WAC_TIME_WARNING_LOST_TIME);
  assert(snapshot().warning == WAC_TIME_WARNING_CONNECTION);
  wac_time_dismiss_warning(WAC_TIME_WARNING_CONNECTION);
  assert(snapshot().warning == WAC_TIME_WARNING_NONE);
  no_storage_activity();
}
static void calendar_checks(void)
{
  reset(); assert(wac_ds3231_init() == ESP_OK);
  time_t value; bool stopped;
  calendar(2024, 2, 29, 23, 59, 59);
  assert(wac_ds3231_read(&value, &stopped) == ESP_OK && !stopped);
  assert(value == utc(2024, 2, 29, 23, 59, 59));
  fake_i2c_registers()[6] = 0x23; /* Non-leap February 29 rejected. */
  assert(wac_ds3231_read(&value, &stopped) == ESP_ERR_INVALID_RESPONSE);
  calendar(2026, 10, 9, 0, 0, 0);
  fake_i2c_registers()[2] = 0x52; /* 12 AM in 12-hour mode. */
  assert(wac_ds3231_read(&value, &stopped) == ESP_OK && value == utc(2026, 10, 9, 0, 0, 0));
  fake_i2c_registers()[2] = 0x72; /* 12 PM. */
  assert(wac_ds3231_read(&value, &stopped) == ESP_OK && value == utc(2026, 10, 9, 12, 0, 0));
  fake_i2c_registers()[2] = 0x61; /* 1 PM. */
  assert(wac_ds3231_read(&value, &stopped) == ESP_OK && value == utc(2026, 10, 9, 13, 0, 0));
  fake_i2c_registers()[2] = 0x40; /* Invalid 12-hour zero. */
  assert(wac_ds3231_read(&value, &stopped) == ESP_ERR_INVALID_RESPONSE);
  calendar(2026, 10, 9, 12, 0, 0); fake_i2c_registers()[5] |= 0x80;
  assert(wac_ds3231_read(&value, &stopped) == ESP_ERR_INVALID_RESPONSE);
  const time_t dates[] = { utc(2000, 1, 1, 0, 0, 0), utc(2024, 2, 29, 23, 59, 59),
                          utc(2099, 12, 31, 23, 59, 59) };
  for (unsigned i = 0; i < sizeof(dates)/sizeof(dates[0]); ++i) {
    assert(wac_ds3231_write(dates[i]) == ESP_OK);
    assert(wac_ds3231_read(&value, &stopped) == ESP_OK && value == dates[i]);
    assert(!(fake_i2c_registers()[2] & 0x40)); /* Writes canonical 24-hour UTC. */
  }
  unsigned count = fake_i2c_event_count();
  assert(wac_ds3231_write(utc(2100, 1, 1, 0, 0, 0)) == ESP_ERR_INVALID_ARG);
  assert(wac_ds3231_write(utc(1999, 12, 31, 23, 59, 59)) == ESP_ERR_INVALID_ARG);
  assert(fake_i2c_event_count() == count);
}
static void *blocked_poll(void *unused) { (void)unused; synchronize(1); return NULL; }
static void responsiveness_checks(void)
{
  reset(); fake_i2c_registers()[0x0f] = 0x80; assert(wac_time_init("") == ESP_OK);
  fake_i2c_block_next_read(); pthread_t worker;
  assert(pthread_create(&worker, NULL, blocked_poll, NULL) == 0);
  assert(fake_i2c_wait_blocked());
  assert(snapshot().usable); /* Network time publishes before optional RTC I/O. */
  wac_time_dismiss_warning(snapshot().warning); assert(snapshot().warning == WAC_TIME_WARNING_NONE);
  fake_i2c_release_read(); assert(pthread_join(worker, NULL) == 0);
  assert(snapshot().warning == WAC_TIME_WARNING_NONE);
  no_storage_activity();
}
int main(void)
{
  timezone_checks(); startup_checks(); correction_checks(); correction_failure_checks();
  dismissal_race_checks(); calendar_checks(); responsiveness_checks();
  puts("Time trust, actual DS3231 driver with simulated I2C, correction, notice and timezone checks passed.");
  return 0;
}

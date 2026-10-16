#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "alarm.h"
#include "test_support.h"

static device_config_t config;
static wac_time_snapshot_t clock_snapshot;
static unsigned completion_calls;
static esp_err_t completion_error;
static wac_alarm_snapshot_t snapshot(void);

/* The settings owner has independent real-NVS tests. This narrow substitute
 * checks that the alarm stops before asking it to save the matching switch. */
esp_err_t device_config_complete_one_time(uint8_t hour, uint8_t minute)
{
  assert(snapshot().phase == WAC_ALARM_IDLE);
  ++completion_calls;
  if (!config.alarm_enabled || config.alarm_repeat_days
      || config.alarm_hour != hour || config.alarm_minute != minute) return ESP_OK;
  if (completion_error != ESP_OK) return completion_error;
  config.alarm_enabled = false;
  return ESP_OK;
}

static time_t at(int day, int hour, int minute, int second)
{
  struct tm utc = { .tm_year = 126, .tm_mon = 9, .tm_mday = day,
    .tm_hour = hour, .tm_min = minute, .tm_sec = second };
  return timegm(&utc);
}
static wac_alarm_snapshot_t snapshot(void)
{
  wac_alarm_snapshot_t result;
  wac_alarm_copy(&result);
  return result;
}
static esp_err_t update(int day, int hour, int minute, int second)
{
  clock_snapshot.utc_now = at(day, hour, minute, second);
  return wac_alarm_update(&config, &clock_snapshot);
}
static void reset(void)
{
  fake_nvs_reset();
  completion_calls = 0;
  completion_error = ESP_OK;
  config = (device_config_t){ .alarm_enabled = true, .alarm_hour = 7,
    .alarm_repeat_days = 0x7f, .alarm_snooze_minutes = 10 };
  clock_snapshot = (wac_time_snapshot_t){ .usable = true, .source = WAC_TIME_SOURCE_RTC };
  assert(wac_alarm_init() == ESP_OK);
  assert(fake_nvs_calls(FAKE_SET) == 0 && fake_nvs_key_count() == 0);
}
static void restart(void)
{
  fake_nvs_clear_activity();
  assert(wac_alarm_init() == ESP_OK);
  assert(fake_nvs_calls(FAKE_SET) == 0);
}
static void arm_once(int day, int hour, int minute, int second)
{
  reset();
  config.alarm_repeat_days = 0;
  assert(update(day, hour, minute, second) == ESP_OK);
}
static void one_time_checks(void)
{
  arm_once(11, 6, 0, 0);
  assert(snapshot().next.utc == at(11, 7, 0, 0));
  assert(fake_nvs_calls(FAKE_SET) == 1 && fake_nvs_key_count() == 1);
  unsigned writes = fake_nvs_calls(FAKE_SET);
  config.alarm_volume = 80;
  assert(update(11, 6, 30, 0) == ESP_OK);
  assert(fake_nvs_calls(FAKE_SET) == writes);
  restart();
  assert(update(11, 7, 30, 0) == ESP_OK && snapshot().phase == WAC_ALARM_RINGING);
  assert(snapshot().current.utc == at(11, 7, 0, 0));
  assert(wac_alarm_dismiss() == ESP_OK);
  assert(!config.alarm_enabled && completion_calls == 1);
  assert(snapshot().phase == WAC_ALARM_IDLE && !snapshot().next.valid);
  assert(config.alarm_repeat_days == 0 && config.alarm_hour == 7);
  restart();
  assert(update(12, 7, 0, 0) == ESP_OK && snapshot().phase == WAC_ALARM_IDLE);
  assert(!snapshot().next.valid);
  /* Observe Off before a new On; this explicitly arms a new dated occurrence. */
  config.alarm_enabled = true;
  assert(update(12, 7, 0, 1) == ESP_OK);
  assert(snapshot().next.utc == at(13, 7, 0, 0));

  arm_once(11, 7, 0, 1); /* First use after today's time chooses tomorrow. */
  assert(snapshot().next.utc == at(12, 7, 0, 0) && !snapshot().missed.valid);
  restart();
  assert(update(12, 0, 0, 0) == ESP_OK && snapshot().next.utc == at(12, 7, 0, 0));
  assert(update(12, 7, 0, 0) == ESP_OK && snapshot().phase == WAC_ALARM_RINGING);

  arm_once(11, 6, 0, 0);
  restart();
  clock_snapshot.usable = false;
  assert(update(13, 8, 0, 0) == ESP_OK && !snapshot().next.valid);
  clock_snapshot.usable = true;
  assert(update(13, 8, 0, 0) == ESP_OK);
  assert(snapshot().phase == WAC_ALARM_IDLE && snapshot().missed.utc == at(11, 7, 0, 0));
  assert(!config.alarm_enabled && !snapshot().next.valid);
  restart();
  assert(snapshot().missed.utc == at(11, 7, 0, 0));
  assert(wac_alarm_dismiss_missed() == ESP_OK && !snapshot().missed.valid);
  assert(!config.alarm_enabled);

  arm_once(11, 7, 0, 0); /* Equality is eligible on initial arming. */
  assert(snapshot().phase == WAC_ALARM_RINGING);
  assert(wac_alarm_snooze(&config, &clock_snapshot) == ESP_OK);
  assert(config.alarm_enabled && completion_calls == 0);
  restart();
  assert(update(11, 7, 9, 59) == ESP_OK && snapshot().phase == WAC_ALARM_SNOOZED);
  assert(update(11, 7, 40, 0) == ESP_OK && snapshot().phase == WAC_ALARM_RINGING);
  assert(snapshot().current.utc == at(11, 7, 0, 0));
  assert(wac_alarm_dismiss() == ESP_OK && !config.alarm_enabled);

  arm_once(11, 7, 0, 0);
  assert(wac_alarm_snooze(&config, &clock_snapshot) == ESP_OK);
  restart();
  assert(update(11, 7, 40, 1) == ESP_OK);
  assert(snapshot().phase == WAC_ALARM_IDLE && !snapshot().missed.valid);
  assert(!config.alarm_enabled && completion_calls == 1);

  arm_once(11, 7, 0, 0);
  assert(wac_alarm_snooze(&config, &clock_snapshot) == ESP_OK);
  restart();
  config.alarm_enabled = false;
  clock_snapshot.usable = false;
  assert(update(11, 7, 5, 0) == ESP_OK && snapshot().phase == WAC_ALARM_IDLE);
  restart();
  clock_snapshot.usable = true;
  assert(update(11, 7, 10, 0) == ESP_OK && snapshot().phase == WAC_ALARM_IDLE);
}
static void one_time_edit_checks(void)
{
  /* A replacement repeating schedule retains captured one-time snooze. */
  arm_once(11, 7, 0, 0);
  assert(wac_alarm_snooze(&config, &clock_snapshot) == ESP_OK);
  config.alarm_repeat_days = 0x7f;
  config.alarm_hour = 8;
  assert(update(11, 7, 1, 0) == ESP_OK);
  restart();
  assert(update(11, 7, 10, 0) == ESP_OK && snapshot().phase == WAC_ALARM_RINGING);
  assert(snapshot().current.utc == at(11, 7, 0, 0));
  assert(wac_alarm_dismiss() == ESP_OK && config.alarm_enabled && completion_calls == 0);
  assert(update(11, 8, 0, 0) == ESP_OK && snapshot().phase == WAC_ALARM_RINGING);

  /* Replacing a captured recurring alarm with None arms a distinct future date.
   * Dismissing the old ringing must leave that new schedule enabled. */
  reset();
  assert(update(11, 7, 0, 0) == ESP_OK);
  assert(wac_alarm_snooze(&config, &clock_snapshot) == ESP_OK);
  config.alarm_repeat_days = 0;
  config.alarm_hour = 8;
  assert(update(11, 7, 1, 0) == ESP_OK && snapshot().next.utc == at(11, 8, 0, 0));
  restart();
  assert(update(11, 7, 10, 0) == ESP_OK && snapshot().phase == WAC_ALARM_RINGING);
  assert(wac_alarm_dismiss() == ESP_OK && config.alarm_enabled && completion_calls == 0);
  assert(update(11, 8, 0, 0) == ESP_OK && snapshot().phase == WAC_ALARM_RINGING);
  assert(wac_alarm_dismiss() == ESP_OK && !config.alarm_enabled && completion_calls == 1);

  arm_once(11, 7, 0, 0);
  config.alarm_hour = 8;
  assert(update(11, 7, 1, 0) == ESP_OK);
  assert(wac_alarm_dismiss() == ESP_OK && config.alarm_enabled && completion_calls == 0);
  assert(snapshot().next.utc == at(11, 8, 0, 0));
  restart();
  assert(update(11, 8, 20, 0) == ESP_OK && snapshot().phase == WAC_ALARM_RINGING);
  assert(wac_alarm_dismiss() == ESP_OK && !config.alarm_enabled);
}
static void one_time_failure_checks(void)
{
  arm_once(11, 7, 0, 0);
  completion_error = ESP_FAIL;
  assert(wac_alarm_dismiss() == ESP_FAIL);
  assert(config.alarm_enabled && snapshot().phase == WAC_ALARM_IDLE);
  assert(!snapshot().next.valid && snapshot().storage_error == ESP_FAIL);
  unsigned writes = fake_nvs_calls(FAKE_SET);
  assert(update(12, 7, 0, 0) == ESP_OK && update(13, 7, 0, 0) == ESP_OK);
  assert(completion_calls == 1 && fake_nvs_calls(FAKE_SET) == writes);
  restart();
  assert(update(13, 7, 0, 0) == ESP_FAIL && snapshot().phase == WAC_ALARM_IDLE);
  assert(!snapshot().next.valid && completion_calls == 2);
  assert(update(14, 7, 0, 0) == ESP_OK && completion_calls == 2);
  restart();
  completion_error = ESP_OK;
  assert(update(14, 7, 0, 0) == ESP_OK && !config.alarm_enabled);

  /* Failed target storage preserves this run's chosen date without tick retries.
   * A restart can lose that date, the ordinary failed-save limitation. */
  for (fake_op_t fail = FAKE_OPEN; fail <= FAKE_COMMIT; ++fail) {
    if (fail == FAKE_READ) continue;
    reset(); config.alarm_repeat_days = 0;
    fake_nvs_clear_activity(); fake_nvs_fail(fail, 1);
    assert(update(11, 6, 0, 0) == ESP_FAIL);
    writes = fake_nvs_calls(FAKE_SET);
    unsigned opens = fake_nvs_calls(FAKE_OPEN);
    assert(update(11, 6, 1, 0) == ESP_OK && snapshot().next.utc == at(11, 7, 0, 0));
    assert(fake_nvs_calls(FAKE_SET) == writes && fake_nvs_calls(FAKE_OPEN) == opens);
    assert(update(11, 7, 0, 0) == ESP_OK && snapshot().phase == WAC_ALARM_RINGING);
  }
  reset(); config.alarm_repeat_days = 0;
  fake_nvs_clear_activity(); fake_nvs_fail(FAKE_SET, 1);
  assert(update(11, 7, 0, 0) == ESP_FAIL && snapshot().phase == WAC_ALARM_RINGING);
  fake_nvs_clear_activity();
  assert(wac_alarm_snooze(&config, &clock_snapshot) == ESP_OK);
  restart();
  assert(update(11, 7, 10, 0) == ESP_OK && snapshot().phase == WAC_ALARM_RINGING);
  assert(wac_alarm_dismiss() == ESP_OK && !config.alarm_enabled);
  arm_once(11, 7, 0, 0);
  assert(wac_alarm_snooze(&config, &clock_snapshot) == ESP_OK);
  fake_nvs_clear_activity(); fake_nvs_fail(FAKE_SET, 2); /* Target saved, old snooze survives. */
  completion_error = ESP_FAIL;
  assert(wac_alarm_dismiss() == ESP_FAIL);
  restart();
  assert(snapshot().phase == WAC_ALARM_SNOOZED);
  assert(update(11, 7, 20, 0) == ESP_FAIL && snapshot().phase == WAC_ALARM_IDLE);
  assert(!snapshot().next.valid);
}
static void one_time_calendar_checks(void)
{
  reset(); config.alarm_repeat_days = 0; config.alarm_hour = 0;
  assert(update(11, 23, 59, 59) == ESP_OK && snapshot().next.utc == at(12, 0, 0, 0));
  restart();
  assert(update(12, 0, 20, 0) == ESP_OK && snapshot().phase == WAC_ALARM_RINGING);
  assert(snapshot().current.local_date == 20261012);

  /* These are constructed UTC clock samples, not hardware/DST observations.
   * Expected UTC values follow the named post-transition offsets directly. */
  const struct { int month, day, utc_hour, utc_minute, hour, minute, date; } dst[] = {
    {3, 8, 6, 30, 2, 30, 20260308},
    {11, 1, 6, 30, 1, 30, 20261101},
  };
  for (size_t i = 0; i < sizeof(dst) / sizeof(dst[0]); ++i) {
    reset();
    assert(setenv("TZ", "EST5EDT,M3.2.0,M11.1.0", 1) == 0); tzset();
    config.alarm_repeat_days = 0;
    config.alarm_hour = dst[i].hour; config.alarm_minute = dst[i].minute;
    struct tm expected = {.tm_year = 126, .tm_mon = dst[i].month - 1,
      .tm_mday = dst[i].day, .tm_hour = dst[i].utc_hour, .tm_min = dst[i].utc_minute};
    time_t target = timegm(&expected);
    clock_snapshot.utc_now = target - 1;
    assert(wac_alarm_update(&config, &clock_snapshot) == ESP_OK);
    assert(snapshot().next.utc == target && snapshot().next.local_date == dst[i].date);
    restart();
    clock_snapshot.utc_now = target + 20 * 60;
    assert(wac_alarm_update(&config, &clock_snapshot) == ESP_OK);
    assert(snapshot().phase == WAC_ALARM_RINGING);
    assert(snapshot().current.utc == target && snapshot().current.local_date == dst[i].date);
    assert(wac_alarm_snooze(&config, &clock_snapshot) == ESP_OK);
    restart();
    clock_snapshot.utc_now += 10 * 60;
    assert(wac_alarm_update(&config, &clock_snapshot) == ESP_OK);
    assert(snapshot().phase == WAC_ALARM_RINGING && snapshot().current.utc == target);
    assert(wac_alarm_dismiss() == ESP_OK && !config.alarm_enabled);
  }
  assert(setenv("TZ", "UTC0", 1) == 0); tzset();
}
static void one_time_storage_checks(void)
{
  reset();
  nvs_handle_t handle;
  assert(nvs_open("wac_alarm", NVS_READWRITE, &handle) == ESP_OK);
  assert(nvs_set_u8(handle, "one_time", 1) == ESP_OK);
  nvs_close(handle);
  assert(wac_alarm_init() == ESP_ERR_NVS_TYPE_MISMATCH);
  struct { int64_t utc; int32_t date; uint16_t minute, reserved; } target = {
    .utc = at(11, 7, 0, 0), .date = 20261011, .minute = 420, .reserved = 2};
  assert(nvs_open("wac_alarm", NVS_READWRITE, &handle) == ESP_OK);
  assert(nvs_set_blob(handle, "one_time", &target, sizeof(target)) == ESP_OK);
  nvs_close(handle);
  assert(wac_alarm_init() == ESP_ERR_INVALID_RESPONSE);
  target.reserved = 0; target.minute = 1440;
  assert(nvs_open("wac_alarm", NVS_READWRITE, &handle) == ESP_OK);
  assert(nvs_set_blob(handle, "one_time", &target, sizeof(target)) == ESP_OK);
  nvs_close(handle);
  assert(wac_alarm_init() == ESP_ERR_INVALID_RESPONSE);
  /* The original operating record's exact layout stays intact alongside None. */
  arm_once(11, 7, 0, 0);
  assert(wac_alarm_snooze(&config, &clock_snapshot) == ESP_OK);
  unsigned char operating[56]; size_t size = sizeof(operating);
  assert(nvs_open("wac_alarm", NVS_READONLY, &handle) == ESP_OK);
  assert(nvs_get_blob(handle, "operating", operating, &size) == ESP_OK && size == 56);
  size = sizeof(target);
  assert(nvs_get_blob(handle, "one_time", &target, &size) == ESP_OK && size == 16);
  nvs_close(handle);
}
static void startup_checks(void)
{
  reset();
  assert(snapshot().phase == WAC_ALARM_IDLE && !snapshot().missed.valid);
  fake_nvs_fail(FAKE_OPEN, 2);
  assert(wac_alarm_init() == ESP_FAIL);
  assert(snapshot().storage_error == ESP_FAIL);
  fake_nvs_clear_activity();
  assert(update(11, 7, 0, 0) == ESP_OK);
  assert(snapshot().phase == WAC_ALARM_RINGING);
  assert(wac_alarm_dismiss() == ESP_OK);
  fake_nvs_clear_activity();
  fake_nvs_fail(FAKE_READ, 1);
  assert(wac_alarm_init() == ESP_FAIL);
  assert(snapshot().storage_error == ESP_FAIL && snapshot().phase == WAC_ALARM_IDLE);
  assert(fake_nvs_calls(FAKE_SET) == 0);
  /* Actual read/type errors are reported rather than silently called missing. */
  nvs_handle_t handle;
  fake_nvs_clear_activity();
  assert(nvs_open("wac_alarm", NVS_READWRITE, &handle) == ESP_OK);
  assert(nvs_set_u8(handle, "operating", 1) == ESP_OK);
  nvs_close(handle);
  assert(wac_alarm_init() == ESP_ERR_NVS_TYPE_MISMATCH);
  assert(snapshot().storage_error == ESP_ERR_NVS_TYPE_MISMATCH);
  assert(nvs_open("wac_alarm", NVS_READWRITE, &handle) == ESP_OK);
  unsigned char short_record = 0;
  assert(nvs_set_blob(handle, "operating", &short_record, 1) == ESP_OK);
  nvs_close(handle);
  assert(wac_alarm_init() == ESP_ERR_INVALID_RESPONSE);
}
static void ordinary_checks(void)
{
  reset();
  clock_snapshot.usable = false;
  assert(update(11, 7, 0, 0) == ESP_OK);
  assert(snapshot().phase == WAC_ALARM_IDLE && !snapshot().next.valid);
  assert(fake_nvs_calls(FAKE_SET) == 0);
  clock_snapshot.usable = true;
  assert(update(11, 7, 30, 0) == ESP_OK); /* Inclusive boundary. */
  assert(snapshot().phase == WAC_ALARM_RINGING);
  assert(snapshot().current.local_date == 20261011 && snapshot().current.minute == 420);
  assert(snapshot().next.utc == at(12, 7, 0, 0));
  assert(fake_nvs_calls(FAKE_SET) == 0); /* Ring state is RAM-only. */
  assert(update(11, 7, 31, 0) == ESP_OK);
  assert(snapshot().phase == WAC_ALARM_RINGING && snapshot().current.local_date == 20261011);
  assert(fake_nvs_calls(FAKE_SET) == 0); /* No automatic stop or tick saves. */
  assert(wac_alarm_dismiss() == ESP_OK);
  unsigned writes = fake_nvs_calls(FAKE_SET);
  assert(update(11, 6, 0, 0) == ESP_OK);
  assert(snapshot().next.utc == at(12, 7, 0, 0));
  assert(update(11, 7, 0, 0) == ESP_OK);
  assert(update(11, 7, 20, 0) == ESP_OK);
  assert(snapshot().phase == WAC_ALARM_IDLE && fake_nvs_calls(FAKE_SET) == writes);
  restart();
  assert(update(11, 7, 20, 0) == ESP_OK && snapshot().phase == WAC_ALARM_IDLE);
  assert(update(12, 7, 4, 0) == ESP_OK && snapshot().phase == WAC_ALARM_RINGING);
  assert(snapshot().current.local_date == 20261012);
  /* A correction across an earlier day cannot replay an older handled alarm. */
  assert(wac_alarm_dismiss() == ESP_OK);
  assert(update(10, 7, 5, 0) == ESP_OK && snapshot().phase == WAC_ALARM_IDLE);
  assert(update(12, 7, 4, 0) == ESP_OK && snapshot().phase == WAC_ALARM_IDLE);
  for (unsigned i = 0; i < fake_nvs_key_count(); ++i)
    assert(!strcmp(fake_nvs_namespace(i), "wac_alarm"));
  assert(fake_nvs_key_count() == 1);
}
static void missed_checks(void)
{
  reset();
  assert(update(11, 7, 30, 1) == ESP_OK);
  assert(snapshot().phase == WAC_ALARM_IDLE && snapshot().missed.valid);
  assert(snapshot().missed.utc == at(11, 7, 0, 0));
  unsigned writes = fake_nvs_calls(FAKE_SET);
  assert(update(11, 8, 0, 0) == ESP_OK && fake_nvs_calls(FAKE_SET) == writes);
  restart();
  assert(snapshot().missed.local_date == 20261011);
  assert(update(12, 8, 0, 0) == ESP_OK);
  assert(snapshot().missed.local_date == 20261011); /* One pending notice survives. */
  assert(wac_alarm_dismiss_missed() == ESP_OK);
  assert(config.alarm_enabled && config.alarm_hour == 7 && config.alarm_repeat_days == 0x7f);
  restart();
  assert(!snapshot().missed.valid);
  assert(update(12, 8, 0, 0) == ESP_OK && !snapshot().missed.valid);
  assert(update(13, 7, 30, 1) == ESP_OK && snapshot().missed.valid);
  assert(update(14, 7, 0, 0) == ESP_OK);
  assert(snapshot().phase == WAC_ALARM_RINGING && !snapshot().missed.valid);
  restart();
  assert(!snapshot().missed.valid);
}
static void snooze_checks(void)
{
  reset();
  assert(update(11, 7, 0, 0) == ESP_OK);
  assert(wac_alarm_snooze(&config, &clock_snapshot) == ESP_OK);
  assert(snapshot().phase == WAC_ALARM_SNOOZED && snapshot().snooze_deadline == at(11, 7, 10, 0));
  restart();
  clock_snapshot.usable = false;
  assert(update(11, 8, 0, 0) == ESP_OK);
  assert(snapshot().phase == WAC_ALARM_SNOOZED && fake_nvs_calls(FAKE_SET) == 0);
  clock_snapshot.usable = true;
  assert(update(11, 7, 9, 59) == ESP_OK && snapshot().phase == WAC_ALARM_SNOOZED);
  assert(update(11, 7, 10, 0) == ESP_OK && snapshot().phase == WAC_ALARM_RINGING);
  config.alarm_snooze_minutes = 5;
  assert(wac_alarm_snooze(&config, &clock_snapshot) == ESP_OK);
  assert(snapshot().snooze_deadline == at(11, 7, 15, 0));
  restart();
  assert(update(11, 7, 45, 0) == ESP_OK && snapshot().phase == WAC_ALARM_RINGING);
  restart();
  assert(update(11, 7, 45, 0) == ESP_OK && snapshot().phase == WAC_ALARM_RINGING);
  assert(wac_alarm_dismiss() == ESP_OK);
  restart();
  assert(update(11, 7, 15, 0) == ESP_OK && snapshot().phase == WAC_ALARM_IDLE);

  reset();
  assert(update(11, 7, 0, 0) == ESP_OK);
  assert(wac_alarm_snooze(&config, &clock_snapshot) == ESP_OK);
  restart();
  assert(update(11, 7, 40, 1) == ESP_OK);
  assert(snapshot().phase == WAC_ALARM_IDLE && snapshot().snooze_deadline == 0);
  assert(!snapshot().missed.valid); /* An expired snooze is discarded. */
  restart();
  assert(update(11, 7, 20, 0) == ESP_OK && snapshot().phase == WAC_ALARM_IDLE);
}
static void changed_schedule_checks(void)
{
  reset();
  assert(update(11, 7, 0, 0) == ESP_OK);
  assert(wac_alarm_snooze(&config, &clock_snapshot) == ESP_OK);
  config.alarm_hour = 8;
  assert(update(11, 7, 5, 0) == ESP_OK);
  assert(snapshot().current.minute == 420 && snapshot().next.minute == 480);
  assert(snapshot().snooze_deadline == at(11, 7, 10, 0));
  assert(update(11, 7, 10, 0) == ESP_OK && snapshot().phase == WAC_ALARM_RINGING);
  assert(wac_alarm_dismiss() == ESP_OK);
  assert(update(11, 8, 0, 0) == ESP_OK && snapshot().phase == WAC_ALARM_RINGING);
  assert(snapshot().current.minute == 480);
}
static void observe_future_sunday(void)
{
  reset();
  config.alarm_hour = 7;
  config.alarm_minute = 30;
  config.alarm_repeat_days = 1; /* Sunday; October 11 is Sunday. */
  config.alarm_enabled = false;
  assert(update(11, 6, 0, 0) == ESP_OK);
  config.alarm_enabled = true;
  assert(update(11, 6, 0, 0) == ESP_OK);
  assert(snapshot().phase == WAC_ALARM_IDLE && !snapshot().missed.valid);
  fake_nvs_clear_activity();
}
static void schedule_edit_arming_checks(void)
{
  /* Constructed edits exercise each calendar input independently, and combined. */
  for (int edit = 0; edit < 4; ++edit) {
    for (int past = 0; past < 2; ++past) {
      observe_future_sunday();
      int hour = 7, minute = 20;
      time_t next;
      if (edit == 0) {
        config.alarm_hour = past ? 6 : 8; /* Hour only. */
        hour = past ? 6 : 7;
        minute = past ? 40 : 20;
        next = at(past ? 18 : 11, config.alarm_hour, 30, 0);
      } else if (edit == 1) {
        config.alarm_minute = past ? 10 : 40; /* Minute only. */
        next = at(past ? 18 : 11, 7, config.alarm_minute, 0);
      } else if (edit == 2) {
        config.alarm_repeat_days = past ? 0x7f : 0x7e; /* Days only. */
        minute = past ? 40 : 20;
        next = at(12, 7, 30, 0);
      } else {
        config.alarm_hour = past ? 7 : 18;
        config.alarm_minute = past ? 10 : 40;
        config.alarm_repeat_days = 0x7f;
        next = at(past ? 12 : 11, config.alarm_hour, config.alarm_minute, 0);
      }
      assert(update(11, hour, minute, 0) == ESP_OK);
      assert(snapshot().phase == WAC_ALARM_IDLE && !snapshot().missed.valid);
      assert(snapshot().next.utc == next);
      assert(update(11, hour, minute, 1) == ESP_OK);
      assert(snapshot().phase == WAC_ALARM_IDLE && !snapshot().missed.valid);
      assert(fake_nvs_calls(FAKE_OPEN) == 1 && fake_nvs_calls(FAKE_SET) == 1);
      assert(fake_nvs_calls(FAKE_COMMIT) == 1);
      restart();
      assert(update(11, hour, minute, 1) == ESP_OK);
      assert(snapshot().phase == WAC_ALARM_IDLE && !snapshot().missed.valid);
      restart();
      clock_snapshot.utc_now = at(11, hour, minute, 0) + (past ? 31 * 60 : 1);
      assert(wac_alarm_update(&config, &clock_snapshot) == ESP_OK);
      assert(snapshot().phase == WAC_ALARM_IDLE && !snapshot().missed.valid);
      /* The skipped occurrence does not suppress the next real due time. */
      restart();
      clock_snapshot.utc_now = next + 5 * 60;
      assert(wac_alarm_update(&config, &clock_snapshot) == ESP_OK);
      assert(snapshot().phase == WAC_ALARM_RINGING && snapshot().current.utc == next);
    }
  }

  /* Reproduce the schedule/time combination from the reported board incident,
   * using an in-memory native clock rather than claiming a new hardware test. */
  reset();
  config.alarm_repeat_days = 0x3e; /* Monday-Friday at 07:00. */
  config.alarm_enabled = false;
  assert(update(11, 8, 0, 0) == ESP_OK);
  config.alarm_enabled = true;
  assert(update(11, 8, 0, 0) == ESP_OK);
  fake_nvs_clear_activity();
  config.alarm_hour = 18;
  config.alarm_minute = 40;
  config.alarm_repeat_days = 0x7f;
  assert(update(11, 8, 1, 0) == ESP_OK && snapshot().phase == WAC_ALARM_IDLE);
  assert(!snapshot().missed.valid && snapshot().next.utc == at(11, 18, 40, 0));
  assert(fake_nvs_calls(FAKE_OPEN) == 1 && fake_nvs_calls(FAKE_SET) == 1);

  observe_future_sunday();
  config.alarm_minute = 10;
  assert(update(11, 7, 9, 59) == ESP_OK && snapshot().phase == WAC_ALARM_IDLE);
  assert(snapshot().next.utc == at(11, 7, 10, 0));
  assert(update(11, 7, 10, 0) == ESP_OK && snapshot().phase == WAC_ALARM_RINGING);
  observe_future_sunday();
  config.alarm_minute = 10;
  assert(update(11, 7, 10, 0) == ESP_OK && snapshot().phase == WAC_ALARM_RINGING);
  assert(snapshot().current.utc == at(11, 7, 10, 0)); /* Edit exactly due. */

  observe_future_sunday();
  clock_snapshot.usable = false;
  config.alarm_minute = 10;
  assert(update(11, 7, 5, 0) == ESP_OK && !snapshot().next.valid);
  assert(update(11, 7, 20, 0) == ESP_OK && !snapshot().next.valid);
  clock_snapshot.usable = true;
  assert(update(11, 7, 20, 0) == ESP_OK);
  assert(snapshot().phase == WAC_ALARM_IDLE && !snapshot().missed.valid);
  assert(snapshot().next.utc == at(18, 7, 10, 0));
  assert(fake_nvs_calls(FAKE_OPEN) == 1 && fake_nvs_calls(FAKE_SET) == 1);
  restart();
  assert(update(11, 7, 20, 0) == ESP_OK && snapshot().phase == WAC_ALARM_IDLE);
  assert(!snapshot().missed.valid);
  restart();
  assert(update(11, 7, 40, 1) == ESP_OK && snapshot().phase == WAC_ALARM_IDLE);
  assert(!snapshot().missed.valid);

  reset();
  assert(update(11, 7, 0, 0) == ESP_OK);
  wac_alarm_occurrence_t captured = snapshot().current;
  config.alarm_hour = 8;
  assert(update(11, 7, 1, 0) == ESP_OK && snapshot().phase == WAC_ALARM_RINGING);
  assert(snapshot().current.utc == captured.utc && snapshot().current.minute == captured.minute);
  assert(fake_nvs_calls(FAKE_SET) == 0);
  assert(wac_alarm_snooze(&config, &clock_snapshot) == ESP_OK);
  fake_nvs_clear_activity();
  config.alarm_hour = 6;
  config.alarm_minute = 55;
  config.alarm_repeat_days = 1;
  assert(update(11, 7, 2, 0) == ESP_OK && snapshot().phase == WAC_ALARM_SNOOZED);
  assert(snapshot().current.utc == captured.utc && snapshot().current.minute == captured.minute);
  assert(snapshot().snooze_deadline == at(11, 7, 11, 0));
  assert(fake_nvs_calls(FAKE_OPEN) == 0 && fake_nvs_calls(FAKE_SET) == 0);
  assert(update(11, 7, 11, 0) == ESP_OK && snapshot().phase == WAC_ALARM_RINGING);
  assert(snapshot().current.utc == captured.utc);

  reset();
  assert(update(11, 8, 0, 0) == ESP_OK);
  wac_alarm_occurrence_t missed = snapshot().missed;
  fake_nvs_clear_activity();
  config.alarm_hour = 18;
  config.alarm_minute = 40;
  assert(update(11, 8, 1, 0) == ESP_OK && snapshot().phase == WAC_ALARM_IDLE);
  assert(snapshot().missed.utc == missed.utc && snapshot().missed.minute == missed.minute);
  assert(fake_nvs_calls(FAKE_OPEN) == 1 && fake_nvs_calls(FAKE_SET) == 1);
  restart();
  assert(snapshot().missed.utc == missed.utc && snapshot().missed.minute == missed.minute);
  assert(update(11, 8, 2, 0) == ESP_OK && snapshot().phase == WAC_ALARM_IDLE);
  assert(snapshot().missed.utc == missed.utc);

  /* Identical updates and sound/volume/snooze-only changes must leave a due
   * occurrence eligible for ordinary catch-up, rather than silently skip it. */
  for (int edit = 0; edit < 4; ++edit) {
    observe_future_sunday();
    if (edit == 1) config.alarm_sound = DEVICE_CONFIG_ALARM_CLASSIC_BELL;
    if (edit == 2) config.alarm_volume = 80;
    if (edit == 3) config.alarm_snooze_minutes = 5;
    assert(update(11, 7, 40, 0) == ESP_OK && snapshot().phase == WAC_ALARM_RINGING);
    assert(snapshot().current.utc == at(11, 7, 30, 0));
    assert(fake_nvs_calls(FAKE_SET) == 0);
  }
}
static void active_edit_restart_checks(void)
{
  reset();
  assert(update(11, 7, 0, 0) == ESP_OK && snapshot().phase == WAC_ALARM_RINGING);
  /* Both the captured occurrence and newer past times must stay RAM-only while
   * ringing. Returning the schedule to 07:00 must preserve its restart recovery. */
  config.alarm_repeat_days = 1;
  assert(update(11, 7, 1, 0) == ESP_OK);
  config.alarm_minute = 1;
  assert(update(11, 7, 2, 0) == ESP_OK);
  config.alarm_minute = 2;
  assert(update(11, 7, 3, 0) == ESP_OK);
  config.alarm_minute = 0;
  assert(update(11, 7, 4, 0) == ESP_OK);
  assert(snapshot().phase == WAC_ALARM_RINGING && snapshot().current.utc == at(11, 7, 0, 0));
  assert(fake_nvs_calls(FAKE_SET) == 0 && fake_nvs_calls(FAKE_OPEN) == 1); /* Init read only. */
  restart();
  assert(update(11, 7, 5, 0) == ESP_OK && snapshot().phase == WAC_ALARM_RINGING);
  assert(snapshot().current.utc == at(11, 7, 0, 0));

  for (int overdue = 0; overdue < 2; ++overdue) {
    reset();
    assert(update(11, 7, 0, 0) == ESP_OK);
    assert(wac_alarm_snooze(&config, &clock_snapshot) == ESP_OK);
    restart();
    /* Editing a restored snooze before usable time must preserve its saved
     * occurrence and deadline when time arrives. */
    clock_snapshot.usable = false;
    config.alarm_minute = 1;
    assert(update(11, 7, 2, 0) == ESP_OK && snapshot().phase == WAC_ALARM_SNOOZED);
    assert(fake_nvs_calls(FAKE_SET) == 0);
    clock_snapshot.usable = true;
    assert(update(11, 7, 2, 0) == ESP_OK && snapshot().phase == WAC_ALARM_SNOOZED);
    config.alarm_minute = 2;
    assert(update(11, 7, 3, 0) == ESP_OK && snapshot().phase == WAC_ALARM_SNOOZED);
    config.alarm_minute = 0;
    assert(update(11, 7, 4, 0) == ESP_OK && snapshot().phase == WAC_ALARM_SNOOZED);
    assert(snapshot().current.utc == at(11, 7, 0, 0));
    assert(snapshot().snooze_deadline == at(11, 7, 10, 0));
    assert(fake_nvs_calls(FAKE_SET) == 0);
    restart();
    assert(snapshot().phase == WAC_ALARM_SNOOZED);
    assert(snapshot().current.utc == at(11, 7, 0, 0));
    assert(snapshot().snooze_deadline == at(11, 7, 10, 0));
    assert(update(11, 7, overdue ? 15 : 5, 0) == ESP_OK);
    assert(snapshot().phase == (overdue ? WAC_ALARM_RINGING : WAC_ALARM_SNOOZED));
    if (!overdue) assert(update(11, 7, 10, 0) == ESP_OK && snapshot().phase == WAC_ALARM_RINGING);
    assert(snapshot().current.utc == at(11, 7, 0, 0));
    restart();
    assert(update(11, 7, 15, 0) == ESP_OK && snapshot().phase == WAC_ALARM_RINGING);
    assert(snapshot().current.utc == at(11, 7, 0, 0));
  }
}
static void arming_failure_checks(void)
{
  for (int edit = 0; edit < 2; ++edit) {
    for (fake_op_t failure = FAKE_OPEN; failure <= FAKE_COMMIT; ++failure) {
      if (failure == FAKE_READ) continue;
      if (edit) {
        observe_future_sunday();
        config.alarm_minute = 10;
      } else {
        reset();
        config.alarm_enabled = false;
        assert(update(11, 7, 20, 0) == ESP_OK);
        config.alarm_enabled = true;
      }
      fake_nvs_clear_activity();
      fake_nvs_fail(failure, 1);
      assert(update(11, 7, 20, 0) == ESP_FAIL);
      assert(snapshot().phase == WAC_ALARM_IDLE && !snapshot().missed.valid);
      assert(snapshot().storage_error == ESP_FAIL);
      unsigned opens = fake_nvs_calls(FAKE_OPEN), writes = fake_nvs_calls(FAKE_SET);
      assert(update(11, 7, 21, 0) == ESP_OK);
      assert(update(11, 8, 0, 0) == ESP_OK);
      assert(snapshot().phase == WAC_ALARM_IDLE && !snapshot().missed.valid);
      assert(snapshot().storage_error == ESP_FAIL);
      assert(fake_nvs_calls(FAKE_OPEN) == opens && fake_nvs_calls(FAKE_SET) == writes);
      restart();
      assert(update(11, 7, 21, 0) == ESP_OK);
      if (failure == FAKE_COMMIT) {
        /* This substitute exposes the set before a failed commit. */
        assert(snapshot().phase == WAC_ALARM_IDLE && !snapshot().missed.valid);
      } else {
        assert(snapshot().phase == WAC_ALARM_RINGING);
        assert(snapshot().current.utc == at(11, 7, edit ? 10 : 0, 0));
        restart();
        assert(update(11, 8, 0, 0) == ESP_OK && snapshot().phase == WAC_ALARM_IDLE);
        assert(snapshot().missed.utc == at(11, 7, edit ? 10 : 0, 0));
      }
    }
  }
}
static void failure_checks(void)
{
  reset();
  assert(update(11, 7, 0, 0) == ESP_OK);
  fake_nvs_clear_activity();
  fake_nvs_fail(FAKE_SET, 1);
  assert(wac_alarm_snooze(&config, &clock_snapshot) == ESP_FAIL);
  assert(snapshot().phase == WAC_ALARM_SNOOZED && snapshot().storage_error == ESP_FAIL);
  assert(update(11, 7, 5, 0) == ESP_OK && fake_nvs_calls(FAKE_SET) == 1);
  restart();
  assert(update(11, 7, 5, 0) == ESP_OK && snapshot().phase == WAC_ALARM_RINGING);
  fake_nvs_clear_activity();
  fake_nvs_fail(FAKE_OPEN, 1);
  assert(wac_alarm_dismiss() == ESP_FAIL);
  assert(snapshot().phase == WAC_ALARM_IDLE && snapshot().storage_error == ESP_FAIL);
  assert(update(11, 7, 5, 0) == ESP_OK && fake_nvs_calls(FAKE_OPEN) == 1);
  restart();
  assert(update(11, 7, 5, 0) == ESP_OK && snapshot().phase == WAC_ALARM_RINGING);
  assert(wac_alarm_snooze(&config, &clock_snapshot) == ESP_OK);
  fake_nvs_clear_activity();
  fake_nvs_fail(FAKE_SET, 1);
  assert(wac_alarm_dismiss() == ESP_FAIL && snapshot().phase == WAC_ALARM_IDLE);
  restart();
  assert(snapshot().phase == WAC_ALARM_SNOOZED); /* Older saved snooze may return. */
  assert(wac_alarm_dismiss() == ESP_OK);

  reset();
  assert(update(11, 8, 0, 0) == ESP_OK);
  fake_nvs_clear_activity();
  fake_nvs_fail(FAKE_SET, 1);
  assert(wac_alarm_dismiss_missed() == ESP_FAIL && !snapshot().missed.valid);
  assert(update(11, 8, 1, 0) == ESP_OK && fake_nvs_calls(FAKE_SET) == 1);
  restart();
  assert(snapshot().missed.valid);
  /* This substitute exposes a set even when the following commit fails.
   * Ordinary NVS errors do not promise restoration of the old record. */
  fake_nvs_clear_activity();
  fake_nvs_fail(FAKE_COMMIT, 1);
  assert(wac_alarm_dismiss_missed() == ESP_FAIL && !snapshot().missed.valid);
  restart();
  assert(!snapshot().missed.valid);

  reset();
  fake_nvs_clear_activity();
  fake_nvs_fail(FAKE_SET, 1);
  assert(update(11, 8, 0, 0) == ESP_FAIL && snapshot().missed.valid);
  assert(update(11, 8, 1, 0) == ESP_OK && fake_nvs_calls(FAKE_SET) == 1);
}
static void off_checks(void)
{
  reset();
  assert(update(11, 7, 0, 0) == ESP_OK && snapshot().phase == WAC_ALARM_RINGING);
  config.alarm_enabled = false;
  device_config_t expected = config;
  assert(update(11, 7, 1, 0) == ESP_OK);
  assert(snapshot().phase == WAC_ALARM_IDLE && !snapshot().current.valid);
  assert(!snapshot().next.valid && snapshot().snooze_deadline == 0);
  assert(memcmp(&config, &expected, sizeof(config)) == 0);
  unsigned writes = fake_nvs_calls(FAKE_SET);
  assert(writes == 1);
  assert(update(11, 7, 2, 0) == ESP_OK && fake_nvs_calls(FAKE_SET) == writes);
  restart();
  assert(snapshot().phase == WAC_ALARM_IDLE);
  config.alarm_enabled = true;
  assert(update(11, 7, 3, 0) == ESP_OK && snapshot().phase == WAC_ALARM_IDLE);
  assert(snapshot().next.utc == at(12, 7, 0, 0));

  reset();
  assert(update(11, 7, 0, 0) == ESP_OK);
  assert(wac_alarm_snooze(&config, &clock_snapshot) == ESP_OK);
  config.alarm_enabled = false;
  expected = config;
  assert(update(11, 7, 5, 0) == ESP_OK);
  assert(snapshot().phase == WAC_ALARM_IDLE && !snapshot().current.valid);
  assert(!snapshot().next.valid && snapshot().snooze_deadline == 0);
  assert(memcmp(&config, &expected, sizeof(config)) == 0);
  restart();
  assert(snapshot().phase == WAC_ALARM_IDLE && snapshot().snooze_deadline == 0);
  config.alarm_enabled = true;
  assert(update(11, 7, 10, 0) == ESP_OK && snapshot().phase == WAC_ALARM_IDLE);

  reset();
  assert(update(11, 7, 0, 0) == ESP_OK);
  assert(wac_alarm_snooze(&config, &clock_snapshot) == ESP_OK);
  restart();
  assert(snapshot().phase == WAC_ALARM_SNOOZED);
  config.alarm_enabled = false;
  clock_snapshot.usable = false;
  assert(update(11, 7, 5, 0) == ESP_OK);
  assert(snapshot().phase == WAC_ALARM_IDLE && !snapshot().current.valid);
  assert(!snapshot().next.valid && snapshot().snooze_deadline == 0);
  assert(fake_nvs_calls(FAKE_SET) == 1);
  assert(update(11, 7, 6, 0) == ESP_OK && fake_nvs_calls(FAKE_SET) == 1);
  restart();
  assert(snapshot().phase == WAC_ALARM_IDLE);

  reset();
  assert(update(11, 8, 0, 0) == ESP_OK && snapshot().missed.valid);
  config.alarm_enabled = false;
  expected = config;
  fake_nvs_clear_activity();
  assert(update(11, 8, 1, 0) == ESP_OK);
  assert(snapshot().missed.local_date == 20261011 && !snapshot().next.valid);
  assert(memcmp(&config, &expected, sizeof(config)) == 0);
  assert(fake_nvs_calls(FAKE_SET) == 0);
  restart();
  assert(snapshot().missed.local_date == 20261011);

  reset();
  assert(update(11, 7, 0, 0) == ESP_OK);
  assert(wac_alarm_snooze(&config, &clock_snapshot) == ESP_OK);
  config.alarm_enabled = false;
  fake_nvs_clear_activity();
  fake_nvs_fail(FAKE_SET, 1);
  assert(update(11, 7, 5, 0) == ESP_FAIL);
  assert(snapshot().phase == WAC_ALARM_IDLE && !snapshot().current.valid);
  assert(!snapshot().next.valid && snapshot().snooze_deadline == 0);
  assert(snapshot().storage_error == ESP_FAIL);
  assert(update(11, 7, 6, 0) == ESP_OK && fake_nvs_calls(FAKE_SET) == 1);
  assert(snapshot().storage_error == ESP_FAIL);
  restart();
  assert(snapshot().phase == WAC_ALARM_SNOOZED); /* Failed save may restore old snooze. */
  clock_snapshot.usable = false;
  assert(update(11, 7, 6, 0) == ESP_OK && snapshot().phase == WAC_ALARM_IDLE);
  restart();
  assert(snapshot().phase == WAC_ALARM_IDLE);
}
static void enable_checks(void)
{
  reset();
  config.alarm_enabled = false;
  assert(update(11, 21, 0, 0) == ESP_OK);
  fake_nvs_clear_activity();
  config.alarm_enabled = true;
  device_config_t expected = config;
  assert(update(11, 21, 0, 0) == ESP_OK);
  assert(snapshot().phase == WAC_ALARM_IDLE && !snapshot().missed.valid);
  assert(snapshot().next.utc == at(12, 7, 0, 0));
  assert(memcmp(&config, &expected, sizeof(config)) == 0);
  assert(update(11, 21, 1, 0) == ESP_OK);
  assert(update(11, 21, 2, 0) == ESP_OK);
  assert(snapshot().phase == WAC_ALARM_IDLE && !snapshot().missed.valid);
  assert(fake_nvs_calls(FAKE_OPEN) == 1 && fake_nvs_calls(FAKE_SET) == 1);
  assert(fake_nvs_calls(FAKE_COMMIT) == 1);
  assert(fake_nvs_key_count() == 1 && !strcmp(fake_nvs_key(0), "operating"));
  restart();
  assert(update(11, 21, 2, 0) == ESP_OK);
  assert(snapshot().phase == WAC_ALARM_IDLE && !snapshot().missed.valid);
  assert(update(11, 6, 59, 0) == ESP_OK); /* A backward correction cannot replay the skipped alarm. */
  assert(snapshot().next.utc == at(12, 7, 0, 0));
  assert(update(11, 7, 0, 0) == ESP_OK && snapshot().phase == WAC_ALARM_IDLE);
  assert(update(12, 7, 0, 0) == ESP_OK && snapshot().phase == WAC_ALARM_RINGING);
  assert(snapshot().current.local_date == 20261012 && !snapshot().missed.valid);
  assert(fake_nvs_calls(FAKE_SET) == 0);

  reset();
  config.alarm_enabled = false;
  assert(update(11, 7, 10, 0) == ESP_OK);
  config.alarm_enabled = true;
  fake_nvs_clear_activity();
  assert(update(11, 7, 10, 0) == ESP_OK);
  assert(update(11, 7, 11, 0) == ESP_OK);
  assert(snapshot().phase == WAC_ALARM_IDLE && !snapshot().missed.valid);
  assert(snapshot().next.utc == at(12, 7, 0, 0));
  assert(fake_nvs_calls(FAKE_OPEN) == 1 && fake_nvs_calls(FAKE_SET) == 1);
  restart();
  assert(update(11, 7, 11, 0) == ESP_OK && snapshot().phase == WAC_ALARM_IDLE);
  assert(!snapshot().missed.valid);
  restart();
  assert(update(11, 7, 31, 0) == ESP_OK && snapshot().phase == WAC_ALARM_IDLE);
  assert(!snapshot().missed.valid);
  assert(update(12, 7, 5, 0) == ESP_OK && snapshot().phase == WAC_ALARM_RINGING);
  assert(snapshot().current.utc == at(12, 7, 0, 0));

  reset();
  config.alarm_enabled = false;
  assert(update(11, 6, 0, 0) == ESP_OK);
  config.alarm_enabled = true;
  fake_nvs_clear_activity();
  assert(update(11, 6, 0, 0) == ESP_OK);
  assert(snapshot().next.utc == at(11, 7, 0, 0) && !snapshot().missed.valid);
  assert(update(11, 6, 59, 59) == ESP_OK && snapshot().phase == WAC_ALARM_IDLE);
  assert(update(11, 7, 0, 0) == ESP_OK && snapshot().phase == WAC_ALARM_RINGING);
  assert(snapshot().current.local_date == 20261011 && fake_nvs_calls(FAKE_SET) == 1);

  reset();
  config.alarm_enabled = false;
  assert(update(11, 6, 59, 59) == ESP_OK);
  config.alarm_enabled = true;
  assert(update(11, 7, 0, 0) == ESP_OK && snapshot().phase == WAC_ALARM_RINGING);
  assert(snapshot().current.utc == at(11, 7, 0, 0)); /* Exact equality remains due. */
  assert(fake_nvs_calls(FAKE_SET) == 0);
  restart();
  assert(update(11, 7, 5, 0) == ESP_OK && snapshot().phase == WAC_ALARM_RINGING);
  assert(snapshot().current.utc == at(11, 7, 0, 0));

  reset();
  clock_snapshot.usable = false;
  config.alarm_enabled = false;
  assert(update(11, 8, 0, 0) == ESP_OK);
  config.alarm_enabled = true;
  fake_nvs_clear_activity();
  assert(update(11, 8, 1, 0) == ESP_OK);
  assert(snapshot().phase == WAC_ALARM_IDLE && !snapshot().next.valid);
  clock_snapshot.usable = true;
  assert(update(11, 8, 2, 0) == ESP_OK);
  assert(snapshot().phase == WAC_ALARM_IDLE && !snapshot().missed.valid);
  assert(snapshot().next.utc == at(12, 7, 0, 0));
  assert(fake_nvs_calls(FAKE_OPEN) == 1 && fake_nvs_calls(FAKE_SET) == 1);

  reset();
  assert(update(10, 8, 0, 0) == ESP_OK && snapshot().missed.local_date == 20261010);
  config.alarm_enabled = false;
  assert(update(11, 8, 0, 0) == ESP_OK);
  config.alarm_enabled = true;
  fake_nvs_clear_activity();
  assert(update(11, 8, 1, 0) == ESP_OK);
  assert(snapshot().phase == WAC_ALARM_IDLE && snapshot().missed.local_date == 20261010);
  assert(snapshot().next.utc == at(12, 7, 0, 0));
  assert(fake_nvs_calls(FAKE_OPEN) == 1 && fake_nvs_calls(FAKE_SET) == 1);
  restart();
  assert(snapshot().missed.local_date == 20261010);
  assert(update(11, 8, 2, 0) == ESP_OK && snapshot().phase == WAC_ALARM_IDLE);
  assert(snapshot().missed.local_date == 20261010);

  reset();
  clock_snapshot.usable = false;
  assert(update(11, 7, 10, 0) == ESP_OK && snapshot().phase == WAC_ALARM_IDLE);
  clock_snapshot.usable = true;
  assert(update(11, 7, 10, 0) == ESP_OK && snapshot().phase == WAC_ALARM_RINGING);
  assert(snapshot().current.local_date == 20261011); /* Already-On startup is recovery, not runtime enable. */
}
static void *blocked_snooze(void *unused)
{
  (void)unused;
  assert(wac_alarm_snooze(&config, &clock_snapshot) == ESP_OK);
  return NULL;
}
static void *blocked_off(void *unused)
{
  (void)unused;
  assert(update(11, 7, 5, 0) == ESP_OK);
  return NULL;
}
static void reader_checks(void)
{
  reset();
  assert(update(11, 7, 0, 0) == ESP_OK);
  fake_nvs_block_next_set();
  pthread_t writer;
  assert(pthread_create(&writer, NULL, blocked_snooze, NULL) == 0);
  assert(fake_nvs_wait_blocked());
  wac_alarm_snapshot_t state = snapshot();
  assert(state.phase == WAC_ALARM_SNOOZED && state.snooze_deadline == at(11, 7, 10, 0));
  fake_nvs_release_set();
  assert(pthread_join(writer, NULL) == 0);
  config.alarm_enabled = false;
  fake_nvs_block_next_set();
  assert(pthread_create(&writer, NULL, blocked_off, NULL) == 0);
  assert(fake_nvs_wait_blocked());
  state = snapshot();
  assert(state.phase == WAC_ALARM_IDLE && !state.current.valid && !state.next.valid);
  assert(state.snooze_deadline == 0);
  fake_nvs_release_set();
  assert(pthread_join(writer, NULL) == 0);
}
#ifdef WAC_ALARM_COMMAND_TEST
#include "app_ui.h"
#include "alarm_commands.inc"

/* Only the task boundary is substituted. The payload size and two-item
 * capacity match the production queue; all alarm work uses the real owner. */
struct fake_queue {
  clock_command_t entries[2];
  unsigned count;
};
static struct fake_queue command_queue;
BaseType_t xQueueSend(QueueHandle_t queue, const void *payload, TickType_t wait)
{
  assert(queue == &command_queue && wait == 0);
  if (queue->count == 2) return pdFALSE;
  memcpy(&queue->entries[queue->count++], payload, sizeof(clock_command_t));
  return pdTRUE;
}
BaseType_t xQueueReceive(QueueHandle_t queue, void *payload, TickType_t wait)
{
  assert(queue == &command_queue && wait == 0);
  if (!queue->count) return pdFALSE;
  memcpy(payload, &queue->entries[0], sizeof(clock_command_t));
  --queue->count;
  memmove(&queue->entries[0], &queue->entries[1], queue->count * sizeof(clock_command_t));
  return pdTRUE;
}
static clock_command_t receive_command(void)
{
  clock_command_t command;
  assert(xQueueReceive(s_clock_commands, &command, 0) == pdTRUE);
  return command;
}
static void command_submission_checks(void)
{
  reset();
  assert(update(11, 7, 0, 0) == ESP_OK);
  wac_alarm_occurrence_t displayed = snapshot().current;
  wac_alarm_occurrence_t original = displayed;
  s_clock_commands = NULL;
  assert(request_alarm_action(APP_ALARM_DISMISS, &displayed) == ESP_ERR_INVALID_STATE);
  assert(request_clock_warning_dismiss(WAC_TIME_WARNING_CONNECTION) == ESP_ERR_INVALID_STATE);
  s_clock_commands = &command_queue;
  command_queue.count = 0;
  assert(request_alarm_action((app_alarm_action_t)-1, &displayed) == ESP_ERR_INVALID_ARG);
  assert(request_alarm_action(APP_ALARM_DISMISS, NULL) == ESP_ERR_INVALID_ARG);
  wac_alarm_occurrence_t invalid = {0};
  assert(request_alarm_action(APP_ALARM_DISMISS, &invalid) == ESP_ERR_INVALID_ARG);
  assert(request_clock_warning_dismiss(WAC_TIME_WARNING_NONE) == ESP_ERR_INVALID_ARG);
  fake_nvs_clear_activity();
  assert(request_alarm_action(APP_ALARM_SNOOZE, &displayed) == ESP_OK);
  displayed.utc = 0; /* The screen can change immediately after submission. */
  assert(request_clock_warning_dismiss(WAC_TIME_WARNING_LOST_TIME) == ESP_OK);
  assert(request_alarm_action(APP_ALARM_DISMISS, &original) == ESP_ERR_TIMEOUT);
  assert(request_clock_warning_dismiss(WAC_TIME_WARNING_CONNECTION) == ESP_ERR_TIMEOUT);
  assert(snapshot().phase == WAC_ALARM_RINGING);
  for (unsigned op = 0; op < FAKE_OP_COUNT; ++op) assert(fake_nvs_calls(op) == 0);
  clock_command_t command = receive_command();
  assert(command.kind == CLOCK_COMMAND_ALARM && command.action == APP_ALARM_SNOOZE);
  assert(alarm_occurrence_matches(&command.occurrence, &original));
  command = receive_command();
  assert(command.kind == CLOCK_COMMAND_WARNING && command.warning == WAC_TIME_WARNING_LOST_TIME);
  assert(request_clock_warning_dismiss(WAC_TIME_WARNING_CONNECTION) == ESP_OK);
  command = receive_command();
  assert(command.kind == CLOCK_COMMAND_WARNING && command.warning == WAC_TIME_WARNING_CONNECTION);
  assert(xQueueReceive(s_clock_commands, &command, 0) == pdFALSE);
}
static void execute_and_publish(app_alarm_action_t action,
  wac_alarm_occurrence_t occurrence, esp_err_t expected)
{
  assert(request_alarm_action(action, &occurrence) == ESP_OK);
  clock_command_t command = receive_command();
  clock_alarm_result_t result = { .ready = true, .action = command.action,
    .occurrence = command.occurrence,
    .error = execute_alarm_action(&command, &config, &clock_snapshot) };
  assert(result.error == expected);
  publish_alarm_result(&result);
  /* No consumer call means a busy display leaves the result available. */
  assert(s_alarm_result.ready);
  assert(request_clock_warning_dismiss(WAC_TIME_WARNING_CONNECTION) == ESP_OK);
  clock_command_t warning = receive_command();
  assert(warning.kind == CLOCK_COMMAND_WARNING);
  assert(s_alarm_result.ready); /* An unrelated queued warning cannot consume it. */
}
static void take_completion(app_alarm_action_t action,
  wac_alarm_occurrence_t occurrence, esp_err_t expected)
{
  clock_alarm_result_t delivered;
  assert(take_alarm_result(&delivered));
  assert(delivered.ready && delivered.action == action && delivered.error == expected);
  assert(alarm_occurrence_matches(&delivered.occurrence, &occurrence));
  assert(!take_alarm_result(&delivered));
}
static void command_execution_checks(void)
{
  reset();
  assert(update(11, 7, 0, 0) == ESP_OK);
  wac_alarm_occurrence_t occurrence = snapshot().current;
  execute_and_publish(APP_ALARM_SNOOZE, occurrence, ESP_OK);
  take_completion(APP_ALARM_SNOOZE, occurrence, ESP_OK);
  wac_alarm_snapshot_t live = snapshot();
  assert(live.phase == WAC_ALARM_SNOOZED);
  assert(live.snooze_deadline == at(11, 7, 10, 0));
  assert(update(11, 7, 10, 0) == ESP_OK);
  fake_nvs_clear_activity();
  fake_nvs_fail(FAKE_SET, 1);
  execute_and_publish(APP_ALARM_DISMISS, occurrence, ESP_FAIL);
  take_completion(APP_ALARM_DISMISS, occurrence, ESP_FAIL);
  live = snapshot();
  assert(live.phase == WAC_ALARM_IDLE && live.storage_error == ESP_FAIL);
  assert(config.alarm_enabled && config.alarm_hour == 7 && config.alarm_repeat_days == 0x7f);

  fake_nvs_clear_activity();
  assert(update(12, 7, 0, 0) == ESP_OK);
  fake_nvs_clear_activity();
  execute_and_publish(APP_ALARM_DISMISS, occurrence, ESP_ERR_INVALID_STATE);
  take_completion(APP_ALARM_DISMISS, occurrence, ESP_ERR_INVALID_STATE);
  live = snapshot();
  assert(live.phase == WAC_ALARM_RINGING && live.current.local_date == 20261012);
  for (unsigned op = 0; op < FAKE_OP_COUNT; ++op) assert(fake_nvs_calls(op) == 0);
  clock_command_t command = { .action = APP_ALARM_SNOOZE, .occurrence = live.current };
  assert(execute_alarm_action(&command, NULL, &clock_snapshot) == ESP_ERR_INVALID_STATE);
  assert(execute_alarm_action(&command, &config, NULL) == ESP_ERR_INVALID_STATE);

  reset();
  assert(update(11, 8, 0, 0) == ESP_OK && snapshot().missed.valid);
  occurrence = snapshot().missed;
  execute_and_publish(APP_ALARM_DISMISS_MISSED, occurrence, ESP_OK);
  take_completion(APP_ALARM_DISMISS_MISSED, occurrence, ESP_OK);
  live = snapshot();
  assert(!live.missed.valid && live.phase == WAC_ALARM_IDLE);
  assert(config.alarm_enabled && config.alarm_hour == 7 && config.alarm_repeat_days == 0x7f);
}
static void delayed_completion_checks(void)
{
  reset();
  assert(update(11, 7, 0, 0) == ESP_OK);
  wac_alarm_occurrence_t occurrence = snapshot().current;
  execute_and_publish(APP_ALARM_SNOOZE, occurrence, ESP_OK);
  /* A busy display may not deliver completion until the same alarm resumes. */
  assert(update(11, 7, 10, 0) == ESP_OK);
  take_completion(APP_ALARM_SNOOZE, occurrence, ESP_OK);
  wac_alarm_snapshot_t live = snapshot();
  assert(live.phase == WAC_ALARM_RINGING);
  assert(alarm_occurrence_matches(&live.current, &occurrence));

  execute_and_publish(APP_ALARM_DISMISS, occurrence, ESP_OK);
  assert(update(12, 7, 0, 0) == ESP_OK);
  take_completion(APP_ALARM_DISMISS, occurrence, ESP_OK);
  live = snapshot();
  assert(live.phase == WAC_ALARM_RINGING && live.current.local_date == 20261012);
  assert(!alarm_occurrence_matches(&live.current, &occurrence));
}

#endif

int main(void)
{
  assert(setenv("TZ", "UTC0", 1) == 0); tzset();
  one_time_checks(); one_time_edit_checks(); one_time_failure_checks();
  one_time_calendar_checks(); one_time_storage_checks();
  startup_checks(); ordinary_checks(); missed_checks(); snooze_checks();
  changed_schedule_checks(); schedule_edit_arming_checks();
  active_edit_restart_checks(); arming_failure_checks();
  failure_checks(); off_checks(); enable_checks(); reader_checks();
#ifdef WAC_ALARM_COMMAND_TEST
  command_submission_checks(); command_execution_checks(); delayed_completion_checks();
  puts("alarm command and completion checks passed");
#else
  puts("alarm operating and recovery checks passed");
#endif
  return 0;
}

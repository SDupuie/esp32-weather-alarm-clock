#include "test_support.h"
#include "device_config.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

typedef esp_err_t (*save_fn)(const device_config_t *, const device_config_t *);

static device_config_t current(void)
{
  device_config_t value;
  assert(device_config_copy(&value) == ESP_OK);
  return value;
}

static device_config_t fresh(void)
{
  fake_nvs_reset();
  assert(device_config_init() == ESP_OK);
  return current();
}

static void assert_same(const device_config_t *a, const device_config_t *b)
{
  assert(memcmp(a, b, sizeof(*a)) == 0);
}

static device_config_t resolved(device_config_t value)
{
  strcpy(value.location, "Cincinnati, Ohio, US");
  strcpy(value.latitude, "39.1031");
  strcpy(value.longitude, "-84.5120");
  strcpy(value.timezone, "America/New_York");
  return value;
}

static device_config_t device_edit(device_config_t value)
{
  strcpy(value.device_id, "bedroom-clock");
  strcpy(value.room_name, "Bedroom");
  strcpy(value.wifi_ssid, "Home network");
  strcpy(value.wifi_password, "example-password");
  value.night_shift_mode = DEVICE_CONFIG_NIGHT_SOLAR;
  strcpy(value.night_shift_start, "21:30");
  strcpy(value.night_shift_end, "07:30");
  value.night_sunset_offset_minutes = -30;
  value.night_sunrise_offset_minutes = 45;
  return value;
}

static device_config_t display_edit(device_config_t value)
{
  strcpy(value.default_clock_face, "analog");
  strcpy(value.time_format, "24");
  value.leading_zero_12h = true;
  value.day_brightness = 90;
  value.night_brightness = 40;
  return value;
}

static device_config_t alarm_edit(device_config_t value)
{
  value.alarm_enabled = true;
  value.alarm_hour = 6;
  value.alarm_minute = 30;
  value.alarm_repeat_days = 0x7f;
  value.alarm_sound = DEVICE_CONFIG_ALARM_CLASSIC_BELL;
  value.alarm_volume = 75;
  value.alarm_snooze_minutes = 15;
  return value;
}

static void test_defaults_and_read_only_copies(void)
{
  device_config_t value = fresh();
  assert(!strcmp(value.room_name, "Clock"));
  assert(!strcmp(value.device_id, "clock-esp32-p4"));
  assert(!strcmp(value.default_clock_face, "digital"));
  assert(!strcmp(value.time_format, "12"));
  assert(!strcmp(value.units, "imperial"));
  assert(!value.leading_zero_12h && !value.wifi_ready && !value.location_ready);
  assert(!value.wifi_ssid[0] && !value.wifi_password[0]);
  assert(!value.location[0] && !value.latitude[0] && !value.longitude[0] && !value.timezone[0]);
  assert(!value.alarm_enabled && value.alarm_hour == 7 && value.alarm_minute == 0);
  assert(value.alarm_repeat_days == 0x3e && value.alarm_volume == 50);
  assert(value.alarm_sound == DEVICE_CONFIG_ALARM_CLASSIC_BEEP && value.alarm_snooze_minutes == 10);
  assert(value.night_shift_mode == DEVICE_CONFIG_NIGHT_OFF);
  assert(!strcmp(value.night_shift_start, "22:00") && !strcmp(value.night_shift_end, "06:00"));
  assert(value.night_sunset_offset_minutes == 0 && value.night_sunrise_offset_minutes == 0);
  assert(value.day_brightness == 80 && value.night_brightness == 30);
  assert(fake_nvs_calls(FAKE_SET) == fake_nvs_key_count());
  assert(fake_nvs_calls(FAKE_COMMIT) == 1);
  for (unsigned i = 0; i < fake_nvs_key_count(); ++i) {
    assert(!strstr(fake_nvs_key(i), "boot"));
    assert(!strstr(fake_nvs_key(i), "setup"));
  }
  fake_nvs_clear_activity();
  assert(device_config_init() == ESP_OK);
  device_config_t restarted = current();
  assert_same(&value, &restarted);
  assert(fake_nvs_calls(FAKE_SET) == 0 && fake_nvs_calls(FAKE_COMMIT) == 0);
  fake_nvs_clear_activity();
  device_config_t draft = value;
  strcpy(draft.room_name, "Draft never submitted");
  for (int i = 0; i < 100; ++i) restarted = current();
  assert_same(&value, &restarted);
  assert(fake_nvs_calls(FAKE_OPEN) == 0 && fake_nvs_calls(FAKE_SET) == 0);
  assert(device_config_copy(NULL) == ESP_ERR_INVALID_ARG);
}

static void test_validation(void)
{
  device_config_t base = fresh(), bad;
#define INVALID(change) do { bad = base; change; assert(device_config_validate(&bad) != ESP_OK); } while (0)
  INVALID(memset(bad.room_name, 'X', sizeof(bad.room_name)));
  INVALID(bad.room_name[0] = 0);
  INVALID(strcpy(bad.default_clock_face, "calendar"));
  INVALID(strcpy(bad.time_format, "13"));
  INVALID(strcpy(bad.units, "kelvin"));
  INVALID(strcpy(bad.message_sharing, "unknown"));
  INVALID(strcpy(bad.message_sharing, "shared"));
  INVALID(bad.alarm_hour = 24);
  INVALID(bad.alarm_minute = 60);
  INVALID(bad.alarm_repeat_days = 0x80);
  INVALID(bad.alarm_repeat_days = 0);
  INVALID(bad.alarm_sound = (device_config_alarm_sound_t)3);
  INVALID(bad.alarm_volume = 101);
  INVALID(bad.alarm_snooze_minutes = 7);
  INVALID(bad.night_shift_mode = (device_config_night_mode_t)4);
  INVALID(strcpy(bad.night_shift_start, "24:00"));
  INVALID(strcpy(bad.night_shift_end, "6:00"));
  INVALID(bad.night_sunset_offset_minutes = 125);
  INVALID(bad.night_sunrise_offset_minutes = 7);
  INVALID(bad.day_brightness = 19);
  INVALID(bad.night_brightness = 19);
  INVALID(bad.day_brightness = 101);
  INVALID(bad.night_brightness = 101);
  INVALID(strcpy(bad.location, "Cincinnati"));
  base = resolved(base);
  assert(device_config_validate(&base) == ESP_OK);
  INVALID(strcpy(bad.timezone, "auto"));
  INVALID(strcpy(bad.timezone, "America/Unknown"));
  INVALID(bad.timezone[0] = 0);
  INVALID(strcpy(bad.latitude, "nan"));
  INVALID(strcpy(bad.longitude, "inf"));
  INVALID(strcpy(bad.latitude, "91"));
  INVALID(strcpy(bad.longitude, "-181"));
  INVALID(strcpy(bad.latitude, "39.1degrees"));
  INVALID(bad.longitude[0] = 0);
#undef INVALID
  bad = base;
  strcpy(bad.device_id, "bedroom-clock");
  strcpy(bad.message_sharing, "shared");
  strcpy(bad.latitude, "0");
  strcpy(bad.longitude, "0");
  strcpy(bad.timezone, "Etc/UTC");
  bad.day_brightness = 20;
  bad.night_brightness = 100;
  bad.night_sunset_offset_minutes = -120;
  bad.night_sunrise_offset_minutes = 120;
  strcpy(bad.night_shift_end, bad.night_shift_start);
  assert(device_config_validate(&bad) == ESP_OK);
  bad.day_brightness = 100;
  bad.night_brightness = 20;
  assert(device_config_validate(&bad) == ESP_OK);
  assert(device_config_validate(NULL) == ESP_ERR_INVALID_ARG);
  device_config_t empty = fresh();
  fake_nvs_clear_activity();
  assert(device_config_save_weather(&empty, &empty) != ESP_OK);
  assert(fake_nvs_calls(FAKE_SET) == 0);
}

static void test_editor_merges_and_readiness(void)
{
  device_config_t base = fresh();
  device_config_t alarm = alarm_edit(base);
  device_config_t device = device_edit(base);
  device_config_t weather = resolved(base);
  strcpy(weather.units, "metric");
  assert(device_config_save_alarm(&base, &alarm) == ESP_OK);
  assert(device_config_save_weather(&base, &weather) == ESP_OK);
  assert(device_config_save_device(&base, &device) == ESP_OK);
  device_config_t applied = current();
  assert(applied.alarm_hour == 6 && applied.alarm_snooze_minutes == 15);
  assert(!strcmp(applied.location, weather.location) && !strcmp(applied.units, "metric"));
  assert(!strcmp(applied.room_name, "Bedroom") && applied.wifi_ready && applied.location_ready);

  /* An older Device editor changes only its room name, after Display
   * has saved brightness. Its unchanged brightness must stay current. */
  base = applied;
  device = base;
  device.day_brightness = 70;
  assert(device_config_save_display(&base, &device) == ESP_OK);
  device = base;
  strcpy(device.room_name, "Guest room");
  device.alarm_hour = 23; /* Not part of this editor. */
  strcpy(device.units, "imperial"); /* Weather editor owns this. */
  assert(device_config_save_device(&base, &device) == ESP_OK);
  applied = current();
  assert(applied.day_brightness == 70 && applied.alarm_hour == 6);
  assert(!strcmp(applied.units, "metric") && !strcmp(applied.room_name, "Guest room"));
  fake_nvs_clear_activity();
  assert(device_config_save_device(&base, &base) == ESP_OK);
  assert(fake_nvs_calls(FAKE_SET) == 10 && fake_nvs_calls(FAKE_COMMIT) == 1);
  assert(device_config_set_alarm_enabled(false) == ESP_OK);
  applied = current();
  assert(!applied.alarm_enabled && applied.alarm_hour == 6 && applied.day_brightness == 70);
  fake_nvs_clear_activity();
  assert(device_config_init() == ESP_OK);
  device_config_t restarted = current();
  assert_same(&applied, &restarted);
  assert(fake_nvs_calls(FAKE_SET) == 0);
  assert(device_config_save_device(NULL, &base) == ESP_ERR_INVALID_ARG);
  assert(device_config_save_alarm(&base, NULL) == ESP_ERR_INVALID_ARG);
}

static void test_display_scope_and_latest_choices(void)
{
  device_config_t base = fresh(), device = device_edit(base);
  device_config_t display = display_edit(base), alarm = alarm_edit(base);
  device_config_t weather = resolved(base);
  assert(device_config_save_device(&base, &device) == ESP_OK);
  assert(device_config_save_alarm(&base, &alarm) == ESP_OK);
  assert(device_config_save_weather(&base, &weather) == ESP_OK);
  device_config_t before = current();
  /* A real Display editor can coexist with incomplete Device edits. These
   * deliberately invalid pending choices must remain unsubmitted. */
  display.room_name[0] = display.device_id[0] = 0;
  strcpy(display.message_sharing, "unknown");
  strcpy(display.wifi_ssid, "Unsubmitted network");
  strcpy(display.wifi_password, "Unsubmitted password");
  display.night_shift_mode = (device_config_night_mode_t) 9;
  display.alarm_hour = 24;
  strcpy(display.timezone, "auto");
  assert(device_config_validate(&display) == ESP_ERR_INVALID_ARG);
  fake_nvs_clear_activity();
  assert(device_config_save_display(&base, &display) == ESP_OK);
  assert(fake_nvs_calls(FAKE_SET) == 5 && fake_nvs_calls(FAKE_COMMIT) == 1);
  device_config_t applied = current(), expected = display_edit(before);
  assert_same(&expected, &applied);
  assert(applied.wifi_ready && applied.location_ready);
  /* Unchanged fields in an older Display draft preserve newer Display choices. */
  base = applied;
  display = base;
  display.day_brightness = 65;
  assert(device_config_save_display(&base, &display) == ESP_OK);
  display = base;
  strcpy(display.time_format, "12");
  assert(device_config_save_display(&base, &display) == ESP_OK);
  applied = current();
  assert(applied.day_brightness == 65 && !strcmp(applied.time_format, "12"));
  /* Device Save cannot apply even valid Display changes from its shared draft. */
  device = display_edit(applied);
  device.day_brightness = 100;
  strcpy(device.room_name, "Office");
  assert(device_config_save_device(&applied, &device) == ESP_OK);
  expected = applied;
  strcpy(expected.room_name, "Office");
  applied = current();
  assert_same(&expected, &applied);
  assert(device_config_save_display(NULL, &display) == ESP_ERR_INVALID_ARG);
  assert(device_config_save_display(&base, NULL) == ESP_ERR_INVALID_ARG);
}

static void test_display_invalid_choices_and_retry(void)
{
  device_config_t base = fresh(), draft;
  fake_nvs_clear_activity();
#define REJECT_DISPLAY(change) do { \
  draft = base; change; \
  assert(device_config_save_display(&base, &draft) == ESP_ERR_INVALID_ARG); \
} while (0)
  REJECT_DISPLAY(strcpy(draft.default_clock_face, "calendar"));
  REJECT_DISPLAY(strcpy(draft.time_format, "13"));
  REJECT_DISPLAY(draft.day_brightness = 19);
  REJECT_DISPLAY(draft.day_brightness = 101);
  REJECT_DISPLAY(draft.night_brightness = 19);
  REJECT_DISPLAY(draft.night_brightness = 101);
#undef REJECT_DISPLAY
  device_config_t applied = current();
  assert_same(&base, &applied);
  assert(fake_nvs_calls(FAKE_SET) == 0 && fake_nvs_calls(FAKE_COMMIT) == 0);
  /* Every ordinary returned storage failure keeps the running settings.
   * Retrying the retained draft rewrites all five keys and repairs partial
   * storage; the fake store permits individual writes before commit. */
  const fake_op_t failures[] = {FAKE_OPEN, FAKE_SET, FAKE_COMMIT};
  for (unsigned kind = 0; kind < 3; ++kind) {
    unsigned attempts = failures[kind] == FAKE_SET ? 5 : 1;
    for (unsigned failure = 1; failure <= attempts; ++failure) {
      base = fresh();
      draft = display_edit(base);
      fake_nvs_clear_activity();
      fake_nvs_fail(failures[kind], failure);
      assert(device_config_save_display(&base, &draft) == ESP_FAIL);
      applied = current();
      assert_same(&base, &applied);
      /* An unrelated immediate toggle after failure must survive retry. */
      fake_nvs_clear_activity();
      assert(device_config_set_alarm_enabled(true) == ESP_OK);
      fake_nvs_clear_activity();
      assert(device_config_save_display(&base, &draft) == ESP_OK);
      assert(fake_nvs_calls(FAKE_SET) == 5 && fake_nvs_calls(FAKE_COMMIT) == 1);
      applied = current();
      device_config_t expected = display_edit(base);
      expected.alarm_enabled = true;
      assert_same(&expected, &applied);
      fake_nvs_clear_activity();
      assert(device_config_init() == ESP_OK);
      applied = current();
      assert_same(&expected, &applied);
      assert(fake_nvs_calls(FAKE_SET) == 0 && fake_nvs_calls(FAKE_COMMIT) == 0);
    }
  }
}

static void test_rejected_saves_keep_applied_choices(void)
{
  device_config_t base = fresh(), draft = base;
  fake_nvs_clear_activity();
  draft.alarm_hour = 24;
  assert(device_config_save_alarm(&base, &draft) == ESP_ERR_INVALID_ARG);
  draft = base;
  strcpy(draft.message_sharing, "shared");
  assert(device_config_save_device(&base, &draft) == ESP_ERR_INVALID_ARG);
  draft = resolved(base);
  strcpy(draft.timezone, "auto");
  assert(device_config_save_weather(&base, &draft) == ESP_ERR_INVALID_ARG);
  draft = resolved(base);
  strcpy(draft.latitude, "nan");
  assert(device_config_save_weather(&base, &draft) == ESP_ERR_INVALID_ARG);
  device_config_t applied = current();
  assert_same(&base, &applied);
  assert(fake_nvs_calls(FAKE_SET) == 0 && fake_nvs_calls(FAKE_COMMIT) == 0);
}

static void test_save_failures(save_fn save, device_config_t (*edit)(device_config_t))
{
  device_config_t base = fresh(), draft = edit(base);
  fake_nvs_clear_activity();
  assert(save(&base, &draft) == ESP_OK);
  unsigned writes = fake_nvs_calls(FAKE_SET);
  assert(writes > 0);
  for (unsigned failure = 1; failure <= writes; ++failure) {
    base = fresh();
    draft = edit(base);
    fake_nvs_clear_activity();
    fake_nvs_fail(FAKE_SET, failure);
    assert(save(&base, &draft) == ESP_FAIL);
    device_config_t applied = current();
    assert_same(&base, &applied);
    assert(fake_nvs_calls(FAKE_SET) == failure);
    assert(fake_nvs_calls(FAKE_COMMIT) == 0);
    fake_nvs_clear_activity();
    assert(device_config_init() == ESP_OK);
    applied = current();
    if (save != device_config_save_weather) assert(device_config_validate(&applied) == ESP_OK);
    else assert(!applied.location_ready);
    /* Single-mode Device saves first rewrite the unchanged sharing mode.
     * Other editors first write a field changed by these test drafts. */
    unsigned first_changed_write = save == device_config_save_device ? 2 : 1;
    if (failure > first_changed_write) assert(memcmp(&applied, &base, sizeof(base)) != 0);
    else assert_same(&applied, &base);
    assert(fake_nvs_calls(FAKE_SET) == 0 && fake_nvs_calls(FAKE_COMMIT) == 0);
    if (save == device_config_save_weather) {
      /* The incomplete location must not block an unrelated alarm or Device
       * save while first-use setup is waiting for a corrected location. */
      draft = applied;
      draft.day_brightness = 75;
      assert(device_config_save_display(&applied, &draft) == ESP_OK);
      device_config_t with_device = current();
      assert(!strcmp(with_device.location, applied.location));
      assert(!strcmp(with_device.latitude, applied.latitude));
      assert(!strcmp(with_device.longitude, applied.longitude));
      assert(!strcmp(with_device.timezone, applied.timezone));
      draft = with_device;
      draft.alarm_enabled = true;
      assert(device_config_save_alarm(&with_device, &draft) == ESP_OK);
      applied = current();
      assert(applied.alarm_enabled && applied.day_brightness == 75);
      assert(!applied.location_ready);
      /* First setup can finish by saving the resolved location again, using
       * the actual partial values now loaded as the editor's baseline. */
      draft = resolved(applied);
      assert(save(&applied, &draft) == ESP_OK);
      device_config_t repaired = current();
      assert(repaired.location_ready && device_config_validate(&repaired) == ESP_OK);
    }
  }
  fake_op_t failures[] = {FAKE_OPEN, FAKE_COMMIT};
  for (unsigned i = 0; i < sizeof(failures) / sizeof(failures[0]); ++i) {
    fake_op_t failure = failures[i];
    base = fresh();
    draft = edit(base);
    fake_nvs_clear_activity();
    fake_nvs_fail(failure, 1);
    assert(save(&base, &draft) == ESP_FAIL);
    device_config_t applied = current();
    assert_same(&base, &applied);
    fake_nvs_clear_activity();
    assert(device_config_init() == ESP_OK);
    applied = current();
    if (failure == FAKE_COMMIT) assert(memcmp(&applied, &base, sizeof(base)) != 0);
    else assert_same(&base, &applied);
  }
}

static void test_startup_failures_and_missing_keys(void)
{
  device_config_t before = fresh();
  fake_nvs_clear_activity();
  fake_nvs_fail(FAKE_OPEN, 1);
  assert(device_config_init() == ESP_FAIL);
  device_config_t unchanged = current();
  assert_same(&before, &unchanged);
  assert(fake_nvs_calls(FAKE_READ) == 0 && fake_nvs_calls(FAKE_SET) == 0);
  fake_nvs_clear_activity();
  assert(device_config_init() == ESP_OK);
  unsigned reads = fake_nvs_calls(FAKE_READ), seeds = fake_nvs_key_count();
  for (unsigned failure = 1; failure <= reads; ++failure) {
    fake_nvs_clear_activity();
    fake_nvs_fail(FAKE_READ, failure);
    assert(device_config_init() == ESP_FAIL);
    device_config_t after = current();
    assert_same(&before, &after);
    assert(fake_nvs_calls(FAKE_SET) == 0);
  }
  for (unsigned failure = 1; failure <= seeds; ++failure) {
    fake_nvs_reset();
    fake_nvs_fail(FAKE_SET, failure);
    assert(device_config_init() == ESP_FAIL);
    device_config_t after = current();
    assert_same(&before, &after);
    fake_nvs_clear_activity();
    assert(device_config_init() == ESP_OK);
    assert(fake_nvs_calls(FAKE_SET) == seeds - failure + 1);
  }
  fake_nvs_reset();
  fake_nvs_fail(FAKE_COMMIT, 1);
  assert(device_config_init() == ESP_FAIL);
  unchanged = current();
  assert_same(&before, &unchanged);
  fake_nvs_clear_activity();
  assert(device_config_init() == ESP_OK);
  assert(fake_nvs_calls(FAKE_SET) == 0);
  fake_nvs_remove("room_name");
  fake_nvs_clear_activity();
  assert(device_config_init() == ESP_OK);
  assert(fake_nvs_calls(FAKE_SET) == 1 && fake_nvs_calls(FAKE_COMMIT) == 1);
  fake_nvs_put_u8("room_name", 2);
  fake_nvs_clear_activity();
  assert(device_config_init() == ESP_ERR_NVS_TYPE_MISMATCH);
  assert(fake_nvs_calls(FAKE_SET) == 0);
  device_config_t after = current();
  assert_same(&before, &after);
  fake_nvs_put_str("room_name", "");
  fake_nvs_clear_activity();
  assert(device_config_init() != ESP_OK);
  after = current();
  assert_same(&before, &after);
  assert(fake_nvs_calls(FAKE_SET) == 0);
  fake_nvs_put_str("room_name", "Bedroom");
  fake_nvs_remove("night_bright");
  fake_nvs_clear_activity();
  assert(device_config_init() == ESP_OK);
  after = current();
  assert(!strcmp(after.room_name, "Bedroom") && after.night_brightness == 30);
  assert(fake_nvs_calls(FAKE_SET) == 1);
  before = after;
  fake_nvs_put_u8("alarm_enabled", 2);
  fake_nvs_clear_activity();
  assert(device_config_init() == ESP_ERR_INVALID_ARG);
  after = current();
  assert_same(&before, &after);
  assert(fake_nvs_calls(FAKE_SET) == 0);
}

static void test_loaded_brightness_minimum(void)
{
  device_config_t base = fresh(), draft = device_edit(base);
  assert(device_config_save_device(&base, &draft) == ESP_OK);
  base = current();
  draft = resolved(base);
  assert(device_config_save_weather(&base, &draft) == ESP_OK);
  base = current();
  draft = alarm_edit(base);
  assert(device_config_save_alarm(&base, &draft) == ESP_OK);
  base = current();
  unsigned keys = fake_nvs_key_count();
  const uint8_t day[] = {0, 15, 19, 20, 100};
  const uint8_t night[] = {5, 10, 19, 100, 20};
  unsigned reads = 0;
  for (unsigned i = 0; i < sizeof(day) / sizeof(day[0]); ++i) {
    /* Controlled stored values exercise both fields below the new limit
     * and values at the valid endpoints. */
    fake_nvs_put_u8("day_brightness", day[i]);
    fake_nvs_put_u8("night_bright", night[i]);
    fake_nvs_clear_activity();
    assert(device_config_init() == ESP_OK);
    reads = fake_nvs_calls(FAKE_READ);
    device_config_t expected = base;
    expected.day_brightness = day[i] < 20 ? 20 : day[i];
    expected.night_brightness = night[i] < 20 ? 20 : night[i];
    device_config_t applied = current();
    assert_same(&expected, &applied);
    assert(fake_nvs_key_count() == keys);
    assert(fake_nvs_calls(FAKE_SET) == 0 && fake_nvs_calls(FAKE_COMMIT) == 0);
    nvs_handle_t handle;
    uint8_t stored;
    assert(nvs_open("wac_cfg", NVS_READONLY, &handle) == ESP_OK);
    assert(nvs_get_u8(handle, "day_brightness", &stored) == ESP_OK && stored == day[i]);
    assert(nvs_get_u8(handle, "night_bright", &stored) == ESP_OK && stored == night[i]);
    nvs_close(handle);
  }
  device_config_t before = current();
  fake_nvs_put_u8("day_brightness", 15);
  fake_nvs_put_u8("night_bright", 10);
  for (unsigned failure = 1; failure <= reads; ++failure) {
    fake_nvs_clear_activity();
    fake_nvs_fail(FAKE_READ, failure);
    assert(device_config_init() == ESP_FAIL);
    device_config_t applied = current();
    assert_same(&before, &applied);
    assert(fake_nvs_calls(FAKE_SET) == 0 && fake_nvs_calls(FAKE_COMMIT) == 0);
  }
  for (unsigned field = 0; field < 2; ++field) {
    fake_nvs_put_u8("day_brightness", field == 0 ? 101 : 15);
    fake_nvs_put_u8("night_bright", field == 1 ? 101 : 10);
    fake_nvs_clear_activity();
    assert(device_config_init() == ESP_ERR_INVALID_ARG);
    device_config_t applied = current();
    assert_same(&before, &applied);
    assert(fake_nvs_calls(FAKE_SET) == 0 && fake_nvs_calls(FAKE_COMMIT) == 0);
  }
}

static void test_brightness_save_limits(void)
{
  device_config_t base = fresh(), draft = base;
  fake_nvs_clear_activity();
  draft.day_brightness = 19;
  assert(device_config_save_display(&base, &draft) == ESP_ERR_INVALID_ARG);
  draft = base;
  draft.night_brightness = 19;
  assert(device_config_save_display(&base, &draft) == ESP_ERR_INVALID_ARG);
  device_config_t applied = current();
  assert_same(&base, &applied);
  assert(fake_nvs_calls(FAKE_SET) == 0 && fake_nvs_calls(FAKE_COMMIT) == 0);
  const uint8_t endpoints[] = {20, 100};
  for (unsigned i = 0; i < sizeof(endpoints) / sizeof(endpoints[0]); ++i) {
    base = current();
    draft = base;
    draft.day_brightness = draft.night_brightness = endpoints[i];
    assert(device_config_save_display(&base, &draft) == ESP_OK);
    assert(device_config_init() == ESP_OK);
    applied = current();
    assert(applied.day_brightness == endpoints[i]
           && applied.night_brightness == endpoints[i]);
  }
}

static void test_shared_identity_partial_saves(void)
{
  for (bool enabling = false;; enabling = true) {
    for (unsigned failure = 1; failure <= (enabling ? 6u : 2u); ++failure) {
      device_config_t base = fresh(), draft = base;
      if (!enabling) {
        strcpy(draft.device_id, "bedroom-clock");
        strcpy(draft.message_sharing, "shared");
        assert(device_config_save_device(&base, &draft) == ESP_OK);
        base = current();
        draft = base;
        strcpy(draft.device_id, "clock-esp32-p4");
        strcpy(draft.message_sharing, "single");
      } else {
        strcpy(draft.device_id, "bedroom-clock");
        strcpy(draft.message_sharing, "shared");
      }
      fake_nvs_clear_activity();
      fake_nvs_fail(FAKE_SET, failure);
      assert(device_config_save_device(&base, &draft) == ESP_FAIL);
      device_config_t applied = current();
      assert_same(&base, &applied);
      fake_nvs_clear_activity();
      assert(device_config_init() == ESP_OK);
      applied = current();
      assert(device_config_validate(&applied) == ESP_OK);
      if (!strcmp(applied.message_sharing, "shared")) {
        assert(strcmp(applied.device_id, "clock-esp32-p4") != 0);
      }
    }
    if (enabling) break;
  }
}

static void test_unchanged_saves_write_only_selected_group(void)
{
  const char *device_keys[] = {"msg_share", "device_id", "room_name", "wifi_ssid", "wifi_pass",
    "night_st", "night_end", "night_mode", "sunset_offset", "sunrise_offset"};
  const char *display_keys[] = {"clock_face", "time_fmt", "lead_zero", "day_brightness", "night_bright"};
  const char *weather_keys[] = {"location", "timezone", "units", "lat", "lon"};
  const char *alarm_keys[] = {"alarm_enabled", "alarm_hour", "alarm_minute", "alarm_days",
    "alarm_sound", "alarm_volume", "alarm_snooze"};
  const char *toggle_keys[] = {"alarm_enabled"};
  const char **groups[] = {device_keys, display_keys, weather_keys, alarm_keys, toggle_keys};
  const unsigned counts[] = {sizeof(device_keys) / sizeof(device_keys[0]),
    sizeof(display_keys) / sizeof(display_keys[0]),
    sizeof(weather_keys) / sizeof(weather_keys[0]), sizeof(alarm_keys) / sizeof(alarm_keys[0]), 1};
  save_fn saves[] = {device_config_save_device, device_config_save_display, device_config_save_weather, device_config_save_alarm};
  for (unsigned group = 0; group < 5; ++group) {
    device_config_t base = fresh(), weather = resolved(base);
    assert(device_config_save_weather(&base, &weather) == ESP_OK);
    base = current();
    unsigned keys = fake_nvs_key_count();
    /* Deliberate storage markers make a write to an unrelated key observable,
     * even when the in-memory value for that key has not changed. This is a
     * constructed isolation check, not a normal saved configuration. */
    for (unsigned i = 0; i < keys; ++i) fake_nvs_put_str(fake_nvs_key(i), "untouched marker");
    fake_nvs_clear_activity();
    if (group < 4) assert(saves[group](&base, &base) == ESP_OK);
    else assert(device_config_set_alarm_enabled(base.alarm_enabled) == ESP_OK);
    assert(fake_nvs_calls(FAKE_SET) == counts[group]);
    assert(fake_nvs_calls(FAKE_COMMIT) == 1);
    device_config_t applied = current();
    assert_same(&base, &applied);
    assert(fake_nvs_key_count() == keys);
    nvs_handle_t handle;
    assert(nvs_open("wac_cfg", NVS_READWRITE, &handle) == ESP_OK);
    for (unsigned i = 0; i < keys; ++i) {
      const char *key = fake_nvs_key(i);
      bool selected = false;
      for (unsigned j = 0; j < counts[group]; ++j) selected |= !strcmp(key, groups[group][j]);
      char value[DEVICE_CONFIG_STR_LEN];
      size_t size = sizeof(value);
      esp_err_t result = nvs_get_str(handle, key, value, &size);
      if (selected) assert(result == ESP_ERR_NVS_TYPE_MISMATCH ||
        (result == ESP_OK && strcmp(value, "untouched marker")));
      else assert(result == ESP_OK && !strcmp(value, "untouched marker"));
    }
    nvs_close(handle);
  }
}

static void test_corrected_retry_repairs_partial_shared_mode(void)
{
  device_config_t base = fresh(), draft = base;
  strcpy(draft.device_id, "bedroom-clock");
  assert(device_config_save_device(&base, &draft) == ESP_OK);
  base = current();
  draft = base;
  strcpy(draft.device_id, "guest-clock");
  strcpy(draft.message_sharing, "shared");
  strcpy(draft.night_shift_start, "21:30");
  fake_nvs_clear_activity();
  /* Shared mode is the fifth write, after its valid custom ID. The night
   * schedule write then fails, leaving stored Shared mode but applied Single mode. */
  fake_nvs_fail(FAKE_SET, 6);
  assert(device_config_save_device(&base, &draft) == ESP_FAIL);
  device_config_t applied = current();
  assert_same(&base, &applied);
  assert(fake_nvs_calls(FAKE_SET) == 6 && fake_nvs_calls(FAKE_COMMIT) == 0);
  fake_nvs_clear_activity();
  nvs_handle_t handle;
  assert(nvs_open("wac_cfg", NVS_READWRITE, &handle) == ESP_OK);
  char value[DEVICE_CONFIG_STR_LEN];
  size_t size = sizeof(value);
  assert(nvs_get_str(handle, "msg_share", value, &size) == ESP_OK && !strcmp(value, "shared"));
  size = sizeof(value);
  assert(nvs_get_str(handle, "device_id", value, &size) == ESP_OK && !strcmp(value, "guest-clock"));
  nvs_close(handle);
  strcpy(draft.message_sharing, "single");
  strcpy(draft.device_id, "clock-esp32-p4");
  assert(device_config_save_device(&base, &draft) == ESP_OK);
  applied = current();
  assert(!strcmp(applied.message_sharing, "single"));
  assert(!strcmp(applied.device_id, "clock-esp32-p4"));
  assert(!strcmp(applied.night_shift_start, "21:30"));
  fake_nvs_clear_activity();
  assert(device_config_init() == ESP_OK);
  device_config_t restarted = current();
  assert_same(&applied, &restarted);
  assert(device_config_validate(&restarted) == ESP_OK);
  assert(fake_nvs_calls(FAKE_SET) == 0 && fake_nvs_calls(FAKE_COMMIT) == 0);
}

static void test_malformed_saved_values(void)
{
  const char *keys[] = {"timezone", "lat", "lon", "night_st"};
  const char *values[] = {"auto", "nan", "181", "25:30"};
  for (unsigned i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
    device_config_t base = fresh();
    fake_nvs_put_str(keys[i], values[i]);
    fake_nvs_clear_activity();
    assert(device_config_init() == ESP_ERR_INVALID_ARG);
    device_config_t applied = current();
    assert_same(&base, &applied);
    assert(fake_nvs_calls(FAKE_SET) == 0);
  }
  const char *numeric_keys[] = {"alarm_days", "alarm_hour", "night_mode", "lead_zero"};
  const uint8_t numeric_values[] = {0, 24, 255, 2};
  for (unsigned i = 0; i < sizeof(numeric_values); ++i) {
    device_config_t base = fresh();
    fake_nvs_put_u8(numeric_keys[i], numeric_values[i]);
    fake_nvs_clear_activity();
    assert(device_config_init() == ESP_ERR_INVALID_ARG);
    device_config_t applied = current();
    assert_same(&base, &applied);
    assert(fake_nvs_calls(FAKE_SET) == 0);
  }
  device_config_t base = fresh();
  char oversized[DEVICE_CONFIG_STR_LEN + 1];
  memset(oversized, 'X', sizeof(oversized) - 1);
  oversized[sizeof(oversized) - 1] = 0;
  fake_nvs_put_str("room_name", oversized);
  fake_nvs_clear_activity();
  assert(device_config_init() == ESP_ERR_NVS_INVALID_LENGTH);
  device_config_t applied = current();
  assert_same(&base, &applied);
  assert(fake_nvs_calls(FAKE_SET) == 0);
}

static void test_immediate_alarm_toggle_failures(void)
{
  fake_op_t failures[] = {FAKE_OPEN, FAKE_SET, FAKE_COMMIT};
  for (unsigned i = 0; i < sizeof(failures) / sizeof(failures[0]); ++i) {
    device_config_t base = fresh();
    fake_nvs_clear_activity();
    fake_nvs_fail(failures[i], 1);
    assert(device_config_set_alarm_enabled(true) == ESP_FAIL);
    device_config_t applied = current();
    assert_same(&base, &applied);
    fake_nvs_clear_activity();
    assert(device_config_init() == ESP_OK);
    applied = current();
    assert(applied.alarm_enabled == (failures[i] == FAKE_COMMIT));
  }
}

typedef struct {
  device_config_t baseline, draft, copied;
  bool copy, started, done;
  save_fn save;
  esp_err_t result;
  pthread_mutex_t lock;
  pthread_cond_t condition;
} worker_t;

static void *run_worker(void *arg)
{
  worker_t *work = arg;
  pthread_mutex_lock(&work->lock);
  work->started = true;
  pthread_cond_broadcast(&work->condition);
  pthread_mutex_unlock(&work->lock);
  work->result = work->copy ? device_config_copy(&work->copied)
    : work->save(&work->baseline, &work->draft);
  pthread_mutex_lock(&work->lock);
  work->done = true;
  pthread_cond_broadcast(&work->condition);
  pthread_mutex_unlock(&work->lock);
  return NULL;
}

static bool wait_worker(worker_t *work, bool done, long milliseconds)
{
  struct timespec until;
  clock_gettime(CLOCK_REALTIME, &until);
  until.tv_nsec += milliseconds * 1000000;
  until.tv_sec += until.tv_nsec / 1000000000;
  until.tv_nsec %= 1000000000;
  pthread_mutex_lock(&work->lock);
  while (!(done ? work->done : work->started)) {
    if (pthread_cond_timedwait(&work->condition, &work->lock, &until) != 0) break;
  }
  bool result = done ? work->done : work->started;
  pthread_mutex_unlock(&work->lock);
  return result;
}

static void test_slow_save_readers_and_serialized_writers(void)
{
  for (unsigned pairing = 0; pairing < 3; ++pairing) {
    device_config_t base = fresh();
    worker_t first = {.baseline = base, .draft = base, .save = device_config_save_device,
      .lock = PTHREAD_MUTEX_INITIALIZER, .condition = PTHREAD_COND_INITIALIZER};
    worker_t second = {.baseline = base, .draft = base, .save = device_config_save_display,
      .lock = PTHREAD_MUTEX_INITIALIZER, .condition = PTHREAD_COND_INITIALIZER};
    worker_t reader = {.copy = true,
      .lock = PTHREAD_MUTEX_INITIALIZER, .condition = PTHREAD_COND_INITIALIZER};
    if (pairing == 0) {
      strcpy(first.draft.room_name, "Bedroom");
      second.draft.day_brightness = 65;
    } else {
      first.save = device_config_save_display;
      first.draft.day_brightness = 65;
      if (pairing == 1) {
        second.save = device_config_save_alarm;
        second.draft = alarm_edit(base);
      } else {
        /* Two Display editors based on the same applied copy can save separate
         * controls without restoring the other's unchanged baseline fields. */
        strcpy(second.draft.time_format, "24");
      }
    }
    pthread_t first_thread, second_thread, reader_thread;
    fake_nvs_clear_activity();
    fake_nvs_block_next_set();
    assert(pthread_create(&first_thread, NULL, run_worker, &first) == 0);
    assert(fake_nvs_wait_blocked());
    assert(pthread_create(&reader_thread, NULL, run_worker, &reader) == 0);
    assert(wait_worker(&reader, true, 500));
    assert(reader.result == ESP_OK);
    assert_same(&reader.copied, &base);
    assert(pthread_create(&second_thread, NULL, run_worker, &second) == 0);
    assert(wait_worker(&second, false, 500));
    assert(!wait_worker(&second, true, 100));
    assert(fake_nvs_calls(FAKE_OPEN) == 1);
    fake_nvs_release_set();
    assert(pthread_join(first_thread, NULL) == 0);
    assert(pthread_join(second_thread, NULL) == 0);
    assert(pthread_join(reader_thread, NULL) == 0);
    assert(first.result == ESP_OK && second.result == ESP_OK);
    device_config_t applied = current();
    assert(applied.day_brightness == 65);
    if (pairing == 0) assert(!strcmp(applied.room_name, "Bedroom"));
    else if (pairing == 1) assert(applied.alarm_enabled && applied.alarm_hour == 6
      && applied.alarm_sound == DEVICE_CONFIG_ALARM_CLASSIC_BELL);
    else assert(!strcmp(applied.time_format, "24"));
    assert(fake_nvs_calls(FAKE_SET) == (pairing == 0 ? 15u : pairing == 1 ? 12u : 10u));
    assert(fake_nvs_calls(FAKE_COMMIT) == 2);
    pthread_mutex_destroy(&first.lock); pthread_cond_destroy(&first.condition);
    pthread_mutex_destroy(&second.lock); pthread_cond_destroy(&second.condition);
    pthread_mutex_destroy(&reader.lock); pthread_cond_destroy(&reader.condition);
  }
}

int main(void)
{
  device_config_t not_started;
  device_config_set_defaults(&not_started);
  assert(device_config_copy(&not_started) == ESP_ERR_INVALID_STATE);
  assert(device_config_set_alarm_enabled(true) == ESP_ERR_INVALID_STATE);
  assert(device_config_save_display(&not_started, &not_started) == ESP_ERR_INVALID_STATE);
  test_defaults_and_read_only_copies();
  test_validation();
  test_editor_merges_and_readiness();
  test_display_scope_and_latest_choices();
  test_display_invalid_choices_and_retry();
  test_rejected_saves_keep_applied_choices();
  test_save_failures(device_config_save_device, device_edit);
  test_save_failures(device_config_save_display, display_edit);
  test_save_failures(device_config_save_weather, resolved);
  test_save_failures(device_config_save_alarm, alarm_edit);
  test_startup_failures_and_missing_keys();
  test_loaded_brightness_minimum();
  test_brightness_save_limits();
  test_shared_identity_partial_saves();
  test_unchanged_saves_write_only_selected_group();
  test_corrected_retry_repairs_partial_shared_mode();
  test_malformed_saved_values();
  test_immediate_alarm_toggle_failures();
  test_slow_save_readers_and_serialized_writers();
  puts("Settings checks passed: defaults, validation, editor merges, partial saves, startup errors, concurrent access.");
  return 0;
}

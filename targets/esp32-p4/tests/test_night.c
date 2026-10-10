#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "night.h"
#include "test_support.h"

static device_config_t config;
static wac_time_snapshot_t clock_snapshot;
static int brightness;
static unsigned brightness_gets, brightness_sets;
static bool brightness_failure;
static pthread_mutex_t bsp_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t bsp_condition = PTHREAD_COND_INITIALIZER;
static bool bsp_block, bsp_pending, bsp_release;

int64_t esp_timer_get_time(void) { return (int64_t)xTaskGetTickCount() * 1000; }
int bsp_display_brightness_get(void) { ++brightness_gets; return brightness; }
esp_err_t bsp_display_brightness_set(int percent)
{
  assert(percent >= 0 && percent <= 100);
  pthread_mutex_lock(&bsp_lock);
  ++brightness_sets;
  /* The actual BSP changes its cached requested percentage before LEDC can
   * report failure. A failed operation is therefore not a successful fade. */
  brightness = percent;
  if (bsp_block) {
    bsp_pending = true;
    pthread_cond_broadcast(&bsp_condition);
    while (!bsp_release) pthread_cond_wait(&bsp_condition, &bsp_lock);
    bsp_block = bsp_pending = false;
  }
  pthread_mutex_unlock(&bsp_lock);
  return brightness_failure ? ESP_FAIL : ESP_OK;
}

static void reset(void)
{
  fake_nvs_reset();
  fake_clock_reset(0);
  config = (device_config_t){ .night_shift_mode = DEVICE_CONFIG_NIGHT_FIXED,
    .night_shift_start = "22:00", .night_shift_end = "06:00",
    .day_brightness = 80, .night_brightness = 20 };
  clock_snapshot = (wac_time_snapshot_t){0};
  brightness = 0;
  brightness_gets = brightness_sets = 0;
  brightness_failure = bsp_block = bsp_pending = bsp_release = false;
}
static void seed(uint8_t value)
{
  nvs_handle_t handle;
  assert(nvs_open("wac_night", NVS_READWRITE, &handle) == ESP_OK);
  assert(nvs_set_u8(handle, "effective", value) == ESP_OK);
  assert(nvs_commit(handle) == ESP_OK);
  nvs_close(handle);
  fake_nvs_clear_activity();
}
static bool at_utc(int year, int month, int day, int hour, int minute)
{
  struct tm utc = { .tm_year = year - 1900, .tm_mon = month - 1,
    .tm_mday = day, .tm_hour = hour, .tm_min = minute };
  clock_snapshot.usable = true;
  clock_snapshot.utc_now = timegm(&utc);
  assert(wac_night_update(&config, &clock_snapshot) == ESP_OK);
  return wac_night_active();
}
static bool at(int hour, int minute) { return at_utc(2026, 10, 9, hour, minute); }
static void tick(unsigned milliseconds)
{
  fake_clock_advance(milliseconds);
  assert(wac_night_update(&config, &clock_snapshot) == ESP_OK);
}
static void fixed_schedule_checks(void)
{
  reset();
  assert(wac_night_init(&config, &clock_snapshot) == ESP_OK);
  assert(!at(21, 59)); assert(at(22, 0)); assert(at(23, 59));
  assert(at(0, 0)); assert(at(5, 59)); assert(!at(6, 0)); assert(!at(12, 0));
  strcpy(config.night_shift_start, "09:00"); strcpy(config.night_shift_end, "17:00");
  assert(!at(8, 59)); assert(at(9, 0)); assert(at(16, 59));
  assert(!at(17, 0)); assert(!at(23, 0));
  strcpy(config.night_shift_end, "09:00");
  assert(at(0, 0)); assert(at(9, 0)); assert(at(23, 59));
  strcpy(config.night_shift_start, "22:00"); strcpy(config.night_shift_end, "06:00");
  assert(setenv("TZ", "CST6CDT,M3.2.0,M11.1.0", 1) == 0); tzset();
  assert(!at_utc(2026, 7, 10, 2, 59)); assert(at_utc(2026, 7, 10, 3, 0));
  assert(at_utc(2026, 7, 10, 10, 59)); assert(!at_utc(2026, 7, 10, 11, 0));
  assert(!at_utc(2026, 1, 10, 3, 59)); assert(at_utc(2026, 1, 10, 4, 0));
  assert(at_utc(2026, 1, 10, 11, 59)); assert(!at_utc(2026, 1, 10, 12, 0));
  assert(setenv("TZ", "UTC0", 1) == 0); tzset();
  const char *invalid[] = { "6:00", "06-00", "ab:cd", "06:60", "24:00", "" };
  for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
    strcpy(config.night_shift_end, invalid[i]); assert(!at(23, 0));
  }
  strcpy(config.night_shift_end, "06:00"); assert(at(23, 0));
  clock_snapshot.usable = false; strcpy(config.night_shift_start, "25:00");
  assert(wac_night_update(&config, &clock_snapshot) == ESP_OK); assert(wac_night_active());
  assert(!at(12, 0)); /* Original unavailable-time retention precedes validation. */
}
static void startup_and_retention_checks(void)
{
  for (unsigned saved = 0; saved <= 2; ++saved) {
    reset(); seed(saved);
    assert(wac_night_init(&config, &clock_snapshot) == ESP_OK);
    assert(wac_night_active() == (saved == 1));
    assert(fake_nvs_calls(FAKE_SET) == 0);
  }
  reset(); assert(wac_night_init(&config, NULL) == ESP_OK); assert(!wac_night_active());
  reset();
  nvs_handle_t handle;
  assert(nvs_open("wac_night", NVS_READWRITE, &handle) == ESP_OK);
  assert(nvs_set_str(handle, "effective", "night") == ESP_OK);
  nvs_close(handle); fake_nvs_clear_activity();
  assert(wac_night_init(&config, NULL) == ESP_OK); assert(!wac_night_active());
  for (unsigned saved = 0; saved <= 1; ++saved) {
    reset(); seed(saved);
    struct tm utc = { .tm_year = 126, .tm_mon = 9, .tm_mday = 9,
      .tm_hour = saved ? 12 : 23 };
    clock_snapshot = (wac_time_snapshot_t){ .usable = true, .utc_now = timegm(&utc) };
    assert(wac_night_init(&config, &clock_snapshot) == ESP_OK);
    assert(wac_night_active() == !saved); /* Resolved schedule beats remembered value. */
    assert(fake_nvs_calls(FAKE_SET) == 1);
  }
  reset(); fake_nvs_fail(FAKE_OPEN, 1);
  assert(wac_night_init(&config, NULL) == ESP_FAIL); assert(!wac_night_active());
  assert(fake_nvs_calls(FAKE_SET) == 0);
  reset(); seed(1); fake_nvs_fail(FAKE_READ, 1);
  assert(wac_night_init(&config, &clock_snapshot) == ESP_FAIL); assert(!wac_night_active());
  assert(fake_nvs_calls(FAKE_SET) == 0);
  reset(); seed(1); config.night_shift_mode = DEVICE_CONFIG_NIGHT_OFF;
  assert(wac_night_init(&config, NULL) == ESP_OK); assert(!wac_night_active());
  assert(fake_nvs_calls(FAKE_SET) == 1);
  reset(); config.night_shift_mode = DEVICE_CONFIG_NIGHT_ALWAYS_ON;
  assert(wac_night_init(&config, NULL) == ESP_OK); assert(wac_night_active());
  assert(fake_nvs_calls(FAKE_SET) == 1);
  config.night_shift_mode = DEVICE_CONFIG_NIGHT_FIXED;
  assert(wac_night_update(&config, NULL) == ESP_OK); assert(wac_night_active());
  config.night_shift_mode = DEVICE_CONFIG_NIGHT_SOLAR;
  assert(wac_night_update(&config, &clock_snapshot) == ESP_OK); assert(wac_night_active());
  assert(at(12, 0)); /* Solar calculations are deferred; events stay unavailable. */
  config.night_shift_mode = DEVICE_CONFIG_NIGHT_OFF;
  assert(wac_night_update(&config, NULL) == ESP_OK); assert(!wac_night_active());
  config.night_shift_mode = DEVICE_CONFIG_NIGHT_SOLAR;
  assert(wac_night_update(&config, NULL) == ESP_OK); assert(!wac_night_active());
  assert(fake_nvs_calls(FAKE_SET) == 2);
}
static void persistence_checks(void)
{
  for (fake_op_t failure = FAKE_OPEN; failure < FAKE_OP_COUNT; ++failure) {
    if (failure == FAKE_READ) continue;
    reset(); seed(0);
    assert(wac_night_init(&config, NULL) == ESP_OK);
    fake_nvs_clear_activity(); fake_nvs_fail(failure, 1);
    config.night_shift_mode = DEVICE_CONFIG_NIGHT_ALWAYS_ON;
    assert(wac_night_update(&config, NULL) == ESP_FAIL);
    assert(wac_night_active());
    fake_nvs_clear_activity();
    for (unsigned i = 0; i < 8; ++i) tick(250);
    assert(fake_nvs_calls(FAKE_SET) == 0); /* No per-tick save or retry service. */
    config.night_shift_mode = DEVICE_CONFIG_NIGHT_FIXED;
    assert(wac_night_init(&config, NULL) == ESP_OK);
    /* Successful fake sets remain readable even after commit failure. This
     * checks an accepted partial-write outcome, not physical power-cut behavior. */
    assert(wac_night_active() == (failure == FAKE_COMMIT));
  }
  reset(); fake_nvs_put_u8("effective", 1); /* Same key in settings namespace. */
  assert(wac_night_init(&config, NULL) == ESP_OK); assert(!wac_night_active());
  config.night_shift_mode = DEVICE_CONFIG_NIGHT_ALWAYS_ON;
  assert(wac_night_update(&config, NULL) == ESP_OK);
  assert(fake_nvs_key_count() == 2);
  assert(!strcmp(fake_nvs_namespace(0), "wac_cfg"));
  assert(!strcmp(fake_nvs_namespace(1), "wac_night"));
  assert(!strcmp(fake_nvs_key(1), "effective"));
  fake_nvs_clear_activity(); config.night_brightness = 25;
  for (unsigned i = 0; i < 10; ++i) tick(250);
  assert(fake_nvs_calls(FAKE_SET) == 0);
}
static void brightness_checks(void)
{
  reset(); seed(1);
  assert(wac_night_init(&config, NULL) == ESP_OK);
  tick(250); assert(brightness_gets == 0 && brightness_sets == 0);
  wac_night_start_backlight(); assert(brightness_gets == 0 && brightness_sets == 0);
  tick(250); assert(brightness_gets == 1 && brightness_sets == 0);
  tick(49); assert(brightness_sets == 0);
  tick(1); assert(brightness == 1);
  tick(250); assert(brightness == 6);
  tick(10000); assert(brightness == 11); /* Delayed update cannot jump to target. */
  tick(250); assert(brightness == 16);
  tick(250); assert(brightness == 20);
  unsigned sets = brightness_sets; tick(500); assert(brightness_sets == sets);
  config.night_shift_mode = DEVICE_CONFIG_NIGHT_OFF;
  tick(250); assert(brightness > 20 && brightness < 80);
  int before = brightness;
  config.night_shift_mode = DEVICE_CONFIG_NIGHT_ALWAYS_ON;
  tick(250); assert(brightness < before && brightness >= 20);
  config.night_brightness = 35;
  tick(250); assert(brightness > 20 && brightness < 35);
  for (unsigned i = 0; i < 10; ++i) tick(250);
  assert(brightness == 35);
  config.day_brightness = 35; config.night_shift_mode = DEVICE_CONFIG_NIGHT_OFF;
  sets = brightness_sets; tick(250); assert(brightness_sets == sets);
  config.day_brightness = 20;
  for (unsigned i = 0; i < 10; ++i) tick(250);
  assert(brightness == 20);
  config.day_brightness = 100;
  for (unsigned i = 0; i < 25; ++i) tick(250);
  assert(brightness == 100);
  config.day_brightness = 80;
  brightness_failure = true; fake_clock_advance(250);
  assert(wac_night_update(&config, NULL) == ESP_FAIL); assert(brightness == 95);
  brightness_failure = false; tick(250);
  assert(brightness == 95); /* Retry from last success 100, not BSP failed cache 95. */
  tick(250); assert(brightness == 90);
  assert(brightness_gets == 1);
  reset(); seed(1); config.night_brightness = 25; brightness = 70;
  assert(wac_night_init(&config, NULL) == ESP_OK);
  wac_night_start_backlight(); tick(250); tick(250);
  assert(brightness == 65); /* Startup fades from the real board setting. */
  for (unsigned i = 0; i < 10; ++i) tick(250);
  assert(brightness == 25); /* Uses today's configured value, not a stored brightness. */
  reset(); config.night_shift_mode = DEVICE_CONFIG_NIGHT_OFF;
  assert(wac_night_init(&config, NULL) == ESP_OK);
  wac_night_start_backlight(); tick(250);
  for (unsigned i = 0; i < 20; ++i) tick(250);
  assert(brightness == 80);
  fake_nvs_clear_activity(); fake_nvs_fail(FAKE_SET, 1);
  config.night_shift_mode = DEVICE_CONFIG_NIGHT_ALWAYS_ON;
  fake_clock_advance(250);
  assert(wac_night_update(&config, NULL) == ESP_FAIL);
  assert(wac_night_active() && brightness == 75); /* Save error does not stop fading. */
  fake_nvs_clear_activity();
  for (unsigned i = 0; i < 20; ++i) tick(250);
  assert(brightness == 20 && fake_nvs_calls(FAKE_SET) == 0);
  config.night_brightness = 30;
  for (unsigned i = 0; i < 4; ++i) tick(250);
  assert(brightness == 30 && fake_nvs_calls(FAKE_SET) == 0);
}
static esp_err_t writer_result;
static void *writer(void *unused)
{
  (void)unused; writer_result = wac_night_update(&config, NULL); return NULL;
}
static void startup_storage_no_memory_checks(void)
{
  const fake_op_t failures[] = { FAKE_OPEN, FAKE_READ, FAKE_SET, FAKE_COMMIT };
  for (size_t i = 0; i < sizeof(failures) / sizeof(failures[0]); ++i) {
    reset(); seed(0); config.night_shift_mode = DEVICE_CONFIG_NIGHT_ALWAYS_ON;
    fake_nvs_fail_with_error(failures[i], 1, ESP_ERR_NO_MEM);
    assert(wac_night_init(&config, NULL) == ESP_ERR_NO_MEM);
    assert(wac_night_active());
    if (failures[i] == FAKE_OPEN || failures[i] == FAKE_READ)
      assert(fake_nvs_calls(FAKE_SET) == 0);
    fake_nvs_clear_activity();
    wac_night_start_backlight(); tick(250);
    for (unsigned step = 0; step < 5; ++step) tick(250);
    assert(brightness == 20 && wac_night_active());
    assert(fake_nvs_calls(FAKE_SET) == 0);
  }
}
static void published_read_checks(void)
{
  reset(); assert(wac_night_init(&config, NULL) == ESP_OK);
  config.night_shift_mode = DEVICE_CONFIG_NIGHT_ALWAYS_ON;
  fake_nvs_block_next_set(); pthread_t thread;
  assert(pthread_create(&thread, NULL, writer, NULL) == 0);
  assert(fake_nvs_wait_blocked()); assert(wac_night_active());
  fake_nvs_release_set(); assert(pthread_join(thread, NULL) == 0); assert(writer_result == ESP_OK);
  wac_night_start_backlight(); tick(250); fake_clock_advance(250);
  bsp_block = true;
  assert(pthread_create(&thread, NULL, writer, NULL) == 0);
  pthread_mutex_lock(&bsp_lock);
  while (!bsp_pending) pthread_cond_wait(&bsp_condition, &bsp_lock);
  pthread_mutex_unlock(&bsp_lock);
  assert(wac_night_active());
  pthread_mutex_lock(&bsp_lock); bsp_release = true;
  pthread_cond_broadcast(&bsp_condition); pthread_mutex_unlock(&bsp_lock);
  assert(pthread_join(thread, NULL) == 0); assert(writer_result == ESP_OK);
}
int main(void)
{
  assert(setenv("TZ", "UTC0", 1) == 0); tzset();
  fixed_schedule_checks(); startup_and_retention_checks(); persistence_checks();
  brightness_checks(); startup_storage_no_memory_checks(); published_read_checks();
  puts("Production Night Shift checks passed: fallback, fixed local-time boundaries, operating-record failures, gradual brightness, and reads during pending I/O.");
  return 0;
}

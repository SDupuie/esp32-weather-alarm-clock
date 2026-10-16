#include "sdkconfig.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_lcd_touch.h"
#include "esp_log.h"
#include "esp_lv_adapter.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "cJSON.h"
#include "bsp/esp32_p4_wifi6_touch_lcd_xc.h"

#include "app_runtime.h"
#include "app_ui.h"
#include "alarm.h"
#include "bsp_shims.h"
#include "connectivity.h"
#include "device_config.h"
#include "message_service.h"
#include "night.h"
#include "weather_client.h"
#include "clock_time.h"

static const char *TAG = "rwd_esp32_p4";

static void *json_alloc(size_t size)
{
  return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

static const uint32_t UI_HEARTBEAT_LOCK_TIMEOUT_MS = 250;
static const uint32_t INITIAL_UI_LOCK_TIMEOUT_MS = 1000;
static const uint32_t RUNTIME_LOCK_TIMEOUT_MS = 250;
static const uint32_t WIFI_CONNECT_TIMEOUT_MS = 30000;
static const uint32_t WEATHER_REFRESH_INTERVAL_MS = 10 * 60 * 1000;
static const uint32_t WEATHER_RETRY_INTERVAL_MS = 20000;
static const uint32_t RADAR_REFRESH_INTERVAL_MS = 5 * 60 * 1000;
static const uint32_t RADAR_STEP_INTERVAL_MS = 100;

typedef struct {
  int64_t due_us;
  bool active, publication_pending;
  app_radar_snapshot_t publication;
} weather_radar_work_t;

static app_runtime_state_t s_runtime_state;
static SemaphoreHandle_t s_runtime_mutex;
static uint32_t s_radar_displayed_generation;
static QueueHandle_t s_clock_commands;

/* Clock queue payloads are copied; callbacks never retain a screen pointer. */
typedef enum { CLOCK_COMMAND_WARNING, CLOCK_COMMAND_ALARM } clock_command_kind_t;
typedef struct {
  clock_command_kind_t kind;
  wac_time_warning_t warning;
  app_alarm_action_t action;
  wac_alarm_occurrence_t occurrence;
} clock_command_t;
typedef struct {
  bool ready;
  app_alarm_action_t action;
  wac_alarm_occurrence_t occurrence;
  esp_err_t error;
} clock_alarm_result_t;
/* The UI permits one outstanding alarm action. Keep its completion until the
 * heartbeat obtains the display lock; unrelated warning commands share queue. */
static portMUX_TYPE s_alarm_result_lock = portMUX_INITIALIZER_UNLOCKED;
static clock_alarm_result_t s_alarm_result;

static esp_err_t read_touch(esp_lcd_touch_handle_t touch,
  esp_lcd_touch_point_data_t *points, uint8_t *point_count,
  uint8_t max_points, void *user_ctx)
{
  (void) user_ctx;
  esp_err_t err = esp_lcd_touch_read_data(touch);
  if (err != ESP_OK) {
    *point_count = 0;
    /* GT911 keeps points on a failed or not-ready read. Clear only failures
     * so a later not-ready poll cannot replay the interrupted press. */
    portENTER_CRITICAL(&touch->data.lock);
    touch->data.points = 0;
    portEXIT_CRITICAL(&touch->data.lock);
    lv_indev_t *active = lv_indev_active();
    if (active != NULL) {
      lv_indev_wait_release(active);
    }
    ESP_LOGE(TAG, "Touch read failed: %s", esp_err_to_name(err));
    return err;
  }
  return esp_lcd_touch_get_data(touch, points, point_count, max_points);
}

static void log_device_clock(const char *context)
{
  wac_time_snapshot_t clock;
  wac_time_snapshot(&clock);
  if (!clock.usable) {
    ESP_LOGI(TAG, "%s: waiting for usable time", context);
    return;
  }
  time_t now = clock.utc_now;

  struct tm local_time = {0};
  struct tm utc_time = {0};
  char local_buf[32] = {0};
  char utc_buf[32] = {0};

  localtime_r(&now, &local_time);
  gmtime_r(&now, &utc_time);
  strftime(local_buf, sizeof(local_buf), "%Y-%m-%d %H:%M:%S %Z", &local_time);
  strftime(utc_buf, sizeof(utc_buf), "%Y-%m-%d %H:%M:%S UTC", &utc_time);

  ESP_LOGI(TAG, "%s: local=%s utc=%s", context, local_buf, utc_buf);
}

static void runtime_state_update(
  const app_weather_snapshot_t *weather,
  bool mark_weather_stale
)
{
  if (s_runtime_mutex == NULL) return;
  if (xSemaphoreTake(s_runtime_mutex, pdMS_TO_TICKS(RUNTIME_LOCK_TIMEOUT_MS)) != pdTRUE) {
    ESP_LOGW(TAG, "Timed out waiting for runtime state lock");
    return;
  }
  if (weather != NULL) s_runtime_state.weather = *weather;
  if (mark_weather_stale && s_runtime_state.weather.valid)
    s_runtime_state.weather.stale = true;
  xSemaphoreGive(s_runtime_mutex);
}

static bool runtime_radar_publish(const app_radar_snapshot_t *radar)
{
  if (!s_runtime_mutex || xSemaphoreTake(s_runtime_mutex,
        pdMS_TO_TICKS(RUNTIME_LOCK_TIMEOUT_MS)) != pdTRUE) return false;
  s_runtime_state.radar = *radar;
  xSemaphoreGive(s_runtime_mutex);
  return true;
}

/* Only acknowledge after LVGL has replaced its image source under the display
 * lock. The client may then reuse the previous slot, never the displayed one. */
static void runtime_radar_acknowledge(uint32_t generation)
{
  if (!s_runtime_mutex || xSemaphoreTake(s_runtime_mutex,
        pdMS_TO_TICKS(RUNTIME_LOCK_TIMEOUT_MS)) != pdTRUE) return;
  s_radar_displayed_generation = generation;
  xSemaphoreGive(s_runtime_mutex);
}

static uint32_t runtime_radar_displayed(void)
{
  if (!s_runtime_mutex || xSemaphoreTake(s_runtime_mutex,
        pdMS_TO_TICKS(RUNTIME_LOCK_TIMEOUT_MS)) != pdTRUE) return 0;
  uint32_t displayed = s_radar_displayed_generation;
  xSemaphoreGive(s_runtime_mutex);
  return displayed;
}

static void runtime_state_snapshot(app_runtime_state_t *out_runtime, const device_config_t *config)
{
  if (out_runtime == NULL) {
    return;
  }

  memset(out_runtime, 0, sizeof(*out_runtime));

  if (s_runtime_mutex != NULL
      && xSemaphoreTake(s_runtime_mutex, pdMS_TO_TICKS(RUNTIME_LOCK_TIMEOUT_MS)) == pdTRUE) {
    *out_runtime = s_runtime_state;
    xSemaphoreGive(s_runtime_mutex);

    /* Gather owner data before the display lock. */
    wac_time_snapshot(&out_runtime->clock);

    app_message_snapshot_t message_snapshot = {0};
    uint32_t unread_count = 0;
    bool has_important_message = false;
    if (message_service_get_snapshot(
          config->device_id,
          &message_snapshot,
          &unread_count,
          &has_important_message
        ) == ESP_OK) {
      out_runtime->message = message_snapshot;
      out_runtime->unread_message_count = unread_count;
      out_runtime->has_important_message = has_important_message;
    }
  }
  out_runtime->night_active = wac_night_active();
  wac_night_solar_times(&out_runtime->solar);
  wac_alarm_copy(&out_runtime->alarm);
  wac_audio_copy(&out_runtime->audio);
}

static esp_err_t request_clock_warning_dismiss(wac_time_warning_t warning)
{
  if (warning != WAC_TIME_WARNING_LOST_TIME && warning != WAC_TIME_WARNING_CONNECTION)
    return ESP_ERR_INVALID_ARG;
  if (s_clock_commands == NULL) return ESP_ERR_INVALID_STATE;
  clock_command_t command = { .kind = CLOCK_COMMAND_WARNING, .warning = warning };
  return xQueueSend(s_clock_commands, &command, 0) == pdTRUE
    ? ESP_OK : ESP_ERR_TIMEOUT;
}

static bool alarm_occurrence_matches(const wac_alarm_occurrence_t *a,
                                     const wac_alarm_occurrence_t *b)
{
  return a->valid && b->valid && a->utc == b->utc
    && a->local_date == b->local_date && a->minute == b->minute;
}

static esp_err_t request_alarm_action(app_alarm_action_t action,
                                      const wac_alarm_occurrence_t *occurrence)
{
  if ((unsigned)action > APP_ALARM_DISMISS_MISSED || !occurrence || !occurrence->valid)
    return ESP_ERR_INVALID_ARG;
  if (!s_clock_commands) return ESP_ERR_INVALID_STATE;
  clock_command_t command = { .kind = CLOCK_COMMAND_ALARM,
    .action = action, .occurrence = *occurrence };
  return xQueueSend(s_clock_commands, &command, 0) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

static esp_err_t execute_alarm_action(const clock_command_t *command,
  const device_config_t *config, const wac_time_snapshot_t *clock)
{
  if (!config || !clock) return ESP_ERR_INVALID_STATE;
  wac_alarm_snapshot_t alarm;
  wac_alarm_copy(&alarm);
  const wac_alarm_occurrence_t *current = command->action == APP_ALARM_DISMISS_MISSED
    ? &alarm.missed : &alarm.current;
  /* A delayed tap belongs to the occurrence displayed when it was submitted. */
  if (!alarm_occurrence_matches(current, &command->occurrence)) return ESP_ERR_INVALID_STATE;
  switch (command->action) {
    case APP_ALARM_SNOOZE: return wac_alarm_snooze(config, clock);
    case APP_ALARM_DISMISS: return wac_alarm_dismiss();
    case APP_ALARM_DISMISS_MISSED: return wac_alarm_dismiss_missed();
    default: return ESP_ERR_INVALID_ARG;
  }
}

static void publish_alarm_result(clock_alarm_result_t *result)
{
  portENTER_CRITICAL(&s_alarm_result_lock);
  s_alarm_result = *result;
  portEXIT_CRITICAL(&s_alarm_result_lock);
}

static bool take_alarm_result(clock_alarm_result_t *out)
{
  portENTER_CRITICAL(&s_alarm_result_lock);
  bool ready = s_alarm_result.ready;
  if (ready) {
    *out = s_alarm_result;
    s_alarm_result.ready = false;
  }
  portEXIT_CRITICAL(&s_alarm_result_lock);
  return ready;
}

static void clock_control_task(void *arg)
{
  (void) arg;
  esp_err_t last_alarm_error = ESP_OK;
  esp_err_t last_rtc_error = ESP_OK;
  esp_err_t last_night_error = ESP_OK;
  wac_alarm_phase_t last_alarm_phase = WAC_ALARM_IDLE;
  time_t last_missed_alarm = 0;
  while (true) {
    device_config_t config;
    bool have_config = device_config_copy(&config) == ESP_OK;
    wac_time_snapshot_t clock;
    wac_time_snapshot(&clock);
    clock_command_t command;
    clock_alarm_result_t alarm_result = {0};
    while (xQueueReceive(s_clock_commands, &command, 0) == pdTRUE) {
      if (command.kind == CLOCK_COMMAND_WARNING) wac_time_dismiss_warning(command.warning);
      else {
        alarm_result.ready = true;
        alarm_result.action = command.action;
        alarm_result.occurrence = command.occurrence;
        alarm_result.error = execute_alarm_action(&command, have_config ? &config : NULL, &clock);
        if (alarm_result.error != ESP_OK)
          ESP_LOGW(TAG, "Alarm action returned %s", esp_err_to_name(alarm_result.error));
      }
    }
    /* Due alarms take priority over optional RTC correction and backlight work.
     * Newly accepted network time is used on the next 250 ms iteration. */
    if (have_config) {
      esp_err_t error = wac_alarm_update(&config, &clock);
      if (error != ESP_OK && error != last_alarm_error) {
        ESP_LOGW(TAG, "Alarm operating-record save failed: %s", esp_err_to_name(error));
      }
      last_alarm_error = error;
      wac_alarm_snapshot_t alarm;
      wac_alarm_copy(&alarm);
      time_t missed = alarm.missed.valid ? alarm.missed.utc : 0;
      if (alarm.phase != last_alarm_phase || missed != last_missed_alarm) {
        static const char *phases[] = { "idle", "ringing", "snoozed" };
        ESP_LOGI(TAG, "Alarm state=%s, scheduled date=%ld, missed date=%ld",
          phases[alarm.phase], (long)alarm.current.local_date, (long)alarm.missed.local_date);
        last_alarm_phase = alarm.phase;
        last_missed_alarm = missed;
      }
    }
    if (alarm_result.ready) publish_alarm_result(&alarm_result);
    wac_time_poll(connectivity_get_time_sync_generation());
    wac_time_snapshot(&clock);
    if (clock.rtc_error != last_rtc_error) {
      if (clock.rtc_error != ESP_OK) {
        ESP_LOGW(TAG, "Backup clock operation failed: %s", esp_err_to_name(clock.rtc_error));
      } else {
        ESP_LOGI(TAG, "Backup clock correction completed");
      }
      last_rtc_error = clock.rtc_error;
    }
    if (have_config) {
      esp_err_t error = wac_night_update(&config, &clock);
      if (error != ESP_OK && error != last_night_error) {
        ESP_LOGW(TAG, "Night Shift storage or backlight operation failed: %s", esp_err_to_name(error));
      }
      last_night_error = error;
    }
    vTaskDelay(pdMS_TO_TICKS(250));
  }
}

static void ui_heartbeat_task(void *arg)
{
  (void) arg;

  const TickType_t delay_ticks = pdMS_TO_TICKS(1000);
  TickType_t wake_tick = xTaskGetTickCount();
  uint32_t reported_stack_min = UINT32_MAX;

  while (true) {
    /* Include work in the period, and do not replay missed updates after a stall. */
    if (xTaskDelayUntil(&wake_tick, delay_ticks) == pdFALSE)
      wake_tick = xTaskGetTickCount();

    /* Static, not on this task's stack: the runtime state carries the
     * full weather snapshot including the conditions summary. Only this
     * task touches it. */
    static app_runtime_state_t runtime_snapshot;
    device_config_t config;
    if (device_config_copy(&config) != ESP_OK) {
      continue;
    }
    runtime_state_snapshot(&runtime_snapshot, &config);

    if (bsp_display_lock(UI_HEARTBEAT_LOCK_TIMEOUT_MS) == ESP_OK) {
      clock_alarm_result_t alarm_result;
      bool have_alarm_result = take_alarm_result(&alarm_result);
      /* The earlier snapshot may precede this action or a resumed ring. This
       * is a short copied read; no I/O or display wait runs in the owner. */
      if (have_alarm_result) wac_alarm_copy(&runtime_snapshot.alarm);
      /* The message owner keeps completion until the UI can accept it. */
      esp_err_t message_result;
      if (message_service_take_acknowledgment_result(&message_result)) {
        app_ui_complete_message_acknowledgment(message_result);
      }
      app_ui_update_runtime(&config, &runtime_snapshot);
      runtime_radar_acknowledge(runtime_snapshot.radar.generation);
      /* Navigation uses the owner's completed phase, including failed saves
       * whose live Snooze/Dismiss action has already taken effect. */
      if (have_alarm_result)
        app_ui_complete_alarm_action(alarm_result.action, &alarm_result.occurrence, alarm_result.error);
      bsp_display_unlock();
      if (runtime_snapshot.weather.valid) {
        uint32_t stack_min = (uint32_t) uxTaskGetStackHighWaterMark(NULL);
        if (stack_min < reported_stack_min) {
          reported_stack_min = stack_min;
          ESP_LOGI(TAG, "UI after weather: stack_free_min_bytes=%u", (unsigned) stack_min);
        }
      }
    }
  }
}

static void weather_service_wait(int64_t weather_due_us, int64_t time_retry_due_us,
                                 int64_t radar_due_us)
{
  if (time_retry_due_us > 0 && time_retry_due_us < weather_due_us) {
    weather_due_us = time_retry_due_us;
  }
  if (radar_due_us < weather_due_us) weather_due_us = radar_due_us;
  int64_t remaining_us = weather_due_us - esp_timer_get_time();
  if (remaining_us > 0) {
    TickType_t ticks = pdMS_TO_TICKS((remaining_us + 999) / 1000);
    vTaskDelay(ticks > 0 ? ticks : 1);
  }
}

static void weather_service_radar(const device_config_t *config,
                                  weather_radar_work_t *work, bool connected)
{
  /* Retain a failed short-copy handoff before asking the client to advance. */
  if (work->publication_pending)
    work->publication_pending = !runtime_radar_publish(&work->publication);
  if (work->publication_pending) return;
  /* Before the first image/attempt, wait for Wi-Fi readiness. An ordinary
   * outage must still mark an existing frame old when its check becomes due. */
  if (!connected && !work->active && !work->publication.valid) return;
  bool start = !work->active && esp_timer_get_time() >= work->due_us;
  if (!start && !work->active) return;
  if (start) work->due_us = esp_timer_get_time()
    + (int64_t) RADAR_REFRESH_INTERVAL_MS * 1000;
  weather_radar_step_t step = !connected ? WEATHER_RADAR_OFFLINE
    : start ? WEATHER_RADAR_CHECK : WEATHER_RADAR_CONTINUE;
  work->publication_pending = weather_client_radar_step(config, step,
    runtime_radar_displayed(), &work->publication, &work->active);
  if (work->publication_pending)
    work->publication_pending = !runtime_radar_publish(&work->publication);
}

static void weather_service_task(void *arg)
{
  (void) arg;

  const TickType_t retry_delay_ticks = pdMS_TO_TICKS(WEATHER_RETRY_INTERVAL_MS);
  const TickType_t wifi_timeout_ticks = pdMS_TO_TICKS(WIFI_CONNECT_TIMEOUT_MS);
  bool connectivity_started = false;
  bool reported_missing_wifi_profile = false;
  int64_t next_weather_attempt_us = 0;
  int64_t time_retry_due_us = 0;
  weather_radar_work_t radar_work = {0};

  while (true) {
    device_config_t config;
    if (device_config_copy(&config) != ESP_OK) {
      vTaskDelay(retry_delay_ticks);
      continue;
    }

    if (!config.wifi_ready) {
      weather_service_radar(&config, &radar_work, false);
      if (!reported_missing_wifi_profile) {
        ESP_LOGI(TAG, "No stored Wi-Fi profile is available yet; waiting for setup");
        reported_missing_wifi_profile = true;
      }

      vTaskDelay(retry_delay_ticks);
      continue;
    }

    reported_missing_wifi_profile = false;

    if (!connectivity_started) {
      ESP_LOGI(TAG, "Initializing Wi-Fi station for SSID '%s'", config.wifi_ssid);
      esp_err_t err = connectivity_init(&config);
      if (err != ESP_OK) {
        ESP_LOGW(TAG, "Wi-Fi init failed: %s", esp_err_to_name(err));
        runtime_state_update(NULL, true);
        weather_service_radar(&config, &radar_work, false);
        vTaskDelay(retry_delay_ticks);
        continue;
      }

      connectivity_started = true;
    }

    if (!connectivity_is_wifi_connected()) {
      ESP_LOGI(TAG, "Waiting up to %u ms for Wi-Fi connection", WIFI_CONNECT_TIMEOUT_MS);
      runtime_state_update(NULL, true);

      esp_err_t err = connectivity_wait_for_wifi(wifi_timeout_ticks);
      if (err != ESP_OK) {
        ESP_LOGW(TAG, "Wi-Fi connect timeout: %s", esp_err_to_name(err));
        runtime_state_update(NULL, true);
        weather_service_radar(&config, &radar_work, false);
        vTaskDelay(retry_delay_ticks);
        continue;
      }

      ESP_LOGI(TAG, "Wi-Fi association completed");
    }

    if (connectivity_is_time_synced()) {
      time_retry_due_us = 0;
    } else if (esp_timer_get_time() >= time_retry_due_us) {
      int64_t retry_due_us = esp_timer_get_time() + (int64_t) WEATHER_RETRY_INTERVAL_MS * 1000;
      esp_err_t err = connectivity_sync_time(0);
      time_retry_due_us = 0;
      /* A zero-wait timeout means transport is running; it retries in the
       * background and the clock owner receives its actual completion. */
      if (err != ESP_OK && err != ESP_ERR_TIMEOUT) {
        ESP_LOGW(TAG, "Network time startup failed: %s", esp_err_to_name(err));
        time_retry_due_us = retry_due_us;
      }
    }

    if (!config.location_ready) {
      vTaskDelay(retry_delay_ticks);
      continue;
    }

    int64_t quota_due = weather_client_next_allowed_us(&config);
    if (quota_due > next_weather_attempt_us) next_weather_attempt_us = quota_due;
    if (esp_timer_get_time() >= next_weather_attempt_us) {
      ESP_LOGI(TAG, "Requesting live weather for %s,%s (%s units)",
               config.latitude, config.longitude, config.units);
      app_weather_snapshot_t weather_snapshot = {0};
      esp_err_t weather_err = weather_client_fetch(&config, &weather_snapshot);
      if (weather_err != ESP_OK) {
        ESP_LOGW(TAG, "Weather refresh failed: %s", esp_err_to_name(weather_err));
        runtime_state_update(NULL, true);
        next_weather_attempt_us = esp_timer_get_time()
          + (int64_t) WEATHER_RETRY_INTERVAL_MS * 1000;
        quota_due = weather_client_next_allowed_us(&config);
        if (quota_due > next_weather_attempt_us) next_weather_attempt_us = quota_due;
      } else {
        /* Never turn a pre-sync download into a measured weather age later. */
        wac_time_snapshot_t download_clock;
        wac_time_snapshot(&download_clock);
        weather_snapshot.updated_at = download_clock.usable
          && download_clock.source == WAC_TIME_SOURCE_NETWORK ? download_clock.utc_now : 0;
        runtime_state_update(&weather_snapshot, false);
        ESP_LOGI(TAG, "Live weather updated (%d°, H:%d° L:%d°)",
                 weather_snapshot.temp, weather_snapshot.high, weather_snapshot.low);
        next_weather_attempt_us = esp_timer_get_time()
          + (int64_t) WEATHER_REFRESH_INTERVAL_MS * 1000;
      }
    }

    /* One bounded step leaves forecast/time eligible between Radar requests. */
    weather_service_radar(&config, &radar_work, true);
    int64_t radar_due_us = (radar_work.active || radar_work.publication_pending)
      ? esp_timer_get_time() + (int64_t) RADAR_STEP_INTERVAL_MS * 1000
      : radar_work.due_us;
    weather_service_wait(next_weather_attempt_us, time_retry_due_us, radar_due_us);
  }
}

void app_main(void)
{
  /* Keep drawing off the audio core; preserve the BSP's display defaults. */
  bsp_display_cfg_t display_config = {
    .lv_adapter_cfg = ESP_LV_ADAPTER_DEFAULT_CONFIG(),
    .rotation = ESP_LV_ADAPTER_ROTATE_0,
    .tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_PARTIAL,
    .touch_flags = {0},
  };
  display_config.lv_adapter_cfg.task_core_id = 0;
  /* Set once before JSON users start. Small response nodes must not consume
   * internal task/driver memory while the board's PSRAM is available. */
  cJSON_Hooks json_hooks = { .malloc_fn = json_alloc, .free_fn = heap_caps_free };
  cJSON_InitHooks(&json_hooks);
  /* Storage failure stops dependent startup; never erase user data to repair it. */
  esp_err_t storage_err = nvs_flash_init();
  if (storage_err == ESP_OK) {
    storage_err = device_config_init();
  }
  if (storage_err != ESP_OK) {
    ESP_LOGE(TAG, "Settings startup failed: %s", esp_err_to_name(storage_err));
    if (bsp_display_start_with_config(&display_config) != NULL) {
      if (bsp_display_lock(INITIAL_UI_LOCK_TIMEOUT_MS) == ESP_OK) {
        app_ui_show_storage_error(storage_err);
        bsp_display_unlock();
      }
      (void) bsp_display_backlight_on();
    }
    return;
  }
  device_config_t config;
  ESP_ERROR_CHECK(device_config_copy(&config));
  ESP_ERROR_CHECK(weather_client_init());
  ESP_ERROR_CHECK(wac_time_init(config.timezone));
  log_device_clock("Clock after boot timezone apply");
  wac_time_snapshot_t startup_clock;
  wac_time_snapshot(&startup_clock);
  esp_err_t alarm_err = wac_alarm_init();
  if (alarm_err != ESP_OK) {
    ESP_LOGW(TAG, "Alarm operating-record startup returned %s", esp_err_to_name(alarm_err));
  }
  esp_err_t night_err = wac_night_init(&config, &startup_clock);
  if (night_err != ESP_OK) {
    ESP_LOGW(TAG, "Night Shift startup returned %s", esp_err_to_name(night_err));
  }

  s_runtime_mutex = xSemaphoreCreateMutex();
  if (s_runtime_mutex == NULL) {
    ESP_LOGE(TAG, "Failed to allocate runtime mutex");
    return;
  }
  s_clock_commands = xQueueCreate(2, sizeof(clock_command_t));
  if (s_clock_commands == NULL) {
    ESP_LOGE(TAG, "Failed to allocate clock command queue");
    return;
  }
  if (xTaskCreate(clock_control_task, "wac_clock", 4096, NULL, 5, NULL) != pdPASS) {
    ESP_LOGE(TAG, "Failed to start clock control task");
    return;
  }
  ESP_ERROR_CHECK(connectivity_prepare_runtime());
  ESP_ERROR_CHECK(message_service_init(&config));

  ESP_LOGI(
    TAG,
    "Bootstrapping Round Weather Display on ESP32-P4 (%dx%d, room=%s, device=%s)",
    BSP_LCD_H_RES,
    BSP_LCD_V_RES,
    config.room_name,
    config.device_id
  );

  lv_display_t *display = bsp_display_start_with_config(&display_config);
  lv_indev_t *input = bsp_display_get_input_dev();
  if (display == NULL) {
    ESP_LOGE(TAG, "Display start failed");
    return;
  }
  (void) display;

  bool touch_ready = (input != NULL);

  /* Audio owns its worker and lazy speaker setup. Start after BSP bus/display
   * initialization; an unavailable speaker must not stop the clock or UI. */
  esp_err_t audio_err = wac_audio_init();
  if (audio_err != ESP_OK)
    ESP_LOGW(TAG, "Audio startup returned %s", esp_err_to_name(audio_err));

  /* Gather owner data before entering the display lock. */
  static app_runtime_state_t startup_runtime;
  runtime_state_snapshot(&startup_runtime, &config);

  esp_err_t lock_err = bsp_display_lock(INITIAL_UI_LOCK_TIMEOUT_MS);
  if (lock_err != ESP_OK) {
    ESP_LOGE(TAG, "Could not lock LVGL during initial draw: %s", esp_err_to_name(lock_err));
    return;
  }

  if (input != NULL) {
    const esp_lv_adapter_touch_callbacks_t callbacks = {.custom_touch_read = read_touch};
    ESP_ERROR_CHECK(esp_lv_adapter_set_touch_callbacks(input, &callbacks));
  }
  esp_err_t image_err = weather_client_init_images();
  if (image_err != ESP_OK)
    ESP_LOGW(TAG, "Radar image setup failed: %s", esp_err_to_name(image_err));
  app_ui_set_clock_warning_dismiss_handler(request_clock_warning_dismiss);
  app_ui_set_alarm_action_handler(request_alarm_action);
  ESP_ERROR_CHECK(app_ui_show_boot_screen(&config, &startup_runtime, touch_ready));
  bsp_display_unlock();

  /* The clock task advances the configured fade after the display is ready. */
  wac_night_start_backlight();

  ESP_LOGI(TAG, "Display ready, touch %s", touch_ready ? "detected" : "not detected");

  BaseType_t ui_task_ok = xTaskCreatePinnedToCore(
    ui_heartbeat_task,
    "rwd_ui_heartbeat",
    12288,
    NULL,
    4,
    NULL,
    0
  );
  if (ui_task_ok != pdPASS) {
    ESP_LOGW(TAG, "Failed to start UI heartbeat task");
  }

  /* Leave the drawing core free; audio shares core 1 at the same priority. */
  BaseType_t weather_task_ok = xTaskCreatePinnedToCore(
    weather_service_task,
    "rwd_weather",
    8192,
    NULL,
    4,
    NULL,
    1
  );
  if (weather_task_ok != pdPASS) {
    ESP_LOGW(TAG, "Failed to start weather service task");
  }
}

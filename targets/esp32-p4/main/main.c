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
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"

#include "bsp/esp32_p4_wifi6_touch_lcd_xc.h"

#include "app_runtime.h"
#include "app_ui.h"
#include "bsp_shims.h"
#include "connectivity.h"
#include "device_config.h"
#include "message_service.h"
#include "night.h"
#include "weather_client.h"
#include "clock_time.h"

static const char *TAG = "rwd_esp32_p4";

static const uint32_t UI_HEARTBEAT_LOCK_TIMEOUT_MS = 250;
static const uint32_t INITIAL_UI_LOCK_TIMEOUT_MS = 1000;
static const uint32_t RUNTIME_LOCK_TIMEOUT_MS = 250;
static const uint32_t WIFI_CONNECT_TIMEOUT_MS = 30000;
static const uint32_t WEATHER_REFRESH_INTERVAL_MS = 10 * 60 * 1000;
static const uint32_t WEATHER_RETRY_INTERVAL_MS = 20000;

static app_runtime_state_t s_runtime_state;
static SemaphoreHandle_t s_runtime_mutex;
static QueueHandle_t s_clock_commands;

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

static void copy_text(char *dest, size_t dest_size, const char *src)
{
  if (dest == NULL || dest_size == 0) {
    return;
  }

  if (src == NULL) {
    dest[0] = '\0';
    return;
  }

  snprintf(dest, dest_size, "%s", src);
}

static void runtime_state_seed(const device_config_t *config)
{
  memset(&s_runtime_state, 0, sizeof(s_runtime_state));
  copy_text(
    s_runtime_state.weather_status,
    sizeof(s_runtime_state.weather_status),
    !config->wifi_ready
      ? "No Wi-Fi profile found. Tap 'Set Up Wi-Fi' to connect."
      : (config->location_ready
          ? "Stored Wi-Fi profile found. Connecting for time sync and live weather..."
          : "Stored Wi-Fi profile found. Finish location setup for local weather.")
  );
}

static void runtime_state_update(
  const app_weather_snapshot_t *weather,
  bool mark_weather_stale,
  const char *status_text
)
{
  if (s_runtime_mutex == NULL) {
    return;
  }

  if (xSemaphoreTake(s_runtime_mutex, pdMS_TO_TICKS(RUNTIME_LOCK_TIMEOUT_MS)) != pdTRUE) {
    ESP_LOGW(TAG, "Timed out waiting for runtime state lock");
    return;
  }

  if (weather != NULL) {
    s_runtime_state.weather = *weather;
  }

  if (mark_weather_stale && s_runtime_state.weather.valid) {
    s_runtime_state.weather.stale = true;
  }

  copy_text(s_runtime_state.weather_status, sizeof(s_runtime_state.weather_status), status_text);
  xSemaphoreGive(s_runtime_mutex);
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
}

static bool runtime_state_has_valid_weather(void)
{
  bool has_valid_weather = false;

  if (s_runtime_mutex == NULL) {
    return false;
  }

  if (xSemaphoreTake(s_runtime_mutex, pdMS_TO_TICKS(RUNTIME_LOCK_TIMEOUT_MS)) != pdTRUE) {
    return false;
  }

  has_valid_weather = s_runtime_state.weather.valid;
  xSemaphoreGive(s_runtime_mutex);
  return has_valid_weather;
}

static esp_err_t request_clock_warning_dismiss(wac_time_warning_t warning)
{
  if (warning != WAC_TIME_WARNING_LOST_TIME && warning != WAC_TIME_WARNING_CONNECTION)
    return ESP_ERR_INVALID_ARG;
  if (s_clock_commands == NULL) return ESP_ERR_INVALID_STATE;
  return xQueueSend(s_clock_commands, &warning, 0) == pdTRUE
    ? ESP_OK : ESP_ERR_TIMEOUT;
}

static void clock_control_task(void *arg)
{
  (void) arg;
  esp_err_t last_rtc_error = ESP_OK;
  esp_err_t last_night_error = ESP_OK;
  while (true) {
    wac_time_warning_t warning;
    while (xQueueReceive(s_clock_commands, &warning, 0) == pdTRUE) {
      wac_time_dismiss_warning(warning);
    }
    wac_time_poll(connectivity_get_time_sync_generation());
    wac_time_snapshot_t clock;
    wac_time_snapshot(&clock);
    if (clock.rtc_error != last_rtc_error) {
      if (clock.rtc_error != ESP_OK) {
        ESP_LOGW(TAG, "Backup clock operation failed: %s", esp_err_to_name(clock.rtc_error));
      } else {
        ESP_LOGI(TAG, "Backup clock correction completed");
      }
      last_rtc_error = clock.rtc_error;
    }
    device_config_t config;
    if (device_config_copy(&config) == ESP_OK) {
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

  while (true) {
    vTaskDelay(delay_ticks);

    /* Static, not on this task's 4 KB stack: the runtime state carries the
     * full weather snapshot including the conditions summary. Only this
     * task touches it. */
    static app_runtime_state_t runtime_snapshot;
    device_config_t config;
    if (device_config_copy(&config) != ESP_OK) {
      continue;
    }
    runtime_state_snapshot(&runtime_snapshot, &config);

    if (bsp_display_lock(UI_HEARTBEAT_LOCK_TIMEOUT_MS) == ESP_OK) {
      /* The message owner keeps completion until the UI can accept it. */
      esp_err_t message_result;
      if (message_service_take_acknowledgment_result(&message_result)) {
        app_ui_complete_message_acknowledgment(message_result);
      }
      app_ui_update_runtime(&config, &runtime_snapshot);
      bsp_display_unlock();
    }
  }
}

static void weather_service_wait(int64_t weather_due_us, int64_t time_retry_due_us)
{
  if (time_retry_due_us > 0 && time_retry_due_us < weather_due_us) {
    weather_due_us = time_retry_due_us;
  }
  int64_t remaining_us = weather_due_us - esp_timer_get_time();
  if (remaining_us > 0) {
    TickType_t ticks = pdMS_TO_TICKS((remaining_us + 999) / 1000);
    vTaskDelay(ticks > 0 ? ticks : 1);
  }
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

  while (true) {
    device_config_t config;
    if (device_config_copy(&config) != ESP_OK) {
      vTaskDelay(retry_delay_ticks);
      continue;
    }

    if (!config.wifi_ready) {
      if (!reported_missing_wifi_profile) {
        ESP_LOGI(TAG, "No stored Wi-Fi profile is available yet; waiting for setup");
        reported_missing_wifi_profile = true;
      }

      runtime_state_update(
        NULL,
        false,
        "No Wi-Fi profile found. Tap 'Set Up Wi-Fi' to connect."
      );
      vTaskDelay(retry_delay_ticks);
      continue;
    }

    reported_missing_wifi_profile = false;

    if (!connectivity_started) {
      ESP_LOGI(TAG, "Initializing Wi-Fi station for SSID '%s'", config.wifi_ssid);
      runtime_state_update(
        NULL,
        false,
        "Connecting to Wi-Fi..."
      );

      esp_err_t err = connectivity_init(&config);
      if (err != ESP_OK) {
        ESP_LOGW(TAG, "Wi-Fi init failed: %s", esp_err_to_name(err));
        runtime_state_update(
          NULL,
          true,
          "Wi-Fi setup failed. Retrying..."
        );
        vTaskDelay(retry_delay_ticks);
        continue;
      }

      connectivity_started = true;
    }

    if (!connectivity_is_wifi_connected()) {
      ESP_LOGI(TAG, "Waiting up to %u ms for Wi-Fi connection", WIFI_CONNECT_TIMEOUT_MS);
      runtime_state_update(
        NULL,
        true,
        "Connecting to Wi-Fi..."
      );

      esp_err_t err = connectivity_wait_for_wifi(wifi_timeout_ticks);
      if (err != ESP_OK) {
        ESP_LOGW(TAG, "Wi-Fi connect timeout: %s", esp_err_to_name(err));
        runtime_state_update(
          NULL,
          true,
          "Still trying to join Wi-Fi..."
        );
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
      runtime_state_update(
        NULL,
        false,
        "Set your location to enable local weather."
      );
      vTaskDelay(retry_delay_ticks);
      continue;
    }

    if (esp_timer_get_time() < next_weather_attempt_us) {
      weather_service_wait(next_weather_attempt_us, time_retry_due_us);
      continue;
    }

    runtime_state_update(NULL, false, "Fetching live weather...");
    ESP_LOGI(
      TAG,
      "Requesting live weather for %s,%s (%s units)",
      config.latitude,
      config.longitude,
      config.units
    );

    app_weather_snapshot_t weather_snapshot = {0};
    esp_err_t weather_err = weather_client_fetch(&config, &weather_snapshot);
    if (weather_err != ESP_OK) {
      bool had_previous_weather = runtime_state_has_valid_weather();
      ESP_LOGW(TAG, "Weather refresh failed: %s", esp_err_to_name(weather_err));
      runtime_state_update(
        NULL,
        true,
        had_previous_weather
          ? "Weather refresh failed. Showing last update until retry."
          : "Weather request failed. Retrying..."
      );
      next_weather_attempt_us = esp_timer_get_time() + (int64_t) WEATHER_RETRY_INTERVAL_MS * 1000;
      weather_service_wait(next_weather_attempt_us, time_retry_due_us);
      continue;
    }

    runtime_state_update(&weather_snapshot, false, "");
    ESP_LOGI(
      TAG,
      "Live weather updated (%d°, H:%d° L:%d°)",
      weather_snapshot.temp,
      weather_snapshot.high,
      weather_snapshot.low
    );
    next_weather_attempt_us = esp_timer_get_time() + (int64_t) WEATHER_REFRESH_INTERVAL_MS * 1000;
    weather_service_wait(next_weather_attempt_us, time_retry_due_us);
  }
}

void app_main(void)
{
  /* Storage failure stops dependent startup; never erase user data to repair it. */
  esp_err_t storage_err = nvs_flash_init();
  if (storage_err == ESP_OK) {
    storage_err = device_config_init();
  }
  if (storage_err != ESP_OK) {
    ESP_LOGE(TAG, "Settings startup failed: %s", esp_err_to_name(storage_err));
    if (bsp_display_start() != NULL) {
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
  ESP_ERROR_CHECK(wac_time_init(config.timezone));
  log_device_clock("Clock after boot timezone apply");
  wac_time_snapshot_t startup_clock;
  wac_time_snapshot(&startup_clock);
  esp_err_t night_err = wac_night_init(&config, &startup_clock);
  if (night_err != ESP_OK) {
    ESP_LOGW(TAG, "Night Shift startup returned %s", esp_err_to_name(night_err));
  }

  s_runtime_mutex = xSemaphoreCreateMutex();
  if (s_runtime_mutex == NULL) {
    ESP_LOGE(TAG, "Failed to allocate runtime mutex");
    return;
  }
  runtime_state_seed(&config);
  s_clock_commands = xQueueCreate(2, sizeof(wac_time_warning_t));
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

  lv_display_t *display = bsp_display_start();
  if (display == NULL) {
    ESP_LOGE(TAG, "Display start failed");
    return;
  }
  (void) display;

  bool touch_ready = (bsp_display_get_input_dev() != NULL);

  /* Gather owner data before entering the display lock. */
  static app_runtime_state_t startup_runtime;
  runtime_state_snapshot(&startup_runtime, &config);

  esp_err_t lock_err = bsp_display_lock(INITIAL_UI_LOCK_TIMEOUT_MS);
  if (lock_err != ESP_OK) {
    ESP_LOGE(TAG, "Could not lock LVGL during initial draw: %s", esp_err_to_name(lock_err));
    return;
  }

  app_ui_set_clock_warning_dismiss_handler(request_clock_warning_dismiss);
  ESP_ERROR_CHECK(app_ui_show_boot_screen(&config, &startup_runtime, touch_ready));
  bsp_display_unlock();

  /* The clock task advances the configured fade after the display is ready. */
  wac_night_start_backlight();

  ESP_LOGI(TAG, "Display ready, touch %s", touch_ready ? "detected" : "not detected");

  BaseType_t ui_task_ok = xTaskCreate(
    ui_heartbeat_task,
    "rwd_ui_heartbeat",
    4096,
    NULL,
    4,
    NULL
  );
  if (ui_task_ok != pdPASS) {
    ESP_LOGW(TAG, "Failed to start UI heartbeat task");
  }

  BaseType_t weather_task_ok = xTaskCreate(
    weather_service_task,
    "rwd_weather",
    8192,
    NULL,
    4,
    NULL
  );
  if (weather_task_ok != pdPASS) {
    ESP_LOGW(TAG, "Failed to start weather service task");
  }
}

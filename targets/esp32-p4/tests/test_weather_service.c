#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_support.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "cJSON.h"
#include "app_runtime.h"
#include "connectivity.h"
#include "weather_client.h"

/* Controlled provider and platform responses exercise the actual decoder and
 * worker. These examples do not establish live network or board behavior. */
static const char provider_response[] =
  "{\"current_weather\":{\"temperature\":68,\"weathercode\":0,\"is_day\":1,"
  "\"time\":\"2026-12-30T12:00\",\"windspeed\":10,\"winddirection\":180},"
  "\"daily\":{\"time\":[\"2026-12-30\",\"2026-12-31\",\"2027-01-01\","
  "\"2027-01-02\",\"2027-01-03\",\"2027-01-04\"],"
  "\"temperature_2m_max\":[80,71,62,53,24,45],"
  "\"temperature_2m_min\":[55,61,52,43,14,35],"
  "\"weathercode\":[3,0,45,61,73,95]},"
  "\"hourly\":{\"time\":[\"2026-12-31T08:00\",\"2026-12-31T09:00\","
  "\"2026-12-31T10:00\",\"2027-01-01T08:00\",\"2027-01-01T09:00\","
  "\"2027-01-01T10:00\"],\"weathercode\":[2,2,2,45,45,45],"
  "\"cloud_cover\":[50,50,50,90,90,90]}}";

/* Current-hour rows plus the original upcoming-day rows preserve the forecast
 * checks while supplying a storm at 13:00 and peak gusts at 14:00. */
static const char conditions_hourly[] =
  "{\"time\":[\"2026-12-30T12:00\",\"2026-12-30T13:00\","
  "\"2026-12-30T14:00\",\"2026-12-30T15:00\",\"2026-12-30T16:00\","
  "\"2026-12-30T17:00\",\"2026-12-30T18:00\","
  "\"2026-12-31T08:00\",\"2026-12-31T09:00\",\"2026-12-31T10:00\","
  "\"2027-01-01T08:00\",\"2027-01-01T09:00\",\"2027-01-01T10:00\"],"
  "\"weathercode\":[0,95,0,0,0,0,0,2,2,2,45,45,45],"
  "\"cloud_cover\":[0,90,0,0,0,0,0,50,50,50,90,90,90],"
  "\"wind_speed_10m\":[10,10,10,10,10,10,10],"
  "\"wind_gusts_10m\":[10,20,80,20,10,10,10],"
  "\"pressure_msl\":[1013.25,1013.25,1013.25,1013.25,1013.25,1013.25,1013.25],"
  "\"temperature_2m\":[68,68,68,68,68,68,68],"
  "\"precipitation\":[0,0,0,0,0,0,0],"
  "\"precipitation_probability\":[0,0,0,0,0,0,0],"
  "\"snowfall\":[0,0,0,0,0,0,0]}";

static char *make_conditions_response(void)
{
  cJSON *root = cJSON_Parse(provider_response);
  cJSON *hourly = cJSON_Parse(conditions_hourly);
  assert(root && hourly);
  assert(cJSON_ReplaceItemInObjectCaseSensitive(root, "hourly", hourly));
  char *body = cJSON_PrintUnformatted(root);
  assert(body);
  cJSON_Delete(root);
  return body;
}

static device_config_t applied_config;
static const char *http_body;
static esp_http_client_config_t http_config;
static unsigned http_requests, http_cleanups;
static TickType_t request_ticks[16], sync_ticks[64];
static unsigned request_durations_ms[16];
static esp_err_t request_results[16];
static int http_status;
static unsigned sync_calls, sync_failures, wifi_init_calls, clock_polls;
static bool wifi_connected, actual_time_synced;
static unsigned delay_count, delay_limit;
static TickType_t delays[64];
static jmp_buf worker_finished;
static void (*on_delay)(void);

esp_err_t device_config_copy(device_config_t *out)
{
  assert(out);
  *out = applied_config;
  return ESP_OK;
}

esp_err_t connectivity_init(const device_config_t *config)
{
  assert(config->wifi_ready);
  ++wifi_init_calls;
  return ESP_OK;
}

bool connectivity_is_wifi_connected(void) { return wifi_connected; }
bool connectivity_is_time_synced(void) { return actual_time_synced; }

esp_err_t connectivity_wait_for_wifi(TickType_t timeout)
{
  assert(timeout == pdMS_TO_TICKS(30000));
  wifi_connected = true;
  return ESP_OK;
}

esp_err_t connectivity_sync_time(TickType_t timeout)
{
  /* Starting transport must not wait for the network-time reply. A timeout
   * here means transport is running, with actual completion still pending. */
  assert(timeout == 0);
  assert(sync_calls < sizeof(sync_ticks) / sizeof(sync_ticks[0]));
  sync_ticks[sync_calls++] = xTaskGetTickCount();
  if (sync_calls <= sync_failures) return ESP_FAIL;
  return actual_time_synced ? ESP_OK : ESP_ERR_TIMEOUT;
}

void wac_time_poll(uint32_t generation)
{
  (void) generation;
  ++clock_polls;
}

int64_t esp_timer_get_time(void)
{
  return (int64_t) xTaskGetTickCount() * 1000;
}

const char *esp_err_to_name(esp_err_t err)
{
  return err == ESP_OK ? "ESP_OK" : "controlled error";
}

esp_err_t esp_crt_bundle_attach(void *config)
{
  (void) config;
  return ESP_OK;
}

esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *config)
{
  assert(config && config->event_handler && config->user_data);
  assert(strstr(config->url, "https://api.open-meteo.com/v1/forecast?"));
  assert(strstr(config->url, "timezone=America%2FChicago"));
  assert(strstr(config->url, "forecast_days=6"));
  assert(config->timeout_ms == 10000);
  http_config = *config;
  return &http_config;
}

esp_err_t esp_http_client_perform(esp_http_client_handle_t handle)
{
  assert(handle == &http_config);
  assert(http_requests < sizeof(request_ticks) / sizeof(request_ticks[0]));
  request_ticks[http_requests] = xTaskGetTickCount();
  fake_clock_advance(request_durations_ms[http_requests]);
  esp_err_t result = request_results[http_requests++];
  if (result != ESP_OK) return result;
  /* Deliberately split the body, exercising the production response buffer. */
  size_t length = strlen(http_body), split = length / 3;
  esp_http_client_event_t event = {
    .event_id = HTTP_EVENT_ON_DATA,
    .data = (void *) http_body,
    .data_len = (int) split,
    .user_data = http_config.user_data,
  };
  result = http_config.event_handler(&event);
  if (result != ESP_OK) return result;
  event.data = (void *) (http_body + split);
  event.data_len = (int) (length - split);
  return http_config.event_handler(&event);
}

int esp_http_client_get_status_code(esp_http_client_handle_t handle)
{
  assert(handle == &http_config);
  return http_status;
}

esp_err_t esp_http_client_cleanup(esp_http_client_handle_t handle)
{
  assert(handle == &http_config);
  ++http_cleanups;
  return ESP_OK;
}

void vTaskDelay(TickType_t ticks)
{
  assert(ticks > 0);
  assert(delay_count < sizeof(delays) / sizeof(delays[0]));
  delays[delay_count++] = ticks;
  if (on_delay) on_delay();
  if (delay_count == delay_limit) longjmp(worker_finished, 1);
  fake_clock_advance(ticks);
}

/* Generated from main.c by the native build. The decoder is separately
 * compiled from weather_client.c; neither production implementation is copied
 * into this maintained test. */
#include "weather_service_task.inc"

static void reset_platform(void)
{
  fake_clock_reset(1798674600); /* 2026-12-30 23:50 UTC: crosses midnight during refresh. */
  memset(&applied_config, 0, sizeof(applied_config));
  applied_config.wifi_ready = applied_config.location_ready = true;
  strcpy(applied_config.wifi_ssid, "controlled-network");
  strcpy(applied_config.latitude, "41.8");
  strcpy(applied_config.longitude, "-87.6");
  strcpy(applied_config.timezone, "America/Chicago");
  strcpy(applied_config.units, "imperial");
  strcpy(applied_config.time_format, "12");
  http_body = provider_response;
  http_status = 200;
  memset(request_results, 0, sizeof(request_results));
  memset(request_durations_ms, 0, sizeof(request_durations_ms));
  memset(request_ticks, 0, sizeof(request_ticks));
  memset(sync_ticks, 0, sizeof(sync_ticks));
  memset(delays, 0, sizeof(delays));
  http_requests = http_cleanups = sync_calls = sync_failures = wifi_init_calls = 0;
  delay_count = clock_polls = 0;
  wifi_connected = true;
  actual_time_synced = false;
  on_delay = NULL;
  memset(&s_runtime_state, 0, sizeof(s_runtime_state));
}

static void assert_forecast(const app_weather_snapshot_t *weather)
{
  const char *dates[] = { "2026-12-31", "2027-01-01", "2027-01-02", "2027-01-03", "2027-01-04" };
  const int highs[] = {71,62,53,24,45}, lows[] = {61,52,43,14,35};
  const char *icons[] = { "partlycloudy-day", "fog", "rain", "snow", "thunderstorm" };
  assert(weather->valid && weather->temp == 68 && weather->high == 80 && weather->low == 55);
  for (int i = 0; i < APP_FORECAST_DAYS; ++i) {
    assert(strcmp(weather->forecast[i].date, dates[i]) == 0);
    assert(weather->forecast[i].high == highs[i] && weather->forecast[i].low == lows[i]);
    assert(strcmp(weather->forecast[i].icon_name, icons[i]) == 0);
  }
}

static void check_decoder(void)
{
  reset_platform();
  fake_clock_set(0); /* A clock awaiting its first trustworthy time. */
  app_weather_snapshot_t weather = {0};
  assert(weather_client_fetch(&applied_config, &weather) == ESP_OK);
  assert_forecast(&weather);
  assert(weather.updated_at == 0 && http_requests == 1 && http_cleanups == 1);
  assert(!weather.has_conditions);
  assert(weather.conditions_alternate_headline[0] == '\0');
  assert(weather.conditions_alternate_detail[0] == '\0');

  const char *dates[] = { "", "bad", "2026-02-30", "2026-13-01", "2026-12-31-extra" };
  for (size_t i = 0; i < sizeof(dates) / sizeof(dates[0]); ++i) {
    cJSON *root = cJSON_Parse(provider_response);
    assert(root);
    cJSON *daily = cJSON_GetObjectItemCaseSensitive(root, "daily");
    cJSON *times = cJSON_GetObjectItemCaseSensitive(daily, "time");
    assert(cJSON_ReplaceItemInArray(times, 1, cJSON_CreateString(dates[i])));
    char *body = cJSON_PrintUnformatted(root);
    assert(body);
    http_body = body;
    assert(weather_client_fetch(&applied_config, &weather) == ESP_OK);
    assert(weather.valid && weather.temp == 68);
    assert(weather.forecast[0].high == 71 && weather.forecast[0].low == 61);
    /* The existing hourly heuristic matches its ten-character date prefix;
     * retaining a date must not silently change that calculation. */
    bool matching_prefix = strncmp(dates[i], "2026-12-31", 10) == 0;
    assert(strcmp(weather.forecast[0].icon_name, matching_prefix ? "partlycloudy-day" : "clear-day") == 0);
    assert(strcmp(weather.forecast[0].date, strlen(dates[i]) == 10 ? dates[i] : "") == 0);
    free(body);
    cJSON_Delete(root);
  }

  cJSON *root = cJSON_Parse(provider_response);
  assert(root);
  cJSON_DeleteItemFromObjectCaseSensitive(cJSON_GetObjectItemCaseSensitive(root, "daily"), "time");
  char *body = cJSON_PrintUnformatted(root);
  assert(body);
  http_body = body;
  assert(weather_client_fetch(&applied_config, &weather) == ESP_OK);
  for (int i = 0; i < APP_FORECAST_DAYS; ++i) assert(weather.forecast[i].date[0] == '\0');
  assert(weather.forecast[0].high == 71 && weather.forecast[4].low == 35);
  free(body);
  cJSON_Delete(root);
}

static void assert_timed_conditions(const app_weather_snapshot_t *weather, bool time_24h)
{
  assert_forecast(weather);
  assert(weather->has_conditions && weather->conditions_time_24h == time_24h);
  const storm_conditions_t *conditions = &weather->conditions;
  assert(strcmp(conditions->headline, time_24h
    ? "Thunderstorms likely by 13:00" : "Thunderstorms likely by 1 PM") == 0);
  assert(conditions->has_detail);
  assert(strcmp(conditions->detail, time_24h
    ? "Gusts up to 50 mph by 14:00" : "Gusts up to 50 mph by 2 PM") == 0);
  assert(strcmp(weather->conditions_alternate_headline, time_24h
    ? "Thunderstorms likely by 1 PM" : "Thunderstorms likely by 13:00") == 0);
  assert(strcmp(weather->conditions_alternate_detail, time_24h
    ? "Gusts up to 50 mph by 2 PM" : "Gusts up to 50 mph by 14:00") == 0);
  assert(conditions->has_speed && conditions->speed == 6);
  assert(!conditions->has_gust && conditions->gust == 0); /* Gust equals live wind. */
  assert(conditions->has_peak_gust && conditions->peak_gust == 50);
  assert(conditions->has_direction && conditions->direction == 180);
  assert(strcmp(conditions->cardinal, "S") == 0);
  assert(conditions->has_pressure && conditions->pressure_value == 29.92);
  assert(conditions->has_pressure_change && conditions->pressure_change == 0);
  assert(strcmp(conditions->pressure_trend, "steady") == 0);
  assert(conditions->has_precip_now && conditions->precip_now == 0);
  assert(conditions->has_precip_outlook && conditions->precip_outlook == 0);
  assert(conditions->alert && conditions->changing && conditions->alert_count == 2);
  assert(conditions->alerts[0].kind == STORM_ALERT_THUNDER);
  assert(strcmp(conditions->alerts[0].label, "Thunderstorms") == 0);
  assert(conditions->alerts[1].kind == STORM_ALERT_GUSTS);
  assert(strcmp(conditions->alerts[1].label, "Strong gusts") == 0);
}

static void check_conditions_decoder(void)
{
  for (int format = 0; format < 2; ++format) {
    reset_platform();
    bool time_24h = format != 0;
    strcpy(applied_config.time_format, time_24h ? "24" : "12");
    char *body = make_conditions_response();
    http_body = body;
    app_weather_snapshot_t weather;
    memset(&weather, 0xa5, sizeof(weather));
    assert(weather_client_fetch(&applied_config, &weather) == ESP_OK);
    assert(http_requests == 1 && http_cleanups == 1);
    assert_timed_conditions(&weather, time_24h);

    /* Compare the entire normal output with the existing calculation once,
     * in addition to the independent expected captions/values above. */
    cJSON *root = cJSON_Parse(body);
    assert(root);
    storm_conditions_config_t config = {.metric = false, .time_24h = time_24h};
    storm_conditions_t normal;
    assert(storm_conditions_build(&config,
      cJSON_GetObjectItemCaseSensitive(root, "current_weather"),
      cJSON_GetObjectItemCaseSensitive(root, "hourly"),
      cJSON_GetObjectItemCaseSensitive(root, "daily"), &normal));
    assert(memcmp(&weather.conditions, &normal, sizeof(normal)) == 0);

    /* Removing the gust alert leaves a timed headline with no detail. */
    cJSON_DeleteItemFromObjectCaseSensitive(
      cJSON_GetObjectItemCaseSensitive(root, "hourly"), "wind_gusts_10m");
    char *single_alert_body = cJSON_PrintUnformatted(root);
    assert(single_alert_body);
    http_body = single_alert_body;
    assert(weather_client_fetch(&applied_config, &weather) == ESP_OK);
    assert(weather.has_conditions && !weather.conditions.has_detail);
    assert(weather.conditions.detail[0] == '\0');
    assert(weather.conditions_alternate_detail[0] == '\0');
    assert(strcmp(weather.conditions_alternate_headline, time_24h
      ? "Thunderstorms likely by 1 PM" : "Thunderstorms likely by 13:00") == 0);
    free(single_alert_body);
    cJSON_Delete(root);
    free(body);

    /* An unavailable current hour clears both previously populated forms. */
    http_body = provider_response;
    assert(weather_client_fetch(&applied_config, &weather) == ESP_OK);
    assert(!weather.has_conditions);
    assert(weather.conditions.headline[0] == '\0');
    assert(weather.conditions_alternate_headline[0] == '\0');
    assert(weather.conditions_alternate_detail[0] == '\0');
    assert(http_requests == 3 && http_cleanups == 3);
  }
}

static void run_worker(unsigned stop_after_delays)
{
  delay_limit = stop_after_delays;
  if (setjmp(worker_finished) == 0) weather_service_task(NULL);
  assert(delay_count == stop_after_delays && wifi_init_calls == 1);
  assert(http_requests == http_cleanups);
  assert(clock_polls == 0); /* Actual synchronization belongs to clock_control_task. */
}

static void check_pending_time(void)
{
  reset_platform();
  fake_clock_set(0);
  wifi_connected = false; /* Joining Wi-Fi also must not add a time prerequisite. */
  run_worker(2);
  assert(http_requests == 2 && request_ticks[0] == 0 && request_ticks[1] == 600000);
  assert(delays[0] == 600000 && delays[1] == 600000);
  assert(!actual_time_synced && sync_calls >= 1);
  assert_forecast(&s_runtime_state.weather);
  assert(!s_runtime_state.weather.stale);
}

static void check_time_start_failure(void)
{
  reset_platform();
  sync_failures = 2;
  run_worker(4);
  assert(sync_calls >= 3 && sync_ticks[0] == 0 && sync_ticks[1] == 20000 && sync_ticks[2] == 40000);
  assert(http_requests == 2 && request_ticks[0] == 0 && request_ticks[1] == 600000);
  assert(delays[0] == 20000 && delays[1] == 20000 && delays[2] == 560000);
  assert_forecast(&s_runtime_state.weather);
  assert(!s_runtime_state.weather.stale && !actual_time_synced);
}

static void check_location_required(void)
{
  reset_platform();
  applied_config.location_ready = false;
  run_worker(2);
  assert(http_requests == 0 && !s_runtime_state.weather.valid);
  assert(delays[0] == 20000 && delays[1] == 20000);
}

static void check_slow_weather_and_time_retry(void)
{
  reset_platform();
  sync_failures = 2;
  request_durations_ms[0] = 10000; /* The client's existing HTTP timeout. */
  run_worker(4);
  assert(sync_ticks[0] == 0 && sync_ticks[1] == 20000 && sync_ticks[2] == 40000);
  assert(http_requests == 2 && request_ticks[0] == 0 && request_ticks[1] == 610000);
  assert(delays[0] == 10000 && delays[1] == 20000 && delays[2] == 570000);
  assert_forecast(&s_runtime_state.weather);
}

static void check_weather_failure(void)
{
  reset_platform();
  sync_failures = 2;
  request_results[0] = ESP_ERR_TIMEOUT;
  run_worker(4);
  assert(sync_ticks[0] == 0 && sync_ticks[1] == 20000 && sync_ticks[2] == 40000);
  assert(http_requests == 3 && request_ticks[0] == 0 && request_ticks[1] == 20000 && request_ticks[2] == 620000);
  assert(delays[0] == 20000 && delays[1] == 20000 && delays[2] == 580000);
  assert_forecast(&s_runtime_state.weather);
}

static void check_overlapping_failure_deadlines(void)
{
  reset_platform();
  sync_failures = 3;
  request_results[0] = ESP_ERR_TIMEOUT;
  request_durations_ms[0] = 10000;
  /* A slow failed weather request puts its retry at 30s, between time-start
   * retries at 20s and 40s. Waking for weather must preserve the latter. */
  run_worker(6);
  const TickType_t expected_sync[] = {0,20000,40000,60000,630000};
  const TickType_t expected_requests[] = {0,30000,630000};
  const TickType_t expected_delays[] = {10000,10000,10000,20000,570000,600000};
  assert(sync_calls == sizeof(expected_sync) / sizeof(expected_sync[0]));
  assert(http_requests == sizeof(expected_requests) / sizeof(expected_requests[0]));
  assert(memcmp(sync_ticks, expected_sync, sizeof(expected_sync)) == 0);
  assert(memcmp(request_ticks, expected_requests, sizeof(expected_requests)) == 0);
  assert(memcmp(delays, expected_delays, sizeof(expected_delays)) == 0);
  assert_forecast(&s_runtime_state.weather);
  assert(!s_runtime_state.weather.stale && !actual_time_synced);
}

static app_weather_snapshot_t retained_weather;

static void observe_retention(void)
{
  if (delay_count == 1) {
    retained_weather = s_runtime_state.weather;
    assert_forecast(&retained_weather);
    if (retained_weather.has_conditions) assert_timed_conditions(&retained_weather, false);
  } else if (delay_count == 2) {
    assert(s_runtime_state.weather.stale);
    app_weather_snapshot_t stale = s_runtime_state.weather;
    stale.stale = false;
    assert(memcmp(&stale, &retained_weather, sizeof(stale)) == 0);
  }
}

static void check_retained_weather(void)
{
  reset_platform();
  request_results[1] = ESP_ERR_TIMEOUT;
  on_delay = observe_retention;
  run_worker(3);
  assert(http_requests == 3 && request_ticks[1] == 600000 && request_ticks[2] == 620000);
  assert(delays[0] == 600000 && delays[1] == 20000);
  assert_forecast(&s_runtime_state.weather);
  assert(!s_runtime_state.weather.stale);
}

static void check_retained_conditions(void)
{
  reset_platform();
  char *body = make_conditions_response();
  http_body = body;
  request_results[1] = ESP_ERR_TIMEOUT;
  on_delay = observe_retention;
  run_worker(2);
  assert(http_requests == 2 && http_cleanups == 2);
  assert(request_ticks[0] == 0 && request_ticks[1] == 600000);
  assert(delays[0] == 600000 && delays[1] == 20000);
  assert(s_runtime_state.weather.stale);
  assert_timed_conditions(&s_runtime_state.weather, false);
  free(body);
}

int main(void)
{
  s_runtime_mutex = xSemaphoreCreateMutex();
  assert(s_runtime_mutex);
  check_decoder();
  check_conditions_decoder();
  check_pending_time();
  check_time_start_failure();
  check_location_required();
  check_slow_weather_and_time_retry();
  check_weather_failure();
  check_overlapping_failure_deadlines();
  check_retained_weather();
  check_retained_conditions();
  assert(pthread_mutex_destroy(s_runtime_mutex) == 0);
  free(s_runtime_mutex);
  puts("PASS: actual weather decoding, provider date/value/icon pairing, invalid dates, independent network-time/weather retries, refresh intervals and retained payload.");
  return 0;
}

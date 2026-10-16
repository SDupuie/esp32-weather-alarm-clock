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
#include "weather_icon_names.h"

/* Compile the actual decoder here so its private normalization can be checked
 * without a firmware test interface or a copied decoding implementation. */
static size_t radar_fail_alloc_size;
static unsigned radar_failed_allocations;
static void *radar_test_malloc(size_t size)
{
  if (radar_fail_alloc_size && size==radar_fail_alloc_size) {
    radar_fail_alloc_size=0;
    ++radar_failed_allocations;
    return NULL;
  }
  return malloc(size);
}
#define malloc radar_test_malloc
#define TAG native_weather_client_tag
#include "../main/weather_client.c"
#undef TAG
#undef malloc

_Static_assert(LODEPNG_MAX_ALLOC==RADAR_PNG_ALLOC_MAX,
  "Native LodePNG must enforce the firmware allocation cap");

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
static bool radar_http_test;
static const unsigned char *radar_base_png, *radar_echo_png;
static size_t radar_base_png_size, radar_echo_png_size;
static const char *radar_metadata_body;
static const char *radar_saved_directory, *radar_saved_sample;
static unsigned char *radar_saved_tile;
static unsigned radar_image_requests, radar_base_requests;
static unsigned char *radar_read_binary(const char *path, size_t *size);
static size_t http_binary_length;
static esp_http_client_config_t http_config;
static unsigned http_requests, http_cleanups;
static TickType_t request_ticks[256], sync_ticks[256];
static unsigned request_durations_ms[256];
static esp_err_t request_results[256];
static int http_status;
static const char *retry_header, *reset_header, *date_header;
static bool expect_max_encoded_key;
static pthread_mutex_t overlap_mutex=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t overlap_condition=PTHREAD_COND_INITIALIZER;
static bool block_http_once, http_blocked, http_released;
static unsigned sync_calls, sync_failures, wifi_init_calls, clock_polls;
static bool wifi_connected, actual_time_synced;
static wac_time_snapshot_t download_clock;
static esp_err_t wifi_init_result, wifi_wait_result;
static unsigned delay_count, delay_limit;
static TickType_t delays[256];
static TickType_t worker_stop_ms;
static jmp_buf worker_finished;
static void (*on_delay)(void);
static void (*on_request)(void);

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
  return wifi_init_result;
}

bool connectivity_is_wifi_connected(void) { return wifi_connected; }
bool connectivity_is_time_synced(void) { return actual_time_synced; }

esp_err_t connectivity_wait_for_wifi(TickType_t timeout)
{
  assert(timeout == pdMS_TO_TICKS(30000));
  if (wifi_wait_result == ESP_OK) wifi_connected = true;
  return wifi_wait_result;
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

void wac_time_snapshot(wac_time_snapshot_t *out) { *out = download_clock; }

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
  if (radar_http_test && config->url && !strstr(config->url,"api.open-meteo.com")) {
    assert(config->user_agent && strstr(config->user_agent,"ESP32-P4"));
    if (strstr(config->url,"weather-maps.json")) http_body=radar_metadata_body;
    else {
      bool base=strstr(config->url,"basemap.nationalmap.gov")!=NULL;
      assert(base || strstr(config->url,"tilecache.rainviewer.com"));
      assert(base || strstr(config->url,"/256/7/")!=NULL);
      assert(base || strstr(config->url,"/2/0_0.png")!=NULL);
      assert(!base || (strstr(config->url,"size=800,800") && strstr(config->url,"format=png32")));
      if (base) ++radar_base_requests;
      else if (radar_saved_directory) {
        int x=-1,y=-1;const char *tile=strstr(config->url,"/256/7/");
        assert(tile && sscanf(tile,"/256/7/%d/%d/2/0_0.png",&x,&y)==2);
        char path[1024];
        snprintf(path,sizeof(path),"%s/rainviewer-%s-z7-%d-%d.png",radar_saved_directory,radar_saved_sample,x,y);
        free(radar_saved_tile);
        radar_saved_tile=radar_read_binary(path,&radar_echo_png_size);
        radar_echo_png=radar_saved_tile;
      }
      ++radar_image_requests;
      http_body=(const char *)(base ? radar_base_png : radar_echo_png);
      http_binary_length=base ? radar_base_png_size : radar_echo_png_size;
    }
    assert(config->timeout_ms==(strstr(config->url,"basemap.nationalmap.gov") ? 30000 : 10000));
    http_config=*config;
    return &http_config;
  }
  if (applied_config.weather_provider == DEVICE_CONFIG_WEATHER_VISUAL_CROSSING || config->host) {
    assert(config->url == NULL);
    assert(strcmp(config->host, "weather.visualcrossing.com") == 0);
    assert(strstr(config->path, "/41.8,-87.6/next6days"));
    assert(strstr(config->query, "include=current,days,hours"));
    assert(strstr(config->query, "lang=id&iconSet=icons2"));
    assert(strstr(config->query, "snowdepth") == NULL);
    assert(config->disable_auto_redirect);
    assert(strstr(config->query,strcmp(applied_config.units,"metric")==0?"unitGroup=metric":"unitGroup=us"));
    if (strstr(config->query,"draft-test")) assert(strstr(config->query,"key=draft-test%2F%26%3F%3D"));
    if (expect_max_encoded_key) {
      const char *key=strstr(config->query,"&key=");assert(key);key+=5;
      assert(strlen(key)==3*(DEVICE_CONFIG_WEATHER_KEY_LEN-1));
      for (size_t i=0;i<strlen(key);i+=3) assert(strncmp(key+i,"%2F",3)==0);
    }
  } else {
    assert(strstr(config->url, "https://api.open-meteo.com/v1/forecast?"));
    assert(strstr(config->url, "timezone=America%2FChicago"));
    assert(strstr(config->url, "forecast_days=6"));
    assert(strstr(config->url, "current=rain,showers,snowfall"));
  }
  assert(config->timeout_ms == 10000);
  http_config = *config;
  return &http_config;
}

esp_err_t esp_http_client_perform(esp_http_client_handle_t handle)
{
  assert(handle == &http_config);
  if (on_request) on_request();
  pthread_mutex_lock(&overlap_mutex);
  if (block_http_once) {
    block_http_once=false; http_blocked=true;
    pthread_cond_broadcast(&overlap_condition);
    while (!http_released) pthread_cond_wait(&overlap_condition,&overlap_mutex);
  }
  pthread_mutex_unlock(&overlap_mutex);
  assert(http_requests < sizeof(request_ticks) / sizeof(request_ticks[0]));
  request_ticks[http_requests] = xTaskGetTickCount();
  fake_clock_advance(request_durations_ms[http_requests]);
  esp_err_t result = request_results[http_requests++];
  if (result != ESP_OK) return result;
  const char *header_values[] = {retry_header, reset_header, date_header};
  const char *header_names[] = {"Retry-After", "X-RateLimit-Reset", "Date"};
  for (int i=0; i<3; ++i) if (header_values[i]) {
    esp_http_client_event_t header = {.event_id=HTTP_EVENT_ON_HEADER,
      .header_key=(char *)header_names[i], .header_value=(char *)header_values[i],
      .user_data=http_config.user_data};
    assert(http_config.event_handler(&header)==ESP_OK);
  }
  /* Deliberately split the body, exercising the production response buffer. */
  size_t length = http_binary_length ? http_binary_length : strlen(http_body), split = length / 3;
  http_binary_length=0;
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
  if ((delay_limit && delay_count == delay_limit) ||
      (worker_stop_ms && xTaskGetTickCount() >= worker_stop_ms))
    longjmp(worker_finished, 1);
  fake_clock_advance(ticks);
}

/* Worker scheduling uses a narrow step substitute for bounded-duration Radar
 * work. The actual client, HTTP/PNG path and slot barrier are checked below;
 * the worker itself is always extracted from production main.c. */
static bool worker_radar_step(const device_config_t *, weather_radar_step_t, uint32_t,
                              app_radar_snapshot_t *, bool *);
#define weather_client_radar_step worker_radar_step
/* Generated from main.c by the native build. The decoder is separately
 * compiled from weather_client.c; neither production implementation is copied
 * into this maintained test. */
#include "weather_service_task.inc"
#undef weather_client_radar_step

enum { RADAR_REAL_CLIENT, RADAR_IDLE_STEPS, RADAR_BUSY_STEPS, RADAR_PENDING_COPY,
       RADAR_RETAINED_FRAME, RADAR_INTERRUPTED_FRAME };
static const uint8_t worker_radar_pixel[4] = {20, 30, 40, 255};
static unsigned worker_radar_mode, worker_radar_calls, worker_radar_starts;
static unsigned worker_radar_previous_delay;
static TickType_t worker_radar_ticks[256], worker_radar_start_ticks[256];
static uint32_t worker_radar_acknowledged[256];
static weather_radar_step_t worker_radar_actions[256];
static SemaphoreHandle_t held_runtime_mutex;

static bool worker_radar_step(const device_config_t *config, weather_radar_step_t action, uint32_t displayed,
                              app_radar_snapshot_t *out, bool *active)
{
  if (worker_radar_mode == RADAR_REAL_CLIENT)
    return weather_client_radar_step(config, action, displayed, out, active);
  assert(worker_radar_calls < 256 && worker_radar_starts < 256);
  if (worker_radar_calls) assert(delay_count > worker_radar_previous_delay);
  worker_radar_previous_delay = delay_count;
  worker_radar_ticks[worker_radar_calls] = xTaskGetTickCount();
  worker_radar_actions[worker_radar_calls] = action;
  worker_radar_acknowledged[worker_radar_calls++] = displayed;
  if (action==WEATHER_RADAR_CHECK) worker_radar_start_ticks[worker_radar_starts++] = xTaskGetTickCount();
  if (worker_radar_mode == RADAR_RETAINED_FRAME ||
      worker_radar_mode == RADAR_INTERRUPTED_FRAME) {
    /* Constructed client replies only: production owns when CHECK/OFFLINE is
     * called. Actual client preservation and cancellation are tested below. */
    if (action == WEATHER_RADAR_OFFLINE) {
      *active = false;
      if (out->stale) return false;
      out->stale = true;
      ++out->generation;
      return true;
    }
    assert(action == WEATHER_RADAR_CHECK);
    *out = (app_radar_snapshot_t){ .valid = true, .generation = 1,
      .frame_time = 1700000000, .pixels = worker_radar_pixel };
    *active = worker_radar_mode == RADAR_INTERRUPTED_FRAME;
    return true;
  }
  if (action==WEATHER_RADAR_OFFLINE) { *active=false; return false; }
  *active = worker_radar_mode != RADAR_IDLE_STEPS;
  if (worker_radar_mode == RADAR_BUSY_STEPS) {
    /* A near-timeout request per turn represents a slow image preparation.
     * Work due at this boundary must already have run before the next step. */
    TickType_t now = xTaskGetTickCount();
    if (now == 20000 || now == 40000) assert(sync_ticks[sync_calls - 1] == now);
    if (now == 600000) assert(http_requests == 2 && request_ticks[1] == now);
    fake_clock_advance(9900);
  }
  if (worker_radar_mode == RADAR_PENDING_COPY && worker_radar_calls == 1) {
    *out = (app_radar_snapshot_t){ .generation = 77, .frame_time = 1700000000 };
    /* A missing runtime mutex makes the actual publication helper decline
     * the copy, without changing native mutex timeout behavior. */
    held_runtime_mutex = s_runtime_mutex;
    s_runtime_mutex = NULL;
    return true;
  }
  if (worker_radar_mode == RADAR_PENDING_COPY) {
    assert(s_runtime_state.radar.generation == 77);
    assert(s_runtime_state.radar.frame_time == 1700000000);
  }
  return false;
}

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
  assert(weather_client_init() == ESP_OK);
  http_body = provider_response;
  http_status = 200;
  retry_header = reset_header = date_header = NULL;
  expect_max_encoded_key=false;
  radar_http_test=false; http_binary_length=0;
  memset(request_results, 0, sizeof(request_results));
  memset(request_durations_ms, 0, sizeof(request_durations_ms));
  memset(request_ticks, 0, sizeof(request_ticks));
  memset(sync_ticks, 0, sizeof(sync_ticks));
  memset(delays, 0, sizeof(delays));
  http_requests = http_cleanups = sync_calls = sync_failures = wifi_init_calls = 0;
  delay_count = clock_polls = 0;
  delay_limit = 0; worker_stop_ms = 0;
  s_radar_displayed_generation = 0;
  worker_radar_mode = RADAR_REAL_CLIENT;
  worker_radar_calls = worker_radar_starts = worker_radar_previous_delay = 0;
  held_runtime_mutex = NULL;
  wifi_connected = true;
  wifi_init_result = wifi_wait_result = ESP_OK;
  actual_time_synced = false;
  memset(&download_clock, 0, sizeof(download_clock));
  on_delay = on_request = NULL;
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
    cJSON *normalized_hourly=cJSON_GetObjectItemCaseSensitive(root,"hourly");
    cJSON *flags=cJSON_AddArrayToObject(normalized_hourly,"condition_flags"); assert(flags);
    cJSON *codes=cJSON_GetObjectItemCaseSensitive(normalized_hourly,"weathercode");
    for (int i=0;i<cJSON_GetArraySize(codes);++i) {
      weather_condition_facts_t facts=weather_facts_from_open_meteo(cJSON_GetArrayItem(codes,i)->valueint,0,0,0);
      assert(cJSON_AddItemToArray(flags,cJSON_CreateNumber(facts.flags)));
    }
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
  assert(delay_count == stop_after_delays);
  assert(http_requests == http_cleanups);
  assert(clock_polls == 0); /* Actual synchronization belongs to clock_control_task. */
}

/* Stop after processing the turn at a real elapsed deadline. Radar may add
 * intermediate wakeups; the observable forecast and retry times stay exact. */
static void run_worker_until(TickType_t stop_ms)
{
  delay_limit = 0;
  worker_stop_ms = stop_ms;
  if (setjmp(worker_finished) == 0) weather_service_task(NULL);
  assert(xTaskGetTickCount() == stop_ms);
  assert(http_requests == http_cleanups && clock_polls == 0);
}

static void move_wall_clock_on_radar_wake(void)
{
  if (xTaskGetTickCount() == 300000) fake_clock_set(100); /* A backward clock correction. */
}

static void resume_radar_publication(void)
{
  assert(worker_radar_calls == 1 && s_runtime_state.radar.generation == 0);
  assert(s_radar_displayed_generation == 0); /* Publication never acknowledges display. */
  if (delay_count == 2) {
    s_runtime_mutex = held_runtime_mutex;
    held_runtime_mutex = NULL;
    s_radar_displayed_generation = 77; /* Controlled UI replacement acknowledgment. */
    on_delay = NULL;
  }
}

static void check_worker_radar_schedule(void)
{
  reset_platform();
  worker_radar_mode = RADAR_IDLE_STEPS;
  on_delay = move_wall_clock_on_radar_wake;
  run_worker_until(600000);
  assert(worker_radar_calls == 3 && worker_radar_starts == 3);
  assert(worker_radar_start_ticks[0] == 0 && worker_radar_start_ticks[1] == 300000 &&
         worker_radar_start_ticks[2] == 600000);
  assert(http_requests == 2 && request_ticks[0] == 0 && request_ticks[1] == 600000);

  reset_platform();
  sync_failures = 2;
  worker_radar_mode = RADAR_BUSY_STEPS;
  run_worker(61);
  assert(worker_radar_calls == 61 && worker_radar_starts == 1);
  assert(worker_radar_ticks[0] == 0 && worker_radar_ticks[60] == 600000);
  assert(sync_ticks[0] == 0 && sync_ticks[1] == 20000 && sync_ticks[2] == 40000);
  assert(http_requests == 2 && request_ticks[1] == 600000);
  for (unsigned i = 0; i < delay_count; ++i) assert(delays[i] == 100);

  reset_platform();
  worker_radar_mode = RADAR_PENDING_COPY;
  on_delay = resume_radar_publication;
  run_worker(3);
  assert(held_runtime_mutex == NULL && worker_radar_calls == 2);
  assert(worker_radar_ticks[0] == 0 && worker_radar_ticks[1] == 200);
  assert(worker_radar_acknowledged[0] == 0 && worker_radar_acknowledged[1] == 77);
  assert(s_runtime_state.radar.generation == 77);
}

static void lose_worker_radar_connection(void)
{
  assert(s_runtime_state.radar.valid);
  assert(s_runtime_state.radar.pixels == worker_radar_pixel);
  assert(s_runtime_state.radar.frame_time == 1700000000);
  if (delay_count == 1) {
    assert(!s_runtime_state.radar.stale);
    wifi_connected = false;
    wifi_wait_result = ESP_ERR_TIMEOUT;
  } else assert(s_runtime_state.radar.stale);
}

static void allow_first_worker_radar_connection(void)
{
  if (xTaskGetTickCount() <= 20000) {
    assert(worker_radar_calls == 0 && !s_runtime_state.radar.valid && http_requests == 0);
    if (xTaskGetTickCount() == 20000) wifi_wait_result = ESP_OK;
  }
}

static void check_worker_radar_offline(void)
{
  reset_platform();
  worker_radar_mode = RADAR_RETAINED_FRAME;
  on_delay = lose_worker_radar_connection;
  run_worker_until(900000);
  assert(worker_radar_calls == 4 && worker_radar_starts == 1);
  assert(worker_radar_actions[0] == WEATHER_RADAR_CHECK && worker_radar_ticks[0] == 0);
  for (unsigned i = 1; i < worker_radar_calls; ++i) {
    assert(worker_radar_actions[i] == WEATHER_RADAR_OFFLINE);
    assert(worker_radar_ticks[i] == i * 300000);
  }
  assert(s_runtime_state.radar.stale && s_runtime_state.radar.generation == 2);
  assert(http_requests == 1 && request_ticks[0] == 0); /* No HTTP while offline. */

  reset_platform();
  worker_radar_mode = RADAR_INTERRUPTED_FRAME;
  on_delay = lose_worker_radar_connection;
  run_worker_until(20100);
  assert(worker_radar_calls == 2 && worker_radar_starts == 1);
  assert(worker_radar_actions[0] == WEATHER_RADAR_CHECK && worker_radar_ticks[0] == 0);
  assert(worker_radar_actions[1] == WEATHER_RADAR_OFFLINE && worker_radar_ticks[1] == 100);
  assert(s_runtime_state.radar.stale && http_requests == 1);

  reset_platform();
  worker_radar_mode = RADAR_RETAINED_FRAME;
  wifi_connected = false;
  wifi_wait_result = ESP_ERR_TIMEOUT;
  on_delay = allow_first_worker_radar_connection;
  run_worker_until(40000);
  assert(worker_radar_calls == 1 && worker_radar_starts == 1);
  assert(worker_radar_actions[0] == WEATHER_RADAR_CHECK && worker_radar_ticks[0] == 40000);
  assert(s_runtime_state.radar.valid && !s_runtime_state.radar.stale);
  assert(http_requests == 1 && request_ticks[0] == 40000);
}

static void check_pending_time(void)
{
  reset_platform();
  fake_clock_set(0);
  wifi_connected = false; /* Joining Wi-Fi also must not add a time prerequisite. */
  run_worker_until(600000);
  assert(http_requests == 2 && request_ticks[0] == 0 && request_ticks[1] == 600000);
  assert(delays[0] == 300000 && delays[1] == 300000);
  assert(!actual_time_synced && sync_calls >= 1);
  assert_forecast(&s_runtime_state.weather);
  assert(!s_runtime_state.weather.stale);
  assert(s_runtime_state.weather.updated_at == 0 && wifi_init_calls == 1);
}

static void check_download_timestamp(void)
{
  /* Usable network time at the download stamps age. RTC-only and unavailable
   * time must not acquire a fabricated age when network time later arrives. */
  for (int source = WAC_TIME_SOURCE_UNAVAILABLE; source <= WAC_TIME_SOURCE_NETWORK; ++source) {
    reset_platform();
    download_clock = (wac_time_snapshot_t){ .source = source,
      .usable = source != WAC_TIME_SOURCE_UNAVAILABLE, .utc_now = 1798674600 };
    run_worker(1);
    assert_forecast(&s_runtime_state.weather);
    assert(s_runtime_state.weather.updated_at ==
      (source == WAC_TIME_SOURCE_NETWORK ? download_clock.utc_now : 0));
  }
}

static void check_time_start_failure(void)
{
  reset_platform();
  sync_failures = 2;
  run_worker_until(600000);
  assert(sync_calls >= 3 && sync_ticks[0] == 0 && sync_ticks[1] == 20000 && sync_ticks[2] == 40000);
  assert(http_requests == 2 && request_ticks[0] == 0 && request_ticks[1] == 600000);
  assert(delays[0] == 20000 && delays[1] == 20000 && delays[2] == 260000);
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

static void check_quiet_connection_waits(void)
{
  /* No profile, failed Wi-Fi initialization, and connection timeout remain
   * setup concerns. None publishes a routine clock footer before weather. */
  for (unsigned scenario = 0; scenario < 3; ++scenario) {
    reset_platform();
    if (scenario == 0) applied_config.wifi_ready = false;
    else if (scenario == 1) wifi_init_result = ESP_FAIL;
    else {
      wifi_connected = false;
      wifi_wait_result = ESP_ERR_TIMEOUT;
    }
    run_worker(2);
    assert(http_requests == 0 && !s_runtime_state.weather.valid);
    assert(wifi_init_calls == (scenario == 0 ? 0 : scenario == 1 ? 2 : 1));
    assert(delays[0] == 20000 && delays[1] == 20000);
  }
}

static void observe_weather_notice_at_request(void)
{
  /* Observe in-flight retries: publication keeps the previous payload and
   * stale state until success; an initial failure never invents data. */
  if (http_requests == 0) assert(!s_runtime_state.weather.valid);
  else if (request_results[http_requests - 1] != ESP_OK)
    assert(!s_runtime_state.weather.valid || s_runtime_state.weather.stale);
  else assert(s_runtime_state.weather.valid);
}

static void check_slow_weather_and_time_retry(void)
{
  reset_platform();
  sync_failures = 2;
  request_durations_ms[0] = 10000; /* The client's existing HTTP timeout. */
  run_worker_until(610000);
  assert(sync_ticks[0] == 0 && sync_ticks[1] == 20000 && sync_ticks[2] == 40000);
  assert(http_requests == 2 && request_ticks[0] == 0 && request_ticks[1] == 610000);
  assert(delays[0] == 10000 && delays[1] == 20000 && delays[2] == 270000);
  assert_forecast(&s_runtime_state.weather);
}

static void check_weather_failure(void)
{
  reset_platform();
  sync_failures = 2;
  request_results[0] = ESP_ERR_TIMEOUT;
  on_request = observe_weather_notice_at_request;
  run_worker_until(620000);
  assert(sync_ticks[0] == 0 && sync_ticks[1] == 20000 && sync_ticks[2] == 40000);
  assert(http_requests == 3 && request_ticks[0] == 0 && request_ticks[1] == 20000 && request_ticks[2] == 620000);
  assert(delays[0] == 20000 && delays[1] == 20000 && delays[2] == 260000);
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
  run_worker_until(630000);
  const TickType_t expected_sync[] = {0,20000,40000,60000,310000,610000,630000};
  const TickType_t expected_requests[] = {0,30000,630000};
  const TickType_t expected_delays[] = {10000,10000,10000,20000,250000,300000,20000,280000};
  assert(sync_calls == sizeof(expected_sync) / sizeof(expected_sync[0]));
  assert(http_requests == sizeof(expected_requests) / sizeof(expected_requests[0]));
  assert(memcmp(sync_ticks, expected_sync, sizeof(expected_sync)) == 0);
  assert(memcmp(request_ticks, expected_requests, sizeof(expected_requests)) == 0);
  assert(memcmp(delays, expected_delays, sizeof(expected_delays)) == 0);
  assert_forecast(&s_runtime_state.weather);
  assert(!s_runtime_state.weather.stale && !actual_time_synced);
}

static app_weather_snapshot_t retained_weather;
static bool retained_weather_seen, stale_weather_seen;

static void observe_retention(void)
{
  if (!retained_weather_seen) {
    retained_weather_seen = true;
    retained_weather = s_runtime_state.weather;
    assert_forecast(&retained_weather);
    if (retained_weather.has_conditions) assert_timed_conditions(&retained_weather, false);
  }
  if (s_runtime_state.weather.stale) {
    stale_weather_seen = true;
    app_weather_snapshot_t stale = s_runtime_state.weather;
    stale.stale = false;
    assert(memcmp(&stale, &retained_weather, sizeof(stale)) == 0);
  }
}

static void check_retained_weather(void)
{
  reset_platform();
  retained_weather_seen = stale_weather_seen = false;
  request_results[1] = ESP_ERR_TIMEOUT;
  on_delay = observe_retention;
  on_request = observe_weather_notice_at_request;
  run_worker_until(620000);
  assert(http_requests == 3 && request_ticks[1] == 600000 && request_ticks[2] == 620000);
  assert(stale_weather_seen);
  assert_forecast(&s_runtime_state.weather);
  assert(!s_runtime_state.weather.stale);
}

static void observe_wifi_loss(void)
{
  observe_retention();
  /* Lose Wi-Fi at the forecast boundary, after the new intervening Radar
   * wakeup. Reconnect only after observing the offline retained payload. */
  if (xTaskGetTickCount() < 600000 &&
      xTaskGetTickCount() + delays[delay_count - 1] == 600000) {
    wifi_connected = false;
    wifi_wait_result = ESP_ERR_TIMEOUT;
  } else if (xTaskGetTickCount() == 600000) {
    assert(s_runtime_state.weather.stale);
    wifi_wait_result = ESP_OK;
  }
}

static void check_retained_weather_after_wifi_loss(void)
{
  reset_platform();
  retained_weather_seen = stale_weather_seen = false;
  on_delay = observe_wifi_loss;
  on_request = observe_weather_notice_at_request;
  run_worker_until(620000);
  assert(http_requests == 2 && wifi_init_calls == 1);
  assert(request_ticks[0] == 0 && request_ticks[1] == 620000);
  assert(stale_weather_seen);
  assert_forecast(&s_runtime_state.weather);
  assert(!s_runtime_state.weather.stale);
}

static void check_retained_conditions(void)
{
  reset_platform();
  retained_weather_seen = stale_weather_seen = false;
  char *body = make_conditions_response();
  http_body = body;
  request_results[1] = ESP_ERR_TIMEOUT;
  on_delay = observe_retention;
  run_worker_until(600000);
  assert(http_requests == 2 && http_cleanups == 2);
  assert(request_ticks[0] == 0 && request_ticks[1] == 600000);
  assert(stale_weather_seen && s_runtime_state.weather.stale);
  assert_timed_conditions(&s_runtime_state.weather, false);
  free(body);
}


/* Constructed values exercise current observations, DST hour identity and unit
 * conversion. These are test examples, not real rare-weather observations. */
static char *make_vc_response(bool metric_units)
{
  cJSON *root=cJSON_CreateObject(); assert(root);
  cJSON_AddNumberToObject(root,"queryCost",1);
  cJSON_AddNumberToObject(root,"tzoffset",-6);
  cJSON *current=cJSON_AddObjectToObject(root,"currentConditions");
  cJSON_AddNumberToObject(current,"datetimeEpoch",1793518200); /* Second 01:30 at fall-back. */
  cJSON_AddNumberToObject(current,"temp",metric_units?1.2:34.2);
  cJSON_AddNumberToObject(current,"windspeed",metric_units?19.312128:12);
  cJSON_AddNumberToObject(current,"winddir",180);
  cJSON_AddStringToObject(current,"conditions","type_40, type_38");
  cJSON_AddStringToObject(current,"icon","rain");
  cJSON *days=cJSON_AddArrayToObject(root,"days");
  for (int i=0;i<6;++i) {
    cJSON *day=cJSON_CreateObject(); assert(day); cJSON_AddItemToArray(days,day);
    char date[12]; snprintf(date,sizeof(date),"2026-11-%02d",i+1);
    cJSON_AddStringToObject(day,"datetime",date);
    cJSON_AddNumberToObject(day,"tempmax",metric_units?15.4:59.72);
    cJSON_AddNumberToObject(day,"tempmin",metric_units?4.5:40.1);
    cJSON_AddStringToObject(day,"conditions","type_43");
    cJSON_AddStringToObject(day,"icon","clear-day");
    cJSON_AddStringToObject(day,"sunrise","07:00:00");
    cJSON_AddStringToObject(day,"sunset","18:00:00");
    cJSON *hours=cJSON_AddArrayToObject(day,"hours");
    for (int h=0;h<(i==0?4:2);++h) {
      cJSON *hour=cJSON_CreateObject(); assert(hour); cJSON_AddItemToArray(hours,hour);
      /* Two 01:00 rows have different epochs and offsets. */
      const char *clock=i==0?(h<2?"01:00:00":h==2?"02:00:00":"03:00:00"):(h?"09:00:00":"08:00:00");
      cJSON_AddStringToObject(hour,"datetime",clock);
      cJSON_AddNumberToObject(hour,"datetimeEpoch",i==0?1793512800+h*3600:1793541600+i*86400+h*3600);
      cJSON_AddNumberToObject(hour,"tzoffset",i==0&&h==0?-5:-6);
      cJSON_AddNumberToObject(hour,"temp",metric_units?1.2:34.2);
      cJSON_AddNumberToObject(hour,"windspeed",metric_units?19.312128:12);
      cJSON_AddNumberToObject(hour,"windgust",metric_units?24.14016:15);
      cJSON_AddNumberToObject(hour,"pressure",i==0&&h==0?999:1013.25);
      cJSON_AddNumberToObject(hour,"cloudcover",0);
      cJSON_AddNumberToObject(hour,"precip",metric_units?2.0:2.0/25.4);
      cJSON_AddNumberToObject(hour,"snow",metric_units?0.5:0.5/2.54);
      cJSON_AddNumberToObject(hour,"precipprob",60);
      cJSON_AddStringToObject(hour,"conditions",i==0?"type_43":"type_31");
      cJSON_AddStringToObject(hour,"icon",i==0?"clear-night":"snow");
    }
  }
  char *body=cJSON_PrintUnformatted(root); assert(body); cJSON_Delete(root); return body;
}
static void use_vc(const char *key)
{
  applied_config.weather_provider=DEVICE_CONFIG_WEATHER_VISUAL_CROSSING;
  snprintf(applied_config.weather_api_key,sizeof(applied_config.weather_api_key),"%s",key);
}
static void check_vc_decoder(void)
{
  for (int units=0;units<2;++units) {
    reset_platform(); use_vc(units?"test-metric":"test-us");
    strcpy(applied_config.units,units?"metric":"imperial");
    char *body=make_vc_response(units); http_body=body;
    app_weather_snapshot_t weather={0};
    assert(weather_client_fetch(&applied_config,&weather)==ESP_OK);
    assert(weather.valid && weather.temp==(units?1:34));
    assert(weather.observed_at==1793518200 && !weather.is_day);
    assert(strcmp(weather.icon_name,"hail")==0 && strcmp(weather.summary,"Hail & Thunder")==0);
    assert(weather.has_conditions && weather.conditions.speed==(units?19:12));
    /* Epoch chooses the second 01:00 hour; pressure=999 belongs to first. */
    assert(weather.conditions.has_pressure && weather.conditions.pressure_value==(units?1013:29.92));
    assert(weather.conditions.precip_now==(units?2:0.08));
    for (int i=0;i<5;++i) {
      char date[12]; snprintf(date,sizeof(date),"2026-11-%02d",i+2);
      assert(strcmp(weather.forecast[i].date,date)==0);
      assert(weather.forecast[i].high==(units?15:60));
      assert(weather.forecast[i].low==(units?5:40));
      assert(strcmp(weather.forecast[i].icon_name,"snow")==0);
    }
    cJSON *root=cJSON_Parse(body); assert(root);
    cJSON *current=cJSON_GetObjectItemCaseSensitive(root,"currentConditions");
    cJSON_ReplaceItemInObjectCaseSensitive(current,"conditions",cJSON_CreateString("type_900"));
    cJSON_DeleteItemFromObjectCaseSensitive(current,"icon");
    cJSON_DeleteItemFromObjectCaseSensitive(current,"windspeed");
    char *optional=cJSON_PrintUnformatted(root); assert(optional); http_body=optional;
    assert(weather_client_fetch(&applied_config,&weather)==ESP_OK);
    assert(weather.icon_name[0]=='\0' && strcmp(weather.summary,"Unknown")==0);
    assert(weather.valid && weather.conditions.has_speed); /* Hourly fallback remains available. */
    app_weather_snapshot_t retained=weather;
    cJSON_DeleteItemFromObjectCaseSensitive(cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(root,"days"),5),"tempmin");
    char *broken=cJSON_PrintUnformatted(root); assert(broken); http_body=broken;
    assert(weather_client_fetch(&applied_config,&weather)!=ESP_OK);
    assert(memcmp(&weather,&retained,sizeof(weather))==0);
    free(broken); free(optional); free(body); cJSON_Delete(root);
  }
}
static void check_current_period_and_failure(void)
{
  reset_platform(); cJSON *root=cJSON_Parse(provider_response); assert(root);
  cJSON *current=cJSON_GetObjectItemCaseSensitive(root,"current_weather");
  cJSON_ReplaceItemInObjectCaseSensitive(current,"weathercode",cJSON_CreateNumber(95));
  cJSON_ReplaceItemInObjectCaseSensitive(current,"temperature",cJSON_CreateNumber(20));
  cJSON *amounts=cJSON_AddObjectToObject(root,"current");
  cJSON_AddNumberToObject(amounts,"rain",0.0); cJSON_AddNumberToObject(amounts,"showers",0.0);
  cJSON_AddNumberToObject(amounts,"snowfall",0.0);
  char *body=cJSON_PrintUnformatted(root); assert(body); http_body=body;
  app_weather_snapshot_t weather={0}; assert(weather_client_fetch(&applied_config,&weather)==ESP_OK);
  assert(strcmp(weather.icon_name,"thunderstorm")==0 && !weather.thundersnow);
  cJSON_ReplaceItemInObjectCaseSensitive(amounts,"snowfall",cJSON_CreateNumber(0.2));
  free(body); body=cJSON_PrintUnformatted(root); http_body=body;
  assert(weather_client_fetch(&applied_config,&weather)==ESP_OK);
  assert(strcmp(weather.icon_name,"thundersnow")==0 && weather.thundersnow);
  app_weather_snapshot_t retained=weather;
  char *large=malloc(200*1024+1); assert(large); memset(large,' ',200*1024);large[200*1024]='\0';http_body=large;
  assert(weather_client_fetch(&applied_config,&weather)==ESP_ERR_INVALID_SIZE);
  assert(memcmp(&weather,&retained,sizeof(weather))==0);
  free(large); free(body); cJSON_Delete(root);
}
static void check_quota(void)
{
  reset_platform(); use_vc("quota-applied"); app_weather_snapshot_t weather={0};
  assert(weather_client_next_allowed_us(&applied_config)==0); /* RAM is empty at boot. */
  http_status=429; http_body="Maximum concurrent jobs has been exceeded";
  assert(weather_client_fetch(&applied_config,&weather)!=ESP_OK);
  assert(weather_client_next_allowed_us(&applied_config)==0);
  http_body="Maximum concurrent jobs exceeded. Daily usage allowance remains available";
  assert(weather_client_fetch(&applied_config,&weather)!=ESP_OK);
  assert(weather_client_next_allowed_us(&applied_config)==0);
  http_body="Daily cost limit has been exceeded";
  assert(weather_client_fetch(&applied_config,&weather)!=ESP_OK);
  assert(weather_client_next_allowed_us(&applied_config)==INT64_C(3600000000));
  unsigned requests=http_requests;
  assert(weather_client_fetch(&applied_config,&weather)==ESP_ERR_INVALID_STATE);
  assert(http_requests==requests && weather_client_next_allowed_us(&applied_config)==INT64_C(3600000000));
  device_config_t draft=applied_config; strcpy(draft.weather_api_key,"quota-draft");
  assert(weather_client_next_allowed_us(&draft)==0);
  retry_header="7200";
  assert(weather_client_fetch(&draft,&weather)!=ESP_OK);
  assert(weather_client_next_allowed_us(&draft)==INT64_C(7200000000));
  /* A newer draft cannot evict the applied-key pause. */
  strcpy(draft.weather_api_key,"quota-other-draft");
  assert(weather_client_next_allowed_us(&draft)==0);
  assert(weather_client_next_allowed_us(&applied_config)==INT64_C(3600000000));
  fake_clock_advance(3600000); retry_header=NULL; http_status=500; http_body="server error";
  assert(weather_client_fetch(&applied_config,&weather)!=ESP_OK);
  assert(weather_client_next_allowed_us(&applied_config)==INT64_C(7200000000));
  fake_clock_advance(3600000); http_status=429; http_body="Daily usage quota exhausted";
  date_header="Wed, 30 Dec 2026 23:50:00 GMT";
  retry_header="Thu, 31 Dec 2026 01:50:00 GMT";
  char reset[32]; snprintf(reset,sizeof(reset),"%lld",(long long)1798674600+10800);reset_header=reset;
  assert(weather_client_fetch(&applied_config,&weather)!=ESP_OK);
  assert(weather_client_next_allowed_us(&applied_config)==INT64_C(18000000000)); /* Later reset: +3h. */
  device_config_t paused_draft=applied_config; strcpy(paused_draft.weather_api_key,"quota-success-independent");
  retry_header="36000";reset_header=NULL;
  assert(weather_client_fetch(&paused_draft,&weather)!=ESP_OK);
  int64_t draft_deadline=weather_client_next_allowed_us(&paused_draft);
  assert(draft_deadline==esp_timer_get_time()+INT64_C(36000000000));
  fake_clock_advance(10800000); http_status=200;
  char *success=make_vc_response(false);http_body=success;
  assert(weather_client_fetch(&applied_config,&weather)==ESP_OK);
  assert(weather_client_next_allowed_us(&applied_config)==0);
  assert(weather_client_next_allowed_us(&paused_draft)==draft_deadline);
  free(success);
  /* Relative timing works even before clock synchronization. Invalid dates
   * cannot invent an absolute wait; approved one-hour recovery still applies. */
  use_vc("quota-no-clock"); fake_clock_set(0); date_header=NULL;reset_header=NULL;retry_header=" 120 ";
  http_status=429;http_body="Daily limit exceeded";
  int64_t before=esp_timer_get_time();
  assert(weather_client_fetch(&applied_config,&weather)!=ESP_OK);
  assert(weather_client_next_allowed_us(&applied_config)==before+INT64_C(120000000));
  fake_clock_advance(120000);retry_header="malformed";
  assert(weather_client_fetch(&applied_config,&weather)!=ESP_OK);
  assert(weather_client_next_allowed_us(&applied_config)==esp_timer_get_time()+INT64_C(3600000000));
}


static char *read_response_file(const char *path)
{
  FILE *file=fopen(path,"rb"); assert(file);
  assert(fseek(file,0,SEEK_END)==0);long size=ftell(file);assert(size>0);
  rewind(file);char *data=malloc((size_t)size+1);assert(data);
  assert(fread(data,1,(size_t)size,file)==(size_t)size);data[size]='\0';fclose(file);return data;
}
static char *read_fixture(const char *filename)
{
  char path[512]; snprintf(path,sizeof(path),"%s/%s",WAC_WEATHER_FIXTURE_DIR,filename);
  return read_response_file(path);
}
static void check_retained_real_samples(void)
{
  /* Sanitized provider captures preserve real forecast facts. VC was captured
   * with forecast-only fields: current observations were not requested. */
  reset_platform();char *om=read_fixture("weather-open-meteo.json");http_body=om;
  app_weather_snapshot_t weather={0};
  assert(weather_client_fetch(&applied_config,&weather)==ESP_OK);
  assert(weather.temp==80 && weather.high==91 && weather.low==75);
  assert(strcmp(weather.forecast[0].date,"2026-10-14")==0);
  assert(weather.forecast[0].high==89 && weather.forecast[4].low==55);
  assert(strcmp(weather.icon_name,"partlycloudy-day")==0);
  free(om);
  char *vc=read_fixture("weather-visual-crossing.json");http_body=vc;use_vc("real-forecast-test");
  app_weather_snapshot_t retained=weather;
  assert(weather_client_fetch(&applied_config,&weather)!=ESP_OK); /* Missing required current. */
  assert(memcmp(&weather,&retained,sizeof(weather))==0);
  cJSON *root=cJSON_Parse(vc);assert(root);
  char *constructed=make_vc_response(false);cJSON *supplement=cJSON_Parse(constructed);assert(supplement);
  /* Only this current observation is constructed. The stored real hourly
   * forecast stays unchanged, including its missing optional measurements. */
  assert(cJSON_AddItemToObject(root,"currentConditions",cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(supplement,"currentConditions"),true)));
  char *body=cJSON_PrintUnformatted(root);assert(body);http_body=body;
  assert(weather_client_fetch(&applied_config,&weather)==ESP_OK);
  assert(strcmp(weather.forecast[0].date,"2026-10-14")==0);
  assert(weather.forecast[0].high==89 && weather.forecast[4].low==53);
  assert(strcmp(weather.forecast[0].icon_name,"partlycloudy-day")==0);
  assert(!weather.has_conditions); /* Stored forecast omitted epochs around constructed current. */
  free(body);free(constructed);free(vc);cJSON_Delete(root);cJSON_Delete(supplement);
}
typedef struct { device_config_t config; app_weather_snapshot_t weather; esp_err_t result; } fetch_thread_t;
static void *run_fetch_thread(void *arg)
{
  fetch_thread_t *test=arg;test->result=weather_client_fetch(&test->config,&test->weather);return NULL;
}
static void check_request_overlap(void)
{
  reset_platform();use_vc("overlap-applied");char *body=make_vc_response(false);http_body=body;
  fetch_thread_t worker={.config=applied_config},validation={.config=applied_config};
  strcpy(validation.config.weather_api_key,"draft-test/&?=");
  pthread_t first,second;block_http_once=true;http_blocked=http_released=false;
  assert(pthread_create(&first,NULL,run_fetch_thread,&worker)==0);
  pthread_mutex_lock(&overlap_mutex);
  while (!http_blocked) pthread_cond_wait(&overlap_condition,&overlap_mutex);
  pthread_mutex_unlock(&overlap_mutex);
  assert(pthread_create(&second,NULL,run_fetch_thread,&validation)==0);
  assert(fake_semaphore_wait_for_waiter()); /* Second fetch waits before HTTP init. */
  assert(http_requests==0);
  pthread_mutex_lock(&overlap_mutex);http_released=true;pthread_cond_broadcast(&overlap_condition);pthread_mutex_unlock(&overlap_mutex);
  assert(pthread_join(first,NULL)==0 && pthread_join(second,NULL)==0);
  assert(worker.result==ESP_OK && validation.result==ESP_OK);
  assert(http_requests==2 && http_cleanups==2);
  /* The request query is transient stack memory, so URL assertions belong in
   * the HTTP init substitute while that memory is live. Never print a key. */
  free(body);
}


static void quota_retention_delay(void)
{
  if (!retained_weather_seen) {
    retained_weather_seen=true;
    retained_weather=s_runtime_state.weather;
    assert(retained_weather.valid);
    http_status=429;http_body="Daily cost limit exceeded";
  } else if (http_requests >= 2) {
    stale_weather_seen=true;
    app_weather_snapshot_t stale=s_runtime_state.weather;
    assert(stale.stale);stale.stale=false;
    assert(memcmp(&stale,&retained_weather,sizeof(stale))==0);
  }
}
static void check_worker_quota_schedule(void)
{
  reset_platform();use_vc("quota-worker-retention");
  retained_weather_seen=stale_weather_seen=false;
  worker_radar_mode=RADAR_IDLE_STEPS;
  char *body=make_vc_response(false);http_body=body;on_delay=quota_retention_delay;
  run_worker_until(4200000);
  assert(http_requests==3 && request_ticks[0]==0 && request_ticks[1]==600000 && request_ticks[2]==4200000);
  assert(stale_weather_seen && worker_radar_starts==15);
  for (unsigned i=0; i<worker_radar_starts; ++i) assert(worker_radar_start_ticks[i]==i*300000);
  for (unsigned i=0; i<delay_count; ++i) assert(delays[i]==300000);
  free(body);
  reset_platform();use_vc("quota-worker-transient");
  http_status=429;http_body="Maximum concurrent jobs has been exceeded";
  run_worker(2);
  assert(http_requests==2 && request_ticks[1]==20000 && delays[0]==20000 && delays[1]==20000);
}


static void check_request_scratch_bounds(void)
{
  reset_platform();
  applied_config.weather_provider=DEVICE_CONFIG_WEATHER_VISUAL_CROSSING;
  memset(applied_config.weather_api_key,'/',DEVICE_CONFIG_WEATHER_KEY_LEN-1);
  applied_config.weather_api_key[DEVICE_CONFIG_WEATHER_KEY_LEN-1]='\0';
  expect_max_encoded_key=true;
  char *body=make_vc_response(false);http_body=body;
  app_weather_snapshot_t weather={0};
  assert(weather_client_fetch(&applied_config,&weather)==ESP_OK);
  assert(weather.valid && http_requests==1 && http_cleanups==1);
  free(body);
}

static void check_vc_local_time_dst(void)
{
  /* Constructed boundary cases use the observed provider shape: local strings
   * and epochs are present; individual day/hour tzoffset fields are absent. */
  static const char dst_response[]=
    "{\"tzoffset\":-5,\"days\":[{\"datetime\":\"2026-11-01\",\"hours\":["
    "{\"datetime\":\"01:00:00\",\"datetimeEpoch\":1793512800},"
    "{\"datetime\":\"01:00:00\",\"datetimeEpoch\":1793516400},"
    "{\"datetime\":\"19:00:00\",\"datetimeEpoch\":1793581200,\"conditions\":\"type_25\",\"precip\":0.04,\"precipprob\":60},"
    "{\"datetime\":\"20:00:00\",\"datetimeEpoch\":1793584800,\"conditions\":\"type_25\",\"precip\":0.04,\"precipprob\":60}]}]}";
  cJSON *root=cJSON_Parse(dst_response);assert(root);
  device_config_t config={0};strcpy(config.units,"imperial");
  cJSON *hourly=normalize_vc_hourly(&config,root);assert(hourly);
  assert(strcmp(string(at(field(hourly,"time"),0)),"2026-11-01T01:00:00")==0);
  assert(strcmp(string(at(field(hourly,"time"),1)),"2026-11-01T01:00:00")==0);
  assert(number(at(field(hourly,"epoch"),1))-number(at(field(hourly,"epoch"),0))==3600);
  assert(strcmp(string(at(field(hourly,"time"),3)),"2026-11-01T20:00:00")==0);
  weather_condition_facts_t daily={0};
  assert(forecast_pick_representative_picture(hourly,"2026-11-01",&daily,70,50,34)==WEATHER_PICTURE_RAIN);
  cJSON_Delete(hourly);cJSON_Delete(root);
  static const char spring_response[]=
    "{\"tzoffset\":-6,\"days\":[{\"datetime\":\"2027-03-14\",\"hours\":["
    "{\"datetime\":\"08:00:00\",\"datetimeEpoch\":1805029200,\"conditions\":\"type_25\",\"precip\":0.04,\"precipprob\":60},"
    "{\"datetime\":\"09:00:00\",\"datetimeEpoch\":1805032800,\"conditions\":\"type_25\",\"precip\":0.04,\"precipprob\":60}]}]}";
  root=cJSON_Parse(spring_response);assert(root);hourly=normalize_vc_hourly(&config,root);assert(hourly);
  assert(strcmp(string(at(field(hourly,"time"),0)),"2027-03-14T08:00:00")==0);
  assert(forecast_pick_representative_picture(hourly,"2027-03-14",&daily,70,50,34)==WEATHER_PICTURE_RAIN);
  cJSON_Delete(hourly);cJSON_Delete(root);
  static const char fallback_response[]=
    "{\"tzoffset\":-5,\"days\":[{\"datetime\":\"2026-11-01\",\"tzoffset\":-6,\"hours\":["
    "{\"datetimeEpoch\":1793520000},"
    "{\"datetime\":\"25:00:00\",\"datetimeEpoch\":1793523600},"
    "{\"datetimeEpoch\":1793520000,\"tzoffset\":-5}]}]}";
  root=cJSON_Parse(fallback_response);assert(root);hourly=normalize_vc_hourly(&config,root);assert(hourly);
  assert(strcmp(string(at(field(hourly,"time"),0)),"2026-11-01T02:00:00")==0);
  assert(strcmp(string(at(field(hourly,"time"),1)),"2026-11-01T03:00:00")==0);
  assert(strcmp(string(at(field(hourly,"time"),2)),"2026-11-01T03:00:00")==0);
  cJSON_Delete(hourly);cJSON_Delete(root);
}
static void check_saved_live_response(const char *path)
{
  /* Reads one previously saved response. Transport remains the native
   * substitute; no credential or live provider request is involved. */
  reset_platform();use_vc("offline-live-response-test");
  char *body=read_response_file(path);http_body=body;
  cJSON *root=cJSON_Parse(body);assert(root);
  assert(!field(root,"data")); /* A real payload, not the older metadata wrapper. */
  const cJSON *days=field(root,"days"), *current=field(root,"currentConditions");
  assert(cJSON_GetArraySize(days)>=6 && cJSON_IsObject(current));
  assert(isfinite(number(field(root,"tzoffset"))));
  app_weather_snapshot_t weather={0};
  assert(weather_client_fetch(&applied_config,&weather)==ESP_OK);
  assert(weather.valid && !weather.stale && http_requests==1 && http_cleanups==1);
  assert(weather.observed_at==(time_t)number(field(current,"datetimeEpoch")));
  assert(weather.temp==(int)round(number(field(current,"temp"))));
  for (int i=0;i<5;++i) {
    const cJSON *day=at(days,i+1);
    assert(strcmp(weather.forecast[i].date,string(field(day,"datetime")))==0);
    assert(weather.forecast[i].high==(int)round(number(field(day,"tempmax"))));
    assert(weather.forecast[i].low==(int)round(number(field(day,"tempmin"))));
  }
  weather_condition_facts_t current_facts=weather_facts_from_visual_crossing(current,number(field(current,"snow"))*2.54);
  cJSON *normalized_current=normalize_vc_current(&applied_config,root,current,current_facts);assert(normalized_current);
  assert(number(field(normalized_current,"temperature"))==number(field(current,"temp")));
  assert(fabs(number(field(normalized_current,"windspeed"))-number(field(current,"windspeed"))*1.609344)<1e-10);
  assert(strcmp(string(field(normalized_current,"time"))+11,string(field(current,"datetime")))==0);
  cJSON_Delete(normalized_current);
  cJSON *hourly=normalize_vc_hourly(&applied_config,root);assert(hourly);
  int index=0,fractional_temps=0,fractional_precip=0,missing_record_offsets=0;
  for (int d=0;d<cJSON_GetArraySize(days);++d) {
    const cJSON *day=at(days,d), *hours=field(day,"hours");
    for (int h=0;h<cJSON_GetArraySize(hours);++h,++index) {
      const cJSON *hour=at(hours,h);
      char expected_time[40];snprintf(expected_time,sizeof(expected_time),"%sT%s",string(field(day,"datetime")),string(field(hour,"datetime")));
      assert(strcmp(string(at(field(hourly,"time"),index)),expected_time)==0);
      assert(number(at(field(hourly,"epoch"),index))==number(field(hour,"datetimeEpoch")));
      double temp=number(field(hour,"temp")),precip=number(field(hour,"precip"));
      assert(number(at(field(hourly,"temperature_2m"),index))==temp);
      if (isfinite(temp)&&temp!=round(temp)) ++fractional_temps;
      if (isfinite(precip)) {
        assert(fabs(number(at(field(hourly,"precipitation"),index))-precip*25.4)<1e-10);
        if (precip!=round(precip)) ++fractional_precip;
      }
      double wind=number(field(hour,"windspeed")),snow=number(field(hour,"snow"));
      if (isfinite(wind)) assert(fabs(number(at(field(hourly,"wind_speed_10m"),index))-wind*1.609344)<1e-10);
      if (isfinite(snow)) assert(fabs(number(at(field(hourly,"snowfall"),index))-snow*2.54)<1e-10);
      assert(number(at(field(hourly,"pressure_msl"),index))==number(field(hour,"pressure")));
      if (!field(day,"tzoffset")&&!field(hour,"tzoffset")) ++missing_record_offsets;
    }
  }
  assert(index==cJSON_GetArraySize(field(hourly,"time")));
  assert(fractional_temps>0 && fractional_precip>0 && missing_record_offsets>0);
  assert(weather.has_conditions);
  printf("PASS: saved live Visual Crossing response decoded offline: current epoch, five dates/highs/lows, %d local hours, %d fractional temperatures, %d fractional precipitation values, %d hours without individual offsets.\n",index,fractional_temps,fractional_precip,missing_record_offsets);
  cJSON_Delete(hourly);cJSON_Delete(root);free(body);
}

/* These generated PNGs and frame times are constructed test inputs.
 * They exercise the bundled decoder and publication rules, not live services. */
static unsigned char *radar_test_png(unsigned width, unsigned height,
                                     const uint8_t color[4], size_t *size)
{
  uint8_t *pixels=malloc((size_t)width*height*4);
  assert(pixels);
  for (size_t i=0; i<(size_t)width*height; ++i) memcpy(pixels+i*4,color,4);
  unsigned char *png=NULL;
  assert(lodepng_encode32(&png,size,pixels,width,height)==0);
  free(pixels);
  return png;
}
static void check_radar_download_growth(void)
{
  /* Binary PNG bytes must survive growth, including embedded zeroes. */
  unsigned char body[10000];
  for (size_t i=0; i<sizeof(body); ++i) body[i]=(unsigned char)i;
  http_response_t response={.external=true,.maximum=sizeof(body)};
  esp_http_client_event_t event={.event_id=HTTP_EVENT_ON_DATA,
    .user_data=&response,.data=body,.data_len=3000};
  assert(http_event(&event)==ESP_OK);
  event.data=body+3000;event.data_len=7000;
  assert(http_event(&event)==ESP_OK);
  assert(response.length==sizeof(body) && response.capacity==sizeof(body)+1);
  assert(memcmp(response.data,body,sizeof(body))==0 && response.data[sizeof(body)]=='\0');
  char *saved=response.data;
  event.data_len=1;
  assert(http_event(&event)==ESP_ERR_INVALID_SIZE && response.data==saved);
  assert(response.length==sizeof(body) && memcmp(response.data,body,sizeof(body))==0);
  free(response.data);

  response=(http_response_t){.external=true,.maximum=sizeof(body)};
  event.data=body;event.data_len=3000;
  assert(http_event(&event)==ESP_OK);
  saved=response.data;
  unsigned previous_failures=radar_failed_allocations;
  radar_fail_alloc_size=sizeof(body)+1;
  event.data=body+3000;event.data_len=7000;
  assert(http_event(&event)==ESP_ERR_NO_MEM && response.data==saved);
  assert(response.length==3000 && response.capacity==4096 && response.data[3000]=='\0');
  assert(memcmp(response.data,body,3000)==0 && radar_failed_allocations==previous_failures+1);
  assert(http_event(&event)==ESP_ERR_NO_MEM && response.data==saved);
  free(response.data);
  radar_failed_allocations=previous_failures;
}

static void check_radar_tile_sampling(void)
{
  /* A half-pixel step gives independently known samples: output 1/2 use
   * source 0, output 511/512 use 255, and all other edges stay untouched. */
  uint8_t *pixels=malloc(256*256*4), *mapped=malloc(256*256*4);
  uint8_t *expected=malloc(RADAR_PIXELS*4);
  assert(pixels && mapped && expected);
  for (unsigned y=0; y<256; ++y) for (unsigned x=0; x<256; ++x) {
    uint32_t color=radar_source_rgba[(x*17+y*29)%128];
    uint8_t *rgba=pixels+((size_t)y*256+x)*4;
    rgba[0]=color>>24;rgba[1]=color>>16;rgba[2]=color>>8;
    rgba[3]=(x+y)%7 ? (uint8_t)color : 0;
    uint8_t *target=mapped+((size_t)y*256+x)*4;
    memcpy(target,rgba,4);assert(radar_map_pixel(target));
  }
  unsigned char *png=NULL;size_t size;
  assert(lodepng_encode32(&png,&size,pixels,256,256)==0);
  radar_http_test=true;http_status=200;radar_echo_png=png;radar_echo_png_size=size;
  radar.tile_x=radar.tile_y=1;
  radar.geometry=(radar_geometry_t){.left=255.5,.top=255.5,.step=0.5};
  radar.slot=0;radar.image[0]=malloc(RADAR_PIXELS*4);assert(radar.image[0]);
  for (size_t pixel=0; pixel<RADAR_PIXELS; ++pixel) {
    uint8_t *bgra=radar.image[0]+pixel*4;
    bgra[0]=pixel%251;bgra[1]=pixel%239;bgra[2]=pixel%227;bgra[3]=255;
  }
  memcpy(expected,radar.image[0],RADAR_PIXELS*4);
  for (int y=1; y<=512; ++y) for (int x=1; x<=512; ++x) {
    const uint8_t *rgba=mapped+((size_t)((y-1)/2)*256+(x-1)/2)*4;
    uint8_t *bgra=expected+((size_t)y*APP_RADAR_WIDTH+x)*4;
    for (unsigned c=0; c<3; ++c)
      bgra[c]=(uint8_t)((rgba[2-c]*rgba[3]+bgra[c]*(255-rgba[3])+127)/255);
  }
  assert(radar_tile()==ESP_OK);
  assert(memcmp(radar.image[0],expected,RADAR_PIXELS*4)==0);
  lv_free(png);
  /* This unknown opaque source pixel is outside all selected output samples.
   * Full source validation must still reject it before changing any output. */
  pixels[0]=1;pixels[1]=2;pixels[2]=3;pixels[3]=255;
  assert(lodepng_encode32(&png,&size,pixels,256,256)==0);
  radar_echo_png=png;radar_echo_png_size=size;
  radar.geometry=(radar_geometry_t){.left=356,.top=356,.step=0.125};
  assert(radar_tile()==ESP_ERR_INVALID_RESPONSE);
  assert(memcmp(radar.image[0],expected,RADAR_PIXELS*4)==0);
  lv_free(png);free(pixels);free(mapped);free(expected);
  free(radar.image[0]);radar.image[0]=NULL;radar_http_test=false;
}

static void check_radar_geometry_palette_png(void)
{
  check_radar_download_growth();
  lv_init();
  assert(weather_client_init_images()==ESP_OK);
  assert(APP_RADAR_WIDTH==800 && APP_RADAR_HEIGHT==800);
  assert(RADAR_PIXELS*9==5760000); /* One luminance and two BGRA buffers. */
  /* Include the decoder's 260-byte speculative reserve and 1.5 growth,
   * which exceed the decoded pixel count near the end of a valid PNG. */
  size_t scanline_bound=RADAR_PIXELS*4+APP_RADAR_HEIGHT*4;
  assert(RADAR_PNG_ALLOC_MAX>=scanline_bound+scanline_bound/2+260);
  assert(WEATHER_RESPONSE_MAX==192*1024);
  static const double latitudes[]={0,30.58,40.45476,70,-60};
  for (unsigned i=0; i<sizeof(latitudes)/sizeof(latitudes[0]); ++i) {
    radar_geometry_t geometry;
    assert(radar_geometry(latitudes[i],179.95,&geometry));
    double meters=APP_RADAR_HEIGHT*geometry.step*156543.033928*
      cos(latitudes[i]*RADAR_PI/180)/128;
    assert(fabs(meters-75*1609.344)<0.00001);
    double world=RADAR_WORLD_PIXELS;
    double center_x=geometry.left+APP_RADAR_WIDTH*geometry.step/2;
    double center_y=geometry.top+APP_RADAR_HEIGHT*geometry.step/2;
    assert(fabs(center_x/world*360-180-179.95)<1e-10);
    assert(fabs(center_y-radar_world_y(latitudes[i],world))<1e-10);
    assert(geometry.last_x>=128);
    assert(radar_wrapped_x(geometry.last_x)<128);
    assert(radar_geometry(latitudes[i],-179.95,&geometry));
    assert(geometry.first_x<0);
    assert(radar_wrapped_x(-1)==127);
  }
  radar_geometry_t geometry;
  assert(radar_geometry(30.58,-97.85,&geometry));
  assert(radar_geometry(40.45476,-105,&geometry));
  assert(geometry.first_x==26 && geometry.last_x==26);
  assert(geometry.first_y==47 && geometry.last_y==48);
  assert(radar_geometry(30.5788,-97.8531,&geometry));
  assert(geometry.first_x==28 && geometry.last_x==29);
  assert(geometry.first_y==52 && geometry.last_y==52);
  assert(location_scoring_label_has_country("Leander, Texas, US","US"));
  assert(location_scoring_label_has_country("Fort Collins, Colorado, us  ","US"));
  assert(!location_scoring_label_has_country("Ottawa, Ontario, CA","US"));
  assert(!location_scoring_label_has_country("Leander, Texas","US"));
  assert(!location_scoring_label_has_country("Leander Texas US","US"));
  assert(!location_scoring_label_has_country("Unknown, USA","US"));
  const struct { int strength; uint8_t rgb[3]; } cases[]={
    {-10,{195,230,189}},{34,{5,107,30}},{35,{255,232,0}},
    {40,{255,194,0}},{44,{255,153,0}},{45,{240,49,36}},
    {55,{188,12,25}},{75,{115,0,17}},{95,{85,0,12}}};
  for (unsigned i=0; i<sizeof(cases)/sizeof(cases[0]); ++i) {
    uint8_t rgb[3]; radar_color(cases[i].strength,rgb);
    assert(memcmp(rgb,cases[i].rgb,3)==0);
  }
  for (unsigned i=0; i<128; ++i) {
    uint32_t packed=radar_source_rgba[i];
    uint8_t rgba[4]={(uint8_t)(packed>>24),(uint8_t)(packed>>16),(uint8_t)(packed>>8),(uint8_t)packed};
    uint8_t alpha=rgba[3];
    assert(radar_map_pixel(rgba) && rgba[3]==alpha);
    if (alpha) {
      unsigned first=0;
      while (radar_source_rgba[first]!=packed) ++first;
      uint8_t expected[3];radar_color((int)first-32,expected);
      assert(memcmp(rgba,expected,3)==0);
      /* Source duplicates occur only inside one of the approved bands. */
      assert(((int)i-32<35)==((int)first-32<35));
      assert(((int)i-32>=45)==((int)first-32>=45));
    }
  }
  uint8_t transparent[4]={100,9,200,0}, unknown[4]={1,2,3,255};
  assert(radar_map_pixel(transparent) && transparent[3]==0);
  assert(!radar_map_pixel(unknown));
  radar.base=malloc(RADAR_PIXELS);
  radar.image[0]=malloc(RADAR_PIXELS*4);assert(radar.base && radar.image[0]);
  radar.slot=0;
  for (size_t pixel=0; pixel<RADAR_PIXELS; ++pixel) radar.base[pixel]=(uint8_t)pixel;
  radar_prepare_image();
  for (size_t pixel=0; pixel<RADAR_PIXELS; ++pixel) {
    double luminance=radar.base[pixel]/255.0;
    const double reference[]={.74-.63*luminance,.68-.60*luminance,.62-.56*luminance};
    for (unsigned channel=0; channel<3; ++channel)
      assert(radar.image[0][pixel*4+channel]==rounded(reference[channel]*255+1e-9));
    assert(radar.image[0][pixel*4+3]==255);
  }
  free(radar.base);free(radar.image[0]);radar.base=radar.image[0]=NULL;
  const uint8_t color[4]={1,20,230,127};
  size_t size;
  unsigned char *png=radar_test_png(256,256,color,&size);
  http_response_t response={.data=(char *)png,.length=size};
  lv_draw_buf_t *decoded=NULL;
  assert(radar_decode_png(&response,false,&decoded)==ESP_OK && decoded);
  assert(memcmp(decoded->data,color,4)==0);
  assert(decoded->header.w==256 && decoded->header.h==256);
  lv_draw_buf_destroy(decoded);
  response.length=size/2;
  assert(radar_decode_png(&response,false,&decoded)!=ESP_OK && !decoded);
  lv_free(png);
  png=radar_test_png(257,256,color,&size);
  response=(http_response_t){.data=(char *)png,.length=size};
  assert(radar_decode_png(&response,false,&decoded)!=ESP_OK && !decoded);
  lv_free(png);
  png=radar_test_png(APP_RADAR_WIDTH,APP_RADAR_HEIGHT,color,&size);
  response=(http_response_t){.data=(char *)png,.length=size};
  assert(radar_decode_png(&response,true,&decoded)==ESP_OK && decoded);
  assert(memcmp(decoded->data,color,4)==0);
  lv_draw_buf_destroy(decoded);
  assert(radar_decode_png(&response,false,&decoded)!=ESP_OK && !decoded);
  response.length=size/2;
  assert(radar_decode_png(&response,true,&decoded)!=ESP_OK && !decoded);
  lv_free(png);
  png=radar_test_png(APP_RADAR_WIDTH,APP_RADAR_HEIGHT+1,color,&size);
  response=(http_response_t){.data=(char *)png,.length=size};
  assert(radar_decode_png(&response,true,&decoded)!=ESP_OK && !decoded);
  lv_free(png);
  http_response_t capped={.maximum=RADAR_BASE_RESPONSE_MAX};
  esp_http_client_event_t oversized={.event_id=HTTP_EVENT_ON_DATA,.user_data=&capped,
    .data=(void *)color,.data_len=RADAR_BASE_RESPONSE_MAX+1};
  assert(http_event(&oversized)==ESP_ERR_INVALID_SIZE && !capped.data);
  capped=(http_response_t){0};
  oversized.user_data=&capped;oversized.data_len=WEATHER_RESPONSE_MAX+1;
  assert(http_event(&oversized)==ESP_ERR_INVALID_SIZE && !capped.data);
  check_radar_tile_sampling();
}
static void check_radar_publication(void)
{
  reset_platform();
  radar_http_test=true;
  strcpy(applied_config.location,"Chicago, Illinois, US");
  radar_image_requests=radar_base_requests=0;
  radar_metadata_body="{\"host\":\"https://tilecache.rainviewer.com\",\"radar\":{\"past\":[{\"time\":1700000000,\"path\":\"/v2/radar/opaque-frame\"}]}}";
  const uint8_t base_color[4]={210,200,180,255}, echo_color[4]={255,238,0,255};
  unsigned char *base=radar_test_png(APP_RADAR_WIDTH,APP_RADAR_HEIGHT,base_color,&radar_base_png_size);
  unsigned char *echo=radar_test_png(256,256,echo_color,&radar_echo_png_size);
  radar_base_png=base; radar_echo_png=echo;
  app_radar_snapshot_t snapshot={0};
  bool active=false;
  assert(weather_client_radar_step(&applied_config,WEATHER_RADAR_CHECK,0,&snapshot,&active));
  assert(active && !snapshot.valid && snapshot.generation!=0 && http_requests==0);
  uint32_t acknowledged=snapshot.generation;
  assert(!weather_client_radar_step(&applied_config,WEATHER_RADAR_CONTINUE,acknowledged,&snapshot,&active));
  assert(active && radar.phase==RADAR_BASE);
  radar_fail_alloc_size=RADAR_PIXELS;
  assert(weather_client_radar_step(&applied_config,WEATHER_RADAR_CONTINUE,acknowledged,&snapshot,&active));
  assert(!active && !snapshot.valid && snapshot.stale && !snapshot.pixels);
  acknowledged=snapshot.generation;
  assert(!weather_client_radar_step(&applied_config,WEATHER_RADAR_CHECK,acknowledged,&snapshot,&active));
  unsigned steps=0;
  while (active && steps++<64) {
    unsigned before=http_requests;
    bool published=weather_client_radar_step(&applied_config,WEATHER_RADAR_CONTINUE,acknowledged,&snapshot,&active);
    assert(http_requests-before<=1);
    if (published) acknowledged=snapshot.generation;
  }
  assert(!active && steps<64 && snapshot.valid && !snapshot.stale);
  assert(snapshot.frame_time==1700000000 && snapshot.pixels);
  assert(radar_base_requests==1 && radar_image_requests>1);
  const uint8_t *old_pixels=snapshot.pixels;
  size_t center=((size_t)APP_RADAR_HEIGHT/2*APP_RADAR_WIDTH+APP_RADAR_WIDTH/2)*4;
  const uint8_t yellow_bgra[4]={0,232,255,255};
  assert(memcmp(old_pixels+center,yellow_bgra,4)==0);
  /* Complete opaque coverage includes every edge and the square's corners;
   * the round display provides the physical crop without an image fade. */
  for (size_t pixel=0; pixel<RADAR_PIXELS; ++pixel)
    assert(memcmp(old_pixels+pixel*4,yellow_bgra,4)==0);
  unsigned images=radar_image_requests, requests=http_requests;
  assert(weather_client_radar_step(&applied_config,WEATHER_RADAR_OFFLINE,acknowledged,&snapshot,&active));
  assert(!active && snapshot.valid && snapshot.stale && snapshot.pixels==old_pixels);
  assert(snapshot.frame_time==1700000000 && http_requests==requests);
  uint32_t offline_generation=snapshot.generation;
  assert(!weather_client_radar_step(&applied_config,WEATHER_RADAR_OFFLINE,acknowledged,&snapshot,&active));
  assert(!active && snapshot.generation==offline_generation && http_requests==requests);
  acknowledged=snapshot.generation;
  assert(weather_client_radar_step(&applied_config,WEATHER_RADAR_CHECK,acknowledged,&snapshot,&active));
  assert(!active && snapshot.valid && !snapshot.stale && snapshot.pixels==old_pixels);
  assert(snapshot.frame_time==1700000000 && radar_image_requests==images);
  acknowledged=snapshot.generation;
  requests=http_requests;
  assert(!weather_client_radar_step(&applied_config,WEATHER_RADAR_CHECK,acknowledged,&snapshot,&active));
  assert(!active && http_requests==requests+1 && radar_image_requests==images);
  assert(snapshot.pixels==old_pixels && snapshot.frame_time==1700000000);
  http_status=503;
  assert(weather_client_radar_step(&applied_config,WEATHER_RADAR_CHECK,acknowledged,&snapshot,&active));
  assert(!active && snapshot.valid && snapshot.stale && snapshot.pixels==old_pixels);
  assert(snapshot.frame_time==1700000000);
  acknowledged=snapshot.generation;
  http_status=200;
  assert(weather_client_radar_step(&applied_config,WEATHER_RADAR_CHECK,acknowledged,&snapshot,&active));
  assert(!active && !snapshot.stale && radar_image_requests==images);
  radar_metadata_body="{\"host\":\"https://tilecache.rainviewer.com\",\"radar\":{\"past\":[{\"time\":1699999400,\"path\":\"/older/cache\"}]}}";
  acknowledged=snapshot.generation;
  assert(weather_client_radar_step(&applied_config,WEATHER_RADAR_CHECK,acknowledged,&snapshot,&active));
  assert(!active && snapshot.stale && snapshot.frame_time==1700000000);
  assert(snapshot.pixels==old_pixels && radar_image_requests==images);
  radar_metadata_body="{\"host\":\"https://tilecache.rainviewer.com\",\"radar\":{\"past\":[{\"time\":1700000000,\"path\":\"/v2/radar/opaque-frame\"}]}}";
  acknowledged=snapshot.generation;
  assert(weather_client_radar_step(&applied_config,WEATHER_RADAR_CHECK,acknowledged,&snapshot,&active));
  assert(!active && !snapshot.stale && radar_image_requests==images);
  /* A newer frame waits for the displayed generation before slot preparation. */
  radar_metadata_body="{\"host\":\"https://tilecache.rainviewer.com\",\"radar\":{\"past\":[{\"time\":1700000600,\"path\":\"/another/opaque-path\"}]}}";
  assert(!weather_client_radar_step(&applied_config,WEATHER_RADAR_CHECK,acknowledged,&snapshot,&active));
  assert(active && radar.phase==RADAR_WAIT_SLOT);
  requests=http_requests;
  assert(!weather_client_radar_step(&applied_config,WEATHER_RADAR_CONTINUE,acknowledged,&snapshot,&active));
  assert(active && radar.phase==RADAR_WAIT_SLOT && http_requests==requests);
  assert(memcmp(old_pixels+center,yellow_bgra,4)==0);
  acknowledged=snapshot.generation;
  radar_fail_alloc_size=RADAR_PIXELS*4;
  assert(weather_client_radar_step(&applied_config,WEATHER_RADAR_CONTINUE,acknowledged,&snapshot,&active));
  assert(!active && snapshot.valid && snapshot.stale && snapshot.pixels==old_pixels);
  assert(snapshot.frame_time==1700000000 && radar_failed_allocations==2);
  acknowledged=snapshot.generation;
  assert(!weather_client_radar_step(&applied_config,WEATHER_RADAR_CHECK,acknowledged,&snapshot,&active));
  while (active) {
    bool published=weather_client_radar_step(&applied_config,WEATHER_RADAR_CONTINUE,acknowledged,&snapshot,&active);
    if (published) acknowledged=snapshot.generation;
  }
  assert(snapshot.valid && snapshot.frame_time==1700000600 && snapshot.pixels!=old_pixels);
  assert(radar_base_requests==1); /* Same-location background is retained. */
  const uint8_t *second_pixels=snapshot.pixels;
  radar_metadata_body="{\"host\":\"https://tilecache.rainviewer.com\",\"radar\":{\"past\":[{\"time\":1700001200,\"path\":\"/newer/frame\"}]}}";
  assert(!weather_client_radar_step(&applied_config,WEATHER_RADAR_CHECK,acknowledged,&snapshot,&active));
  assert(!weather_client_radar_step(&applied_config,WEATHER_RADAR_CONTINUE,acknowledged,&snapshot,&active));
  assert(active && radar.phase==RADAR_ECHO);
  requests=http_requests;
  assert(weather_client_radar_step(&applied_config,WEATHER_RADAR_OFFLINE,acknowledged,&snapshot,&active));
  assert(!active && snapshot.stale && snapshot.pixels==second_pixels);
  assert(snapshot.frame_time==1700000600 && http_requests==requests && radar.phase==RADAR_IDLE);
  acknowledged=snapshot.generation;
  assert(!weather_client_radar_step(&applied_config,WEATHER_RADAR_CONTINUE,acknowledged,&snapshot,&active));
  assert(!active && http_requests==requests);
  assert(!weather_client_radar_step(&applied_config,WEATHER_RADAR_CHECK,acknowledged,&snapshot,&active));
  assert(active && snapshot.stale); /* A newer incomplete frame cannot clear old. */
  assert(!weather_client_radar_step(&applied_config,WEATHER_RADAR_CONTINUE,acknowledged,&snapshot,&active));
  radar_echo_png_size/=2; /* Actual decoder rejects the truncated replacement. */
  assert(weather_client_radar_step(&applied_config,WEATHER_RADAR_CONTINUE,acknowledged,&snapshot,&active));
  assert(!active && snapshot.valid && snapshot.stale && snapshot.pixels==second_pixels);
  assert(snapshot.frame_time==1700000600);
  radar_echo_png_size*=2;
  /* A complete transparent echo image is valid and removes prior echoes. */
  const uint8_t clear_color[4]={0,0,0,0};
  size_t clear_size;
  unsigned char *clear=radar_test_png(256,256,clear_color,&clear_size);
  radar_echo_png=clear; radar_echo_png_size=clear_size;
  acknowledged=snapshot.generation;
  assert(!weather_client_radar_step(&applied_config,WEATHER_RADAR_CHECK,acknowledged,&snapshot,&active));
  while (active) {
    bool published=weather_client_radar_step(&applied_config,WEATHER_RADAR_CONTINUE,acknowledged,&snapshot,&active);
    if (published) acknowledged=snapshot.generation;
  }
  assert(snapshot.valid && !snapshot.stale && snapshot.frame_time==1700001200);
  /* Luminance 200 maps to the approved brighter blue-gray background. */
  assert(radar.base[center/4]==200);
  const uint8_t expected_base[4]={63,53,46,255};
  for (size_t pixel=0; pixel<RADAR_PIXELS; ++pixel)
    assert(memcmp(snapshot.pixels+pixel*4,expected_base,4)==0);
  lv_free(clear);
  strcpy(applied_config.location,"Ottawa, Ontario, CA");
  requests=http_requests;
  assert(weather_client_radar_step(&applied_config,WEATHER_RADAR_CHECK,acknowledged,&snapshot,&active));
  assert(!active && !snapshot.valid && !snapshot.pixels && http_requests==requests);
  assert(!weather_client_radar_step(&applied_config,WEATHER_RADAR_CHECK,snapshot.generation,&snapshot,&active));
  assert(!active && http_requests==requests);
  strcpy(applied_config.location,"Fort Collins, Colorado, US");
  strcpy(applied_config.latitude,"40.45476");
  strcpy(applied_config.longitude,"-105");
  requests=http_requests;
  assert(weather_client_radar_step(&applied_config,WEATHER_RADAR_OFFLINE,acknowledged,&snapshot,&active));
  assert(!active && !snapshot.valid && !snapshot.pixels && snapshot.frame_time==0);
  assert(snapshot.latitude==40.45476 && http_requests==requests);
  assert(!radar.base_ready);
  /* A failed first image remains unavailable, without leaking the old place. */
  http_status=503;
  assert(weather_client_radar_step(&applied_config,WEATHER_RADAR_CHECK,snapshot.generation,&snapshot,&active));
  assert(!active && !snapshot.valid && !snapshot.pixels);
  lv_free(base);lv_free(echo);
  free(radar.base);free(radar.image[0]);free(radar.image[1]);
  memset(&radar,0,sizeof(radar));
  radar_http_test=false;
}

static unsigned char *radar_read_binary(const char *path, size_t *size)
{
  FILE *file=fopen(path,"rb");assert(file);
  assert(fseek(file,0,SEEK_END)==0);long length=ftell(file);assert(length>0);
  *size=(size_t)length;rewind(file);
  unsigned char *bytes=malloc(*size);assert(bytes);
  assert(fread(bytes,1,*size,file)==*size);fclose(file);return bytes;
}
static void check_saved_radar_data(const char *directory)
{
  /* Consume captured public provider data through the real client. Outputs are
   * data-only map compositions, not finished UI/board proof. */
  static const struct { const char *name,*latitude,*longitude,*location; } cases[]={
    {"leander","30.5788","-97.8531","Leander, Texas, US"},
    {"colorado","40.4547622191","-105","Fort Collins, Colorado, US"}};
  for (unsigned i=0; i<sizeof(cases)/sizeof(cases[0]); ++i) {
    reset_platform();lv_init();assert(weather_client_init_images()==ESP_OK);
    lv_mem_monitor_t initial;lv_mem_monitor(&initial);
    radar_http_test=true;
    snprintf(applied_config.latitude,sizeof(applied_config.latitude),"%s",cases[i].latitude);
    snprintf(applied_config.longitude,sizeof(applied_config.longitude),"%s",cases[i].longitude);
    snprintf(applied_config.location,sizeof(applied_config.location),"%s",cases[i].location);
    char path[1024];
    snprintf(path,sizeof(path),"%s/usgs-%s-800x800.png",directory,cases[i].name);
    unsigned char *base=radar_read_binary(path,&radar_base_png_size);radar_base_png=base;
    radar_saved_directory=directory;radar_saved_sample=cases[i].name;
    snprintf(path,sizeof(path),"%s/rainviewer-live-metadata.json",directory);
    char *metadata=read_response_file(path);radar_metadata_body=metadata;
    app_radar_snapshot_t snapshot={0};bool active=false;uint32_t acknowledged=0;
    assert(weather_client_radar_step(&applied_config,WEATHER_RADAR_CHECK,acknowledged,&snapshot,&active));
    acknowledged=snapshot.generation;
    unsigned steps=0;
    while (active && steps++<32) {
      if (weather_client_radar_step(&applied_config,WEATHER_RADAR_CONTINUE,acknowledged,&snapshot,&active)) acknowledged=snapshot.generation;
    }
    assert(!active && snapshot.valid);
    assert(radar.geometry.first_x==(i==0 ? 28 : 26));
    assert(radar.geometry.last_x==(i==0 ? 29 : 26));
    assert(radar.geometry.first_y==(i==0 ? 52 : 47));
    assert(radar.geometry.last_y==(i==0 ? 52 : 48));
    for (size_t pixel=0; pixel<RADAR_PIXELS; ++pixel) assert(snapshot.pixels[pixel*4+3]==255);
    snprintf(path,sizeof(path),"%s/client-%s-composed.bgra",directory,cases[i].name);
    FILE *out=fopen(path,"wb");assert(out);
    assert(fwrite(snapshot.pixels,1,RADAR_PIXELS*4,out)==RADAR_PIXELS*4);fclose(out);
    cJSON *report=cJSON_CreateObject();assert(report);
    cJSON_AddStringToObject(report,"sample",cases[i].name);
    cJSON_AddNumberToObject(report,"width",APP_RADAR_WIDTH);cJSON_AddNumberToObject(report,"height",APP_RADAR_HEIGHT);
    cJSON_AddNumberToObject(report,"frame_time",snapshot.frame_time);
    cJSON_AddNumberToObject(report,"latitude",snapshot.latitude);cJSON_AddNumberToObject(report,"longitude",snapshot.longitude);
    cJSON_AddNumberToObject(report,"compressed_basemap_bytes",radar_base_png_size);
    cJSON_AddNumberToObject(report,"ground_diameter_meters",RADAR_GROUND_M);
    cJSON_AddNumberToObject(report,"retained_pixel_bytes",RADAR_PIXELS*9);
    cJSON_AddNumberToObject(report,"first_tile_x",radar.geometry.first_x);
    cJSON_AddNumberToObject(report,"last_tile_x",radar.geometry.last_x);
    cJSON_AddNumberToObject(report,"first_tile_y",radar.geometry.first_y);
    cJSON_AddNumberToObject(report,"last_tile_y",radar.geometry.last_y);
    lv_mem_monitor_t memory;lv_mem_monitor(&memory);
    cJSON_AddNumberToObject(report,"native_codec_peak_above_init",memory.max_used-(initial.total_size-initial.free_size));
    assert(memory.free_size==initial.free_size); /* Decoder scratch is released. */
    char *json=cJSON_Print(report);assert(json);
    snprintf(path,sizeof(path),"%s/client-%s-composed.json",directory,cases[i].name);
    out=fopen(path,"wb");assert(out);assert(fwrite(json,1,strlen(json),out)==strlen(json));fclose(out);
    cJSON_free(json);cJSON_Delete(report);
    printf("PASS: saved real %s USGS/RainViewer inputs composed; %u client steps.\n",cases[i].name,steps);
    free(base);free(radar_saved_tile);free(metadata);
    radar_saved_tile=NULL;radar_saved_directory=radar_saved_sample=NULL;
    free(radar.base);free(radar.image[0]);free(radar.image[1]);memset(&radar,0,sizeof(radar));
    radar_http_test=false;lv_deinit();
  }
}

int main(int argc, char **argv)
{
  s_runtime_mutex = xSemaphoreCreateMutex();
  assert(s_runtime_mutex);
  if (argc==3 && strcmp(argv[1],"--radar-saved-data")==0) {
    check_saved_radar_data(argv[2]);
    assert(pthread_mutex_destroy(s_runtime_mutex)==0);free(s_runtime_mutex);
    return 0;
  }
  if (argc>1 && strcmp(argv[1],"--radar-only")==0) {
    check_worker_radar_schedule();
    check_worker_radar_offline();
    check_radar_geometry_palette_png();
    check_radar_publication();
    assert(pthread_mutex_destroy(s_runtime_mutex)==0);
    free(s_runtime_mutex);
    puts("PASS: constructed Radar PNG, palette, ground coverage, replacement and failure cases.");
    return 0;
  }
  check_decoder();
  check_conditions_decoder();
  check_worker_radar_schedule();
  check_worker_radar_offline();
  check_pending_time();
  check_download_timestamp();
  check_time_start_failure();
  check_location_required();
  check_quiet_connection_waits();
  check_slow_weather_and_time_retry();
  check_weather_failure();
  check_overlapping_failure_deadlines();
  check_retained_weather();
  check_retained_weather_after_wifi_loss();
  check_retained_conditions();
  check_current_period_and_failure();
  check_vc_decoder();
  check_quota();
  check_retained_real_samples();
  check_request_overlap();
  check_worker_quota_schedule();
  check_vc_local_time_dst();
  check_request_scratch_bounds();
  const char *live_response=argc>1?argv[1]:getenv("WAC_WEATHER_LIVE_RESPONSE");
  if (live_response && live_response[0]) check_saved_live_response(live_response);
  check_radar_geometry_palette_png();
  check_radar_publication();
  assert(pthread_mutex_destroy(s_runtime_mutex) == 0);
  free(s_runtime_mutex);
  puts("PASS: both providers and units, real forecast captures, constructed current/DST/optional/error cases, bounded responses, serialized overlap, key-specific quota recovery, independent time/weather scheduling and retained data.");
  return 0;
}

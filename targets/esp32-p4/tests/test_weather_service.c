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

bool esp_http_client_is_complete_data_received(esp_http_client_handle_t handle);

/* Compile the actual decoder here so its private normalization can be checked
 * without a firmware test interface or a copied decoding implementation. */
static size_t radar_fail_alloc_size;
static unsigned radar_failed_allocations;
static void *radar_retired_slots[2];
static unsigned radar_retired_frees;
static void radar_test_free(void *pointer)
{
  if (pointer) for (unsigned slot=0;slot<2;++slot) if (pointer==radar_retired_slots[slot]) {
    radar_retired_slots[slot]=NULL;++radar_retired_frees;
  }
  free(pointer);
}
static void *radar_test_malloc(size_t size)
{
  if (radar_fail_alloc_size && size==radar_fail_alloc_size) {
    radar_fail_alloc_size=0;
    ++radar_failed_allocations;
    return NULL;
  }
  return malloc(size);
}
static bool fail_weather_copy;
static unsigned weather_copy_failures;
static BaseType_t weather_test_take(SemaphoreHandle_t semaphore,TickType_t ticks);
#define xSemaphoreTake weather_test_take
#define cJSON_Delete alert_test_delete
static void alert_test_delete(cJSON *root);
#define malloc radar_test_malloc
#define free radar_test_free
#define TAG native_weather_client_tag
#include "../main/weather_client.c"
#undef TAG
#undef xSemaphoreTake
#undef malloc
#undef free
#undef cJSON_Delete

/* Native pthread locks do not model a finite FreeRTOS timeout. Supply that
 * platform failure for the real owner lock, preserving its requested bound. */
static BaseType_t weather_test_take(SemaphoreHandle_t semaphore,TickType_t ticks)
{
  if (semaphore==weather_copy_lock && fail_weather_copy) {
    assert(ticks==pdMS_TO_TICKS(250));++weather_copy_failures;return pdFALSE;
  }
  return xSemaphoreTake(semaphore,ticks);
}

/* Place one UI dismissal exactly after owner publication, during response
 * cleanup. Only timing is substituted; the real check, owner and action run. */
static bool tap_during_alert_cleanup;
static void alert_test_delete(cJSON *root)
{
  cJSON_Delete(root);
  if (tap_during_alert_cleanup && nws_count) {
    tap_during_alert_cleanup = false;
    weather_client_alerts_dismiss();
  }
}

_Static_assert(LODEPNG_MAX_ALLOC==RADAR_PNG_ALLOC_MAX,
  "Native LodePNG must enforce the firmware allocation cap");

/* Controlled provider and platform responses exercise the actual decoder and
 * worker. These examples do not establish live network or board behavior. */
static const char provider_response[] =
  "{\"current_weather\":{\"temperature\":68,\"weathercode\":0,\"is_day\":1,"
  "\"time\":\"2026-12-30T12:00\",\"windspeed\":10},"
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


static device_config_t applied_config;
static wac_audio_snapshot_t alert_audio;
static unsigned alert_sound_starts,alert_sound_stops,alert_sound_acks;
static bool alert_audio_reject;
void wac_audio_copy(wac_audio_snapshot_t *out) { *out=alert_audio; }
esp_err_t wac_audio_weather_start(wac_audio_weather_kind_t kind,uint32_t *token)
{
  *token=0;
  if (alert_audio_reject||alert_audio.weather_token) return ESP_ERR_INVALID_STATE;
  *token=++alert_sound_starts;
  alert_audio=(wac_audio_snapshot_t){.weather_token=*token,.weather_kind=kind,
    .weather_state=WAC_AUDIO_WEATHER_PENDING};
  return ESP_OK;
}
esp_err_t wac_audio_weather_stop(uint32_t token)
{
  if (token==alert_audio.weather_token) { ++alert_sound_stops;alert_audio.weather_state=WAC_AUDIO_WEATHER_CANCELED; }
  return ESP_OK;
}
void wac_audio_weather_ack(uint32_t token)
{
  if (token==alert_audio.weather_token) { ++alert_sound_acks;memset(&alert_audio,0,sizeof(alert_audio)); }
}
static unsigned forecast_requests, metadata_requests, nws_requests;
static TickType_t forecast_ticks[256], metadata_ticks[256], nws_ticks[256];
static unsigned radar_request_duration_ms;

static const char *http_body;
static const char *mixed_nws_body;
static const char *mixed_forecast_body;
static int mixed_forecast_status;
static bool nws_http_test;
static bool nws_transfer_complete=true;
static unsigned nws_accept_headers;
static bool radar_http_test;
static const unsigned char *radar_base_png, *radar_echo_png;
static size_t radar_base_png_size, radar_echo_png_size;
static const char *radar_metadata_body;
static const char *radar_saved_directory, *radar_saved_sample;
static const char *radar_six_frame_directory;
static cJSON *radar_six_frame_manifest;
static unsigned char *radar_saved_tile;
static unsigned radar_image_requests, radar_base_requests;
static bool radar_assert_released_base;
static int radar_requested_x, radar_requested_y;
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
static bool wifi_connected, actual_time_synced, native_night;
bool wac_night_active(void) { return native_night; }
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
  if (config->url && strstr(config->url,"api.weather.gov")) {
    assert(nws_http_test);
    assert(strstr(config->url,"https://api.weather.gov/alerts/active?point="));
    assert(strstr(config->url,"status=actual") && !strstr(config->url,"key="));
    assert(config->user_agent && strstr(config->user_agent,"ESP32-P4"));
    assert(config->crt_bundle_attach==esp_crt_bundle_attach && config->timeout_ms==10000);
    if (mixed_nws_body) http_body=mixed_nws_body;
    http_config=*config;return &http_config;
  }
  if (radar_http_test && config->url && !strstr(config->url,"api.open-meteo.com")) {
    assert(config->user_agent && strstr(config->user_agent,"ESP32-P4"));
    const http_response_t *response=config->user_data;
    assert(response->external && response->maximum==
      (strstr(config->url,"basemap.nationalmap.gov") ? RADAR_BASE_RESPONSE_MAX : WEATHER_RESPONSE_MAX));
    if (strstr(config->url,"weather-maps.json")) http_body=radar_metadata_body;
    else {
      bool base=strstr(config->url,"basemap.nationalmap.gov")!=NULL;
      assert(base || strstr(config->url,"tilecache.rainviewer.com"));
      assert(base || strstr(config->url,"/512/7/")!=NULL);
      assert(base || strstr(config->url,"/2/1_0.png")!=NULL);
      if (!base) {
        const char *tile=strstr(config->url,"/512/7/");
        assert(sscanf(tile,"/512/7/%d/%d/2/1_0.png",&radar_requested_x,&radar_requested_y)==2);
        assert(radar_requested_x>=0 && radar_requested_x<128);
      }
      assert(!base || (strstr(config->url,"size=800,800") && strstr(config->url,"format=png32")));
      if (base) {
        if (radar_assert_released_base) {
          assert(!radar.image[0] && !radar.image[1] && radar_retired_frees==2);
          radar_assert_released_base=false;
        }
        ++radar_base_requests;
      }
      else if (radar_six_frame_directory) {
        const cJSON *frames=field(radar_six_frame_manifest,"frames"),*frame=NULL;
        unsigned index=0;
        for (;index<6;++index) if (strstr(config->url,string(field(at(frames,index),"path")))) {
          frame=at(frames,index);break;
        }
        assert(frame);
        char path[1024];
        snprintf(path,sizeof(path),"%s/frames/frame-%u-%.0f-z7-%d-%d-512-smooth.png",
          radar_six_frame_directory,index,number(field(frame,"time")),radar_requested_x,radar_requested_y);
        free(radar_saved_tile);radar_saved_tile=radar_read_binary(path,&radar_echo_png_size);
        radar_echo_png=radar_saved_tile;
      }
      else if (radar_saved_directory) {
        int x=-1,y=-1;const char *tile=strstr(config->url,"/512/7/");
        assert(tile && sscanf(tile,"/512/7/%d/%d/2/1_0.png",&x,&y)==2);
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
  if (mixed_forecast_body) http_body=mixed_forecast_body;
  if (applied_config.weather_provider == DEVICE_CONFIG_WEATHER_VISUAL_CROSSING || config->host) {
    assert(config->url == NULL);
    assert(strcmp(config->host, "weather.visualcrossing.com") == 0);
    assert(strstr(config->path, "/41.8,-87.6/next6days"));
    assert(strstr(config->query, "include=current,days,hours"));
    assert(strstr(config->query, "lang=id&iconSet=icons2"));
    assert(strstr(config->query, "snowdepth") == NULL);
    assert(!strstr(config->query,"windspeed") && !strstr(config->query,"windgust"));
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
    assert(!strstr(config->url,"wind_speed") && !strstr(config->url,"wind_gust"));
  }
  assert(config->timeout_ms == 10000);
  http_config = *config;
  return &http_config;
}

esp_err_t esp_http_client_set_header(esp_http_client_handle_t handle,const char *name,const char *value)
{
  assert(handle==&http_config && !strcmp(name,"Accept")&&!strcmp(value,"application/geo+json"));
  ++nws_accept_headers;return ESP_OK;
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
  const char *url=http_config.url ? http_config.url : "";
  bool is_nws=strstr(url,"api.weather.gov")!=NULL;
  bool is_metadata=strstr(url,"weather-maps.json")!=NULL;
  bool is_radar=is_metadata || strstr(url,"tilecache.rainviewer.com") || strstr(url,"basemap.nationalmap.gov");
  if (is_nws) nws_ticks[nws_requests++]=xTaskGetTickCount();
  else if (is_metadata) metadata_ticks[metadata_requests++]=xTaskGetTickCount();
  else if (!is_radar) forecast_ticks[forecast_requests++]=xTaskGetTickCount();
  if (is_radar && radar_request_duration_ms) request_durations_ms[http_requests]=radar_request_duration_ms;
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
  if (mixed_forecast_status && (http_config.host || (http_config.url && strstr(http_config.url,"api.open-meteo.com"))))
    return mixed_forecast_status;
  return http_status;
}

esp_err_t esp_http_client_cleanup(esp_http_client_handle_t handle)
{
  assert(handle == &http_config);
  ++http_cleanups;
  return ESP_OK;
}
bool esp_http_client_is_complete_data_received(esp_http_client_handle_t handle)
{
  assert(handle==&http_config);return nws_transfer_complete;
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

/* Worker scenarios run the included production worker directly. HTTP duration,
 * responses and delay callbacks are controlled; no scheduler step is replaced. */
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
  mixed_nws_body=mixed_forecast_body=NULL;mixed_forecast_status=0;
  http_status = 200;
  retry_header = reset_header = date_header = NULL;
  expect_max_encoded_key=false;
  nws_http_test=false;
  nws_transfer_complete=true;
  native_night=false;
  radar_http_test=false; http_binary_length=0;radar_assert_released_base=false;
  memset(request_results, 0, sizeof(request_results));
  memset(request_durations_ms, 0, sizeof(request_durations_ms));
  memset(request_ticks, 0, sizeof(request_ticks));
  memset(sync_ticks, 0, sizeof(sync_ticks));
  memset(delays, 0, sizeof(delays));
  http_requests = http_cleanups = sync_calls = sync_failures = wifi_init_calls = 0;
  delay_count = clock_polls = 0;
  delay_limit = 0; worker_stop_ms = 0;
  radar_displayed_generation = 0;
  fail_weather_copy=false;weather_copy_failures=0;
  forecast_requests = metadata_requests = nws_requests = radar_request_duration_ms = 0;
  wifi_connected = true;
  wifi_init_result = wifi_wait_result = ESP_OK;
  actual_time_synced = false;
  memset(&download_clock, 0, sizeof(download_clock));
  on_delay = on_request = NULL;
  memset(&weather_completed, 0, sizeof(weather_completed));
  weather_client_alerts_sync_location(&applied_config);
  /* These forecast-only scenarios use an actual retained unsupported-location
   * result. Mixed scenarios below enable real NWS transport and scheduling. */
  nws_status=APP_ALERT_UNSUPPORTED;
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

static void run_worker(unsigned stop_after_delays)
{
  delay_limit = stop_after_delays;
  if (setjmp(worker_finished) == 0) weather_client_task(NULL);
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
  if (setjmp(worker_finished) == 0) weather_client_task(NULL);
  assert(xTaskGetTickCount() == stop_ms);
  assert(http_requests == http_cleanups && clock_polls == 0);
}

static unsigned char *radar_test_png(unsigned width,unsigned height,const uint8_t color[4],size_t *size);
static unsigned char *worker_base_png, *worker_echo_png;
static unsigned frozen_delays;
static bool copied_before_ack;
static app_radar_snapshot_t frozen_radar;
static unsigned frozen_requests;
static bool disconnect_during_preparation;

static void worker_radar_setup(void)
{
  reset_platform();lv_init();assert(weather_client_init_images()==ESP_OK);
  strcpy(applied_config.location,"Chicago, IL, US");
  weather_client_alerts_sync_location(&applied_config);nws_status=APP_ALERT_UNSUPPORTED;
  const uint8_t base[4]={100,100,100,255}, echo[4]={255,238,0,255};
  worker_base_png=radar_test_png(800,800,base,&radar_base_png_size);
  worker_echo_png=radar_test_png(512,512,echo,&radar_echo_png_size);
  radar_base_png=worker_base_png;radar_echo_png=worker_echo_png;
  radar_metadata_body="{\"host\":\"https://tilecache.rainviewer.com\",\"radar\":{\"past\":[{\"time\":1700000000,\"path\":\"/v2/radar/1700000000\"}]}}";
  radar_http_test=true;mixed_forecast_body=provider_response;
  radar_image_requests=radar_base_requests=0;
}
static void worker_radar_cleanup(void)
{
  assert(!fail_weather_copy);
  lv_free(worker_base_png);lv_free(worker_echo_png);
  free(radar.base);free(radar.image[0]);free(radar.image[1]);radar_clear_history();
  memset(&radar,0,sizeof(radar));radar_http_test=false;lv_deinit();
}
static void acknowledge_worker_radar(void)
{
  weather_client_radar_acknowledge(weather_completed.radar.generation);
}
static void move_wall_clock_on_radar_wake(void)
{
  acknowledge_worker_radar();
  if (xTaskGetTickCount()==300000) fake_clock_set(100);
}
static void hold_completed_radar_copy(void)
{
  if (radar.phase==RADAR_ECHO && radar.tile_x==radar.geometry.last_x &&
      radar.tile_y==radar.geometry.last_y) {
    fail_weather_copy=true;
    on_request=NULL;
  }
}
static void observe_completed_radar_copy(void)
{
  assert(weather_completed.radar.generation==frozen_radar.generation);
  uint32_t previous_ack=radar_displayed_generation;
  assert(previous_ack!=frozen_radar.generation);
  app_weather_snapshot_t weather;app_radar_snapshot_t copy;
  assert(weather_client_copy(&weather,&copy));
  assert(copy.generation==frozen_radar.generation && radar_displayed_generation==previous_ack);
  copied_before_ack=true;acknowledge_worker_radar();on_delay=acknowledge_worker_radar;
}
static void resume_radar_publication(void)
{
  if (!fail_weather_copy) { acknowledge_worker_radar();return; }
  ++frozen_delays;
  if (frozen_delays==1) {
    frozen_radar=radar.published;frozen_requests=http_requests;
    assert(frozen_radar.valid && frozen_radar.generation!=weather_completed.radar.generation);
    app_weather_snapshot_t weather={.temp=-99};app_radar_snapshot_t copy={.generation=99};
    assert(!weather_client_copy(&weather,&copy));assert(weather.temp==-99 && copy.generation==99);
  } else {
    assert(!memcmp(&frozen_radar,&radar.published,sizeof(frozen_radar)));
    assert(http_requests==frozen_requests);
    fail_weather_copy=false;
    on_delay=observe_completed_radar_copy;
  }
}
static void check_worker_radar_schedule(void)
{
  worker_radar_setup();on_delay=move_wall_clock_on_radar_wake;
  run_worker_until(600000);
  assert(metadata_requests==3 && metadata_ticks[0]==100 && metadata_ticks[1]==300000 && metadata_ticks[2]==600000);
  assert(forecast_requests==2 && forecast_ticks[0]==0 && forecast_ticks[1]==600000);
  assert(weather_completed.radar.valid && radar_displayed_generation==weather_completed.radar.generation);
  worker_radar_cleanup();

  worker_radar_setup();sync_failures=2;radar_request_duration_ms=9900;
  on_delay=acknowledge_worker_radar;run_worker_until(609900);
  assert(sync_calls>=3 && sync_ticks[0]==0);
  assert(sync_ticks[1]>=20000 && sync_ticks[1]<30000 && sync_ticks[2]>=40000 && sync_ticks[2]<50000);
  assert(forecast_requests==2 && forecast_ticks[1]==600000);
  assert(metadata_requests==3 && metadata_ticks[1]==300000 && metadata_ticks[2]==600000);
  worker_radar_cleanup();

  worker_radar_setup();frozen_delays=0;copied_before_ack=false;on_request=hold_completed_radar_copy;
  on_delay=resume_radar_publication;run_worker_until(300000);
  assert(frozen_delays==2 && weather_copy_failures>=3 && copied_before_ack && weather_completed.radar.valid);
  assert(weather_completed.radar.generation==frozen_radar.generation);
  /* Reading a publication cannot authorize output-slot reuse. */
  uint32_t before_ack=radar_displayed_generation;
  app_weather_snapshot_t weather;app_radar_snapshot_t copy;
  assert(weather_client_copy(&weather,&copy));assert(radar_displayed_generation==before_ack);
  worker_radar_cleanup();
}
static void lose_worker_radar_connection(void)
{
  acknowledge_worker_radar();
  if (wifi_connected && (weather_completed.radar.valid ||
      (disconnect_during_preparation && radar.phase==RADAR_ECHO))) {
    wifi_connected=false;wifi_wait_result=ESP_ERR_TIMEOUT;
  }
}
static void allow_first_worker_radar_connection(void)
{
  assert(!weather_completed.radar.valid);
  if (xTaskGetTickCount()<=20000) {
    assert(!metadata_requests && !forecast_requests);
    if (xTaskGetTickCount()==20000) wifi_wait_result=ESP_OK;
  }
  acknowledge_worker_radar();
}
static void check_worker_radar_offline(void)
{
  worker_radar_setup();disconnect_during_preparation=false;on_delay=lose_worker_radar_connection;
  run_worker_until(900000);
  assert(metadata_requests==1 && forecast_requests==1);
  assert(weather_completed.radar.valid && weather_completed.radar.stale);
  assert(weather_completed.radar.frame_time==1700000000);
  worker_radar_cleanup();

  worker_radar_setup();disconnect_during_preparation=true;on_delay=lose_worker_radar_connection;
  run_worker_until(20400);
  assert(metadata_requests==1 && forecast_requests==1 && !weather_completed.radar.valid);
  assert(weather_completed.radar.stale && radar.phase==RADAR_IDLE);
  worker_radar_cleanup();

  worker_radar_setup();wifi_connected=false;wifi_wait_result=ESP_ERR_TIMEOUT;
  on_delay=allow_first_worker_radar_connection;run_worker_until(40100);
  assert(forecast_requests==1 && forecast_ticks[0]==40000);
  assert(metadata_requests==1 && metadata_ticks[0]==40100);
  worker_radar_cleanup();
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
  assert_forecast(&weather_completed.weather);
  assert(!weather_completed.weather.stale);
  assert(weather_completed.weather.updated_at == 0 && wifi_init_calls == 1);
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
    assert_forecast(&weather_completed.weather);
    assert(weather_completed.weather.updated_at ==
      (source == WAC_TIME_SOURCE_NETWORK ? download_clock.utc_now : 0));
  }
}

static void check_validation_nonpublication(void)
{
  reset_platform();download_clock=(wac_time_snapshot_t){.usable=true,
    .source=WAC_TIME_SOURCE_NETWORK,.utc_now=1798674600};
  run_worker(1);
  app_weather_snapshot_t applied;app_radar_snapshot_t retained_radar;
  assert(weather_client_copy(&applied,&retained_radar));
  cJSON *root=cJSON_Parse(provider_response);assert(root);
  cJSON *current=cJSON_GetObjectItemCaseSensitive(root,"current_weather");
  assert(cJSON_ReplaceItemInObjectCaseSensitive(current,"temperature",cJSON_CreateNumber(41)));
  char *body=cJSON_PrintUnformatted(root);assert(body);http_body=body;
  device_config_t draft=applied_config;strcpy(draft.units,"metric");
  app_weather_snapshot_t validation={0};
  assert(weather_client_fetch(&draft,&validation)==ESP_OK && validation.temp==41);
  app_weather_snapshot_t after;app_radar_snapshot_t after_radar;
  assert(weather_client_copy(&after,&after_radar));
  assert(!memcmp(&applied,&after,sizeof(applied)) && !memcmp(&retained_radar,&after_radar,sizeof(after_radar)));
  free(body);cJSON_Delete(root);
}

static void check_time_start_failure(void)
{
  reset_platform();
  sync_failures = 2;
  run_worker_until(600000);
  assert(sync_calls >= 3 && sync_ticks[0] == 0 && sync_ticks[1] == 20000 && sync_ticks[2] == 40000);
  assert(http_requests == 2 && request_ticks[0] == 0 && request_ticks[1] == 600000);
  assert(delays[0] == 20000 && delays[1] == 20000 && delays[2] == 260000);
  assert_forecast(&weather_completed.weather);
  assert(!weather_completed.weather.stale && !actual_time_synced);
}

static void check_location_required(void)
{
  reset_platform();
  applied_config.location_ready = false;
  run_worker(2);
  assert(http_requests == 0 && !weather_completed.weather.valid);
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
    assert(http_requests == 0 && !weather_completed.weather.valid);
    assert(wifi_init_calls == (scenario == 0 ? 0 : scenario == 1 ? 2 : 1));
    assert(delays[0] == 20000 && delays[1] == 20000);
  }
}

static void observe_weather_notice_at_request(void)
{
  /* Observe in-flight retries: publication keeps the previous payload and
   * stale state until success; an initial failure never invents data. */
  if (http_requests == 0) assert(!weather_completed.weather.valid);
  else if (request_results[http_requests - 1] != ESP_OK)
    assert(!weather_completed.weather.valid || weather_completed.weather.stale);
  else assert(weather_completed.weather.valid);
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
  assert_forecast(&weather_completed.weather);
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
  assert_forecast(&weather_completed.weather);
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
  assert_forecast(&weather_completed.weather);
  assert(!weather_completed.weather.stale && !actual_time_synced);
}

static app_weather_snapshot_t retained_weather;
static bool retained_weather_seen, stale_weather_seen;

static void observe_retention(void)
{
  if (!retained_weather_seen) {
    retained_weather_seen = true;
    retained_weather = weather_completed.weather;
    assert_forecast(&retained_weather);
  }
  if (weather_completed.weather.stale) {
    stale_weather_seen = true;
    app_weather_snapshot_t stale = weather_completed.weather;
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
  assert_forecast(&weather_completed.weather);
  assert(!weather_completed.weather.stale);
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
    assert(weather_completed.weather.stale);
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
  assert_forecast(&weather_completed.weather);
  assert(!weather_completed.weather.stale);
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
      /* A strong gust belongs only to the first repeated local hour. */
      cJSON_AddNumberToObject(hour,"windgust",i==0&&h==0?(metric_units?80:50):(metric_units?24.14016:15));
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
    assert(strcmp(weather.icon_name,"hail")==0 && strcmp(weather.summary,"Hail & Thunder")==0);
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
    assert(weather.valid);
    cJSON *hour=cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(root,"days"),0),"hours"),1);
    cJSON_ReplaceItemInObjectCaseSensitive(hour,"windspeed",cJSON_CreateNumber(units?40:25));
    free(optional); optional=cJSON_PrintUnformatted(root); assert(optional); http_body=optional;
    assert(weather_client_fetch(&applied_config,&weather)==ESP_OK);
    app_weather_snapshot_t retained=weather;
    cJSON_DeleteItemFromObjectCaseSensitive(cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(root,"days"),5),"tempmin");
    char *broken=cJSON_PrintUnformatted(root); assert(broken); http_body=broken;
    assert(weather_client_fetch(&applied_config,&weather)!=ESP_OK);
    assert(memcmp(&weather,&retained,sizeof(weather))==0);
    free(broken); free(optional);cJSON_Delete(root);
    root=cJSON_Parse(body);assert(root);current=cJSON_GetObjectItemCaseSensitive(root,"currentConditions");
    assert(cJSON_ReplaceItemInObjectCaseSensitive(current,"conditions",cJSON_CreateString("type_43")));
    cJSON_DeleteItemFromObjectCaseSensitive(current,"icon");
    char *day_body=cJSON_PrintUnformatted(root);assert(day_body);http_body=day_body;
    assert(weather_client_fetch(&applied_config,&weather)==ESP_OK);
    assert(!strcmp(weather.icon_name,"clear-night"));free(day_body);
    assert(cJSON_ReplaceItemInObjectCaseSensitive(current,"datetimeEpoch",cJSON_CreateNumber(1793556000)));
    day_body=cJSON_PrintUnformatted(root);assert(day_body);http_body=day_body;
    assert(weather_client_fetch(&applied_config,&weather)==ESP_OK);
    assert(!strcmp(weather.icon_name,"clear-day"));free(day_body);
    retained=weather;
    const double invalid_epochs[]={-1,4102444801.0};
    for (size_t i=0;i<sizeof(invalid_epochs)/sizeof(invalid_epochs[0]);++i) {
      assert(cJSON_ReplaceItemInObjectCaseSensitive(current,"datetimeEpoch",cJSON_CreateNumber(invalid_epochs[i])));
      day_body=cJSON_PrintUnformatted(root);assert(day_body);http_body=day_body;
      assert(weather_client_fetch(&applied_config,&weather)!=ESP_OK && !memcmp(&weather,&retained,sizeof(weather)));
      free(day_body);
    }
    cJSON_DeleteItemFromObjectCaseSensitive(current,"datetimeEpoch");
    day_body=cJSON_PrintUnformatted(root);assert(day_body);http_body=day_body;
    assert(weather_client_fetch(&applied_config,&weather)!=ESP_OK && !memcmp(&weather,&retained,sizeof(weather)));
    free(day_body);free(body);cJSON_Delete(root);
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
  assert(strcmp(weather.icon_name,"thunderstorm")==0);
  cJSON_ReplaceItemInObjectCaseSensitive(amounts,"snowfall",cJSON_CreateNumber(0.2));
  free(body); body=cJSON_PrintUnformatted(root); http_body=body;
  assert(weather_client_fetch(&applied_config,&weather)==ESP_OK);
  assert(strcmp(weather.icon_name,"thundersnow")==0);
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
  acknowledge_worker_radar();
  if (!retained_weather_seen) {
    retained_weather_seen=true;
    retained_weather=weather_completed.weather;
    assert(retained_weather.valid);
    mixed_forecast_status=429;mixed_forecast_body="Daily cost limit exceeded";
  } else if (forecast_requests >= 2) {
    stale_weather_seen=true;
    app_weather_snapshot_t stale=weather_completed.weather;
    assert(stale.stale);stale.stale=false;
    assert(memcmp(&stale,&retained_weather,sizeof(stale))==0);
  }
}
static void check_worker_quota_schedule(void)
{
  worker_radar_setup();use_vc("quota-worker-retention");
  retained_weather_seen=stale_weather_seen=false;
  char *body=make_vc_response(false);http_body=body;mixed_forecast_body=body;on_delay=quota_retention_delay;
  run_worker_until(4200000);
  assert(forecast_requests==3 && forecast_ticks[0]==0 && forecast_ticks[1]==600000 && forecast_ticks[2]==4200000);
  assert(stale_weather_seen && metadata_requests==15);
  for (unsigned i=1;i<metadata_requests;++i) assert(metadata_ticks[i]==i*300000);
  free(body);worker_radar_cleanup();
  reset_platform();use_vc("quota-worker-transient");
  http_status=429;http_body="Maximum concurrent jobs has been exceeded";
  run_worker(2);
  assert(http_requests==2 && request_ticks[1]==20000 && delays[0]==20000 && delays[1]==20000);
}

static void check_worker_service_independence(void)
{
  worker_radar_setup();use_vc("worker-mixed-quota");
  mixed_forecast_status=429;mixed_forecast_body="Daily cost limit exceeded";
  nws_http_test=true;mixed_nws_body="{\"type\":\"FeatureCollection\",\"features\":[]}";
  nws_status=APP_ALERT_CHECKING;radar_request_duration_ms=9900;sync_failures=2;
  on_delay=acknowledge_worker_radar;run_worker_until(609900);
  assert(forecast_requests==1 && forecast_ticks[0]==0);
  assert(weather_client_next_allowed_us(&applied_config)==INT64_C(3600000000));
  assert(nws_requests==11 && metadata_requests==3);
  for (unsigned i=0;i<nws_requests;++i) assert(nws_ticks[i]==i*60000);
  assert(metadata_ticks[0]==100 && metadata_ticks[1]==300000 && metadata_ticks[2]==600000);
  assert(sync_ticks[0]==0 && sync_ticks[1]>=20000 && sync_ticks[1]<30000 && sync_ticks[2]>=40000 && sync_ticks[2]<50000);
  assert(weather_completed.radar.valid && nws_status==APP_ALERT_CURRENT);
  worker_radar_cleanup();
  puts("PASS: actual worker keeps NWS minute checks, Radar checks and time startup independent of a forecast daily quota during slow real PNG requests.");
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
  assert(isfinite(number(field(current,"datetimeEpoch"))));
  assert(weather.temp==(int)round(number(field(current,"temp"))));
  for (int i=0;i<5;++i) {
    const cJSON *day=at(days,i+1);
    assert(strcmp(weather.forecast[i].date,string(field(day,"datetime")))==0);
    assert(weather.forecast[i].high==(int)round(number(field(day,"tempmax"))));
    assert(weather.forecast[i].low==(int)round(number(field(day,"tempmin"))));
  }
  cJSON *hourly=normalize_vc_hourly(&applied_config,root);assert(hourly);
  int index=0,fractional_precip=0,missing_record_offsets=0;
  for (int d=0;d<cJSON_GetArraySize(days);++d) {
    const cJSON *day=at(days,d), *hours=field(day,"hours");
    for (int h=0;h<cJSON_GetArraySize(hours);++h,++index) {
      const cJSON *hour=at(hours,h);
      char expected_time[40];snprintf(expected_time,sizeof(expected_time),"%sT%s",string(field(day,"datetime")),string(field(hour,"datetime")));
      assert(strcmp(string(at(field(hourly,"time"),index)),expected_time)==0);
      assert(number(at(field(hourly,"epoch"),index))==number(field(hour,"datetimeEpoch")));
      double precip=number(field(hour,"precip"));
      if (isfinite(precip)) {
        assert(fabs(number(at(field(hourly,"precipitation"),index))-precip*25.4)<1e-10);
        if (precip!=round(precip)) ++fractional_precip;
      }
      double snow=number(field(hour,"snow"));
      if (isfinite(snow)) assert(fabs(number(at(field(hourly,"snowfall"),index))-snow*2.54)<1e-10);
      if (!field(day,"tzoffset")&&!field(hour,"tzoffset")) ++missing_record_offsets;
    }
  }
  assert(index==cJSON_GetArraySize(field(hourly,"time")));
  assert(fractional_precip>0 && missing_record_offsets>0);
  printf("PASS: saved live Visual Crossing response decoded offline: current epoch, five dates/highs/lows, %d local hours, %d fractional precipitation values, %d hours without individual offsets.\n",index,fractional_precip,missing_record_offsets);
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
/* Test-only copy of the old scan is the independent equivalence oracle. */
static bool radar_reference_map_pixel(uint8_t rgba[4])
{
  if (!rgba[3]) return true;
  uint32_t source=((uint32_t)rgba[0]<<24)|((uint32_t)rgba[1]<<16)|((uint32_t)rgba[2]<<8);
  for (unsigned i=0; i<128; ++i)
    if ((radar_source_rgba[i]&255) && (radar_source_rgba[i]&0xFFFFFF00u)==source) {
      radar_color((int)i-32,rgba);return true;
    }
  return false;
}
static void check_radar_lookup_equivalence(void)
{
  for (unsigned i=0; i<128; ++i) for (unsigned alpha=0; alpha<=255; ++alpha) {
    uint32_t source=radar_source_rgba[i];
    uint8_t actual[4]={source>>24,source>>16,source>>8,alpha},expected[4];
    memcpy(expected,actual,4);
    assert(radar_map_pixel(actual)==radar_reference_map_pixel(expected));
    assert(!memcmp(actual,expected,4));
  }
  assert(radar_lookup_count==77);
  for (unsigned alpha=0; alpha<=255; ++alpha) {
    uint8_t actual[4]={1,2,3,alpha},expected[4];memcpy(expected,actual,4);
    assert(radar_map_pixel(actual)==radar_reference_map_pixel(expected));
    assert(!memcmp(actual,expected,4));
  }
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
  /* A half-native-pixel step uses every 512px source index once: output
   * 1 uses source 0, output 512 uses 511, and other edges stay untouched.
   * Constructed altered alpha verifies support beyond the captured tiles. */
  uint8_t *pixels=malloc(512*512*4), *mapped=malloc(512*512*4);
  uint8_t *expected=malloc(RADAR_PIXELS*4);
  assert(pixels && mapped && expected);
  for (unsigned y=0; y<512; ++y) for (unsigned x=0; x<512; ++x) {
    uint32_t color=radar_source_rgba[(x*17+y*29)%128];
    uint8_t *rgba=pixels+((size_t)y*512+x)*4;
    rgba[0]=color>>24;rgba[1]=color>>16;rgba[2]=color>>8;
    rgba[3]=(uint8_t)color && (x+y)%7 ? 127 : 0;
    uint8_t *target=mapped+((size_t)y*512+x)*4;
    memcpy(target,rgba,4);assert(radar_reference_map_pixel(target));
  }
  unsigned char *png=NULL;size_t size;
  assert(lodepng_encode32(&png,&size,pixels,512,512)==0);
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
    const uint8_t *rgba=mapped+((size_t)(y-1)*512+x-1)*4;
    uint8_t *bgra=expected+((size_t)y*APP_RADAR_WIDTH+x)*4;
    for (unsigned c=0; c<3; ++c)
      bgra[c]=(uint8_t)((rgba[2-c]*rgba[3]+bgra[c]*(255-rgba[3])+127)/255);
  }
  radar.tile_count=1;radar.work_frame=0;
  assert(radar_tile_image(&radar.frames[0],0,true)==ESP_OK);
  assert(memcmp(radar.image[0],expected,RADAR_PIXELS*4)==0);
  radar_free_frame(&radar.frames[0]);
  lv_free(png);
  /* This unknown opaque source pixel is outside all selected output samples.
   * Full source validation must still reject it before changing any output. */
  pixels[0]=1;pixels[1]=2;pixels[2]=3;pixels[3]=255;
  assert(lodepng_encode32(&png,&size,pixels,512,512)==0);
  radar_echo_png=png;radar_echo_png_size=size;
  radar.geometry=(radar_geometry_t){.left=356,.top=356,.step=0.125};
  assert(radar_tile_image(&radar.frames[0],0,true)==ESP_ERR_INVALID_RESPONSE);
  radar_free_frame(&radar.frames[0]);
  assert(memcmp(radar.image[0],expected,RADAR_PIXELS*4)==0);
  lv_free(png);free(pixels);free(mapped);free(expected);
  free(radar.image[0]);radar.image[0]=NULL;radar_http_test=false;
}

static void check_radar_tile_seams(void)
{
  /* Constructed partial-opacity tiles must cover every output pixel exactly
   * once, including both sides of the native 256px seam and longitude wrap. */
  const uint8_t color[4]={255,238,0,127};
  size_t size;
  unsigned char *png=radar_test_png(512,512,color,&size);
  radar_http_test=true;http_status=200;radar_echo_png=png;radar_echo_png_size=size;
  radar.geometry=(radar_geometry_t){.left=-0.5,.top=255.5,.step=0.5,
    .first_x=-1,.last_x=1,.first_y=0,.last_y=2};
  radar.slot=0;radar.image[0]=malloc(RADAR_PIXELS*4);assert(radar.image[0]);
  const uint8_t background[4]={19,31,43,255};
  uint8_t expected[4]={0,232,255,255};
  for (unsigned c=0; c<3; ++c)
    expected[c]=(uint8_t)((expected[c]*127+background[c]*128+127)/255);
  for (size_t pixel=0; pixel<RADAR_PIXELS; ++pixel)
    memcpy(radar.image[0]+pixel*4,background,4);
  radar_first_tile(&radar.geometry);
  radar.tile_count=9;radar.work_frame=0;
  unsigned tiles=0;
  do {
    assert(radar_tile()==ESP_OK);
    assert(radar_requested_x==(radar.tile_x<0 ? 127 : radar.tile_x));
    assert(radar_requested_y==radar.tile_y);
    ++tiles;
  } while (radar_next_tile(&radar.geometry));
  assert(tiles==9);
  radar_free_frame(&radar.frames[0]);
  for (size_t pixel=0; pixel<RADAR_PIXELS; ++pixel)
    assert(memcmp(radar.image[0]+pixel*4,expected,4)==0);
  free(radar.image[0]);radar.image[0]=NULL;lv_free(png);radar_http_test=false;
}

static void check_radar_geometry_palette_png(void)
{
  check_radar_download_growth();
  check_radar_lookup_equivalence();
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
  const struct { uint8_t source[4],expected[3]; } source_cases[]={
    {{0,71,104,127},{5,107,30}},       /* 34: green side of the boundary. */
    {{255,238,0,127},{255,232,0}},     /* 35: yellow begins. */
    {{255,129,0,127},{255,153,0}},     /* 44: yellow side of the boundary. */
    {{255,68,0,127},{240,49,36}},      /* 45: red begins. */
    {{255,255,255,127},{152,6,21}},    /* Repeated white uses 65, not 74. */
    {{0,255,0,127},{115,0,17}}};      /* Repeated green uses 75, not 95. */
  for (unsigned i=0; i<sizeof(source_cases)/sizeof(source_cases[0]); ++i) {
    uint8_t rgba[4];memcpy(rgba,source_cases[i].source,4);
    assert(radar_map_pixel(rgba) && rgba[3]==127);
    assert(memcmp(rgba,source_cases[i].expected,3)==0);
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
      /* These altered alphas are constructed, not observed provider values. */
      static const uint8_t altered_alpha[]={1,17,127,254};
      for (unsigned a=0; a<sizeof(altered_alpha); ++a) {
        uint8_t altered[4]={(uint8_t)(packed>>24),(uint8_t)(packed>>16),
          (uint8_t)(packed>>8),altered_alpha[a]};
        assert(radar_map_pixel(altered) && altered[3]==altered_alpha[a]);
        assert(memcmp(altered,expected,3)==0);
      }
    }
  }
  uint8_t transparent[4]={100,9,200,0}, unknown[4]={1,2,3,255}, black[4]={0,0,0,1};
  assert(radar_map_pixel(transparent) && transparent[3]==0);
  assert(!radar_map_pixel(unknown));
  assert(!radar_map_pixel(black)); /* Clear table rows do not support visible black. */
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
  unsigned char *png=radar_test_png(512,512,color,&size);
  http_response_t response={.data=(char *)png,.length=size};
  lv_draw_buf_t *decoded=NULL;
  lv_mem_monitor_t before,after;lv_mem_monitor(&before);
  assert(radar_decode_png(&response,false,&decoded)==ESP_OK && decoded);
  assert(memcmp(decoded->data,color,4)==0);
  assert(decoded->header.w==512 && decoded->header.h==512);
  lv_draw_buf_destroy(decoded);
  lv_mem_monitor(&after);assert(after.free_size==before.free_size);
  void *held[64];unsigned held_count=0;
  while (held_count<64 && (held[held_count]=lv_malloc(256*1024))) ++held_count;
  assert(held_count && held_count<64);
  assert(radar_decode_png(&response,false,&decoded)!=ESP_OK && !decoded);
  while (held_count) lv_free(held[--held_count]);
  lv_mem_monitor(&after);assert(after.free_size==before.free_size);
  png[size-1]^=1; /* Corrupt a chunk CRC without changing the dimensions. */
  assert(radar_decode_png(&response,false,&decoded)!=ESP_OK && !decoded);
  png[size-1]^=1;
  response.length=size/2;
  assert(radar_decode_png(&response,false,&decoded)!=ESP_OK && !decoded);
  lv_free(png);
  png=radar_test_png(256,256,color,&size);
  response=(http_response_t){.data=(char *)png,.length=size};
  assert(radar_decode_png(&response,false,&decoded)!=ESP_OK && !decoded);
  lv_free(png);
  png=radar_test_png(513,512,color,&size);
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
  check_radar_tile_seams();
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
  unsigned char *echo=radar_test_png(512,512,echo_color,&radar_echo_png_size);
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
  /* A valid PNG with an unsupported visible color also keeps the completed
   * frame and timestamp, then allows an ordinary incomplete-image retry. */
  const uint8_t unsupported[4]={1,2,3,127};size_t unsupported_size;
  unsigned char *invalid=radar_test_png(512,512,unsupported,&unsupported_size);
  radar_echo_png=invalid;radar_echo_png_size=unsupported_size;
  acknowledged=snapshot.generation;
  assert(!weather_client_radar_step(&applied_config,WEATHER_RADAR_CHECK,acknowledged,&snapshot,&active));
  assert(!weather_client_radar_step(&applied_config,WEATHER_RADAR_CONTINUE,acknowledged,&snapshot,&active));
  assert(weather_client_radar_step(&applied_config,WEATHER_RADAR_CONTINUE,acknowledged,&snapshot,&active));
  assert(!active && snapshot.valid && snapshot.stale && snapshot.pixels==second_pixels);
  assert(snapshot.frame_time==1700000600);
  lv_free(invalid);
  /* A complete transparent echo image is valid and removes prior echoes. */
  const uint8_t clear_color[4]={0,0,0,0};
  size_t clear_size;
  unsigned char *clear=radar_test_png(512,512,clear_color,&clear_size);
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
  radar_clear_history();memset(&radar,0,sizeof(radar));
  radar_http_test=false;
}

static bool radar_test_step(app_radar_snapshot_t *snapshot,uint32_t *ack,bool *active)
{
  unsigned requests=http_requests;
  bool result=weather_client_radar_step(&applied_config,WEATHER_RADAR_CONTINUE,*ack,snapshot,active);
  assert(http_requests-requests<=1);
  if (result) *ack=snapshot->generation;
  return result;
}
static void radar_test_drain(app_radar_snapshot_t *snapshot,uint32_t *ack,bool *active)
{
  unsigned steps=0;
  while (*active && steps++<256) radar_test_step(snapshot,ack,active);
  assert(!*active && steps<256);
}
static time_t radar_pause_http_time;
static void radar_pause_during_http(void)
{
  on_request=NULL;
  assert(weather_client_radar_control(APP_RADAR_PAUSE,radar_pause_http_time)==ESP_OK);
}
static void check_radar_history_playback(void)
{
  reset_platform();assert(weather_client_init_images()==ESP_OK);radar_http_test=true;
  radar_image_requests=radar_base_requests=0;
  strcpy(applied_config.location,"Chicago, Illinois, US");
  const uint8_t base_color[4]={210,200,180,255},echo_color[4]={255,238,0,127};
  unsigned char *base=radar_test_png(800,800,base_color,&radar_base_png_size);
  unsigned char *echo=radar_test_png(512,512,echo_color,&radar_echo_png_size);
  radar_base_png=base;radar_echo_png=echo;
  /* Out-of-order, duplicate, too-old and off-interval entries are constructed. */
  radar_metadata_body="{\"host\":\"https://tilecache.rainviewer.com\",\"radar\":{\"past\":["
    "{\"time\":1700000000,\"path\":\"/latest\"},"
    "{\"time\":1699996400,\"path\":\"/excluded-hour-boundary\"},"
    "{\"time\":1699997000,\"path\":\"/oldest\"},"
    "{\"time\":1699998800,\"path\":\"/fourth\"},"
    "{\"time\":1699997600,\"path\":\"/second\"},"
    "{\"time\":1699998200,\"path\":\"/third\"},"
    "{\"time\":1699999400,\"path\":\"/fifth\"},"
    "{\"time\":1699999400,\"path\":\"/duplicate\"},"
    "{\"time\":1699999999,\"path\":\"/off-interval\"}]}}";
  app_radar_snapshot_t snapshot={0};bool active=false;uint32_t ack=0;
  assert(weather_client_radar_step(&applied_config,WEATHER_RADAR_CHECK,ack,&snapshot,&active));
  ack=snapshot.generation;
  while (!snapshot.valid) radar_test_step(&snapshot,&ack,&active);
  assert(snapshot.frame_time==1700000000 && snapshot.history_count==1 && snapshot.history_loading);
  assert(radar_base_requests==1);
  unsigned latest_requests=radar_image_requests;
  radar_test_drain(&snapshot,&ack,&active);
  assert(snapshot.history_count==6 && !snapshot.history_loading);
  assert(radar.frame_count==6 && radar.cache_bytes>0 && radar.cache_bytes<=RADAR_CACHE_MAX);
  assert(radar_image_requests==(latest_requests-1)*6+1);
  for (unsigned i=0; i<6; ++i) assert(radar.frames[i].time==1699997000+(time_t)i*600);
  assert(!strcmp(radar.frames[4].path,"/fifth"));
  unsigned requests=radar_image_requests;
  const uint8_t *latest_pixels=snapshot.pixels;
  weather_client_radar_step(&applied_config,WEATHER_RADAR_CHECK,ack,&snapshot,&active);
  radar_test_drain(&snapshot,&ack,&active);
  assert(radar_image_requests==requests && snapshot.pixels==latest_pixels);

  /* Play each available frame once, publish completion, then hold Latest. */
  unsigned pass_requests=http_requests;
  assert(weather_client_radar_control(APP_RADAR_PLAY,snapshot.frame_time)==ESP_OK);
  radar_test_step(&snapshot,&ack,&active);
  for (unsigned i=0;i<6;++i) {
    time_t expected=1699997000+(time_t)i*600;
    fake_clock_advance(1500);
    unsigned steps=0;
    while (snapshot.frame_time!=expected && steps++<16) radar_test_step(&snapshot,&ack,&active);
    assert(snapshot.frame_time==expected && steps<16);
  }
  uint32_t final_generation=snapshot.generation;
  const uint8_t *final_pixels=snapshot.pixels;
  assert(radar_test_step(&snapshot,&ack,&active));
  assert(snapshot.generation>final_generation && !active && !snapshot.playing && !snapshot.paused);
  fake_clock_advance(15000);
  for (unsigned i=0;i<8;++i) assert(!radar_test_step(&snapshot,&ack,&active));
  assert(snapshot.frame_time==1700000000 && snapshot.pixels==final_pixels && http_requests==pass_requests);
  /* Resuming a manual Pause on the newest frame also ends without rewinding. */
  assert(weather_client_radar_control(APP_RADAR_PAUSE,snapshot.frame_time)==ESP_OK);
  radar_test_step(&snapshot,&ack,&active);
  assert(snapshot.paused);
  assert(weather_client_radar_control(APP_RADAR_PLAY,snapshot.frame_time)==ESP_OK);
  radar_test_step(&snapshot,&ack,&active);
  assert(radar_test_step(&snapshot,&ack,&active));
  assert(!snapshot.playing && !snapshot.paused && !active && snapshot.frame_time==1700000000);

  /* Play after completion starts a fresh pass at the oldest frame. */
  assert(weather_client_radar_control(APP_RADAR_PLAY,snapshot.frame_time)==ESP_OK);
  assert(weather_client_radar_control_pending());
  radar_test_step(&snapshot,&ack,&active);
  assert(snapshot.playing && !snapshot.paused && active && !weather_client_radar_control_pending());
  while (snapshot.frame_time!=1699997000) radar_test_step(&snapshot,&ack,&active);
  int64_t first_us=esp_timer_get_time();
  while (radar.phase!=RADAR_READY) radar_test_step(&snapshot,&ack,&active);
  assert(snapshot.frame_time==1699997000 && radar_image_requests==requests);
  fake_clock_advance(1499);radar_test_step(&snapshot,&ack,&active);
  assert(snapshot.frame_time==1699997000);
  fake_clock_advance(1);radar_test_step(&snapshot,&ack,&active);
  assert(snapshot.frame_time==1699997600 && esp_timer_get_time()-first_us==INT64_C(1500000));

  /* Delayed UI ack: publish a newer frame while UI still displays the old one,
   * then Pause must restore the old slot, with its exact original bytes. */
  time_t displayed_time=snapshot.frame_time;const uint8_t *displayed=snapshot.pixels;
  uint8_t displayed_center[4];memcpy(displayed_center,displayed+RADAR_PIXELS*2,4);
  while (radar.phase!=RADAR_READY) radar_test_step(&snapshot,&ack,&active);
  fake_clock_advance(5000); /* No catch-up: one frame only, then reset the deadline. */
  assert(weather_client_radar_step(&applied_config,WEATHER_RADAR_CONTINUE,ack,&snapshot,&active));
  assert(snapshot.frame_time==displayed_time+600);
  uint32_t pending_generation=snapshot.generation;
  assert(weather_client_radar_control(APP_RADAR_PAUSE,displayed_time)==ESP_OK);
  assert(weather_client_radar_step(&applied_config,WEATHER_RADAR_CONTINUE,ack,&snapshot,&active));
  assert(snapshot.generation>pending_generation && snapshot.paused && !snapshot.playing);
  assert(snapshot.frame_time==displayed_time && snapshot.pixels==displayed);
  assert(!memcmp(displayed+RADAR_PIXELS*2,displayed_center,4));
  ack=snapshot.generation;
  fake_clock_advance(20000);
  for (unsigned i=0;i<8;++i) radar_test_step(&snapshot,&ack,&active);
  assert(snapshot.frame_time==displayed_time && snapshot.pixels==displayed && !active);

  /* Refresh metadata while paused. Five cache records survive; the selected
   * output remains frozen even after its history record eventually ages out. */
  radar_metadata_body="{\"host\":\"https://tilecache.rainviewer.com\",\"radar\":{\"past\":["
    "{\"time\":1700000600,\"path\":\"/new-latest\"},"
    "{\"time\":1700000000,\"path\":\"/latest\"},"
    "{\"time\":1699999400,\"path\":\"/fifth\"},"
    "{\"time\":1699998800,\"path\":\"/fourth\"},"
    "{\"time\":1699998200,\"path\":\"/third\"},"
    "{\"time\":1699997600,\"path\":\"/second\"}]}}";
  if (weather_client_radar_step(&applied_config,WEATHER_RADAR_CHECK,ack,&snapshot,&active)) ack=snapshot.generation;
  radar_test_drain(&snapshot,&ack,&active);
  assert(snapshot.paused && snapshot.frame_time==displayed_time && snapshot.pixels==displayed);
  assert(snapshot.history_count==6 && radar_image_requests==requests+latest_requests-1);
  requests=radar_image_requests;
  assert(weather_client_radar_control(APP_RADAR_PLAY,displayed_time)==ESP_OK);
  radar_test_step(&snapshot,&ack,&active);
  while (snapshot.frame_time!=displayed_time+600) radar_test_step(&snapshot,&ack,&active);
  /* CHECK while playing uses one metadata request, retains same tile bytes and
   * continues playback. A failed check keeps old indication on cached output. */
  unsigned before=http_requests;
  weather_client_radar_step(&applied_config,WEATHER_RADAR_CHECK,ack,&snapshot,&active);
  assert(http_requests==before+1 && active && radar.playing && radar_image_requests==requests);
  http_status=503;
  assert(weather_client_radar_step(&applied_config,WEATHER_RADAR_CHECK,ack,&snapshot,&active));ack=snapshot.generation;
  assert(snapshot.stale && snapshot.playing && active);
  http_status=200;
  time_t before_frame=snapshot.frame_time;
  fake_clock_advance(2000);
  while (snapshot.frame_time==before_frame) radar_test_step(&snapshot,&ack,&active);
  assert(snapshot.stale);
  if (weather_client_radar_step(&applied_config,WEATHER_RADAR_CHECK,ack,&snapshot,&active)) ack=snapshot.generation;
  assert(!snapshot.stale);

  assert(weather_client_radar_control(APP_RADAR_LATEST,snapshot.frame_time)==ESP_OK);
  radar_test_step(&snapshot,&ack,&active);radar_test_drain(&snapshot,&ack,&active);
  assert(!snapshot.playing && !snapshot.paused && snapshot.frame_time==1700000600);
  assert(radar_image_requests==requests && radar_base_requests==1);
  /* A control arriving on the five-minute deadline cannot consume its CHECK.
   * The copied Pause is published now and metadata runs on the next step. */
  before=http_requests;
  assert(weather_client_radar_control(APP_RADAR_PAUSE,snapshot.frame_time)==ESP_OK);
  assert(weather_client_radar_step(&applied_config,WEATHER_RADAR_CHECK,ack,&snapshot,&active));ack=snapshot.generation;
  assert(snapshot.paused && active && radar.phase==RADAR_METADATA && http_requests==before);
  radar_test_step(&snapshot,&ack,&active);assert(http_requests==before+1);
  radar_test_drain(&snapshot,&ack,&active);
  /* Offline Pause acknowledges the displayed identity even if already old;
   * an accepted Play is explicitly stopped and published during an outage. */
  assert(weather_client_radar_control(APP_RADAR_PLAY,snapshot.frame_time)==ESP_OK);
  assert(weather_client_radar_step(&applied_config,WEATHER_RADAR_OFFLINE,ack,&snapshot,&active));ack=snapshot.generation;
  assert(!snapshot.playing && snapshot.stale && !active && !weather_client_radar_control_pending());
  assert(weather_client_radar_control(APP_RADAR_PAUSE,snapshot.frame_time)==ESP_OK);
  assert(weather_client_radar_step(&applied_config,WEATHER_RADAR_OFFLINE,ack,&snapshot,&active));ack=snapshot.generation;
  assert(snapshot.paused && snapshot.stale && !active && !weather_client_radar_control_pending());
  if (weather_client_radar_step(&applied_config,WEATHER_RADAR_CHECK,ack,&snapshot,&active)) ack=snapshot.generation;
  radar_test_drain(&snapshot,&ack,&active);
  assert(weather_client_radar_control(APP_RADAR_LATEST,snapshot.frame_time)==ESP_OK);
  radar_test_step(&snapshot,&ack,&active);radar_test_drain(&snapshot,&ack,&active);

  /* A full rolling-minute budget waits without an HTTP request, failure or
   * damaged image. At the exact minute boundary the next check is admitted. */
  for (unsigned i=0;i<100;++i) radar.request_times[i]=esp_timer_get_time();
  radar.request_times_count=100;before=http_requests;
  assert(!weather_client_radar_step(&applied_config,WEATHER_RADAR_CHECK,ack,&snapshot,&active));
  assert(active && http_requests==before && !snapshot.stale);
  fake_clock_advance(59999);radar_test_step(&snapshot,&ack,&active);assert(http_requests==before);
  fake_clock_advance(1);radar_test_step(&snapshot,&ack,&active);assert(http_requests==before+1);
  radar_test_drain(&snapshot,&ack,&active);

  /* Pause copied during the final tile HTTP must cancel the new publication.
   * The requested displayed frame is no longer in the metadata history, so
   * retaining its output slot is essential to finish the Pause handoff. */
  radar_metadata_body="{\"host\":\"https://tilecache.rainviewer.com\",\"radar\":{\"past\":["
    "{\"time\":1700001200,\"path\":\"/inflight-replacement\"}]}}";
  displayed_time=snapshot.frame_time;displayed=snapshot.pixels;
  weather_client_radar_step(&applied_config,WEATHER_RADAR_CHECK,ack,&snapshot,&active);
  assert(radar.phase==RADAR_WAIT_SLOT);
  radar_test_step(&snapshot,&ack,&active);assert(radar.phase==RADAR_ECHO);
  radar_test_step(&snapshot,&ack,&active);assert(radar.phase==RADAR_ECHO);
  radar_pause_http_time=displayed_time;on_request=radar_pause_during_http;
  assert(!weather_client_radar_step(&applied_config,WEATHER_RADAR_CONTINUE,ack,&snapshot,&active));
  assert(radar.phase==RADAR_READY && weather_client_radar_control_pending());
  assert(snapshot.frame_time==displayed_time && snapshot.pixels==displayed);
  radar_test_step(&snapshot,&ack,&active);radar_test_drain(&snapshot,&ack,&active);
  assert(snapshot.paused && snapshot.frame_time==displayed_time && snapshot.pixels==displayed);
  assert(snapshot.history_count==1 && radar.frames[0].time==1700001200);
  /* Explicitly exhaust the byte budget. The downloaded PNG cannot be retained,
   * and the current published bytes, timestamp and existing cache survive. */
  radar_frame_t capped={.time=1700001200};strcpy(capped.path,"/budget-test");
  size_t bytes=radar.cache_bytes;
  radar.cache_bytes=RADAR_CACHE_MAX-radar.tile_count*sizeof(radar_cached_tile_t)-1;
  assert(radar_tile_image(&capped,0,false)==ESP_ERR_NO_MEM);
  assert(capped.tiles && !capped.tiles[0].png && capped.loaded==0);
  radar.cache_bytes=bytes+capped.tile_count*sizeof(radar_cached_tile_t);
  radar_free_frame(&capped);assert(radar.cache_bytes==bytes);
  assert(snapshot.frame_time==1700000600 && snapshot.pixels);
  /* Latest also restores a complete cached frame during an outage, one image
   * step per call. It waits for display ack and issues no HTTP. */
  before=http_requests;
  assert(weather_client_radar_control(APP_RADAR_LATEST,snapshot.frame_time)==ESP_OK);
  assert(weather_client_radar_step(&applied_config,WEATHER_RADAR_OFFLINE,ack,&snapshot,&active));
  assert(active && snapshot.stale && !snapshot.paused);
  assert(!weather_client_radar_step(&applied_config,WEATHER_RADAR_OFFLINE,ack,&snapshot,&active));
  assert(active && radar.phase==RADAR_WAIT_SLOT && http_requests==before);
  ack=snapshot.generation;
  unsigned offline_steps=0;
  while (active && offline_steps++<16) {
    if (weather_client_radar_step(&applied_config,WEATHER_RADAR_OFFLINE,ack,&snapshot,&active)) ack=snapshot.generation;
  }
  assert(!active && offline_steps<16 && snapshot.frame_time==1700001200 && snapshot.stale);
  assert(http_requests==before);
  strcpy(applied_config.location,"Ottawa, Ontario, CA");
  before=http_requests;
  assert(weather_client_radar_step(&applied_config,WEATHER_RADAR_CHECK,ack,&snapshot,&active));
  assert(!snapshot.valid && !snapshot.pixels && !active && !radar.cache_bytes && !radar.frame_count);
  assert(http_requests==before);
  lv_free(base);lv_free(echo);free(radar.base);free(radar.image[0]);free(radar.image[1]);
  radar_clear_history();memset(&radar,0,sizeof(radar));radar_http_test=false;
}

static void check_radar_history_failure_status(void)
{
  reset_platform();assert(weather_client_init_images()==ESP_OK);radar_http_test=true;
  strcpy(applied_config.location,"Chicago, Illinois, US");
  const uint8_t base_color[4]={210,200,180,255},echo_color[4]={255,238,0,127};
  unsigned char *base=radar_test_png(800,800,base_color,&radar_base_png_size);
  unsigned char *echo=radar_test_png(512,512,echo_color,&radar_echo_png_size);
  radar_base_png=base;radar_echo_png=echo;
  const char *initial="{\"host\":\"https://tilecache.rainviewer.com\",\"radar\":{\"past\":["
    "{\"time\":1700000000,\"path\":\"/current\"},"
    "{\"time\":1699999400,\"path\":\"/past1\"},"
    "{\"time\":1699998800,\"path\":\"/past2\"},"
    "{\"time\":1699998200,\"path\":\"/past3\"}]}}";
  const char *newest="{\"host\":\"https://tilecache.rainviewer.com\",\"radar\":{\"past\":["
    "{\"time\":1700000600,\"path\":\"/newest\"},"
    "{\"time\":1700000000,\"path\":\"/current\"},"
    "{\"time\":1699999400,\"path\":\"/past1\"},"
    "{\"time\":1699998800,\"path\":\"/past2\"},"
    "{\"time\":1699998200,\"path\":\"/past3\"}]}}";
  const char *older="{\"host\":\"https://tilecache.rainviewer.com\",\"radar\":{\"past\":["
    "{\"time\":1700000600,\"path\":\"/newest\"},"
    "{\"time\":1700000000,\"path\":\"/current\"},"
    "{\"time\":1699999400,\"path\":\"/past1\"},"
    "{\"time\":1699998800,\"path\":\"/past2\"},"
    "{\"time\":1699998200,\"path\":\"/past3\"},"
    "{\"time\":1699997600,\"path\":\"/older\"}]}}";
  radar_metadata_body=initial;
  app_radar_snapshot_t snapshot={0};uint32_t ack=0;bool active=false;
  assert(weather_client_radar_step(&applied_config,WEATHER_RADAR_CHECK,ack,&snapshot,&active));ack=snapshot.generation;
  radar_test_drain(&snapshot,&ack,&active);assert(snapshot.history_count==4 && !snapshot.stale);
  const uint8_t *pixels=snapshot.pixels;time_t when=snapshot.frame_time;
  assert(weather_client_radar_control(APP_RADAR_PAUSE,when)==ESP_OK);
  radar_test_step(&snapshot,&ack,&active);
  radar_metadata_body=newest;
  if (weather_client_radar_step(&applied_config,WEATHER_RADAR_CHECK,ack,&snapshot,&active)) ack=snapshot.generation;
  http_status=503;radar_test_drain(&snapshot,&ack,&active);
  assert(snapshot.stale && radar.metadata_old && snapshot.history_count==4);
  assert(snapshot.paused && snapshot.pixels==pixels && snapshot.frame_time==when);
  /* A successful ordinary retry clears Old while keeping the held image. */
  http_status=200;
  if (weather_client_radar_step(&applied_config,WEATHER_RADAR_CHECK,ack,&snapshot,&active)) ack=snapshot.generation;
  radar_test_drain(&snapshot,&ack,&active);
  assert(!snapshot.stale && !radar.metadata_old && snapshot.history_count==5);
  assert(snapshot.pixels==pixels && snapshot.frame_time==when);
  /* Losing one older history download does not label a confirmed latest check
   * old, remove good frames, or prevent playback of the five complete samples. */
  radar_metadata_body=older;
  if (weather_client_radar_step(&applied_config,WEATHER_RADAR_CHECK,ack,&snapshot,&active)) ack=snapshot.generation;
  http_status=503;radar_test_drain(&snapshot,&ack,&active);
  assert(!snapshot.stale && !radar.metadata_old && snapshot.history_count==5);
  assert(snapshot.paused && snapshot.pixels==pixels && snapshot.frame_time==when);
  assert(weather_client_radar_control(APP_RADAR_PLAY,when)==ESP_OK);
  radar_test_step(&snapshot,&ack,&active);assert(snapshot.playing && active);
  unsigned requests=http_requests,steps=0;
  while (active && steps++<16) {
    fake_clock_advance(1500);radar_test_step(&snapshot,&ack,&active);
  }
  assert(!active && steps<16 && !snapshot.playing && !snapshot.paused);
  assert(snapshot.frame_time==1700000600 && http_requests==requests);
  lv_free(base);lv_free(echo);radar_clear_history();free(radar.base);free(radar.image[0]);free(radar.image[1]);
  memset(&radar,0,sizeof(radar));radar_http_test=false;
}

static void check_radar_location_release_and_entry_latest(void)
{
  reset_platform();assert(weather_client_init_images()==ESP_OK);radar_http_test=true;
  radar_image_requests=radar_base_requests=radar_retired_frees=0;
  strcpy(applied_config.location,"Chicago, Illinois, US");
  const uint8_t base_color[4]={210,200,180,255},yellow_source[4]={255,238,0,255},red_source[4]={255,68,0,255};
  const uint8_t yellow[4]={0,232,255,255},red[4]={36,49,240,255};
  unsigned char *base=radar_test_png(800,800,base_color,&radar_base_png_size);
  unsigned char *echo=radar_test_png(512,512,yellow_source,&radar_echo_png_size);
  radar_base_png=base;radar_echo_png=echo;
  radar_metadata_body="{\"host\":\"https://tilecache.rainviewer.com\",\"radar\":{\"past\":["
    "{\"time\":1700000000,\"path\":\"/location-a-first\"}]}}";
  app_radar_snapshot_t snapshot={0};uint32_t ack=0;bool active=false;
  assert(weather_client_radar_step(&applied_config,WEATHER_RADAR_CHECK,ack,&snapshot,&active));ack=snapshot.generation;
  /* Entering Radar asks for Latest. Before the first provider response that
   * command must leave startup metadata due, rather than waiting five minutes. */
  assert(radar.phase==RADAR_METADATA && !radar.frame_count);
  assert(weather_client_radar_control(APP_RADAR_LATEST,0)==ESP_OK);
  radar_test_step(&snapshot,&ack,&active);
  assert(active && radar.phase==RADAR_METADATA && http_requests==0);
  radar_test_step(&snapshot,&ack,&active);assert(radar.phase==RADAR_BASE);
  unsigned requests=http_requests;
  assert(weather_client_radar_control(APP_RADAR_LATEST,0)==ESP_OK);
  radar_test_step(&snapshot,&ack,&active);
  assert(active && radar.phase==RADAR_BASE && http_requests==requests);
  radar_test_step(&snapshot,&ack,&active);assert(radar.phase==RADAR_WAIT_SLOT);
  assert(weather_client_radar_control(APP_RADAR_LATEST,0)==ESP_OK);
  radar_test_step(&snapshot,&ack,&active);assert(active && radar.phase==RADAR_WAIT_SLOT);
  radar_test_step(&snapshot,&ack,&active);assert(radar.phase==RADAR_ECHO && radar.tile_count==2);
  radar_test_step(&snapshot,&ack,&active);assert(radar.phase==RADAR_ECHO);
  int tile_x=radar.tile_x,tile_y=radar.tile_y;uint8_t *preparing=radar.image[radar.slot];requests=http_requests;
  assert(weather_client_radar_control(APP_RADAR_LATEST,0)==ESP_OK);
  radar_test_step(&snapshot,&ack,&active);
  assert(active && radar.phase==RADAR_ECHO && radar.tile_x==tile_x && radar.tile_y==tile_y);
  assert(radar.image[radar.slot]==preparing && http_requests==requests);
  radar_test_drain(&snapshot,&ack,&active);
  assert(snapshot.valid && snapshot.frame_time==1700000000);
  for (size_t pixel=0;pixel<RADAR_PIXELS;++pixel) assert(!memcmp(snapshot.pixels+pixel*4,yellow,4));

  /* A real second publication allocates the other output slot. */
  lv_free(echo);echo=radar_test_png(512,512,red_source,&radar_echo_png_size);radar_echo_png=echo;
  radar_metadata_body="{\"host\":\"https://tilecache.rainviewer.com\",\"radar\":{\"past\":["
    "{\"time\":1700000000,\"path\":\"/location-a-first\"},"
    "{\"time\":1700000600,\"path\":\"/location-a-second\"}]}}";
  if (weather_client_radar_step(&applied_config,WEATHER_RADAR_CHECK,ack,&snapshot,&active)) ack=snapshot.generation;
  radar_test_drain(&snapshot,&ack,&active);
  assert(radar.image[0] && radar.image[1] && snapshot.frame_time==1700000600 && snapshot.history_count==2);
  const uint8_t *current=snapshot.pixels;
  for (size_t pixel=0;pixel<RADAR_PIXELS;++pixel) assert(!memcmp(current+pixel*4,red,4));
  /* Latest still cancels obsolete historical playback preparation. */
  assert(weather_client_radar_control(APP_RADAR_PLAY,snapshot.frame_time)==ESP_OK);
  radar_test_step(&snapshot,&ack,&active);
  radar_test_step(&snapshot,&ack,&active);assert(radar.phase==RADAR_ECHO && radar.candidate_time==1700000000);
  assert(weather_client_radar_control(APP_RADAR_LATEST,snapshot.frame_time)==ESP_OK);
  radar_test_step(&snapshot,&ack,&active);
  assert(!active && !snapshot.playing && radar.phase==RADAR_IDLE && snapshot.pixels==current);

  void *old_slots[2]={radar.image[0],radar.image[1]};
  uint8_t old_first[2][4],old_last[2][4];
  for (unsigned slot=0;slot<2;++slot) {
    radar_retired_slots[slot]=old_slots[slot];
    memcpy(old_first[slot],old_slots[slot],4);
    memcpy(old_last[slot],(uint8_t *)old_slots[slot]+(RADAR_PIXELS-1)*4,4);
  }
  uint32_t old_ack=ack;
  strcpy(applied_config.location,"Leander, Texas, US");strcpy(applied_config.latitude,"30.5788");strcpy(applied_config.longitude,"-97.8531");
  assert(weather_client_radar_step(&applied_config,WEATHER_RADAR_CHECK,old_ack,&snapshot,&active));
  assert(active && !snapshot.valid && !snapshot.pixels && snapshot.generation!=old_ack && !radar_retired_frees);
  uint32_t detached_generation=snapshot.generation;
  assert(!weather_client_radar_step(&applied_config,WEATHER_RADAR_CONTINUE,old_ack,&snapshot,&active));
  assert(active && radar.phase==RADAR_BASE && !radar_retired_frees);
  unsigned base_requests=radar_base_requests;requests=http_requests;
  radar_assert_released_base=true;
  for (unsigned retry=0;retry<3;++retry) {
    assert(!weather_client_radar_step(&applied_config,WEATHER_RADAR_CONTINUE,old_ack,&snapshot,&active));
    assert(active && radar.phase==RADAR_BASE && http_requests==requests && radar_base_requests==base_requests);
    assert(!radar_retired_frees && radar.image[0]==old_slots[0] && radar.image[1]==old_slots[1]);
    for (unsigned slot=0;slot<2;++slot) {
      assert(!memcmp(old_slots[slot],old_first[slot],4));
      assert(!memcmp((uint8_t *)old_slots[slot]+(RADAR_PIXELS-1)*4,old_last[slot],4));
    }
  }
  /* The transport assertion observes both real frees before the new location's
   * background request starts, therefore also before its PNG decoder scratch. */
  ack=detached_generation;
  assert(!weather_client_radar_step(&applied_config,WEATHER_RADAR_CONTINUE,ack,&snapshot,&active));
  assert(active && radar.phase==RADAR_WAIT_SLOT && radar_retired_frees==2 && !radar_assert_released_base);
  assert(!radar.image[0] && !radar.image[1] && radar_base_requests==base_requests+1);
  radar_test_drain(&snapshot,&ack,&active);
  assert(snapshot.valid && !snapshot.stale && snapshot.frame_time==1700000600);
  assert(snapshot.latitude==30.5788 && snapshot.longitude==-97.8531 && snapshot.history_count==2);
  for (size_t pixel=0;pixel<RADAR_PIXELS;++pixel) assert(!memcmp(snapshot.pixels+pixel*4,red,4));
  assert((radar.image[0]!=NULL)!=(radar.image[1]!=NULL));
  lv_free(base);lv_free(echo);radar_clear_history();free(radar.base);free(radar.image[0]);free(radar.image[1]);
  memset(&radar,0,sizeof(radar));radar_http_test=false;
}

static unsigned char *radar_read_binary(const char *path, size_t *size)
{
  FILE *file=fopen(path,"rb");assert(file);
  assert(fseek(file,0,SEEK_END)==0);long length=ftell(file);assert(length>0);
  *size=(size_t)length;rewind(file);
  unsigned char *bytes=malloc(*size);assert(bytes);
  assert(fread(bytes,1,*size,file)==*size);fclose(file);return bytes;
}
static void check_saved_radar_tiles(const char *directory)
{
  /* Captured precipitation tiles exercise real provider bytes. The flat base
   * below is a constructed sampling reference, not a USGS or board image. */
  static const int coordinates[][2]={{27,46},{28,53},{24,50},{32,49},{35,53},{35,54}};
  reset_platform();lv_init();assert(weather_client_init_images()==ESP_OK);
  radar_http_test=true;http_status=200;
  radar.slot=0;radar.image[0]=malloc(RADAR_PIXELS*4);assert(radar.image[0]);
  const uint8_t background[4]={19,31,43,255};
  for (unsigned i=0; i<sizeof(coordinates)/sizeof(coordinates[0]); ++i) {
    char path[1024];
    snprintf(path,sizeof(path),"%s/z7-%d-%d-512-smoothing1-snow0.png",
      directory,coordinates[i][0],coordinates[i][1]);
    size_t size;unsigned char *png=radar_read_binary(path,&size);
    assert(size<=WEATHER_RESPONSE_MAX);
    radar_echo_png=png;radar_echo_png_size=size;
    http_response_t response={.data=(char *)png,.length=size};
    lv_mem_monitor_t before,after;lv_mem_monitor(&before);
    lv_draw_buf_t *reference=NULL;
    assert(radar_decode_png(&response,false,&reference)==ESP_OK && reference);
    unsigned visible=0;
    for (unsigned y=0; y<512; ++y) for (unsigned x=0; x<512; ++x) {
      uint8_t *rgba=reference->data+y*reference->header.stride+x*4;
      visible+=rgba[3]!=0;
      assert(radar_reference_map_pixel(rgba));
    }
    assert(visible>0);
    radar.tile_x=coordinates[i][0];radar.tile_y=coordinates[i][1];
    radar.geometry=(radar_geometry_t){.left=radar.tile_x*256-0.25,
      .top=radar.tile_y*256-0.25,.step=0.5};
    for (size_t pixel=0; pixel<RADAR_PIXELS; ++pixel)
      memcpy(radar.image[0]+pixel*4,background,4);
    radar.tile_count=1;radar.work_frame=0;
    assert(radar_tile_image(&radar.frames[0],0,true)==ESP_OK);
    radar_free_frame(&radar.frames[0]);
    for (unsigned y=0; y<APP_RADAR_HEIGHT; ++y) for (unsigned x=0; x<APP_RADAR_WIDTH; ++x) {
      uint8_t expected[4];memcpy(expected,background,4);
      if (x<512 && y<512) {
        const uint8_t *rgba=reference->data+y*reference->header.stride+x*4;
        for (unsigned c=0; c<3; ++c)
          expected[c]=(uint8_t)((rgba[2-c]*rgba[3]+background[c]*(255-rgba[3])+127)/255);
      }
      assert(memcmp(radar.image[0]+((size_t)y*APP_RADAR_WIDTH+x)*4,expected,4)==0);
    }
    lv_draw_buf_destroy(reference);
    lv_mem_monitor(&after);assert(after.free_size==before.free_size);
    printf("PASS: real 512px smoothed tile %d/%d; %zu compressed bytes, %u visible pixels; mapping, sampling, alpha blending and decoder cleanup.\n",
      radar.tile_x,radar.tile_y,size,visible);
    free(png);
  }
  free(radar.image[0]);memset(&radar,0,sizeof(radar));radar_http_test=false;lv_deinit();
}
static void check_saved_radar_six_frames(const char *directory)
{
  reset_platform();lv_init();assert(weather_client_init_images()==ESP_OK);
  radar_http_test=true;radar_image_requests=radar_base_requests=0;
  char path[1024];snprintf(path,sizeof(path),"%s/frames/manifest.json",directory);
  char *manifest=read_response_file(path);radar_six_frame_manifest=cJSON_Parse(manifest);assert(radar_six_frame_manifest);
  snprintf(path,sizeof(path),"%s/provider-metadata.json",directory);
  char *metadata=read_response_file(path);radar_metadata_body=metadata;
  const cJSON *location=field(radar_six_frame_manifest,"location");
  snprintf(applied_config.latitude,sizeof(applied_config.latitude),"%.15g",number(field(location,"latitude")));
  snprintf(applied_config.longitude,sizeof(applied_config.longitude),"%.15g",number(field(location,"longitude")));
  strcpy(applied_config.location,"Florida fixture, US");
  snprintf(path,sizeof(path),"%s/frames/usgs-background-800.png",directory);
  unsigned char *base=radar_read_binary(path,&radar_base_png_size);radar_base_png=base;
  radar_six_frame_directory=directory;
  app_radar_snapshot_t snapshot={0};uint32_t ack=0;bool active=false;
  assert(weather_client_radar_step(&applied_config,WEATHER_RADAR_CHECK,ack,&snapshot,&active));ack=snapshot.generation;
  radar_test_drain(&snapshot,&ack,&active);
  assert(snapshot.history_count==6 && radar.tile_count==2 && radar_base_requests==1 && radar_image_requests==13);
  assert(weather_client_radar_control(APP_RADAR_PLAY,snapshot.frame_time)==ESP_OK);
  radar_test_step(&snapshot,&ack,&active);
  for (unsigned index=0;index<6;++index) {
    time_t when=(time_t)number(field(at(field(radar_six_frame_manifest,"frames"),index),"time"));
    fake_clock_advance(2000);
    unsigned steps=0;
    while (snapshot.frame_time!=when && steps++<16) radar_test_step(&snapshot,&ack,&active);
    assert(snapshot.frame_time==when && steps<16);
    snprintf(path,sizeof(path),"%s/expected-frame-%u.png",directory,index);
    size_t size;unsigned char *png=radar_read_binary(path,&size);
    http_response_t response={.data=(char *)png,.length=size};lv_draw_buf_t *expected=NULL;
    assert(radar_decode_png(&response,true,&expected)==ESP_OK);
    for (unsigned y=0;y<800;++y) for (unsigned x=0;x<800;++x) {
      const uint8_t *rgba=expected->data+y*expected->header.stride+x*4;
      const uint8_t *bgra=snapshot.pixels+((size_t)y*800+x)*4;
      assert(bgra[0]==rgba[2] && bgra[1]==rgba[1] && bgra[2]==rgba[0] && bgra[3]==rgba[3]);
    }
    lv_draw_buf_destroy(expected);free(png);
    printf("PASS: real six-frame fixture output %u matches all 800x800 approved reference pixels.\n",index);
  }
  assert(radar_image_requests==13);
  assert(radar_test_step(&snapshot,&ack,&active));
  assert(!active && !snapshot.playing && !snapshot.paused);
  time_t newest=snapshot.frame_time;const uint8_t *newest_pixels=snapshot.pixels;
  fake_clock_advance(15000);
  for (unsigned i=0;i<8;++i) assert(!radar_test_step(&snapshot,&ack,&active));
  assert(snapshot.frame_time==newest && snapshot.pixels==newest_pixels && radar_image_requests==13);
  printf("PASS: six real frames; 12 compressed tiles, %zu bounded cache bytes; playback performs no HTTP.\n",radar.cache_bytes);
  free(base);free(metadata);free(manifest);free(radar_saved_tile);radar_saved_tile=NULL;
  cJSON_Delete(radar_six_frame_manifest);radar_six_frame_manifest=NULL;radar_six_frame_directory=NULL;
  radar_clear_history();free(radar.base);free(radar.image[0]);free(radar.image[1]);memset(&radar,0,sizeof(radar));
  radar_http_test=false;lv_deinit();
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
    free(radar.base);free(radar.image[0]);free(radar.image[1]);radar_clear_history();memset(&radar,0,sizeof(radar));
    radar_http_test=false;lv_deinit();
  }
}

/* Constructed CAP cases use the real parser/owner; their names and quantities
 * are test data, never assertions about an actual local warning. */
static void alert_forget_owner(void)
{
  for (size_t i=0;i<nws_count;++i) nws_free(nws_events[i]);
  memset(nws_events,0,sizeof(nws_events));nws_count=0;
  memset(&nws_location,0,sizeof(nws_location));
  nws_initialized=nws_dirty=nws_binding_dirty=nws_save_failed=nws_has_success=false;
  nws_generation=nws_revision=nws_dismiss_serial=nws_audio_token=0;
  nws_epoch=nws_next_id=1;nws_audio_event=0;nws_last_success=0;nws_status=APP_ALERT_CHECKING;
  memset(&alert_audio,0,sizeof(alert_audio));alert_sound_starts=alert_sound_stops=alert_sound_acks=0;
  alert_audio_reject=false;nws_accept_headers=0;
}
static void alert_reset(bool keep_storage)
{
  time_t retained_time=time(NULL);
  reset_platform();nws_http_test=true;
  if (keep_storage) fake_clock_reset(retained_time);
  if (!keep_storage) fake_nvs_reset();else fake_nvs_clear_activity();
  alert_forget_owner();
  applied_config.alert_warning_sounds=UINT32_MAX;applied_config.alert_watch_sounds=0;
  applied_config.alert_update_sounds=DEVICE_CONFIG_ALERT_UPDATES_OFF;
  strcpy(applied_config.location,"Controlled location");
  assert(weather_client_alerts_init(&applied_config)==ESP_OK);
}
static cJSON *alert_feature(const char *name,const char *id,const char *vtec)
{
  cJSON *feature=cJSON_CreateObject(),*p=cJSON_AddObjectToObject(feature,"properties");assert(p);
  cJSON_AddStringToObject(feature,"type","Feature");cJSON_AddNullToObject(feature,"geometry");
  cJSON_AddStringToObject(p,"status","Actual");cJSON_AddStringToObject(p,"event",name);
  cJSON_AddStringToObject(p,"id",id);cJSON_AddStringToObject(p,"sender","w-nws.webmaster@noaa.gov");
  cJSON_AddStringToObject(p,"messageType","Alert");cJSON_AddStringToObject(p,"urgency","Immediate");
  cJSON_AddStringToObject(p,"severity","Severe");cJSON_AddStringToObject(p,"certainty","Likely");
  cJSON_AddStringToObject(p,"sent","2026-12-30T23:40:00+00:00");
  cJSON_AddStringToObject(p,"onset","2026-12-30T23:40:00+00:00");
  cJSON_AddStringToObject(p,"expires","2026-12-31T01:00:00+00:00");
  cJSON_AddStringToObject(p,"ends","2026-12-31T02:00:00+00:00");
  cJSON_AddStringToObject(p,"areaDesc","Controlled county");cJSON_AddStringToObject(p,"headline",name);
  cJSON_AddStringToObject(p,"description","A constructed hazard description.");
  cJSON_AddStringToObject(p,"instruction","Follow these constructed protective instructions.");
  cJSON_AddArrayToObject(p,"references");cJSON *parameters=cJSON_AddObjectToObject(p,"parameters");
  if (vtec) { cJSON *a=cJSON_AddArrayToObject(parameters,"VTEC");cJSON_AddItemToArray(a,cJSON_CreateString(vtec)); }
  return feature;
}
static cJSON *alert_collection(void)
{
  cJSON *root=cJSON_CreateObject();assert(root);cJSON_AddStringToObject(root,"type","FeatureCollection");
  assert(cJSON_AddArrayToObject(root,"features"));return root;
}
static void alert_add(cJSON *root,cJSON *feature) { assert(cJSON_AddItemToArray(cJSON_GetObjectItem(root,"features"),feature)); }
static void alert_replace(cJSON *object,const char *name,const char *value)
{
  assert(cJSON_ReplaceItemInObjectCaseSensitive(object,name,value?cJSON_CreateString(value):cJSON_CreateNull()));
}
static void alert_reference(cJSON *feature,const char *id,const char *sent)
{
  cJSON *p=cJSON_GetObjectItem(feature,"properties"),*references=cJSON_GetObjectItem(p,"references");
  cJSON *reference=cJSON_CreateObject();assert(reference);
  cJSON_AddStringToObject(reference,"identifier",id);cJSON_AddStringToObject(reference,"sender","w-nws.webmaster@noaa.gov");
  cJSON_AddStringToObject(reference,"sent",sent);assert(cJSON_AddItemToArray(references,reference));
  alert_replace(p,"messageType","Update");
}
static void alert_tag(cJSON *feature,const char *name,const char *value)
{
  cJSON *parameters=cJSON_GetObjectItem(cJSON_GetObjectItem(feature,"properties"),"parameters");
  cJSON_DeleteItemFromObjectCaseSensitive(parameters,name);
  cJSON *a=cJSON_AddArrayToObject(parameters,name);assert(a);assert(cJSON_AddItemToArray(a,cJSON_CreateString(value)));
}
static esp_err_t alert_download(cJSON *root)
{
  char *body=cJSON_PrintUnformatted(root);assert(body);http_body=body;
  int64_t pause=-1;esp_err_t error=weather_client_alerts_check(&applied_config,true,&pause);
  assert(pause==0);free(body);http_body=NULL;return error;
}
static app_alert_snapshot_t alert_summary(void)
{
  app_alert_snapshot_t state;weather_client_alerts_copy(&state);return state;
}
static void alert_finish(void)
{
  if (alert_audio.weather_token) {
    if (!alert_audio.weather_started_us) alert_audio.weather_started_us=esp_timer_get_time()+1;
    alert_audio.weather_state=WAC_AUDIO_WEATHER_FINISHED;
    weather_client_alerts_maintain(&applied_config);
  }
}
static void check_alert_catalog_and_details(void)
{
  alert_reset(false);size_t count=0;const weather_alert_type_t *catalog=weather_client_alert_catalog(&count);
  assert(count==55);
  for (size_t i=0;i<count;++i) {
    assert(catalog[i].warning==(i>=23));assert(catalog[i].bit==(i<23?i:i-23));assert(catalog[i].priority);
    if (i && i!=23) assert(strcmp(catalog[i-1].name,catalog[i].name)<0);
  }
  cJSON *root=alert_collection();
  const char *names[]={"Freeze Warning","Tornado Watch","Flood Watch","Gale Warning","Blizzard Warning"};
  for (int i=0;i<5;++i) { char id[24];snprintf(id,sizeof(id),"constructed-%d",i);alert_add(root,alert_feature(names[i],id,NULL)); }
  assert(alert_download(root)==ESP_OK);app_alert_snapshot_t state=alert_summary();
  assert(state.status==APP_ALERT_CURRENT&&state.has_success&&state.warning_count==3&&state.watch_count==2);
  weather_alert_row_t row;assert(weather_client_alerts_row_copy(1,&row)==ESP_OK);
  assert(!strcmp(catalog[row.type].name,"Tornado Watch"));
  assert(weather_client_alerts_row_copy(5,&row)==ESP_ERR_NOT_FOUND);
  weather_alert_detail_t detail;size_t needed=0;
  assert(weather_client_alerts_detail_copy(state.headline_id,&detail,NULL,0,&needed)==ESP_ERR_INVALID_SIZE&&needed>0);
  char *text=malloc(needed);assert(text);
  assert(weather_client_alerts_detail_copy(state.headline_id,&detail,text,needed,&needed)==ESP_OK);
  assert(strstr(text+detail.instruction_offset,"protective instructions"));assert(detail.row.revision==state.revision);
  free(text);assert(nws_accept_headers==1);
  alert_add(root,alert_feature("Freeze Warning","arrived-before-tap",NULL));
  assert(alert_download(root)==ESP_OK);weather_client_alerts_dismiss();
  assert(!alert_summary().headline_id&&alert_summary().warning_count==4);
  cJSON_Delete(root);
  puts("PASS: NWS full catalog, cross-family ordering, five rows and complete copied details.");
}
static void check_alert_relationships(void)
{
  alert_reset(false);cJSON *root=alert_collection();
  cJSON *feature=alert_feature("Tornado Warning","initial","/O.NEW.KLOT.TO.W.0001.261230T2340Z-261231T0200Z/");alert_add(root,feature);
  assert(alert_download(root)==ESP_OK);uint64_t id=alert_summary().headline_id;
  weather_client_alerts_dismiss();alert_finish();
  assert(!alert_summary().headline_id&&alert_sound_starts==1);
  cJSON_Delete(root);root=alert_collection();
  feature=alert_feature("Tornado Warning","revision","/O.CON.KLOT.TO.W.0001.000000T0000Z-261231T0200Z/");
  alert_reference(feature,"initial","2026-12-30T23:40:00+00:00");
  alert_replace(cJSON_GetObjectItem(feature,"properties"),"sent","2026-12-30T23:45:00+00:00");alert_add(root,feature);
  assert(alert_download(root)==ESP_OK&&nws_events[0]->id==id&&!alert_summary().headline_id&&alert_sound_starts==1);
  /* A changed ID and missing immediate reference still follows the stable VTEC
   * identity. Ordinary continuation preserves dismissal across restart. */
  alert_replace(cJSON_GetObjectItem(feature,"properties"),"id","missed-link");
  cJSON_DeleteItemFromObjectCaseSensitive(cJSON_GetObjectItem(feature,"properties"),"references");
  cJSON_AddArrayToObject(cJSON_GetObjectItem(feature,"properties"),"references");
  assert(alert_download(root)==ESP_OK&&nws_events[0]->id==id&&!alert_summary().headline_id);
  alert_tag(feature,"tornadoDetection","OBSERVED");
  assert(alert_download(root)==ESP_OK&&alert_summary().headline_id==id&&alert_sound_starts==1);
  weather_client_alerts_dismiss();
  cJSON *lower=alert_feature("Freeze Warning","new-lower",NULL);alert_add(root,lower);
  assert(alert_download(root)==ESP_OK);
  weather_alert_row_t row;assert(weather_client_alerts_row_copy(1,&row)==ESP_OK);
  assert(alert_summary().headline_id==row.id&&row.id!=id);
  alert_finish();weather_client_alerts_maintain(&applied_config);
  alert_reset(true);assert(alert_summary().status==APP_ALERT_CHECKING&&alert_summary().warning_count==2);
  assert(alert_download(root)==ESP_OK&&alert_sound_starts==0);cJSON_Delete(root);
  puts("PASS: CAP reference/VTEC updates, shared dismissal, confirmation-only visual escalation, new lower-priority event and restart no replay.");
}
static void check_alert_updates_and_sound_history(void)
{
  alert_reset(false);applied_config.alert_update_sounds=DEVICE_CONFIG_ALERT_UPDATES_INCREASED;
  cJSON *root=alert_collection(),*feature=alert_feature("Severe Thunderstorm Warning","severe",NULL);alert_add(root,feature);
  assert(alert_download(root)==ESP_OK&&alert_sound_starts==1);alert_finish();
  alert_tag(feature,"thunderstormDamageThreat","CONSIDERABLE");
  assert(alert_download(root)==ESP_OK&&alert_sound_starts==1); /* Inside ten minutes: consume without delay. */
  fake_clock_advance(601000);assert(alert_download(root)==ESP_OK&&alert_sound_starts==1);
  alert_tag(feature,"thunderstormDamageThreat","DESTRUCTIVE");
  assert(alert_download(root)==ESP_OK&&alert_sound_starts==2);alert_finish();
  alert_tag(feature,"thunderstormDamageThreat","CONSIDERABLE");assert(alert_download(root)==ESP_OK);
  fake_clock_advance(601000);alert_tag(feature,"thunderstormDamageThreat","DESTRUCTIVE");
  assert(alert_download(root)==ESP_OK&&alert_sound_starts==2);
  applied_config.alert_update_sounds=DEVICE_CONFIG_ALERT_UPDATES_ALL;
  alert_replace(cJSON_GetObjectItem(feature,"properties"),"instruction","Changed protective instructions.");
  assert(alert_download(root)==ESP_OK&&alert_sound_starts==3);alert_finish();
  alert_reset(true);applied_config.alert_update_sounds=DEVICE_CONFIG_ALERT_UPDATES_ALL;
  assert(alert_download(root)==ESP_OK&&alert_sound_starts==0);
  alert_replace(cJSON_GetObjectItem(feature,"properties"),"instruction","Another meaningful change.");
  assert(alert_download(root)==ESP_OK&&alert_sound_starts==0); /* Saved actual start, including after restart. */
  fake_clock_advance(601000);
  alert_replace(cJSON_GetObjectItem(feature,"properties"),"instruction","A later meaningful change.");
  assert(alert_download(root)==ESP_OK&&alert_sound_starts==1);alert_finish();
  native_night=true;
  fake_clock_advance(601000);alert_replace(cJSON_GetObjectItem(feature,"properties"),"instruction","Night update.");
  assert(alert_download(root)==ESP_OK&&alert_sound_starts==1);
  cJSON_Delete(root);
  puts("PASS: monotone danger milestones, ten-minute actual-start separation, substantive updates, restored history and Night permission.");
}
static void check_alert_failures_and_cutoffs(void)
{
  alert_reset(false);cJSON *root=alert_collection();
  cJSON *feature=alert_feature("Flood Warning","retained",NULL);alert_add(root,feature);
  assert(alert_download(root)==ESP_OK);alert_finish();uint64_t id=alert_summary().headline_id;
  const char *bad[]={"{}","{\"type\":\"FeatureCollection\",\"features\":null}",
    "{\"type\":\"FeatureCollection\",\"features\":[]", "{\"type\":\"FeatureCollection\",\"features\":[]}garbage"};
  for (size_t i=0;i<sizeof(bad)/sizeof(bad[0]);++i) {
    http_body=bad[i];int64_t pause;assert(weather_client_alerts_check(&applied_config,true,&pause)!=ESP_OK);
    assert(alert_summary().status==APP_ALERT_FAILED&&alert_summary().headline_id==id);
  }
  const char *invalid[]={"{\"type\":\"FeatureCollection\",\"features\":[],\"features\":[{}]}",
    "{\"type\":\"FeatureCollection\",\"features\":[],\"bad\":\"a\\u0000b\"}"};
  for (size_t i=0;i<sizeof(invalid)/sizeof(invalid[0]);++i) {
    http_body=invalid[i];int64_t pause;assert(weather_client_alerts_check(&applied_config,true,&pause)!=ESP_OK);
    assert(alert_summary().warning_count==1);
  }
  http_body="{\"type\":\"FeatureCollection\",\"features\":[]}";nws_transfer_complete=false;
  int64_t incomplete_pause;assert(weather_client_alerts_check(&applied_config,true,&incomplete_pause)==ESP_ERR_INVALID_RESPONSE);
  assert(alert_summary().warning_count==1);nws_transfer_complete=true;
  char *huge=malloc(WEATHER_RESPONSE_MAX+2);assert(huge);memset(huge,' ',WEATHER_RESPONSE_MAX+1);huge[WEATHER_RESPONSE_MAX+1]=0;
  http_body=huge;int64_t pause;assert(weather_client_alerts_check(&applied_config,true,&pause)==ESP_ERR_INVALID_SIZE);free(huge);
  cJSON *overflow=alert_collection();
  for (unsigned i=0;i<65;++i) { char key[20];snprintf(key,sizeof(key),"excess-%u",i);alert_add(overflow,alert_feature("Freeze Warning",key,NULL)); }
  assert(alert_download(overflow)==ESP_ERR_INVALID_SIZE&&alert_summary().warning_count==1);cJSON_Delete(overflow);
  unsigned requests=http_requests;assert(weather_client_alerts_check(&applied_config,false,&pause)!=ESP_OK&&http_requests==requests);
  fake_clock_advance(4500000); /* expires passed at 01:00; ends remains 02:00. */
  assert(alert_summary().warning_count==1&&alert_summary().status==APP_ALERT_FAILED);
  fake_clock_advance(3600000);assert(alert_summary().warning_count==0&&alert_summary().status==APP_ALERT_FAILED);
  cJSON_Delete(root);root=alert_collection();assert(alert_download(root)==ESP_OK&&alert_summary().status==APP_ALERT_CURRENT);
  cJSON_Delete(root);
  http_status=429;http_body="{\"title\":\"temporary\"}";retry_header="120";
  assert(weather_client_alerts_check(&applied_config,true,&pause)!=ESP_OK&&pause==esp_timer_get_time()+120000000);
  retry_header="malformed";assert(weather_client_alerts_check(&applied_config,true,&pause)!=ESP_OK&&pause==0);
  http_status=400;http_body="{\"status\":400,\"type\":\"https://api.weather.gov/problems/InvalidParameter\",\"detail\":\"Parameter \\\"point\\\" is invalid: out of bounds\"}";
  assert(weather_client_alerts_check(&applied_config,true,&pause)!=ESP_OK&&alert_summary().status==APP_ALERT_UNSUPPORTED);
  requests=http_requests;assert(weather_client_alerts_check(&applied_config,true,&pause)==ESP_ERR_NOT_FOUND&&http_requests==requests);
  strcpy(applied_config.latitude,"41.9");assert(weather_client_alerts_sync_location(&applied_config));assert(alert_summary().status==APP_ALERT_CHECKING);
  puts("PASS: malformed/truncated/oversized/overflow rejection, failed-check retention, ends precedence, independent expiry, offline due check, retry pause and confirmed unsupported coverage.");
}
static void check_alert_sound_dismiss_handoff(void)
{
  alert_reset(false);
  cJSON *root=alert_collection();
  alert_add(root,alert_feature("Freeze Warning","dismiss-during-cleanup",NULL));
  tap_during_alert_cleanup=true;
  assert(alert_download(root)==ESP_OK && !tap_during_alert_cleanup);
  assert(alert_summary().warning_count==1 && !alert_summary().headline_id);
  assert(alert_sound_starts==0); /* A completed dismissal must not start sound later. */
  cJSON_Delete(root);

  /* A tap during HTTP covers old events, not a genuinely new event received
   * afterward. Capturing the counter before the request would break this. */
  alert_reset(false);root=alert_collection();
  alert_add(root,alert_feature("Freeze Warning","received-after-tap",NULL));
  on_request=weather_client_alerts_dismiss;
  assert(alert_download(root)==ESP_OK);
  on_request=NULL;
  assert(alert_summary().headline_id && alert_sound_starts==1);
  alert_finish();weather_client_alerts_dismiss();
  fake_clock_advance(600001); /* First-start receipt may be one microsecond after time zero. */
  applied_config.alert_update_sounds=DEVICE_CONFIG_ALERT_UPDATES_ALL;
  cJSON *feature=cJSON_GetArrayItem(cJSON_GetObjectItem(root,"features"),0);
  alert_replace(cJSON_GetObjectItem(feature,"properties"),"description","Revised protective details after an earlier headline dismissal.");
  assert(alert_download(root)==ESP_OK && !alert_summary().headline_id && alert_sound_starts==2);
  cJSON_Delete(root);
  puts("PASS: dismissal during response cleanup prevents late sound; pre-publication taps preserve new alerts and later substantive-update sounds.");
}
static void check_alert_storage_failures(void)
{
  alert_reset(false);cJSON *root=alert_collection();alert_add(root,alert_feature("Freeze Warning","save-failure",NULL));
  fake_nvs_clear_activity();fake_nvs_fail(FAKE_SET,1);
  assert(alert_download(root)==ESP_OK&&alert_summary().save_failed&&alert_summary().warning_count==1);
  unsigned writes=fake_nvs_calls(FAKE_SET);weather_client_alerts_dismiss();
  weather_client_alerts_maintain(&applied_config);assert(fake_nvs_calls(FAKE_SET)==writes&&!alert_summary().headline_id);
  fake_nvs_clear_activity();assert(alert_download(root)==ESP_OK&&!alert_summary().save_failed);
  alert_reset(true);assert(alert_summary().warning_count==1&&!alert_summary().headline_id);
  /* A missing/unreadable individual operating record receives the ordinary
   * fresh-alert sound rule; no initial silent batch is introduced. */
  alert_forget_owner();fake_nvs_clear_activity();fake_nvs_fail(FAKE_READ,2);
  assert(weather_client_alerts_init(&applied_config)==ESP_OK&&alert_summary().warning_count==0);
  fake_nvs_clear_activity();assert(alert_download(root)==ESP_OK&&alert_sound_starts==1);alert_finish();
  nvs_handle_t handle;assert(nvs_open(NWS_NAMESPACE,NVS_READWRITE,&handle)==ESP_OK);
  uint8_t corrupt[sizeof(nws_stored_event_t)]={0};assert(nvs_set_blob(handle,"e00",corrupt,sizeof(corrupt))==ESP_OK);nvs_close(handle);
  alert_forget_owner();fake_nvs_clear_activity();assert(weather_client_alerts_init(&applied_config)==ESP_OK&&alert_summary().warning_count==0);
  assert(alert_download(root)==ESP_OK&&alert_sound_starts==1);alert_finish();
  strcpy(applied_config.latitude,"44.0");assert(weather_client_alerts_sync_location(&applied_config));
  fake_nvs_clear_activity();fake_nvs_fail(FAKE_SET,2); /* New location binding succeeds; event write fails. */
  assert(alert_download(root)==ESP_OK&&alert_summary().save_failed);
  alert_forget_owner();fake_nvs_clear_activity();assert(weather_client_alerts_init(&applied_config)==ESP_OK);
  assert(alert_summary().warning_count==0&&alert_summary().status==APP_ALERT_CHECKING);
  cJSON_Delete(root);
  puts("PASS: live/dismissal save failure, scheduled retry, restored dismissal, unreadable/corrupt fresh-alert fallback and interrupted location-bound save.");
}
static void check_alert_canonical_updates(void)
{
  alert_reset(false);applied_config.alert_update_sounds=DEVICE_CONFIG_ALERT_UPDATES_ALL;
  cJSON *root=alert_collection(),*feature=alert_feature("Tornado Warning","canonical-first","/O.NEW.KLOT.TO.W.0009.261230T2340Z-261231T0200Z/");
  cJSON *p=cJSON_GetObjectItem(feature,"properties"),*geocode=cJSON_AddObjectToObject(p,"geocode");
  cJSON *codes=cJSON_AddArrayToObject(geocode,"UGC");cJSON_AddItemToArray(codes,cJSON_CreateString("ILC031"));cJSON_AddItemToArray(codes,cJSON_CreateString("ILC043"));
  alert_tag(feature,"windThreat","RADAR INDICATED");alert_tag(feature,"hailThreat","OBSERVED");alert_add(root,feature);
  assert(alert_download(root)==ESP_OK);alert_finish();fake_clock_advance(601000);
  alert_replace(p,"id","canonical-transport-only");alert_replace(p,"sent","2026-12-30T23:49:00+00:00");
  alert_reference(feature,"canonical-first","2026-12-30T23:40:00+00:00");
  alert_tag(feature,"AWIPSidentifier","different-packaging");alert_tag(feature,"WMOidentifier","different-issue-time");
  cJSON_DeleteItemFromObjectCaseSensitive(p,"geocode");geocode=cJSON_AddObjectToObject(p,"geocode");
  codes=cJSON_AddArrayToObject(geocode,"UGC");cJSON_AddItemToArray(codes,cJSON_CreateString("ILC043"));cJSON_AddItemToArray(codes,cJSON_CreateString("ILC031"));
  alert_tag(feature,"windThreat","RADAR INDICATED"); /* Moves this parameter after hailThreat. */
  assert(alert_download(root)==ESP_OK&&alert_sound_starts==1);
  alert_replace(p,"description","A changed forecast amount: winds of 80 MPH.");
  assert(alert_download(root)==ESP_OK&&alert_sound_starts==2);alert_finish();
  cJSON_Delete(root);
  puts("PASS: changed identifiers/issue packaging and reordered source arrays stay silent; changed protective content sounds in All updates.");
}
typedef struct { esp_err_t error;int64_t pause; } alert_request_result_t;
static void *alert_request_thread(void *argument)
{
  alert_request_result_t *result=argument;
  result->error=weather_client_alerts_check(&applied_config,true,&result->pause);return NULL;
}
static void check_alert_blocked_operations(void)
{
  alert_reset(false);cJSON *root=alert_collection();alert_add(root,alert_feature("Freeze Warning","blocked",NULL));
  char *body=cJSON_PrintUnformatted(root);assert(body);http_body=body;
  fake_nvs_block_next_set();alert_request_result_t result={0};pthread_t thread;
  assert(pthread_create(&thread,NULL,alert_request_thread,&result)==0);assert(fake_nvs_wait_blocked());
  app_alert_snapshot_t state=alert_summary();assert(state.warning_count==1);
  weather_client_alerts_dismiss();assert(!alert_summary().headline_id);
  fake_clock_advance(8100000);assert(alert_summary().warning_count==0);
  fake_nvs_release_set();assert(pthread_join(thread,NULL)==0);assert(result.error==ESP_OK);
  free(body);cJSON_Delete(root);
  alert_reset(false);root=alert_collection();alert_add(root,alert_feature("Tornado Warning","late-location",NULL));
  body=cJSON_PrintUnformatted(root);assert(body);http_body=body;
  pthread_mutex_lock(&overlap_mutex);block_http_once=true;http_blocked=http_released=false;pthread_mutex_unlock(&overlap_mutex);
  assert(pthread_create(&thread,NULL,alert_request_thread,&result)==0);
  pthread_mutex_lock(&overlap_mutex);while (!http_blocked) pthread_cond_wait(&overlap_condition,&overlap_mutex);pthread_mutex_unlock(&overlap_mutex);
  state=alert_summary();weather_client_alerts_dismiss();
  strcpy(applied_config.latitude,"42.0");assert(weather_client_alerts_sync_location(&applied_config));
  assert(alert_summary().warning_count==0&&alert_summary().status==APP_ALERT_CHECKING);
  pthread_mutex_lock(&overlap_mutex);http_released=true;pthread_cond_broadcast(&overlap_condition);pthread_mutex_unlock(&overlap_mutex);
  assert(pthread_join(thread,NULL)==0);assert(result.error==ESP_ERR_INVALID_STATE);
  assert(alert_summary().status==APP_ALERT_CHECKING&&alert_sound_starts==0);
  free(body);cJSON_Delete(root);
  puts("PASS: real owner snapshots/dismissal/expiry during blocked NVS and rejection of a blocked old-location HTTP result.");
}
static void check_alert_source_markers_and_removal(void)
{
  alert_reset(false);
  const struct { const char *name,*key,*value;uint32_t bits; } cases[]={
    {"Tornado Warning","tornadoDamageThreat","CONSIDERABLE",1},
    {"Tornado Warning","tornadoDamageThreat","CATASTROPHIC",3},
    {"Severe Thunderstorm Warning","thunderstormDamageThreat","DESTRUCTIVE",3},
    {"Flash Flood Warning","flashFloodDamageThreat","CATASTROPHIC",3},
    {"Snow Squall Warning","snowSquallImpact","SIGNIFICANT",4},
    {"Red Flag Warning","NWSheadline","PARTICULARLY DANGEROUS SITUATION (PDS) RED FLAG IN EFFECT",8},
    {"Blizzard Warning","NWSheadline","PARTICULARLY DANGEROUS SITUATION BLIZZARD WARNING IN EFFECT",8},
    {"Ice Storm Warning","NWSheadline","PARTICULARLY DANGEROUS SITUATION ICE STORM WARNING",8},
  };
  for (size_t i=0;i<sizeof(cases)/sizeof(cases[0]);++i) {
    cJSON *feature=alert_feature(cases[i].name,"marker",NULL);alert_tag(feature,cases[i].key,cases[i].value);
    const cJSON *p=field(feature,"properties");assert(nws_milestones(nws_type(cases[i].name),p,cases[i].name)==cases[i].bits);
    cJSON_Delete(feature);
  }
  cJSON *feature=alert_feature("Blizzard Warning","winter",NULL);
  alert_replace(cJSON_GetObjectItem(feature,"properties"),"description","THIS IS A PARTICULARLY DANGEROUS SITUATION. Travel is dangerous.");
  assert(nws_milestones(nws_type("Blizzard Warning"),field(feature,"properties"),"Blizzard Warning")==8);
  alert_replace(cJSON_GetObjectItem(feature,"properties"),"description","A particularly dangerous situation may develop later.");
  assert(nws_milestones(nws_type("Blizzard Warning"),field(feature,"properties"),"Blizzard Warning")==0);cJSON_Delete(feature);
  cJSON *root=alert_collection();feature=alert_feature("Flood Warning","first",NULL);alert_add(root,feature);
  assert(alert_download(root)==ESP_OK);weather_client_alerts_dismiss();alert_finish();
  cJSON_Delete(root);root=alert_collection();feature=alert_feature("Flood Warning","later",NULL);
  alert_tag(feature,"expiredReferences","w-nws.webmaster@noaa.gov,first,2026-12-30T23:40:00+00:00");
  alert_add(root,feature);assert(alert_download(root)==ESP_OK&&!alert_summary().headline_id);
  cJSON *cancel=alert_feature("Flood Warning","cancel",NULL);alert_reference(cancel,"later","2026-12-30T23:40:00+00:00");
  alert_replace(cJSON_GetObjectItem(cancel,"properties"),"messageType","Cancel");alert_add(root,cancel);
  assert(alert_download(root)==ESP_OK&&alert_summary().warning_count==0);
  cJSON_Delete(root);
  puts("PASS: exact documented damage/Significant/PDS inputs, affirmative winter declaration, expired CAP references and cancellation.");
}
static void check_alert_upgrade_and_year_boundary(void)
{
  alert_reset(false);cJSON *root=alert_collection();
  alert_add(root,alert_feature("Winter Storm Watch","watch",NULL));
  assert(alert_download(root)==ESP_OK&&alert_sound_starts==0);
  weather_client_alerts_dismiss();
  cJSON_Delete(root);root=alert_collection();cJSON *feature=alert_feature("Winter Storm Warning","warning",NULL);
  alert_reference(feature,"watch","2026-12-30T23:40:00+00:00");alert_add(root,feature);
  assert(alert_download(root)==ESP_OK&&alert_summary().headline_id&&alert_sound_starts==1);
  alert_finish();
  /* The next update explicitly supersedes two same-type messages. CAP permits
   * multiple references; the single incoming event must not duplicate sound. */
  alert_add(root,alert_feature("Winter Storm Warning","second-warning",NULL));
  assert(alert_download(root)==ESP_OK);alert_finish();
  weather_client_alerts_dismiss();
  cJSON_Delete(root);root=alert_collection();feature=alert_feature("Winter Storm Warning","merged",NULL);
  alert_reference(feature,"warning","2026-12-30T23:40:00+00:00");
  alert_reference(feature,"second-warning","2026-12-30T23:40:00+00:00");alert_add(root,feature);
  assert(alert_download(root)==ESP_OK&&alert_summary().warning_count==1&&!alert_summary().headline_id);
  cJSON_Delete(root);
  alert_reset(false);fake_clock_reset((time_t)civil_seconds(2026,12,31,23,50,0));root=alert_collection();
  feature=alert_feature("Freeze Warning","year-old","/O.NEW.KLOT.FZ.W.0001.261231T2200Z-270102T0200Z/");
  cJSON *p=cJSON_GetObjectItem(feature,"properties");
  alert_replace(p,"sent","2026-12-31T22:00:00+00:00");alert_replace(p,"expires","2027-01-02T01:00:00+00:00");
  alert_replace(p,"ends","2027-01-02T02:00:00+00:00");alert_add(root,feature);
  assert(alert_download(root)==ESP_OK);uint64_t id=alert_summary().headline_id;alert_finish();weather_client_alerts_dismiss();
  fake_clock_set((time_t)civil_seconds(2027,1,1,1,0,0));
  alert_replace(p,"id","year-continuation");alert_replace(p,"sent","2027-01-01T00:30:00+00:00");
  alert_tag(feature,"VTEC","/O.CON.KLOT.FZ.W.0001.000000T0000Z-270102T0200Z/");
  assert(alert_download(root)==ESP_OK&&nws_events[0]->id==id&&nws_events[0]->origin_year==2026&&!alert_summary().headline_id);
  alert_replace(p,"id","year-new-event");alert_tag(feature,"VTEC","/O.NEW.KLOT.FZ.W.0001.270101T0030Z-270102T0200Z/");
  assert(alert_download(root)==ESP_OK&&alert_summary().headline_id!=id&&nws_events[0]->origin_year==2027);
  cJSON_Delete(root);
  puts("PASS: watch-to-warning sounds with update sounds Off, multi-reference replacement and VTEC continuation/new-year number reuse.");
}
static void check_alert_scheduler(void)
{
  alert_reset(false);cJSON *root=alert_collection();char *body=cJSON_PrintUnformatted(root);assert(body);http_body=body;
  int64_t due=0;weather_service_alerts(&applied_config,&due,true);
  assert(http_requests==1&&due==60000000&&alert_summary().status==APP_ALERT_CURRENT);
  /* Forecast-provider quota is a different owner/deadline, even with a key. */
  use_vc("controlled-quota");quota_record_t *record=record_for(&applied_config);assert(record);
  record->paused=true;record->next_us=INT64_C(3600000000);
  fake_clock_advance(59999);weather_service_alerts(&applied_config,&due,true);assert(http_requests==1);
  fake_clock_advance(1);native_night=true;weather_service_alerts(&applied_config,&due,true);
  assert(http_requests==2&&due==120000000);
  fake_clock_advance(60000);request_results[http_requests]=ESP_FAIL;weather_service_alerts(&applied_config,&due,true);
  assert(http_requests==3&&due==180000000&&alert_summary().status==APP_ALERT_FAILED);
  fake_clock_advance(60000);weather_service_alerts(&applied_config,&due,false);
  assert(http_requests==3&&due==240000000&&alert_summary().status==APP_ALERT_FAILED);
  fake_clock_advance(60000);http_status=503;retry_header="360";
  weather_service_alerts(&applied_config,&due,true);assert(http_requests==4&&due==600000000);
  fake_clock_advance(60000);weather_service_alerts(&applied_config,&due,true);assert(http_requests==4);
  retry_header=NULL;http_status=200;strcpy(applied_config.latitude,"42.1");
  weather_client_alerts_sync_location(&applied_config);weather_service_alerts(&applied_config,&due,true);
  assert(http_requests==5&&due==360000000&&alert_summary().status==APP_ALERT_CURRENT);
  http_status=400;http_body="{\"status\":400,\"type\":\"https://api.weather.gov/problems/InvalidParameter\",\"detail\":\"Parameter \\\"point\\\" is invalid: out of bounds\"}";
  fake_clock_advance(60000);weather_service_alerts(&applied_config,&due,true);
  assert(http_requests==6&&alert_summary().status==APP_ALERT_UNSUPPORTED);
  weather_service_alerts(&applied_config,&due,true);assert(due==INT64_MAX&&http_requests==6);
  free(body);cJSON_Delete(root);
  puts("PASS: production scheduler startup/new-location checks, independent Day/Night minute checks, provider quota, offline retries, explicit long pause and unsupported suspension.");
}
static void change_night_during_alert_request(void)
{
  native_night=!native_night;
  on_request=NULL;
}
static void check_alert_night_transition(void)
{
  /* Change only the Night owner's copied value while HTTP is in progress.
   * The real scheduler and event owner must use the post-request permission. */
  for (unsigned before=0;before<2;++before) for (unsigned allowed=0;allowed<2;++allowed) {
    alert_reset(false);native_night=before;
    applied_config.alert_night_sounds=allowed;
    cJSON *root=alert_collection();alert_add(root,alert_feature("Freeze Warning","night-boundary",NULL));
    char *body=cJSON_PrintUnformatted(root);assert(body);http_body=body;
    on_request=change_night_during_alert_request;
    int64_t due=0;weather_service_alerts(&applied_config,&due,true);
    assert(native_night!=before && on_request==NULL && http_requests==1);
    app_alert_snapshot_t state=alert_summary();
    assert(state.status==APP_ALERT_CURRENT && state.warning_count==1 && state.headline_id);
    unsigned expected=!native_night || allowed;
    assert(alert_sound_starts==expected);
    alert_finish();
    fake_clock_advance(60000);weather_service_alerts(&applied_config,&due,true);
    assert(http_requests==2 && alert_sound_starts==expected); /* No deferred or unchanged replay. */
    free(body);http_body=NULL;cJSON_Delete(root);
  }
  puts("PASS: request-spanning dusk/dawn use current Night permission, both sound preferences, retained visuals and no unchanged replay.");
}
static void check_alert_capacity_workload(void)
{
  alert_reset(false);cJSON *root=alert_collection();
  const char *names[]={"Severe Thunderstorm Warning","Tornado Warning","Flood Watch","Flash Flood Warning","Flood Warning"};
  const size_t sizes[5][4]={{22,115,933,678},{46,96,923,383},{107,93,1926,477},{90,103,1503,361},{21,65,2957,261}};
  const char *fields[]={"areaDesc","headline","description","instruction"};
  for (int i=0;i<5;++i) {
    char id[20];snprintf(id,sizeof(id),"capacity-%d",i);cJSON *feature=alert_feature(names[i],id,NULL);
    cJSON *p=cJSON_GetObjectItem(feature,"properties");
    for (int j=0;j<4;++j) { char *text=malloc(sizes[i][j]+1);assert(text);memset(text,'A'+j,sizes[i][j]);text[sizes[i][j]]=0;alert_replace(p,fields[j],text);free(text); }
    alert_add(root,feature);
  }
  assert(alert_download(root)==ESP_OK&&alert_summary().warning_count==4&&alert_summary().watch_count==1);
  size_t total=0,largest=0,entries=80; /* Synthetic existing-settings reserve, not a board measurement. */
  for (size_t i=0;i<nws_count;++i) {
    size_t length=0;uint8_t *bytes=nws_serialize(nws_events[i],nws_epoch,&length);assert(bytes);
    total+=length;if (length>largest) largest=length;
    entries+=(length+31)/32+3; /* Index plus up to two chunk headers. */
    nws_event_t *restored=nws_deserialize(bytes,length,nws_epoch,nws_events[i]->slot);assert(restored);
    for (int j=0;j<4;++j) assert(!strcmp(restored->text[j],nws_events[i]->text[j]));
    nws_free(restored);free(bytes);
  }
  entries+=(largest+31)/32+3+(sizeof(nws_binding_t)+31)/32+3;
  assert(total>11160 && (2*total+31)/32>630 && entries<=630);
  printf("PASS: constructed five-event full-text workload: %zu serialized bytes, %zu largest event, <=%zu estimated replacement entries including synthetic80-entry reserve; native NVS is not a board-capacity test.\n",total,largest,entries);
  cJSON_Delete(root);
}
/* Actual public NWS CAP feature captured October 19, 2026 UTC, placed in an
 * active-collection envelope. Source: https://api.weather.gov/alerts/urn:oid:2.49.0.1.840.0.cb161830b749cbd0e733cc365ccc1a651272c8dc.001.2
 * This is historical provider content, not a current local alert. The second
 * revision below is explicitly constructed to exercise the observed references. */
static void check_alert_official_cap_sample(void)
{
  static const char captured[] =
    "{\"type\":\"FeatureCollection\",\"features\":[{\"@context\":[\"https://geojson.org/geojson-ld/geojson-context"
    ".jsonld\",{\"@version\":\"1.1\",\"wx\":\"https://api.weather.gov/ontology#\",\"@vocab\":\"https://api.weather.go"
    "v/ontology#\"}],\"id\":\"https://api.weather.gov/alerts/urn:oid:2.49.0.1.840.0.cb161830b749cbd0e733cc365"
    "ccc1a651272c8dc.001.2\",\"type\":\"Feature\",\"geometry\":null,\"properties\":{\"@id\":\"https://api.weather.gov"
    "/alerts/urn:oid:2.49.0.1.840.0.cb161830b749cbd0e733cc365ccc1a651272c8dc.001.2\",\"@type\":\"wx:Alert\",\"i"
    "d\":\"urn:oid:2.49.0.1.840.0.cb161830b749cbd0e733cc365ccc1a651272c8dc.001.2\",\"areaDesc\":\"Hatteras Isla"
    "nd\",\"geocode\":{\"SAME\":[\"037055\"],\"UGC\":[\"NCZ205\"]},\"affectedZones\":[\"https://api.weather.gov/zones/f"
    "orecast/NCZ205\"],\"references\":[{\"@id\":\"https://api.weather.gov/alerts/urn:oid:2.49.0.1.840.0.0fa2b1c"
    "f35c0ffa63c715482f0e3077ecdd92f31.001.1\",\"identifier\":\"urn:oid:2.49.0.1.840.0.0fa2b1cf35c0ffa63c7154"
    "82f0e3077ecdd92f31.001.1\",\"sender\":\"w-nws.webmaster@noaa.gov\",\"sent\":\"2026-10-18T11:02:00-04:00\"}],\""
    "sent\":\"2026-10-18T16:04:00-04:00\",\"effective\":\"2026-10-18T16:04:00-04:00\",\"onset\":\"2026-10-18T16:04:"
    "00-04:00\",\"expires\":\"2026-10-19T04:15:00-04:00\",\"ends\":\"2026-10-20T17:00:00-04:00\",\"status\":\"Actual\""
    ",\"messageType\":\"Update\",\"category\":\"Met\",\"severity\":\"Severe\",\"certainty\":\"Likely\",\"urgency\":\"Expecte"
    "d\",\"event\":\"Coastal Flood Warning\",\"sender\":\"w-nws.webmaster@noaa.gov\",\"senderName\":\"NWS Newport/Mor"
    "ehead City NC\",\"headline\":\"Coastal Flood Warning issued October 18 at 4:04PM EDT until October 20 at"
    " 5:00PM EDT by NWS Newport/Morehead City NC\",\"description\":\"* WHAT...For the Coastal Flood Warning, "
    "2 to 4 feet of inundation\\nabove ground level expected in low-lying areas near shorelines\\nand tidal"
    " waterways. For the High Surf Advisory, large breaking\\nwaves of 9 to 14 feet in the surf zone. For "
    "the Beach Hazards\\nStatement, dangerous rip currents, large breaking waves, and\\nstrong longshore cu"
    "rrents in the surf zone.\\n\\n* WHERE...Hatteras Island.\\n\\n* WHEN...For the High Surf Advisory, until"
    " 6 AM EDT Thursday.\\nFor the Coastal Flood Warning, until 5 PM EDT Tuesday. For the\\nBeach Hazards S"
    "tatement, until 8 PM EDT this evening.\\n\\n* IMPACTS...2 to 4 feet of inundation above ground level i"
    "s\\nexpected in some areas near shorelines and tidal waterways\\nresulting in an elevated threat of pr"
    "operty damage. Flooding\\nwill extend inland from the waterfront threatening some homes\\nand business"
    "es. Numerous road closures and flooding of vehicles\\nwill be possible. Vulnerable portions of NC-12 "
    "may be inundated,\\nespecially around high tide. There is a possibility of being\\nstranded on Hattera"
    "s Island for multiple days. This could be\\nlife threatening if you have an emergency. Dangerous swim"
    "ming\\nand surfing conditions and localized beach erosion. Rip currents\\ncan sweep even the best swim"
    "mers away from shore into deeper\\nwater. Dangerous shore break can throw a swimmer or surfer head\\nf"
    "irst into the bottom causing neck and back injuries. Strong\\nlongshore currents will create dangerou"
    "s swimming conditions.\\n\\n* ADDITIONAL DETAILS...This is a long duration event with\\nuncertainty on "
    "when conditions will improve. Significant ocean\\noverwash impacts are expected at vulnerable oceansi"
    "de areas\\nthrough multiple high tide cycles into the work week. Minor\\nsoundside water level rises w"
    "ill also be possible.\",\"instruction\":\"Take the necessary actions to protect flood-prone property. If"
    "\\ntravel is required, do not drive around barricades or through\\nwater of unknown depth.\\n\\nSwimmers"
    " should remain out of the water due to dangerous surf\\nconditions.\",\"response\":\"Avoid\",\"note\":null,\""
    "parameters\":{\"AWIPSidentifier\":[\"CFWMHX\"],\"WMOidentifier\":[\"WHUS42 KMHX 182004\"],\"NWSheadline\":[\"HIG"
    "H SURF ADVISORY NOW IN EFFECT UNTIL 6 AM EDT THURSDAY... ...COASTAL FLOOD WARNING REMAINS IN EFFECT "
    "UNTIL 5 PM EDT TUESDAY... ...BEACH HAZARDS STATEMENT REMAINS IN EFFECT UNTIL 8 PM EDT THIS EVENING\"]"
    ",\"BLOCKCHANNEL\":[\"EAS\",\"NWEM\",\"CMAS\"],\"EAS-ORG\":[\"WXR\"],\"VTEC\":[\"/O.CON.KMHX.CF.W.0003.000000T0000Z-"
    "261020T2100Z/\"],\"eventEndingTime\":[\"2026-10-20T17:00:00-04:00\"],\"expiredReferences\":[\"w-nws.webmaste"
    "r@noaa.gov,urn:oid:2.49.0.1.840.0.523cfd40838f89a4c2dee3d1c22dbaad7c188f65.001.1,2026-10-18T03:24:00"
    "-04:00 w-nws.webmaster@noaa.gov,urn:oid:2.49.0.1.840.0.13ae7e1da47230cfff91b99011363c9ace051d59.001."
    "1,2026-10-17T20:33:00-04:00 w-nws.webmaster@noaa.gov,urn:oid:2.49.0.1.840.0.1c2331befea2bbe6962538ac"
    "28872fa4c0a0a9d8.001.3,2026-10-17T19:40:00-04:00 w-nws.webmaster@noaa.gov,urn:oid:2.49.0.1.840.0.b0a"
    "ce643ad96eb8e139c9106400a2f229ef03ece.002.1,2026-10-17T15:46:00-04:00 w-nws.webmaster@noaa.gov,urn:o"
    "id:2.49.0.1.840.0.f462365b1bb348e8cba6bfb4b4def2f6f738643a.001.1,2026-10-17T05:02:00-04:00 w-nws.web"
    "master@noaa.gov,urn:oid:2.49.0.1.840.0.c636071d8131427853ca9202fcfbad16464ef59f.001.2,2026-10-17T03:"
    "35:00-04:00\"]},\"scope\":\"Public\",\"code\":\"IPAWSv1.0\",\"language\":\"en-US\",\"web\":\"http://www.weather.gov\""
    ",\"eventCode\":{\"SAME\":[\"CFW\"],\"NationalWeatherService\":[\"CFW\"]}}}]}";
  alert_reset(false);fake_clock_reset((time_t)civil_seconds(2026,10,18,22,0,0));
  http_body=captured;int64_t pause;
  assert(weather_client_alerts_check(&applied_config,true,&pause)==ESP_OK);
  app_alert_snapshot_t state=alert_summary();assert(state.warning_count==1&&alert_sound_starts==1);
  uint64_t id=state.headline_id;weather_alert_row_t row;
  assert(weather_client_alerts_row_copy(0,&row)==ESP_OK&&row.cutoff==civil_seconds(2026,10,20,21,0,0));
  weather_client_alerts_dismiss();alert_finish();
  cJSON *root=cJSON_Parse(captured);assert(root);cJSON *feature=cJSON_GetArrayItem(cJSON_GetObjectItem(root,"features"),0);
  cJSON *p=cJSON_GetObjectItem(feature,"properties");
  alert_reference(feature,"urn:oid:2.49.0.1.840.0.cb161830b749cbd0e733cc365ccc1a651272c8dc.001.2","2026-10-18T16:04:00-04:00");
  alert_replace(p,"id","controlled-followup-to-real-cap");alert_replace(p,"sent","2026-10-19T00:00:00+00:00");
  assert(alert_download(root)==ESP_OK&&nws_events[0]->id==id&&!alert_summary().headline_id&&alert_sound_starts==1);
  fake_clock_set((time_t)civil_seconds(2026,10,19,10,0,0));assert(alert_summary().warning_count==1);
  fake_clock_set((time_t)civil_seconds(2026,10,20,21,0,1));assert(alert_summary().warning_count==0);
  cJSON_Delete(root);
  puts("PASS: actual historical NWS Coastal Flood Warning feature, full protective text, published references/expiredReferences, constructed follow-up and ends later than expires.");
}
static void check_alert_owner(void)
{
  check_alert_official_cap_sample();check_alert_catalog_and_details();check_alert_relationships();
  check_alert_updates_and_sound_history();check_alert_failures_and_cutoffs();check_alert_storage_failures();
  check_alert_canonical_updates();check_alert_sound_dismiss_handoff();
  check_alert_blocked_operations();check_alert_source_markers_and_removal();check_alert_upgrade_and_year_boundary();
  check_alert_scheduler();check_alert_night_transition();check_alert_capacity_workload();
  alert_forget_owner();fake_nvs_reset();
  nws_http_test=false;
}

int main(int argc, char **argv)
{
  assert(weather_client_init()==ESP_OK);
  if (argc>1&&!strcmp(argv[1],"--alerts-only")) {
    check_alert_owner();assert(pthread_mutex_destroy(weather_copy_lock)==0);free(weather_copy_lock);return 0;
  }
  if (argc==3 && strcmp(argv[1],"--radar-saved-tiles")==0) {
    check_saved_radar_tiles(argv[2]);
    return 0;
  }
  if (argc==3 && strcmp(argv[1],"--radar-six-frame-data")==0) {
    check_saved_radar_six_frames(argv[2]);return 0;
  }
  if (argc==3 && strcmp(argv[1],"--radar-saved-data")==0) {
    check_saved_radar_data(argv[2]);
    assert(pthread_mutex_destroy(weather_copy_lock)==0);free(weather_copy_lock);
    return 0;
  }
  if (argc>1 && strcmp(argv[1],"--radar-only")==0) {
    check_worker_radar_schedule();
    check_worker_radar_offline();
    check_radar_geometry_palette_png();
    check_radar_publication();
    check_radar_history_playback();
    check_radar_history_failure_status();
    check_radar_location_release_and_entry_latest();
    assert(pthread_mutex_destroy(weather_copy_lock)==0);
    free(weather_copy_lock);
    puts("PASS: constructed Radar PNG/palette/coverage, six-frame cache/playback, copied controls, deadlines, request/cap limits and failure retention.");
    return 0;
  }
  check_decoder();
  check_worker_radar_schedule();
  check_worker_radar_offline();
  check_pending_time();
  check_download_timestamp();
  check_validation_nonpublication();
  check_time_start_failure();
  check_location_required();
  check_quiet_connection_waits();
  check_slow_weather_and_time_retry();
  check_weather_failure();
  check_overlapping_failure_deadlines();
  check_retained_weather();
  check_retained_weather_after_wifi_loss();
  check_current_period_and_failure();
  check_vc_decoder();
  check_quota();
  check_retained_real_samples();
  check_request_overlap();
  check_worker_quota_schedule();
  check_worker_service_independence();
  check_vc_local_time_dst();
  check_request_scratch_bounds();
  const char *live_response=argc>1?argv[1]:getenv("WAC_WEATHER_LIVE_RESPONSE");
  if (live_response && live_response[0]) check_saved_live_response(live_response);
  check_radar_geometry_palette_png();
  check_radar_publication();
  check_radar_history_playback();
  check_radar_history_failure_status();
  check_radar_location_release_and_entry_latest();
  check_alert_owner();
  assert(pthread_mutex_destroy(weather_copy_lock) == 0);
  free(weather_copy_lock);
  puts("PASS: both providers and units, real forecast captures, constructed current/DST/optional/error cases, bounded responses, serialized overlap, key-specific quota recovery, independent time/weather scheduling and retained data.");
  return 0;
}

#include "weather_client.h"

#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "connectivity.h"
#include "forecast_representative.h"
#include "weather_icon_names.h"
#include "location_scoring.h"
#include "nvs.h"
#include "night.h"
#include "mbedtls/sha256.h"
#include "lvgl.h"
#include "src/libs/lodepng/lodepng.h"
#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#include "src/draw/lv_draw_buf_private.h"
#endif

static const char *TAG = "rwd_weather";
#define WEATHER_HTTP_TIMEOUT_MS 10000
#define WEATHER_RESPONSE_MAX (192 * 1024)
#define WEATHER_HOURS_MAX 192
#define QUOTA_HOUR_US INT64_C(3600000000)

static SemaphoreHandle_t request_gate;
static StaticSemaphore_t request_gate_storage;
static StaticSemaphore_t nws_lock_storage;
static SemaphoreHandle_t nws_lock;
typedef struct {
  char key[DEVICE_CONFIG_WEATHER_KEY_LEN];
  bool paused;
  int64_t next_us;
} quota_record_t;
static quota_record_t quota[2];
static unsigned request_count;

static const uint32_t WEATHER_COPY_TIMEOUT_MS = 250;
static const uint32_t WIFI_CONNECT_TIMEOUT_MS = 30000;
static const uint32_t WEATHER_REFRESH_INTERVAL_MS = 10 * 60 * 1000;
static const uint32_t WEATHER_RETRY_INTERVAL_MS = 20000;
static const uint32_t RADAR_REFRESH_INTERVAL_MS = 5 * 60 * 1000;
static const uint32_t RADAR_STEP_INTERVAL_MS = 100;
static const int64_t NWS_CHECK_INTERVAL_US = INT64_C(60000000);

typedef struct {
  int64_t due_us;
  bool active, publication_pending;
} weather_radar_work_t;

static struct {
  app_weather_snapshot_t weather;
  app_radar_snapshot_t radar;
} weather_completed;
static SemaphoreHandle_t weather_copy_lock;
static uint32_t radar_displayed_generation;
static TaskHandle_t weather_ui_task;

typedef struct {
  char *data;
  size_t length, capacity, maximum;
  esp_err_t error;
  bool external; /* Radar response bytes use PSRAM; forecast allocation is unchanged. */
  char retry_after[96], reset[96], date[96];
} http_response_t;

static const cJSON *field(const cJSON *obj, const char *name)
{
  return cJSON_GetObjectItemCaseSensitive(obj, name);
}
static const char *string(const cJSON *item)
{
  return cJSON_IsString(item) ? cJSON_GetStringValue(item) : NULL;
}
static double number(const cJSON *item)
{
  return cJSON_IsNumber(item) && isfinite(item->valuedouble) ? item->valuedouble : NAN;
}
static const cJSON *at(const cJSON *array, int i) { return cJSON_GetArrayItem(array, i); }
static int rounded(double value) { return (int) round(value); }
static bool metric(const device_config_t *config) { return strcmp(config->units, "metric") == 0; }
static bool equal_case(const char *a, const char *b)
{
  if (!a || !b) return false;
  while (*a && *b && tolower((unsigned char)*a) == tolower((unsigned char)*b)) { ++a; ++b; }
  return *a == '\0' && *b == '\0';
}
static bool append_number(cJSON *array, double value)
{
  cJSON *item = isfinite(value) ? cJSON_CreateNumber(value) : cJSON_CreateNull();
  if (!item) return false;
  if (cJSON_AddItemToArray(array, item)) return true;
  cJSON_Delete(item); return false;
}

esp_err_t weather_client_init(void)
{
  if (!weather_copy_lock) weather_copy_lock=xSemaphoreCreateMutex();
  if (!weather_copy_lock) return ESP_ERR_NO_MEM;
  if (!nws_lock) nws_lock=xSemaphoreCreateMutexStatic(&nws_lock_storage);
  if (!nws_lock) return ESP_ERR_NO_MEM;
  if (request_gate) return ESP_OK;
  /* Serialize whole requests without retaining an SDK mutex's inherited
   * priority through the remaining network and decode work. */
  request_gate = xSemaphoreCreateBinaryStatic(&request_gate_storage);
  if (request_gate) xSemaphoreGive(request_gate);
#ifdef ESP_PLATFORM
  /* IDF's DEBUG request-header log includes the credential-bearing query.
   * VC uses host/path/query below, avoiding URL parse errors that echo a URL. */
  esp_log_level_set("HTTP_CLIENT", ESP_LOG_WARN);
#endif
  return request_gate ? ESP_OK : ESP_ERR_NO_MEM;
}
static quota_record_t *record_for(const device_config_t *config)
{
  /* Called under request_gate. Promote a newly applied draft key's pause so
   * saving it cannot bypass quota admission; an unrelated draft has its own slot. */
  if (config->weather_provider != DEVICE_CONFIG_WEATHER_VISUAL_CROSSING) return NULL;
  device_config_t applied;
  if (device_config_copy(&applied) == ESP_OK) {
    const char *key = applied.weather_provider == DEVICE_CONFIG_WEATHER_VISUAL_CROSSING
      ? applied.weather_api_key : "";
    if (strcmp(quota[0].key, key)) {
      if (key[0] && strcmp(quota[1].key, key) == 0) quota[0] = quota[1];
      else { memset(&quota[0], 0, sizeof(quota[0])); snprintf(quota[0].key, sizeof(quota[0].key), "%s", key); }
      if (strcmp(quota[1].key, key) == 0) memset(&quota[1], 0, sizeof(quota[1]));
    }
  }
  if (strcmp(quota[0].key, config->weather_api_key) == 0) return &quota[0];
  if (strcmp(quota[1].key, config->weather_api_key)) {
    memset(&quota[1], 0, sizeof(quota[1]));
    snprintf(quota[1].key, sizeof(quota[1].key), "%s", config->weather_api_key);
  }
  return &quota[1];
}
static int64_t weather_client_next_allowed_us(const device_config_t *config)
{
  if (!config || !request_gate) return 0;
  xSemaphoreTake(request_gate, portMAX_DELAY);
  quota_record_t *record = record_for(config);
  int64_t deadline = record && record->paused ? record->next_us : 0;
  xSemaphoreGive(request_gate);
  return deadline;
}

static void *radar_alloc(size_t size)
{
#ifdef ESP_PLATFORM
  return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
  return malloc(size);
#endif
}

static esp_err_t http_event(esp_http_client_event_t *event)
{
  http_response_t *response = event->user_data;
  if (event->event_id == HTTP_EVENT_ON_HEADER) {
    char *dest = NULL; size_t size = 0;
    if (equal_case(event->header_key, "Retry-After")) { dest = response->retry_after; size = sizeof(response->retry_after); }
    else if (equal_case(event->header_key, "X-RateLimit-Reset")) { dest = response->reset; size = sizeof(response->reset); }
    else if (equal_case(event->header_key, "Date")) { dest = response->date; size = sizeof(response->date); }
    if (dest && event->header_value) {
      snprintf(dest, size, "%s", event->header_value);
      size_t length=strlen(dest);
      while (length && isspace((unsigned char)dest[length-1])) dest[--length]='\0';
      size_t start=0;
      while (isspace((unsigned char)dest[start])) ++start;
      if (start) memmove(dest,dest+start,length-start+1);
    }
  }
  if (event->event_id != HTTP_EVENT_ON_DATA || event->data_len <= 0) return ESP_OK;
  if (response->error != ESP_OK) return response->error;
  size_t length = response->length + (size_t) event->data_len;
  size_t maximum=response->maximum ? response->maximum : WEATHER_RESPONSE_MAX;
  if (length > maximum) return response->error = ESP_ERR_INVALID_SIZE;
  if (length + 1 > response->capacity) {
    size_t capacity = response->capacity ? response->capacity : 4096;
    while (capacity < length + 1) capacity *= 2;
    if (capacity > maximum + 1) capacity = maximum + 1;
    char *data;
    if (response->external) {
      /* IDF realloc copies while its heap lock masks interrupts. Keep large
       * Radar-response copies outside that lock so audio can keep running. */
      data = radar_alloc(capacity);
      if (data) {
        if (response->length) memcpy(data, response->data, response->length);
        free(response->data);
      }
    } else data = realloc(response->data, capacity);
    if (!data) return response->error = ESP_ERR_NO_MEM;
    response->data = data; response->capacity = capacity;
  }
  memcpy(response->data + response->length, event->data, event->data_len);
  response->length = length; response->data[length] = '\0';
  return ESP_OK;
}
/* UTC civil date conversion, independent of the process's local timezone. */
static int64_t civil_seconds(int year, int month, int day, int hour, int minute, int second)
{
  if (year < 1970 || month < 1 || month > 12 || day < 1 || day > 31 || hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59) return 0;
  static const int month_days[]={31,28,31,30,31,30,31,31,30,31,30,31};
  int maximum=month_days[month-1]+(month==2 && year%4==0 && (year%100!=0 || year%400==0));
  if (day>maximum) return 0;
  year -= month <= 2;
  int era = year / 400, yoe = year - era * 400;
  int doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return ((int64_t)era * 146097 + doe - 719468) * 86400 + hour * 3600 + minute * 60 + second;
}
static int64_t date_seconds(const char *value)
{
  if (!value || !*value) return 0;
  int day, year, hour, minute, second, used = 0; char month[4], weekday[4], zone[4];
  if (sscanf(value, "%3s, %d %3s %d %d:%d:%d %3s%n", weekday, &day, month, &year, &hour, &minute, &second, zone, &used) != 8 || value[used] || strcmp(zone,"GMT")) return 0;
  const char *weekdays="SunMonTueWedThuFriSat";
  const char *weekday_found=strstr(weekdays,weekday);
  if (strlen(month)!=3 || strlen(weekday)!=3 || !weekday_found || (weekday_found-weekdays)%3) return 0;
  const char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
  const char *found = strstr(months, month);
  if (!found || (found - months) % 3) return 0;
  return civil_seconds(year, (int)(found - months) / 3 + 1, day, hour, minute, second);
}
static int64_t integer_seconds(const char *value)
{
  if (!value || !isdigit((unsigned char)*value)) return -1;
  char *end; long long seconds = strtoll(value, &end, 10);
  if (*end || seconds < 0 || seconds > INT64_MAX / 1000000) return -1;
  return seconds;
}
static int64_t recovery_deadline(const http_response_t *response)
{
  /* Convert provider wall-clock hints once into elapsed-time deadlines so a
   * later system-clock correction cannot shorten the required wait. */
  int64_t now = esp_timer_get_time(), reference = date_seconds(response->date);
  if (!reference && time(NULL) >= 1577836800) reference = time(NULL);
  int64_t wait = -1, retry = integer_seconds(response->retry_after);
  if (retry >= 0) wait = retry;
  else { retry = date_seconds(response->retry_after); if (retry && reference) wait = retry > reference ? retry - reference : 0; }
  int64_t reset = integer_seconds(response->reset);
  if (reset < 0) reset = date_seconds(response->reset);
  if (reset > 0 && reference) {
    int64_t reset_wait = reset > reference ? reset - reference : 0;
    if (reset_wait > wait) wait = reset_wait;
  }
  if (wait < 0) return now + QUOTA_HOUR_US;
  if (wait > (INT64_MAX - now) / 1000000) return INT64_MAX;
  return now + wait * 1000000;
}
static bool confirms_daily_quota(const char *body)
{
  if (!body) return false;
  /* Only inspect a bounded error message, never print provider content (which
   * may echo the credential). A generic 429/concurrency/monthly limit is not daily. */
  char message[2048]; size_t i = 0;
  for (; body[i] && i < sizeof(message)-1; ++i) message[i] = (char)tolower((unsigned char)body[i]);
  message[i] = '\0';
  /* Keep the exhaustion and daily allowance in one sentence. Concurrency
   * errors may also mention a still-available daily allowance separately. */
  char *sentence=message;
  while (*sentence) {
    char *end=strpbrk(sentence,".\n");
    if (end) *end='\0';
    bool daily = strstr(sentence,"daily") || strstr(sentence,"per day") || strstr(sentence,"for the day");
    bool limit = strstr(sentence,"limit") || strstr(sentence,"quota") || strstr(sentence,"allowance") || strstr(sentence,"maximum number");
    bool exhausted = strstr(sentence,"exceeded") || strstr(sentence,"exhausted") || strstr(sentence,"reached");
    if (daily && limit && exhausted) return true;
    if (!end) break;
    sentence=end+1;
  }
  return false;
}
static void encode(const char *input, char *output, size_t size)
{
  const char *hex = "0123456789ABCDEF"; size_t n = 0;
  for (; input && *input && n + 1 < size; ++input) {
    unsigned char ch = (unsigned char)*input;
    if (isalnum(ch) || strchr("-_.~", ch)) output[n++] = ch;
    else { if (n + 3 >= size) break; output[n++]='%'; output[n++]=hex[ch>>4]; output[n++]=hex[ch&15]; }
  }
  output[n] = '\0';
}

static bool local_timestamp(double epoch, double offset_hours, char *out, size_t size)
{
  if (!isfinite(epoch) || !isfinite(offset_hours) || epoch < 0 || epoch > 4102444800.0 || fabs(offset_hours) > 24) return false;
  time_t local = (time_t)(epoch + offset_hours * 3600);
  struct tm tm;
  if (!gmtime_r(&local, &tm)) return false;
  return strftime(out, size, "%Y-%m-%dT%H:%M:%S", &tm) != 0;
}
static double vc_offset(const cJSON *root, const cJSON *day, const cJSON *record)
{
  double offset = number(field(record,"tzoffset"));
  if (!isfinite(offset)) offset = number(field(day,"tzoffset"));
  if (!isfinite(offset)) offset = number(field(root,"tzoffset"));
  return offset;
}
static bool vc_timestamp(const cJSON *root, const cJSON *day, const cJSON *record,
                         char *out, size_t size)
{
  /* VC returns local day/hour strings even when individual records omit an
   * offset. The root offset can describe today rather than a future DST day;
   * keep the returned wall time and retain the epoch separately for identity. */
  const char *date = string(field(day,"datetime")), *clock = string(field(record,"datetime"));
  int year, month, day_number, hour, minute, second=0, used=0;
  bool valid_date=date && strlen(date)==10 && date[4]=='-' && date[7]=='-' &&
    isdigit((unsigned char)date[0]) && isdigit((unsigned char)date[1]) &&
    isdigit((unsigned char)date[2]) && isdigit((unsigned char)date[3]) &&
    isdigit((unsigned char)date[5]) && isdigit((unsigned char)date[6]) &&
    isdigit((unsigned char)date[8]) && isdigit((unsigned char)date[9]) &&
    sscanf(date,"%4d-%2d-%2d%n",&year,&month,&day_number,&used)==3 && used==10 &&
    civil_seconds(year,month,day_number,0,0,0)>0;
  bool valid_clock=false;
  if (clock && strlen(clock)==8) {
    valid_clock=sscanf(clock,"%2d:%2d:%2d%n",&hour,&minute,&second,&used)==3 && used==8;
  } else if (clock && strlen(clock)==5) {
    valid_clock=sscanf(clock,"%2d:%2d%n",&hour,&minute,&used)==2 && used==5;
  }
  valid_clock=valid_clock && clock[2]==':' &&
    isdigit((unsigned char)clock[0]) && isdigit((unsigned char)clock[1]) &&
    isdigit((unsigned char)clock[3]) && isdigit((unsigned char)clock[4]) &&
    (strlen(clock)==5 || (clock[5]==':' && isdigit((unsigned char)clock[6]) && isdigit((unsigned char)clock[7])));
  if (valid_date && valid_clock && hour>=0 && hour<=23 && minute>=0 && minute<=59 && second>=0 && second<=59) {
    int length=snprintf(out,size,"%sT%s",date,clock);
    return length>=0 && (size_t)length<size;
  }
  return local_timestamp(number(field(record,"datetimeEpoch")),vc_offset(root,day,record),out,size);
}
static bool normalize_om(cJSON *hourly)
{
  if (!cJSON_IsObject(hourly)) return true;
  cJSON *flags = cJSON_AddArrayToObject(hourly,"condition_flags");
  cJSON *sky = cJSON_AddArrayToObject(hourly,"sky");
  if (!flags || !sky) return false;
  int count = cJSON_GetArraySize(field(hourly,"time"));
  if (count > WEATHER_HOURS_MAX) return false;
  for (int i=0; i<count; ++i) {
    double code = number(at(field(hourly,"weathercode"),i));
    weather_condition_facts_t facts = weather_facts_from_open_meteo(isfinite(code)?rounded(code):-1,
      number(at(field(hourly,"rain"),i)), number(at(field(hourly,"showers"),i)), number(at(field(hourly,"snowfall"),i)));
    if (!append_number(flags,facts.flags) || !append_number(sky,facts.sky)) return false;
  }
  return true;
}
/* Numeric columns retain missing values as null. VC has no separate liquid
 * rain/showers quantities; its explicit facts provide those subtype counters. */
static cJSON *normalize_vc_hourly(const device_config_t *config, const cJSON *root)
{
  static const char *names[] = {"time","epoch","condition_flags","sky", "precipitation_probability","precipitation","rain","showers","snowfall","cloud_cover"};
  cJSON *result=cJSON_CreateObject(), *columns[10];
  if (!result) return NULL;
  for (int i=0;i<10;++i) { columns[i]=cJSON_AddArrayToObject(result,names[i]); if (!columns[i]) goto fail; }
  int count=0; const cJSON *days=field(root,"days");
  double precip_factor=metric(config)?1:25.4, snow_factor=metric(config)?1:2.54;
  for (int d=0; d<cJSON_GetArraySize(days); ++d) {
    const cJSON *day=at(days,d), *hours=field(day,"hours");
    for (int h=0;h<cJSON_GetArraySize(hours);++h) {
      if (++count>WEATHER_HOURS_MAX) goto fail;
      const cJSON *record=at(hours,h); char timestamp[40]="";
      vc_timestamp(root,day,record,timestamp,sizeof(timestamp));
      cJSON *clock=cJSON_CreateString(timestamp);
      if (!clock) goto fail;
      if (!cJSON_AddItemToArray(columns[0],clock)) { cJSON_Delete(clock); goto fail; }
      double snow=number(field(record,"snow"))*snow_factor;
      weather_condition_facts_t facts=weather_facts_from_visual_crossing(record,snow);
      double values[]={number(field(record,"datetimeEpoch")),facts.flags,facts.sky,
        number(field(record,"precipprob")),number(field(record,"precip"))*precip_factor,
        NAN,NAN,snow,number(field(record,"cloudcover"))};
      for (int i=1;i<10;++i) if (!append_number(columns[i],values[i-1])) goto fail;
    }
  }
  return result;
fail:
  cJSON_Delete(result); return NULL;
}
static bool vc_is_day(const cJSON *root,const cJSON *current)
{
  const char *icon=string(field(current,"icon"));
  if (icon && strstr(icon,"-night")) return false;
  if (icon && strstr(icon,"-day")) return true;
  double epoch=number(field(current,"datetimeEpoch"));
  char timestamp[40]=""; local_timestamp(epoch,vc_offset(root,NULL,current),timestamp,sizeof(timestamp));
  const cJSON *days=field(root,"days");
  for (int i=0;i<cJSON_GetArraySize(days);++i) {
    const cJSON *day=at(days,i); const char *date=string(field(day,"datetime"));
    if (!date || strncmp(timestamp,date,10)) continue;
    double sunrise=number(field(day,"sunriseEpoch")), sunset=number(field(day,"sunsetEpoch"));
    if (isfinite(sunrise)&&isfinite(sunset)) return epoch>=sunrise && epoch<sunset;
    const char *rise=string(field(day,"sunrise")), *set=string(field(day,"sunset"));
    if (rise&&set) return strcmp(timestamp+11,rise)>=0 && strcmp(timestamp+11,set)<0;
  }
  return true; /* No variant is invented when no source day/night evidence exists. */
}
static esp_err_t decode(const device_config_t *config,cJSON *root,app_weather_snapshot_t *out)
{
  bool vc=config->weather_provider==DEVICE_CONFIG_WEATHER_VISUAL_CROSSING;
  const cJSON *current=field(root,vc?"currentConditions":"current_weather"), *days=field(root,"days");
  cJSON *daily=(cJSON *)field(root,"daily"), *hourly=(cJSON *)field(root,"hourly");
  double temperature=number(field(current,vc?"temp":"temperature"));
  if (!isfinite(temperature)||fabs(temperature)>1000 || !cJSON_IsObject(current)) return ESP_FAIL;
  if (vc) {
    double epoch=number(field(current,"datetimeEpoch"));
    if (!isfinite(epoch) || epoch<0 || epoch>4102444800.0) return ESP_FAIL;
    hourly=normalize_vc_hourly(config,root);
    if (!hourly) return ESP_ERR_NO_MEM;
  } else if (!cJSON_IsObject(daily)||!normalize_om(hourly)) return ESP_FAIL;
  /* Required upcoming high/low pairs are checked before publishing any field. */
  for (int i=1;i<=APP_FORECAST_DAYS;++i) {
    double high=vc?number(field(at(days,i),"tempmax")):number(at(field(daily,"temperature_2m_max"),i));
    double low=vc?number(field(at(days,i),"tempmin")):number(at(field(daily,"temperature_2m_min"),i));
    if (!isfinite(high)||!isfinite(low)||fabs(high)>1000||fabs(low)>1000||high<low) goto invalid;
    out->forecast[i-1].high=rounded(high); out->forecast[i-1].low=rounded(low);
    const char *date=vc?string(field(at(days,i),"datetime")):string(at(field(daily,"time"),i));
    if (date && strlen(date)==10) memcpy(out->forecast[i-1].date,date,11);
    weather_condition_facts_t facts;
    if (vc) facts=weather_facts_from_visual_crossing(at(days,i),NAN);
    else { double code=number(at(field(daily,"weathercode"),i)); facts=weather_facts_from_open_meteo(isfinite(code)?rounded(code):-1,NAN,NAN,NAN); }
    weather_picture_t picture=forecast_pick_representative_picture(hourly,date,&facts,high,low,metric(config)?1:34);
    snprintf(out->forecast[i-1].icon_name,sizeof(out->forecast[i-1].icon_name),"%s",weather_icon_name_for_picture(picture,true));
  }
  double high=vc?number(field(at(days,0),"tempmax")):number(at(field(daily,"temperature_2m_max"),0));
  double low=vc?number(field(at(days,0),"tempmin")):number(at(field(daily,"temperature_2m_min"),0));
  out->temp=rounded(temperature); out->high=isfinite(high)?rounded(fmax(high,temperature)):out->temp;
  out->low=isfinite(low)?rounded(fmin(low,temperature)):out->temp;
  weather_condition_facts_t facts;
  bool is_day;
  if (vc) {
    facts=weather_facts_from_visual_crossing(current,number(field(current,"snow"))*(metric(config)?1:2.54));
    is_day=vc_is_day(root,current);
  } else {
    double code=number(field(current,"weathercode"));
    is_day=number(field(current,"is_day"))==1;
    /* New OM `current` supplies same-interval amounts. Older stored samples
     * lack these optional fields; never borrow an earlier hourly interval. */
    const cJSON *amounts=field(root,"current");
    facts=weather_facts_from_open_meteo(isfinite(code)?rounded(code):-1,number(field(amounts,"rain")),number(field(amounts,"showers")),number(field(amounts,"snowfall")));

  }
  weather_picture_t picture=weather_picture_for_facts(&facts);
  snprintf(out->summary,sizeof(out->summary),"%s",weather_caption_for_facts(&facts,is_day));
  snprintf(out->icon_name,sizeof(out->icon_name),"%s",weather_icon_name_for_picture(picture,is_day));
  out->provider=config->weather_provider;
  out->valid=true; out->updated_at=time(NULL);
  if (vc) { cJSON_Delete(hourly); }
  return ESP_OK;
invalid:
  if (vc) { cJSON_Delete(hourly); }
  return ESP_FAIL;
}

/* Keep request scratch out of the decoding call stack. The target
 * compiler must preserve this boundary: HTTP finishes before JSON is parsed. */
static __attribute__((noinline)) esp_err_t perform_request(
  const device_config_t *config,http_response_t *response,int *status)
{
  union {
    struct { char url[1024], timezone[DEVICE_CONFIG_STR_LEN*3]; } om;
    struct { char path[160], query[1024]; } vc;
  } request;
  esp_http_client_config_t http_config={.event_handler=http_event,.user_data=response,
    .timeout_ms=WEATHER_HTTP_TIMEOUT_MS,.buffer_size=2048,.crt_bundle_attach=esp_crt_bundle_attach,
    .disable_auto_redirect=true};
  if (config->weather_provider==DEVICE_CONFIG_WEATHER_VISUAL_CROSSING) {
    snprintf(request.vc.path,sizeof(request.vc.path),"/VisualCrossingWebServices/rest/services/timeline/%s,%s/next6days",config->latitude,config->longitude);
    int length=snprintf(request.vc.query,sizeof(request.vc.query),"unitGroup=%s&include=current,days,hours&lang=id&iconSet=icons2&contentType=json"
      "&elements=datetime,datetimeEpoch,tzoffset,temp,tempmax,tempmin,precip,precipprob,preciptype,snow,cloudcover,conditions,icon,sunrise,sunset,sunriseEpoch,sunsetEpoch&key=",
      metric(config)?"metric":"us");
    if (length<0 || (size_t)length>=sizeof(request.vc.query)) return ESP_ERR_INVALID_SIZE;
    encode(config->weather_api_key,request.vc.query+length,sizeof(request.vc.query)-(size_t)length);
    http_config.host="weather.visualcrossing.com"; http_config.path=request.vc.path; http_config.query=request.vc.query;
    http_config.transport_type=HTTP_TRANSPORT_OVER_SSL;
  } else {
    encode(config->timezone,request.om.timezone,sizeof(request.om.timezone));
    snprintf(request.om.url,sizeof(request.om.url),"https://api.open-meteo.com/v1/forecast?latitude=%s&longitude=%s&current_weather=true"
      "&current=rain,showers,snowfall&hourly=weathercode,precipitation_probability,precipitation,rain,showers,snowfall,cloud_cover"
      "&daily=temperature_2m_max,temperature_2m_min,weathercode&temperature_unit=%s&timezone=%s&forecast_days=6",
      config->latitude,config->longitude,metric(config)?"celsius":"fahrenheit",request.om.timezone[0]?request.om.timezone:"UTC");
    http_config.url=request.om.url;
  }
  esp_http_client_handle_t client=esp_http_client_init(&http_config);
  if (!client) return ESP_FAIL;
  ++request_count;
  esp_err_t err=esp_http_client_perform(client);
  *status=esp_http_client_get_status_code(client);
  esp_http_client_cleanup(client);
  return response->error!=ESP_OK ? response->error : err;
}

esp_err_t weather_client_fetch(const device_config_t *config,app_weather_snapshot_t *out)
{
  if (!config||!out) return ESP_ERR_INVALID_ARG;
  if (!request_gate || !config->latitude[0] || !config->longitude[0] ||
      (config->weather_provider!=DEVICE_CONFIG_WEATHER_OPEN_METEO && config->weather_provider!=DEVICE_CONFIG_WEATHER_VISUAL_CROSSING)) return ESP_ERR_INVALID_STATE;
  bool vc=config->weather_provider==DEVICE_CONFIG_WEATHER_VISUAL_CROSSING;
  if (vc && !config->weather_api_key[0]) return ESP_ERR_INVALID_ARG;
  xSemaphoreTake(request_gate,portMAX_DELAY);
  quota_record_t *record=record_for(config);
  if (record&&record->paused&&esp_timer_get_time()<record->next_us) {
    xSemaphoreGive(request_gate); return ESP_ERR_INVALID_STATE;
  }
  http_response_t response={0};
  int status=0; double query_cost=NAN;
  esp_err_t err=perform_request(config,&response,&status);
  if (err==ESP_OK && (status<200||status>=300)) err=ESP_FAIL;
  if (record && status==429 && confirms_daily_quota(response.data)) record->paused=true;
  if (err==ESP_OK) {
    cJSON *root=response.data?cJSON_Parse(response.data):NULL;
    app_weather_snapshot_t *candidate=root?calloc(1,sizeof(*candidate)):NULL;
    query_cost=number(field(root,"queryCost"));
    if (!root) err=ESP_FAIL;
    else if (!candidate) err=ESP_ERR_NO_MEM;
    else { err=decode(config,root,candidate); if (err==ESP_OK) *out=*candidate; }
    free(candidate); cJSON_Delete(root);
  }
  if (record && record->paused) {
    if (err==ESP_OK) { record->paused=false; record->next_us=0; }
    else record->next_us=recovery_deadline(&response);
  }
  ESP_LOGI(TAG,"provider=%s request=%u status=%d bytes=%u queryCost=%.0f result=%d",
    vc?"Visual Crossing":"Open-Meteo",request_count,status,(unsigned)response.length,query_cost,err);
  free(response.data);
  xSemaphoreGive(request_gate); return err;
}

/* Official alerts share the existing weather worker and HTTP gate. The short
 * state lock is never held across a request, NVS call or audio command. */
#define NWS_EVENT_MAX 64
#define NWS_ID_MAX 256
#define NWS_NAMESPACE "wac_nws"
#define NWS_SOUND_SEPARATION_US INT64_C(600000000)
static const weather_alert_type_t nws_catalog[] = {
  {"Avalanche Watch",false,0,86},{"Coastal Flood Watch",false,1,90},
  {"Extreme Cold Watch",false,2,94},{"Extreme Heat Watch",false,3,93},
  {"Fire Weather Watch",false,4,96},{"Flash Flood Watch",false,5,47},
  {"Flood Watch",false,6,89},{"Freeze Watch",false,7,95},
  {"Gale Watch",false,8,85},{"Hazardous Seas Watch",false,9,87},
  {"Heavy Freezing Spray Watch",false,10,88},{"High Wind Watch",false,11,92},
  {"Hurricane Force Wind Watch",false,12,55},{"Hurricane Watch",false,13,54},
  {"Lakeshore Flood Watch",false,14,91},{"Severe Thunderstorm Watch",false,15,46},
  {"Storm Surge Watch",false,16,53},{"Storm Watch",false,17,58},
  {"Tornado Watch",false,18,45},{"Tropical Storm Watch",false,19,57},
  {"Tsunami Watch",false,20,34},{"Typhoon Watch",false,21,56},
  {"Winter Storm Watch",false,22,82},
  {"Ashfall Warning",true,0,38},{"Blizzard Warning",true,1,22},
  {"Blowing Dust Warning",true,2,29},{"Coastal Flood Warning",true,3,40},
  {"Dust Storm Warning",true,4,28},{"Extreme Cold Warning",true,5,50},
  {"Extreme Heat Warning",true,6,44},{"Extreme Wind Warning",true,7,3},
  {"Flash Flood Warning",true,8,5},{"Flood Warning",true,9,39},
  {"Freeze Warning",true,10,51},{"Gale Warning",true,11,48},
  {"Hazardous Seas Warning",true,12,72},{"Heavy Freezing Spray Warning",true,13,25},
  {"High Surf Warning",true,14,43},{"High Wind Warning",true,15,30},
  {"Hurricane Force Wind Warning",true,16,18},{"Hurricane Warning",true,17,19},
  {"Ice Storm Warning",true,18,24},{"Lake Effect Snow Warning",true,19,27},
  {"Lakeshore Flood Warning",true,20,41},{"Red Flag Warning",true,21,52},
  {"Severe Thunderstorm Warning",true,22,4},{"Snow Squall Warning",true,23,23},
  {"Special Marine Warning",true,24,21},{"Storm Surge Warning",true,25,17},
  {"Storm Warning",true,26,32},{"Tornado Warning",true,27,2},
  {"Tropical Storm Warning",true,28,31},{"Tsunami Warning",true,29,1},
  {"Typhoon Warning",true,30,20},{"Winter Storm Warning",true,31,26},
};
/* Phenomena are part of the same catalog owner; significance comes from the
 * family. Flood Warning can be areal FA or forecast-point FL. */
static const char *const nws_phenomena[] = {
  "AV","CF","EC","EH","FW","FF","FA","FZ","GL","SE","UP","HW",
  "HF","HU","LS","SV","SS","SR","TO","TR","TS","TY","WS",
  "AF","BZ","DU","CF","DS","EC","EH","EW","FF","FAFL","FZ","GL",
  "SE","UP","SU","HW","HF","HU","IS","LE","LS","FW","SV","SQ",
  "MA","SS","SR","TO","TR","TS","TY","WS"
};
_Static_assert(sizeof(nws_catalog)/sizeof(nws_catalog[0])==55 &&
  sizeof(nws_phenomena)/sizeof(nws_phenomena[0])==55,"complete reviewed NWS catalog");
const weather_alert_type_t *weather_client_alert_catalog(size_t *count)
{
  if (count) *count=55;
  return nws_catalog;
}
typedef struct {
  uint64_t id;
  uint32_t version, saved_version, reached, visual_reached;
  int64_t issued, onset, expires, ends, last_sound_utc, last_sound_us;
  bool dismissed, restored_sound, vtec_new;
  uint16_t origin_year;
  uint8_t type, slot;
  uint8_t extra_digest[32];
  char *message, *sender, *vtec, *text[4];
} nws_event_t;
typedef struct {
  char latitude[DEVICE_CONFIG_COORD_LEN], longitude[DEVICE_CONFIG_COORD_LEN];
  char name[DEVICE_CONFIG_STR_LEN];
} nws_location_t;
/* A task mutex keeps reconciliation interruptible by audio; no HTTP or NVS
 * operation runs while this state lock is held. */
static nws_event_t *nws_events[NWS_EVENT_MAX];
static size_t nws_count;
static nws_location_t nws_location;
static uint32_t nws_generation, nws_revision;
static uint64_t nws_next_id=1, nws_audio_event;
static uint32_t nws_audio_token;
static bool nws_initialized, nws_dirty, nws_save_failed;
static app_alert_status_t nws_status=APP_ALERT_CHECKING;
static bool nws_has_success;
static int64_t nws_last_success;

static nws_location_t nws_location_for(const device_config_t *config)
{
  nws_location_t location={0};
  if (config) {
    snprintf(location.latitude,sizeof(location.latitude),"%s",config->latitude);
    snprintf(location.longitude,sizeof(location.longitude),"%s",config->longitude);
    snprintf(location.name,sizeof(location.name),"%s",config->location);
  }
  return location;
}
static bool nws_same_location(nws_location_t a,nws_location_t b)
{
  return !strcmp(a.latitude,b.latitude) && !strcmp(a.longitude,b.longitude) && !strcmp(a.name,b.name);
}
static void nws_free(nws_event_t *event)
{
  if (!event) return;
  free(event->message);free(event->sender);free(event->vtec);
  for (int i=0;i<4;++i) free(event->text[i]);
  free(event);
}
static char *nws_dup(const char *value)
{
  size_t length=strlen(value)+1;char *copy=malloc(length);
  if (copy) memcpy(copy,value,length);
  return copy;
}
static int64_t nws_cutoff(const nws_event_t *event) { return event->ends?event->ends:event->expires; }
static bool nws_active(const nws_event_t *event,int64_t now)
{
  return !now || nws_cutoff(event)>now;
}
static int nws_compare(const void *left,const void *right)
{
  const nws_event_t *a=*(nws_event_t *const *)left,*b=*(nws_event_t *const *)right;
  int difference=(int)nws_catalog[a->type].priority-(int)nws_catalog[b->type].priority;
  if (difference) return difference;
  int identity=strcmp(a->vtec,b->vtec);
  if (identity) return identity;
  return a->id<b->id?-1:a->id>b->id;
}
static int64_t nws_now(void) { int64_t now=time(NULL);return now>=1577836800?now:0; }
/* Strict ISO-8601 seconds, including UTC Z and numeric offsets. */
static bool nws_timestamp(const char *value,int64_t *out)
{
  if (!value || strlen(value)<20 || strlen(value)>25) return false;
  const int positions[]={4,7,10,13,16}; const char separators[]="--T::";
  for (int i=0;i<5;++i) if (value[positions[i]]!=separators[i]) return false;
  for (int i=0;i<19;++i) {
    if (i==4||i==7||i==10||i==13||i==16) continue;
    if (!isdigit((unsigned char)value[i])) return false;
  }
  int y,m,d,h,min,s,zh=0,zm=0;
  if (sscanf(value,"%4d-%2d-%2dT%2d:%2d:%2d",&y,&m,&d,&h,&min,&s)!=6) return false;
  int64_t seconds=civil_seconds(y,m,d,h,min,s);if (!seconds) return false;
  if (value[19]=='Z' && !value[20]) { *out=seconds;return true; }
  if (strlen(value)!=25 || (value[19]!='+'&&value[19]!='-') || value[22]!=':' ||
      !isdigit((unsigned char)value[20])||!isdigit((unsigned char)value[21])||
      !isdigit((unsigned char)value[23])||!isdigit((unsigned char)value[24]) ||
      sscanf(value+20,"%2d:%2d",&zh,&zm)!=2 || zh>23 || zm>59) return false;
  *out=seconds+(value[19]=='+'?-1:1)*(zh*3600+zm*60);
  return *out>0;
}
static bool nws_time_field(const cJSON *properties,const char *name,bool nullable,int64_t *out)
{
  const cJSON *value=field(properties,name);
  if (nullable && cJSON_IsNull(value)) { *out=0;return true; }
  return nws_timestamp(string(value),out);
}
static int nws_type(const char *name)
{
  if (name) for (int i=0;i<55;++i) if (!strcmp(name,nws_catalog[i].name)) return i;
  return -1;
}
static const char *nws_parameter(const cJSON *properties,const char *name)
{
  const cJSON *values=field(field(properties,"parameters"),name);
  return cJSON_IsArray(values)&&cJSON_GetArraySize(values)==1?string(at(values,0)):NULL;
}
static bool nws_marker(const cJSON *properties,const char *name,const char *value)
{
  const char *actual=nws_parameter(properties,name);return actual && !strcmp(actual,value);
}
static bool nws_pds(const char *headline,bool declaration)
{
  /* Only an affirmative designation in the NWS headline, never an incidental
   * substring in a forecast discussion or protective instructions. */
  if (!headline) return false;
  char *normalized=malloc(strlen(headline)+1);if (!normalized) return false;
  size_t n=0;
  for (size_t i=0;headline[i];++i) {
    unsigned char c=(unsigned char)headline[i];
    if (isspace(c)) { if (n && normalized[n-1]!=' ') normalized[n++]=' '; }
    else normalized[n++]=(char)toupper(c);
  }
  normalized[n]='\0';
  bool designated=strstr(normalized,"PARTICULARLY DANGEROUS SITUATION") &&
    !strstr(normalized,"NOT A PARTICULARLY") && !strstr(normalized,"POSSIBLE PARTICULARLY") &&
    !strstr(normalized,"POTENTIAL PARTICULARLY");
  if (declaration && !strstr(normalized,"THIS IS A PARTICULARLY DANGEROUS SITUATION")) designated=false;
  free(normalized);return designated;
}
static uint32_t nws_milestones(int type,const cJSON *properties,const char *headline)
{
  const char *name=nws_catalog[type].name;
  const char *parameter=NULL,*highest=NULL;
  if (!strcmp(name,"Tornado Warning")) { parameter="tornadoDamageThreat";highest="CATASTROPHIC"; }
  if (!strcmp(name,"Severe Thunderstorm Warning")) { parameter="thunderstormDamageThreat";highest="DESTRUCTIVE"; }
  if (!strcmp(name,"Flash Flood Warning")) { parameter="flashFloodDamageThreat";highest="CATASTROPHIC"; }
  if (parameter) {
    if (nws_marker(properties,parameter,highest)) return 3;
    if (nws_marker(properties,parameter,"CONSIDERABLE")) return 1;
  }
  if (!strcmp(name,"Snow Squall Warning") && nws_marker(properties,"snowSquallImpact","SIGNIFICANT")) return 4;
  if ((!strcmp(name,"Red Flag Warning")||!strcmp(name,"Blizzard Warning")||!strcmp(name,"Ice Storm Warning")) &&
      (nws_pds(headline,false)||nws_pds(nws_parameter(properties,"NWSheadline"),false)||
       nws_pds(string(field(properties,"description")),true))) return 8;
  return 0;
}
static uint32_t nws_visual(int type,const cJSON *properties,uint32_t milestones)
{
  if (!strcmp(nws_catalog[type].name,"Tornado Warning") && nws_marker(properties,"tornadoDetection","OBSERVED")) milestones|=16;
  return milestones;
}
static cJSON *nws_canonical(const cJSON *value,unsigned depth)
{
  if (!value||depth>20) return NULL;
  if (!cJSON_IsObject(value)&&!cJSON_IsArray(value)) return cJSON_Duplicate(value,false);
  cJSON *copy=cJSON_IsObject(value)?cJSON_CreateObject():cJSON_CreateArray();if (!copy) return NULL;
  size_t count=(size_t)cJSON_GetArraySize(value);
  if (count>WEATHER_RESPONSE_MAX/2) { cJSON_Delete(copy);return NULL; }
  const cJSON **items=count?malloc(count*sizeof(*items)):NULL,*item;
  if (count&&!items) { cJSON_Delete(copy);return NULL; }
  size_t index=0;cJSON_ArrayForEach(item,value) items[index++]=item;
  /* Objects are sets of named fields; UGC/SAME and parameter string arrays
   * are sets too. Preserve the order of coordinate arrays. */
  bool ordered_object=cJSON_IsObject(value),strings=!ordered_object;
  if (strings) for (size_t i=0;i<count;++i) if (!cJSON_IsString(items[i])) strings=false;
  for (size_t i=1;i<count && (ordered_object||strings);++i) {
    const cJSON *selected=items[i];size_t j=i;
    while (j && strcmp(ordered_object?items[j-1]->string:string(items[j-1]),
                         ordered_object?selected->string:string(selected))>0) { items[j]=items[j-1];--j; }
    items[j]=selected;
  }
  for (size_t i=0;i<count;++i) {
    if (ordered_object && (!items[i]->string || (i && !strcmp(items[i-1]->string,items[i]->string)))) goto failure;
    cJSON *child=nws_canonical(items[i],depth+1);
    if (!child) goto failure;
    bool added=ordered_object?cJSON_AddItemToObject(copy,items[i]->string,child):cJSON_AddItemToArray(copy,child);
    if (!added) { cJSON_Delete(child);goto failure; }
  }
  free(items);return copy;
failure:
  free(items);cJSON_Delete(copy);return NULL;
}
/* Canonical operational parameters: object and string-array order do not turn
 * an identical re-download into an update sound. Transport packaging is omitted. */
static char *nws_extra(const cJSON *properties,const cJSON *geometry)
{
  const cJSON *parameters=field(properties,"parameters");
  if (!cJSON_IsObject(parameters)) return NULL;
  cJSON *copy=cJSON_CreateObject();if (!copy) return NULL;
  cJSON *operational=cJSON_AddObjectToObject(copy,"parameters");if (!operational) goto failure;
  const cJSON *item;
  cJSON_ArrayForEach(item,parameters) {
    if (!item->string || !cJSON_IsArray(item)) goto failure;
    const char *key=item->string;
    if (!strcmp(key,"AWIPSidentifier")||!strcmp(key,"WMOidentifier")||!strcmp(key,"VTEC")||
        !strcmp(key,"expiredReferences")||!strcmp(key,"BLOCKCHANNEL")||!strcmp(key,"eventEndingTime")) continue;
    cJSON *value=cJSON_Duplicate(item,true);
    if (!value || !cJSON_AddItemToObject(operational,key,value)) { cJSON_Delete(value);goto failure; }
  }
  /* Includes area geometry and UGC/SAME codes, so moving an area is substantive
   * even when the free-form area description happens to remain unchanged. */
  const char *fields[]={"geocode","severity","certainty","urgency"};
  for (size_t i=0;i<sizeof(fields)/sizeof(fields[0]);++i) {
    item=field(properties,fields[i]);
    if (item) {
      cJSON *value=cJSON_Duplicate(item,true);
      if (!value || !cJSON_AddItemToObject(copy,fields[i],value)) { cJSON_Delete(value);goto failure; }
    }
  }
  if (geometry) {
    cJSON *value=cJSON_Duplicate(geometry,true);
    if (!value || !cJSON_AddItemToObject(copy,"geometry",value)) { cJSON_Delete(value);goto failure; }
  }
  cJSON *canonical=nws_canonical(copy,0);cJSON_Delete(copy);
  char *result=canonical?cJSON_PrintUnformatted(canonical):NULL;cJSON_Delete(canonical);return result;
failure:
  cJSON_Delete(copy);return NULL;
}
/* VTEC's begin/end dates and action are mutable. Keep office/phenomenon/
 * significance/ETN; CAP relationships carry the original identity over years. */
static bool nws_vtec_time(const char *text)
{
  if (!strncmp(text,"000000T0000Z",12)) return true;
  for (int i=0;i<10;++i) {
    if (i==6) { if (text[i]!='T') return false; }
    else if (i==11) { if (text[i]!='Z') return false; }
    else if (!isdigit((unsigned char)text[i])) return false;
  }
  char iso[24];snprintf(iso,sizeof(iso),"20%.2s-%.2s-%.2sT%.2s:%.2s:00Z",text,text+2,text+4,text+7,text+9);
  int64_t ignored;return nws_timestamp(iso,&ignored);
}
static char *nws_vtec(const cJSON *properties,int type,bool *is_new)
{
  const cJSON *array=field(field(properties,"parameters"),"VTEC");
  char result[64]="";
  if (!array) return nws_dup("");
  if (!cJSON_IsArray(array)) return NULL;
  const cJSON *item;
  cJSON_ArrayForEach(item,array) {
    const char *text=string(item);if (!text) return NULL;
    while (*text) {
      while (isspace((unsigned char)*text)) ++text;
      if (!*text) break;
      if (strlen(text)<48 || text[0]!='/'||text[1]!='O'||text[2]!='.'||text[6]!='.'||
          text[11]!='.'||text[14]!='.'||text[16]!='.'||text[21]!='.'||text[34]!='-'||text[47]!='/'||
          (text[48]&&!isspace((unsigned char)text[48]))) return NULL;
      for (int i=17;i<21;++i) if (!isdigit((unsigned char)text[i])) return NULL;
      char action[4],office[5],phenomenon[3],significance,begin[14],end[14];unsigned etn;int used=0;
      if (sscanf(text,"/O.%3[A-Z].%4[A-Z].%2[A-Z].%c.%4u.%12[0-9TZ]-%12[0-9TZ]/%n",
          action,office,phenomenon,&significance,&etn,begin,end,&used)!=7 || !used ||
          strlen(action)!=3||strlen(office)!=4||strlen(phenomenon)!=2||strlen(begin)!=12||strlen(end)!=12) return NULL;
      if (strcmp(action,"NEW")&&strcmp(action,"CON")&&strcmp(action,"EXT")&&strcmp(action,"EXA")&&
          strcmp(action,"EXB")&&strcmp(action,"CAN")&&strcmp(action,"EXP")&&strcmp(action,"COR")&&strcmp(action,"UPG")) return NULL;
      if (!nws_vtec_time(begin)||!nws_vtec_time(end)) return NULL;
      const char *allowed=nws_phenomena[type];
      if (significance==(nws_catalog[type].warning?'W':'A') &&
          (!strncmp(allowed,phenomenon,2) || (strlen(allowed)==4&&!strcmp(allowed+2,phenomenon)))) {
        if (strcmp(action,"CAN") && strcmp(action,"EXP") && strcmp(action,"UPG")) {
          char key[64];snprintf(key,sizeof(key),"%s.%s.%c.%04u",office,phenomenon,significance,etn);
          if (result[0] && strcmp(result,key)) return NULL;
          strcpy(result,key);
          if (!strcmp(action,"NEW")) *is_new=true;
        }
      }
      text+=used;
    }
  }
  return nws_dup(result);
}
static nws_event_t *nws_decode_event(const cJSON *feature,int type)
{
  const cJSON *properties=field(feature,"properties");
  const char *id=string(field(properties,"id")),*sender=string(field(properties,"sender"));
  if (!id||!*id||strlen(id)>NWS_ID_MAX || !sender||!*sender||strlen(sender)>128) return NULL;
  nws_event_t *event=calloc(1,sizeof(*event));if (!event) return NULL;
  event->type=(uint8_t)type;
  if (!nws_time_field(properties,"sent",false,&event->issued)||
      !nws_time_field(properties,"onset",true,&event->onset)||
      !nws_time_field(properties,"expires",false,&event->expires)||
      !nws_time_field(properties,"ends",true,&event->ends)) goto failure;
  event->message=nws_dup(id);event->sender=nws_dup(sender);
  event->vtec=nws_vtec(properties,type,&event->vtec_new);
  char *extra=nws_extra(properties,field(feature,"geometry"));
  if (!extra) goto failure;
  int digest_error=mbedtls_sha256((const uint8_t *)extra,strlen(extra),event->extra_digest,0);
  free(extra);if (digest_error) goto failure;
  time_t issued=(time_t)event->issued;struct tm issued_tm;
  if (!gmtime_r(&issued,&issued_tm)) goto failure;
  event->origin_year=(uint16_t)(issued_tm.tm_year+1900);
  if (!event->message||!event->sender||!event->vtec) goto failure;
  const char *names[]={"areaDesc","headline","description","instruction"};
  for (int i=0;i<4;++i) {
    const cJSON *value=field(properties,names[i]);const char *text=string(value);
    if (!text && ((i==1||i==3)&&cJSON_IsNull(value))) text="";
    if (!text || strlen(text)>WEATHER_RESPONSE_MAX) goto failure;
    event->text[i]=nws_dup(text);if (!event->text[i]) goto failure;
  }
  event->reached=nws_milestones(type,properties,event->text[1]);
  event->visual_reached=nws_visual(type,properties,event->reached);
  return event;
failure:
  nws_free(event);return NULL;
}

typedef struct {
  uint32_t magic, format;
  uint64_t epoch;
  nws_location_t location;
  int64_t last_success;
  uint8_t has_success, unsupported, reserved[6];
} nws_binding_t;
typedef struct {
  uint32_t magic, format;
  uint64_t epoch, id;
  int64_t issued, onset, expires, ends, last_sound;
  uint32_t reached, visual_reached, lengths[7];
  uint8_t extra_digest[32];
  uint16_t origin_year;
  uint8_t type, dismissed, had_sound, reserved[3];
} nws_stored_event_t;
static uint64_t nws_epoch=1;
static bool nws_binding_dirty;
static uint32_t nws_dismiss_serial;
static const uint32_t nws_binding_magic=UINT32_C(0x4e57534c),nws_event_magic=UINT32_C(0x4e575345);
static void nws_key(char key[8],unsigned slot) { snprintf(key,8,"e%02u",slot); }
static uint8_t *nws_serialize(const nws_event_t *event,uint64_t epoch,size_t *length)
{
  const char *values[]={event->message,event->sender,event->vtec,
    event->text[0],event->text[1],event->text[2],event->text[3]};
  nws_stored_event_t header={.magic=nws_event_magic,.format=1,.epoch=epoch,.id=event->id,
    .issued=event->issued,.onset=event->onset,.expires=event->expires,.ends=event->ends,
    .last_sound=event->last_sound_utc,.reached=event->reached,.visual_reached=event->visual_reached,
    .type=event->type,.dismissed=event->dismissed,.origin_year=event->origin_year,
    .had_sound=event->restored_sound||event->last_sound_us>=0};
  memcpy(header.extra_digest,event->extra_digest,sizeof(header.extra_digest));
  size_t total=sizeof(header);
  for (int i=0;i<7;++i) {
    size_t size=strlen(values[i])+1;
    if (size>WEATHER_RESPONSE_MAX || total>WEATHER_RESPONSE_MAX-size) return NULL;
    header.lengths[i]=(uint32_t)size;total+=size;
  }
  uint8_t *data=malloc(total);if (!data) return NULL;
  memcpy(data,&header,sizeof(header));size_t offset=sizeof(header);
  for (int i=0;i<7;++i) { memcpy(data+offset,values[i],header.lengths[i]);offset+=header.lengths[i]; }
  *length=total;return data;
}
static nws_event_t *nws_deserialize(const uint8_t *data,size_t length,uint64_t epoch,uint8_t slot)
{
  if (length<sizeof(nws_stored_event_t)) return NULL;
  nws_stored_event_t h;memcpy(&h,data,sizeof(h));
  if (h.magic!=nws_event_magic||h.format!=1||h.epoch!=epoch||!h.id||h.type>=55||h.dismissed>1||h.had_sound>1 ||
      h.issued<=0||h.expires<=0||h.onset<0||h.ends<0||h.last_sound<0||
      (h.reached&~15u)||(h.visual_reached&~31u)||h.origin_year<1970||h.origin_year>9999) return NULL;
  size_t offset=sizeof(h);const char *values[7];
  for (int i=0;i<7;++i) {
    size_t size=h.lengths[i];
    if (!size || size>length-offset || data[offset+size-1] || memchr(data+offset,'\0',size-1)) return NULL;
    values[i]=(const char *)data+offset;offset+=size;
  }
  if (offset!=length || !*values[0]||strlen(values[0])>NWS_ID_MAX||!*values[1]||strlen(values[1])>128||strlen(values[2])>=64) return NULL;
  nws_event_t *event=calloc(1,sizeof(*event));if (!event) return NULL;
  event->id=h.id;event->type=h.type;event->slot=slot;event->issued=h.issued;event->onset=h.onset;
  event->expires=h.expires;event->ends=h.ends;event->last_sound_utc=h.last_sound;
  event->last_sound_us=-1;event->restored_sound=h.had_sound;
  event->reached=h.reached;event->visual_reached=h.visual_reached;event->dismissed=h.dismissed;
  event->origin_year=h.origin_year;
  memcpy(event->extra_digest,h.extra_digest,sizeof(event->extra_digest));
  event->version=event->saved_version=1;
  event->message=nws_dup(values[0]);event->sender=nws_dup(values[1]);
  event->vtec=nws_dup(values[2]);
  for (int i=0;i<4;++i) event->text[i]=nws_dup(values[i+3]);
  if (!event->message||!event->sender||!event->vtec||
      !event->text[0]||!event->text[1]||!event->text[2]||!event->text[3]) { nws_free(event);return NULL; }
  return event;
}
static void nws_sweep_locked(int64_t now)
{
  for (size_t i=0;i<nws_count;) {
    if (nws_active(nws_events[i],now)) { ++i;continue; }
    nws_free(nws_events[i]);
    memmove(nws_events+i,nws_events+i+1,(nws_count-i-1)*sizeof(nws_events[0]));
    --nws_count;++nws_revision;nws_dirty=true;
  }
}
bool weather_client_alerts_sync_location(const device_config_t *config)
{
  if (!config) return false;
  nws_location_t location=nws_location_for(config);uint32_t stop=0;bool changed=false;
  xSemaphoreTake(nws_lock,portMAX_DELAY);
  if (!nws_initialized || !nws_same_location(location,nws_location)) {
    for (size_t i=0;i<nws_count;++i) nws_free(nws_events[i]);
    nws_count=0;nws_location=location;++nws_generation;++nws_revision;++nws_epoch;
    if (!nws_epoch) ++nws_epoch;
    nws_initialized=true;nws_status=APP_ALERT_CHECKING;nws_has_success=false;nws_last_success=0;
    nws_dirty=nws_binding_dirty=true;nws_save_failed=false;
    stop=nws_audio_token;++nws_dismiss_serial;changed=true;
  }
  xSemaphoreGive(nws_lock);
  if (stop) wac_audio_weather_stop(stop);
  return changed;
}
esp_err_t weather_client_alerts_init(const device_config_t *config)
{
  if (!config) return ESP_ERR_INVALID_ARG;
  weather_client_alerts_sync_location(config);
  nvs_handle_t handle;esp_err_t error=nvs_open(NWS_NAMESPACE,NVS_READONLY,&handle);
  if (error!=ESP_OK) return ESP_OK; /* Alert-record fallback is intentionally local. */
  nws_binding_t binding={0};size_t size=sizeof(binding);
  error=nvs_get_blob(handle,"location",&binding,&size);
  bool valid=error==ESP_OK&&size==sizeof(binding)&&binding.magic==nws_binding_magic&&
    binding.format==1&&binding.epoch&&binding.has_success<=1&&binding.unsupported<=1&&
    memchr(binding.location.latitude,0,sizeof(binding.location.latitude))&&
    memchr(binding.location.longitude,0,sizeof(binding.location.longitude))&&
    memchr(binding.location.name,0,sizeof(binding.location.name));
  if (valid && binding.epoch>=nws_epoch) nws_epoch=binding.epoch+1;
  if (!valid || !nws_same_location(binding.location,nws_location)) { nvs_close(handle);return ESP_OK; }
  nws_epoch=binding.epoch;nws_binding_dirty=false;
  nws_status=binding.unsupported?APP_ALERT_UNSUPPORTED:APP_ALERT_CHECKING;
  nws_has_success=binding.has_success;nws_last_success=binding.last_success;
  int64_t now=nws_now();
  for (unsigned slot=0;slot<NWS_EVENT_MAX;++slot) {
    char key[8];nws_key(key,slot);size=0;
    if (nvs_get_blob(handle,key,NULL,&size)!=ESP_OK || size<sizeof(nws_stored_event_t)||size>WEATHER_RESPONSE_MAX) continue;
    uint8_t *data=malloc(size);if (!data) continue;
    error=nvs_get_blob(handle,key,data,&size);
    nws_event_t *event=error==ESP_OK?nws_deserialize(data,size,nws_epoch,(uint8_t)slot):NULL;
    free(data);
    if (!event) continue;
    bool duplicate=false;
    for (size_t i=0;i<nws_count;++i) if (nws_events[i]->id==event->id ||
        (!strcmp(nws_events[i]->message,event->message)&&!strcmp(nws_events[i]->sender,event->sender))) duplicate=true;
    if (!nws_active(event,now)||duplicate) { nws_free(event);nws_dirty=true;continue; }
    ++nws_revision;nws_events[nws_count++]=event;
    if (event->id>=nws_next_id) nws_next_id=event->id+1;
  }
  qsort(nws_events,nws_count,sizeof(nws_events[0]),nws_compare);
  nvs_close(handle);return ESP_OK;
}
/* Direct per-event writes deliberately have no all-events snapshot selector.
 * A partially saved event is either independently readable or falls back to a
 * fresh alert. A location epoch makes leftover old-location slots unusable. */
static void nws_save(bool retry_failed)
{
  xSemaphoreTake(nws_lock,portMAX_DELAY);
  if (!nws_dirty || (nws_save_failed&&!retry_failed)) { xSemaphoreGive(nws_lock);return; }
  uint32_t generation=nws_generation,revision=nws_revision;
  uint64_t epoch=nws_epoch;
  nws_binding_t binding={.magic=nws_binding_magic,.format=1,.epoch=epoch,.location=nws_location,
    .last_success=nws_last_success,.has_success=nws_has_success,.unsupported=nws_status==APP_ALERT_UNSUPPORTED};
  bool write_binding=nws_binding_dirty;
  xSemaphoreGive(nws_lock);
  nvs_handle_t handle;esp_err_t error=nvs_open(NWS_NAMESPACE,NVS_READWRITE,&handle);
  if (error==ESP_OK) {
    if (write_binding) error=nvs_set_blob(handle,"location",&binding,sizeof(binding));
    for (unsigned slot=0;error==ESP_OK && slot<NWS_EVENT_MAX;++slot) {
      uint8_t *data=NULL;size_t length=0;uint64_t id=0;uint32_t version=0;bool present=false,changed=false;
      xSemaphoreTake(nws_lock,portMAX_DELAY);
      if (generation!=nws_generation) { xSemaphoreGive(nws_lock);break; }
      for (size_t i=0;i<nws_count;++i) if (nws_events[i]->slot==slot) {
        nws_event_t *event=nws_events[i];present=true;
        if (event->version!=event->saved_version) {
          changed=true;id=event->id;version=event->version;data=nws_serialize(event,epoch,&length);
        }
        break;
      }
      xSemaphoreGive(nws_lock);
      char key[8];nws_key(key,slot);
      if (changed) error=data?nvs_set_blob(handle,key,data,length):ESP_ERR_NO_MEM;
      else if (!present) { error=nvs_erase_key(handle,key);if (error==ESP_ERR_NVS_NOT_FOUND) error=ESP_OK; }
      free(data);
      if (error==ESP_OK && changed) {
        xSemaphoreTake(nws_lock,portMAX_DELAY);
        if (generation==nws_generation) for (size_t i=0;i<nws_count;++i)
          if (nws_events[i]->id==id && nws_events[i]->version==version) nws_events[i]->saved_version=version;
        xSemaphoreGive(nws_lock);
      }
    }
    if (error==ESP_OK) error=nvs_commit(handle);
    nvs_close(handle);
  }
  xSemaphoreTake(nws_lock,portMAX_DELAY);
  if (generation==nws_generation) {
    bool failed=error!=ESP_OK;
    bool unchanged=revision==nws_revision;
    if (nws_save_failed!=failed) { nws_save_failed=failed;++nws_revision; }
    if (!failed && unchanged) nws_dirty=nws_binding_dirty=false;
  }
  xSemaphoreGive(nws_lock);
}
static void nws_row_locked(const nws_event_t *event,weather_alert_row_t *out)
{
  *out=(weather_alert_row_t){.revision=nws_revision,.id=event->id,.type=event->type,
    .issued=(time_t)event->issued,.cutoff=(time_t)nws_cutoff(event)};
}
void weather_client_alerts_copy(app_alert_snapshot_t *out)
{
  if (!out) return;
  int64_t now=nws_now();
  xSemaphoreTake(nws_lock,portMAX_DELAY);
  nws_sweep_locked(now);
  *out=(app_alert_snapshot_t){.revision=nws_revision,.status=nws_status,.has_success=nws_has_success,
    .save_failed=nws_save_failed,.last_success=(time_t)nws_last_success};
  for (size_t i=0;i<nws_count;++i) {
    const nws_event_t *event=nws_events[i];
    if (nws_catalog[event->type].warning) ++out->warning_count;else ++out->watch_count;
    if (!out->headline_id && !event->dismissed) { out->headline_id=event->id;out->headline_type=event->type; }
  }
  xSemaphoreGive(nws_lock);
}
void weather_client_alerts_dismiss(void)
{
  /* The tap covers all events present under this lock. */
  uint32_t token;xSemaphoreTake(nws_lock,portMAX_DELAY);
  ++nws_dismiss_serial;token=nws_audio_token;
  for (size_t i=0;i<nws_count;++i) if (!nws_events[i]->dismissed) {
    nws_events[i]->dismissed=true;++nws_events[i]->version;nws_dirty=true;
  }
  ++nws_revision;xSemaphoreGive(nws_lock);
  if (token) wac_audio_weather_stop(token);
}
esp_err_t weather_client_alerts_row_copy(size_t index,weather_alert_row_t *out)
{
  if (!out) return ESP_ERR_INVALID_ARG;
  xSemaphoreTake(nws_lock,portMAX_DELAY);nws_sweep_locked(nws_now());
  esp_err_t error=index<nws_count?ESP_OK:ESP_ERR_NOT_FOUND;
  if (error==ESP_OK) nws_row_locked(nws_events[index],out);
  xSemaphoreGive(nws_lock);return error;
}
esp_err_t weather_client_alerts_detail_copy(uint64_t id,weather_alert_detail_t *out,
  char *buffer,size_t capacity,size_t *needed)
{
  if (!out||!needed||(!buffer&&capacity)) return ESP_ERR_INVALID_ARG;
  xSemaphoreTake(nws_lock,portMAX_DELAY);nws_sweep_locked(nws_now());
  nws_event_t *event=NULL;for (size_t i=0;i<nws_count;++i) if (nws_events[i]->id==id) event=nws_events[i];
  if (!event) { xSemaphoreGive(nws_lock);return ESP_ERR_NOT_FOUND; }
  size_t offsets[4],length=0;
  for (int i=0;i<4;++i) { offsets[i]=length;length+=strlen(event->text[i])+1; }
  *needed=length;
  if (capacity<length) { xSemaphoreGive(nws_lock);return ESP_ERR_INVALID_SIZE; }
  for (int i=0;i<4;++i) memcpy(buffer+offsets[i],event->text[i],strlen(event->text[i])+1);
  nws_row_locked(event,&out->row);
  out->area_offset=offsets[0];out->headline_offset=offsets[1];
  out->description_offset=offsets[2];out->instruction_offset=offsets[3];
  xSemaphoreGive(nws_lock);return ESP_OK;
}
static bool nws_references_valid(const cJSON *properties)
{
  const cJSON *references=field(properties,"references");
  if (!cJSON_IsArray(references)) return false;
  const cJSON *reference;
  cJSON_ArrayForEach(reference,references) {
    const char *id=string(field(reference,"identifier")),*sender=string(field(reference,"sender"));int64_t sent;
    if (!cJSON_IsObject(reference)||!id||!*id||strlen(id)>NWS_ID_MAX||!sender||!*sender||strlen(sender)>128||
        !nws_timestamp(string(field(reference,"sent")),&sent)) return false;
  }
  const cJSON *expired=field(field(properties,"parameters"),"expiredReferences");
  if (!expired) return true;
  if (!cJSON_IsArray(expired)) return false;
  cJSON_ArrayForEach(reference,expired) {
    const char *value=string(reference);if (!value) return false;
    while (*value) {
      while (isspace((unsigned char)*value)) ++value;
      if (!*value) break;
      const char *a=strchr(value,','),*b=a?strchr(a+1,','):NULL;
      if (!a||!b || a==value || a-value>128||b==a+1||b-a-1>NWS_ID_MAX) return false;
      const char *end=b+1;while (*end&&!isspace((unsigned char)*end)) ++end;
      size_t length=(size_t)(end-b-1);char timestamp[26];int64_t sent;
      if (length>=sizeof(timestamp)) return false;
      memcpy(timestamp,b+1,length);timestamp[length]='\0';
      if (!nws_timestamp(timestamp,&sent)) return false;
      value=end;
    }
  }
  return true;
}
static bool nws_refers(const cJSON *properties,const nws_event_t *event)
{
  const cJSON *reference;
  cJSON_ArrayForEach(reference,field(properties,"references")) {
    int64_t sent=0;
    if (!strcmp(string(field(reference,"identifier")),event->message)&&
        !strcmp(string(field(reference,"sender")),event->sender)&&
        nws_timestamp(string(field(reference,"sent")),&sent)&&sent==event->issued) return true;
  }
  cJSON_ArrayForEach(reference,field(field(properties,"parameters"),"expiredReferences")) {
    const char *value=string(reference);
    while (value&&*value) {
      while (isspace((unsigned char)*value)) ++value;
      if (!*value) break;
      const char *a=strchr(value,','),*b=a?strchr(a+1,','):NULL;
      if (!a||!b) break;
      const char *end=b+1;while (*end&&!isspace((unsigned char)*end)) ++end;
      size_t length=(size_t)(end-b-1);char timestamp[26];int64_t sent=0;
      if (length<sizeof(timestamp)) {
        memcpy(timestamp,b+1,length);timestamp[length]='\0';
        if ((size_t)(a-value)==strlen(event->sender)&&!memcmp(value,event->sender,(size_t)(a-value))&&
            (size_t)(b-a-1)==strlen(event->message)&&!memcmp(a+1,event->message,(size_t)(b-a-1))&&
            nws_timestamp(timestamp,&sent)&&sent==event->issued) return true;
      }
      value=end;
    }
  }
  return false;
}
static bool nws_same_message(const nws_event_t *a,const nws_event_t *b)
{
  return a->issued==b->issued&&!strcmp(a->sender,b->sender)&&!strcmp(a->message,b->message);
}
static bool nws_same_content(const nws_event_t *a,const nws_event_t *b)
{
  if (a->type!=b->type||a->onset!=b->onset||a->expires!=b->expires||a->ends!=b->ends||
      memcmp(a->extra_digest,b->extra_digest,sizeof(a->extra_digest))) return false;
  for (int i=0;i<4;++i) if (strcmp(a->text[i],b->text[i])) return false;
  return true;
}
static bool nws_enabled(const device_config_t *config,const nws_event_t *event,bool night)
{
  uint32_t selections=nws_catalog[event->type].warning?config->alert_warning_sounds:config->alert_watch_sounds;
  return (!night||config->alert_night_sounds) && (selections&(UINT32_C(1)<<nws_catalog[event->type].bit));
}
static bool nws_sound_separated(const nws_event_t *event,int64_t now,int64_t now_us)
{
  if (!event->last_sound_utc && event->last_sound_us<0 && !event->restored_sound) return true;
  if (!event->restored_sound && event->last_sound_us>=0) return now_us-event->last_sound_us>=NWS_SOUND_SEPARATION_US;
  if (now && event->last_sound_utc) return now>=event->last_sound_utc+600;
  return now_us>=NWS_SOUND_SEPARATION_US;
}
static bool nws_audio_terminal(wac_audio_weather_state_t state)
{
  return state==WAC_AUDIO_WEATHER_FINISHED||state==WAC_AUDIO_WEATHER_CANCELED||
    state==WAC_AUDIO_WEATHER_SKIPPED_RING||state==WAC_AUDIO_WEATHER_FAILED;
}
/* Worker maintenance: audio receipts, cutoff cleanup and newly dirty saves.
 * Failed saves retry only in check(), at the next scheduled opportunity. */
static void weather_client_alerts_maintain(const device_config_t *config)
{
  if (!config) return;
  weather_client_alerts_sync_location(config);
  wac_audio_snapshot_t audio;wac_audio_copy(&audio);
  uint32_t acknowledge=0,stop=0;int64_t now=nws_now(),now_us=esp_timer_get_time();
  xSemaphoreTake(nws_lock,portMAX_DELAY);
  nws_sweep_locked(now);
  if (nws_audio_token && audio.weather_token==nws_audio_token) {
    nws_event_t *event=NULL;
    for (size_t i=0;i<nws_count;++i) if (nws_events[i]->id==nws_audio_event) event=nws_events[i];
    if (event && audio.weather_started_us>0 &&
        (event->restored_sound||event->last_sound_us!=audio.weather_started_us)) {
      event->last_sound_us=audio.weather_started_us;
      event->last_sound_utc=now?now-(now_us-audio.weather_started_us)/1000000:0;
      event->restored_sound=false;++event->version;nws_dirty=true;
    }
    if (!event && !nws_audio_terminal(audio.weather_state)) stop=nws_audio_token;
    if (nws_audio_terminal(audio.weather_state)) { acknowledge=nws_audio_token;nws_audio_token=0;nws_audio_event=0; }
  }
  xSemaphoreGive(nws_lock);
  if (stop) wac_audio_weather_stop(stop);
  if (acknowledge) wac_audio_weather_ack(acknowledge);
  nws_save(false);
}
/* A complete collection is staged before touching live state. References are
 * retained in the parsed tree only for this reconciliation, not as a history. */
static esp_err_t nws_apply(const cJSON *root,uint32_t generation,const device_config_t *config,uint64_t *sound_id,
  uint32_t *sound_dismiss_serial)
{
  const cJSON *features=field(root,"features");
  if (!cJSON_IsObject(root)||!string(field(root,"type"))||strcmp(string(field(root,"type")),"FeatureCollection")||!cJSON_IsArray(features)) return ESP_ERR_INVALID_RESPONSE;
  const cJSON *pagination=field(root,"pagination");
  if (pagination && !cJSON_IsNull(pagination) && field(pagination,"next") && !cJSON_IsNull(field(pagination,"next"))) return ESP_ERR_INVALID_SIZE;
  nws_event_t *candidate[NWS_EVENT_MAX]={0};const cJSON *properties[NWS_EVENT_MAX]={0};
  const cJSON *ended[NWS_EVENT_MAX]={0};size_t count=0,ended_count=0,selected=0;
  const cJSON *feature;esp_err_t error=ESP_ERR_INVALID_RESPONSE;
  int64_t now=nws_now();
  cJSON_ArrayForEach(feature,features) {
    const cJSON *p=field(feature,"properties");
    if (!cJSON_IsObject(feature)||!cJSON_IsObject(p)||!string(field(feature,"type"))||strcmp(string(field(feature,"type")),"Feature")) goto cleanup;
    const char *status=string(field(p,"status")),*event_name=string(field(p,"event"));
    if (!status||!event_name) goto cleanup;
    if (strcmp(status,"Actual")) {
      if (strcmp(status,"Test")&&strcmp(status,"Exercise")&&strcmp(status,"System")&&strcmp(status,"Draft")) goto cleanup;
      continue;
    }
    int type=nws_type(event_name);if (type<0) continue;
    if (++selected>NWS_EVENT_MAX) { error=ESP_ERR_INVALID_SIZE;goto cleanup; }
    const char *message_type=string(field(p,"messageType")),*urgency=string(field(p,"urgency"));
    if (!message_type||!urgency||!nws_references_valid(p)) goto cleanup;
    if (!strcmp(message_type,"Cancel")||!strcmp(urgency,"Past")) { ended[ended_count++]=p;continue; }
    if (strcmp(message_type,"Alert")&&strcmp(message_type,"Update")) goto cleanup;
    nws_event_t *event=nws_decode_event(feature,type);if (!event) goto cleanup;
    if (!nws_active(event,now)) { nws_free(event);continue; }
    for (size_t i=0;i<count;++i) if (nws_same_message(candidate[i],event)) { nws_free(event);goto cleanup; }
    candidate[count]=event;properties[count++]=p;
  }
  /* Remove an explicitly canceled/superseded candidate even if its older
   * representation is also present in a complete upstream collection. */
  for (size_t i=0;i<count;) {
    bool remove=false;
    for (size_t j=0;j<ended_count;++j) if (nws_refers(ended[j],candidate[i])) remove=true;
    for (size_t j=0;j<count;++j) if (j!=i && candidate[j]->issued>=candidate[i]->issued && nws_refers(properties[j],candidate[i])) remove=true;
    if (!remove) { ++i;continue; }
    nws_free(candidate[i]);--count;
    memmove(candidate+i,candidate+i+1,(count-i)*sizeof(candidate[0]));
    memmove(properties+i,properties+i+1,(count-i)*sizeof(properties[0]));candidate[count]=NULL;
  }
  xSemaphoreTake(nws_lock,portMAX_DELAY);
  if (generation!=nws_generation) { xSemaphoreGive(nws_lock);error=ESP_ERR_INVALID_STATE;goto cleanup; }
  nws_sweep_locked(now);
  bool slots[NWS_EVENT_MAX]={false};uint64_t used=0,relations[NWS_EVENT_MAX]={0};
  int matches[NWS_EVENT_MAX];
  /* Establish every relationship before modifying anything: contradictory
   * duplicate matches invalidate the whole result rather than losing history. */
  for (size_t i=0;i<count;++i) {
    matches[i]=-1;
    for (size_t j=0;j<nws_count;++j) {
      bool relation=nws_same_message(candidate[i],nws_events[j])||nws_refers(properties[i],nws_events[j]);
      if (!relation && candidate[i]->type==nws_events[j]->type && candidate[i]->vtec[0] &&
          !strcmp(candidate[i]->vtec,nws_events[j]->vtec) &&
          (!candidate[i]->vtec_new||candidate[i]->origin_year==nws_events[j]->origin_year)) relation=true;
      if (!relation) continue;
      relations[i]|=UINT64_C(1)<<j;
      if (matches[i]<0 || (nws_events[j]->type==candidate[i]->type &&
          nws_events[matches[i]]->type!=candidate[i]->type)) matches[i]=(int)j;
    }
    if (matches[i]>=0) {
      if (used&relations[i]) { xSemaphoreGive(nws_lock);goto cleanup; }
      used|=relations[i];slots[nws_events[matches[i]]->slot]=true;
    }
    for (size_t j=0;j<i;++j) if (candidate[i]->vtec[0] && candidate[i]->type==candidate[j]->type &&
        !strcmp(candidate[i]->vtec,candidate[j]->vtec)) { xSemaphoreGive(nws_lock);goto cleanup; }
  }
  ++nws_revision;*sound_id=0;*sound_dismiss_serial=nws_dismiss_serial;
  for (size_t i=0;i<count;++i) {
    nws_event_t *event=candidate[i],*old=matches[i]>=0?nws_events[matches[i]]:NULL;
    uint32_t previous_reached=old&&old->type==event->type?old->reached:0;
    uint32_t previous_visual=old&&old->type==event->type?old->visual_reached:0;
    for (size_t j=0;j<nws_count;++j) if ((relations[i]&(UINT64_C(1)<<j)) && nws_events[j]->type==event->type) {
      previous_reached|=nws_events[j]->reached;previous_visual|=nws_events[j]->visual_reached;
    }
    bool newly=!old||old->type!=event->type,substantive=!old||!nws_same_content(old,event);
    uint32_t increased=event->reached&~previous_reached,visual=event->visual_reached&~previous_visual;
    if (old && event->issued<old->issued) {
      /* A stale cache response cannot roll back newer protective information. */
      nws_free(event);candidate[i]=old;nws_events[matches[i]]=NULL;continue;
    }
    if (old) {
      event->id=old->id;event->slot=old->slot;
      if (event->type==old->type) event->origin_year=old->origin_year;
      event->dismissed=old->dismissed && !visual && event->type==old->type;
      event->last_sound_us=old->last_sound_us;event->last_sound_utc=old->last_sound_utc;
      event->restored_sound=old->restored_sound;event->reached|=previous_reached;event->visual_reached|=previous_visual;
      /* A CAP update may replace several prior messages. Prefer the matching
       * event type's identity, but retain the most recent actual sound across
       * every explicitly replaced event and require all dismissals to agree. */
      for (size_t j=0;j<nws_count;++j) if (relations[i]&(UINT64_C(1)<<j)) {
        nws_event_t *related=nws_events[j];
        if (!related->dismissed) event->dismissed=false;
        if (related->last_sound_utc>event->last_sound_utc ||
            (!related->restored_sound && related->last_sound_us>event->last_sound_us)) {
          event->last_sound_utc=related->last_sound_utc;event->last_sound_us=related->last_sound_us;
          event->restored_sound=related->restored_sound;
        }
      }
      event->version=old->version+1;event->saved_version=old->saved_version;
    } else {
      event->id=nws_next_id++;event->version=1;event->last_sound_us=-1;
      for (unsigned slot=0;slot<NWS_EVENT_MAX;++slot) if (!slots[slot]) { event->slot=(uint8_t)slot;slots[slot]=true;break; }
    }
    bool update=(config->alert_update_sounds==DEVICE_CONFIG_ALERT_UPDATES_ALL&&substantive)||
      (config->alert_update_sounds==DEVICE_CONFIG_ALERT_UPDATES_INCREASED&&increased);
    /* Night may have changed while HTTP was in progress. */
    if ((newly||update)&&nws_enabled(config,event,wac_night_active())&&nws_sound_separated(event,now,esp_timer_get_time())) {
      if (!*sound_id) *sound_id=event->id;
      else {
        nws_event_t *chosen=NULL;for (size_t j=0;j<i;++j) if (candidate[j]->id==*sound_id) chosen=candidate[j];
        if (chosen && nws_compare(&event,&chosen)<0) *sound_id=event->id;
      }
    }
  }
  for (size_t i=0;i<nws_count;++i) nws_free(nws_events[i]);
  nws_count=count;
  for (size_t i=0;i<count;++i) { nws_events[i]=candidate[i];candidate[i]=NULL; }
  qsort(nws_events,nws_count,sizeof(nws_events[0]),nws_compare);
  nws_status=APP_ALERT_CURRENT;nws_has_success=true;nws_last_success=now;
  nws_dirty=nws_binding_dirty=true;
  xSemaphoreGive(nws_lock);error=ESP_OK;
cleanup:
  for (size_t i=0;i<count;++i) nws_free(candidate[i]);
  return error;
}
static int64_t nws_retry_deadline(const http_response_t *response)
{
  int64_t seconds=integer_seconds(response->retry_after),reference=date_seconds(response->date);
  if (!reference) reference=nws_now();
  if (seconds<0) {
    int64_t date=date_seconds(response->retry_after);
    if (date && reference) seconds=date>reference?date-reference:0;
  }
  if (seconds<=60) return 0;
  int64_t now=esp_timer_get_time();
  return seconds>(INT64_MAX-now)/1000000?INT64_MAX:now+seconds*1000000;
}
static bool nws_valid_json(const cJSON *value,unsigned depth)
{
  if (!value||depth>20) return false;
  if (cJSON_IsNumber(value)&&!isfinite(value->valuedouble)) return false;
  const cJSON *item;
  cJSON_ArrayForEach(item,value) {
    if (cJSON_IsObject(value)) {
      if (!item->string) return false;
      for (const cJSON *other=item->next;other;other=other->next)
        if (!other->string||!strcmp(item->string,other->string)) return false;
    }
    if (!nws_valid_json(item,depth+1)) return false;
  }
  return true;
}
static cJSON *nws_complete_json(const http_response_t *response)
{
  if (!response->data || !response->length || memchr(response->data,0,response->length)) return NULL;
  /* CAP text cannot contain NUL. cJSON stores strings as C strings; accepting
   * an escaped NUL would otherwise silently shorten protective instructions. */
  for (size_t i=0;i+1<response->length;++i) if (response->data[i]=='\\') {
    if (i+5<response->length&&!memcmp(response->data+i+1,"u0000",5)) return NULL;
    ++i;
  }
  const char *end=NULL;
  cJSON *root=cJSON_ParseWithLengthOpts(response->data,response->length+1,&end,true);
  if (!root || !end || (size_t)(end-response->data)!=response->length || !nws_valid_json(root,0)) { cJSON_Delete(root);return NULL; }
  return root;
}
/* pause is a monotonic deadline, zero unless NWS explicitly requests >1 minute.
 * A failed/incomplete check never clears retained events. */
static esp_err_t weather_client_alerts_check(const device_config_t *config,bool connected,int64_t *pause)
{
  if (!config||!pause) return ESP_ERR_INVALID_ARG;
  *pause=0;weather_client_alerts_sync_location(config);
  xSemaphoreTake(nws_lock,portMAX_DELAY);uint32_t generation=nws_generation;
  bool unsupported=nws_status==APP_ALERT_UNSUPPORTED;xSemaphoreGive(nws_lock);
  if (unsupported) { nws_save(true);return ESP_ERR_NOT_FOUND; }
  esp_err_t error=ESP_FAIL;http_response_t response={.maximum=WEATHER_RESPONSE_MAX};int status=0;
  uint64_t sound=0;uint32_t sound_dismiss_serial=0;bool confirmed_unsupported=false;
  if (connected && request_gate && config->location_ready) {
    char url[192];snprintf(url,sizeof(url),"https://api.weather.gov/alerts/active?point=%s,%s&status=actual",config->latitude,config->longitude);
    esp_http_client_config_t http={.url=url,.event_handler=http_event,.user_data=&response,
      .timeout_ms=WEATHER_HTTP_TIMEOUT_MS,.buffer_size=2048,.crt_bundle_attach=esp_crt_bundle_attach,
      .user_agent="ESP32-P4-Weather-Alarm-Clock/1.0",.disable_auto_redirect=false};
    xSemaphoreTake(request_gate,portMAX_DELAY);
    esp_http_client_handle_t client=esp_http_client_init(&http);
    if (client) {
      error=esp_http_client_set_header(client,"Accept","application/geo+json");
      if (error==ESP_OK) error=esp_http_client_perform(client);
      if (error==ESP_OK&&!esp_http_client_is_complete_data_received(client)) error=ESP_ERR_INVALID_RESPONSE;
      status=esp_http_client_get_status_code(client);esp_http_client_cleanup(client);
      if (response.error!=ESP_OK) error=response.error;
    }
    xSemaphoreGive(request_gate);
    *pause=nws_retry_deadline(&response);
    if (error==ESP_OK) {
      cJSON *root=nws_complete_json(&response);
      if (!root) error=ESP_ERR_INVALID_RESPONSE;
      else if (status==200) error=nws_apply(root,generation,config,&sound,&sound_dismiss_serial);
      else {
        const char *type=string(field(root,"type")),*detail=string(field(root,"detail"));
        confirmed_unsupported=status==400 && number(field(root,"status"))==400 && type && detail &&
          !strcmp(type,"https://api.weather.gov/problems/InvalidParameter") &&
          !strcmp(detail,"Parameter \"point\" is invalid: out of bounds");
        error=ESP_FAIL;
      }
      cJSON_Delete(root);
    }
  }
  free(response.data);
  xSemaphoreTake(nws_lock,portMAX_DELAY);
  bool current=generation==nws_generation;
  if (current && error!=ESP_OK) {
    nws_status=confirmed_unsupported?APP_ALERT_UNSUPPORTED:APP_ALERT_FAILED;
    ++nws_revision;if (confirmed_unsupported) nws_dirty=nws_binding_dirty=true;
  }
  bool available=current&&!nws_audio_token&&sound_dismiss_serial==nws_dismiss_serial;xSemaphoreGive(nws_lock);
  if (!current) { *pause=0;return ESP_ERR_INVALID_STATE; }
  if (sound && available) {
    wac_audio_weather_kind_t kind=WAC_AUDIO_WEATHER_WATCH;
    xSemaphoreTake(nws_lock,portMAX_DELAY);
    for (size_t i=0;i<nws_count;++i) if (nws_events[i]->id==sound && nws_catalog[nws_events[i]->type].warning) kind=WAC_AUDIO_WEATHER_WARNING;
    xSemaphoreGive(nws_lock);
    uint32_t token=0;
    if (wac_audio_weather_start(kind,&token)==ESP_OK && token) {
      xSemaphoreTake(nws_lock,portMAX_DELAY);
      nws_audio_token=token;nws_audio_event=sound;
      bool canceled=generation!=nws_generation||sound_dismiss_serial!=nws_dismiss_serial;
      xSemaphoreGive(nws_lock);
      if (canceled) wac_audio_weather_stop(token);
    }
  }
  nws_save(true);return error;
}

/* The weather worker owns this bounded pipeline and all mutable image memory.
 * A single grayscale basemap cache preserves the dark reference's terrain/roads
 * without retaining another full RGBA map. Final slots are immutable until the
 * display acknowledges their publication. Compressed histories are RAM-only. */
#define RADAR_PIXELS ((size_t)APP_RADAR_WIDTH * APP_RADAR_HEIGHT)
#define RADAR_GROUND_M 120700.8
#define RADAR_ZOOM 7
#define RADAR_TILE_PIXELS 512
#define RADAR_WORLD_PIXELS 32768.0
#define RADAR_WORLD_METERS 40075016.68557849
#define RADAR_BASE_RESPONSE_MAX (3 * 1024 * 1024)
/* LodePNG reserves another 260 bytes while inflating, then grows its
 * exact-size scanline allocation by half. Valid 800px RGBA needs about
 * 3.84 MB of scratch even though its decoded pixels occupy 2.56 MB. */
#define RADAR_PNG_ALLOC_MAX (4 * 1024 * 1024)
#define RADAR_PATH_LEN 192
#define RADAR_PI 3.14159265358979323846
#define RADAR_HISTORY_MAX 6
#define RADAR_TILES_MAX 64
#define RADAR_CACHE_MAX (4 * 1024 * 1024)
#define RADAR_FRAME_US INT64_C(1500000)

typedef struct {
  double left, top, step; /* Native RainViewer zoom-7 world pixels. */
  int first_x, last_x, first_y, last_y;
} radar_geometry_t;

typedef struct {
  uint8_t *png;
  size_t size, capacity;
} radar_cached_tile_t;
typedef struct {
  time_t time;
  char path[RADAR_PATH_LEN];
  radar_cached_tile_t *tiles;
  unsigned tile_count, loaded;
  bool failed;
} radar_frame_t;
typedef enum { RADAR_IDLE, RADAR_METADATA, RADAR_BASE,
               RADAR_WAIT_SLOT, RADAR_ECHO, RADAR_READY } radar_phase_t;
typedef enum {
  WEATHER_RADAR_CONTINUE = 0,
  WEATHER_RADAR_CHECK = 1,
  WEATHER_RADAR_OFFLINE = 2,
} weather_radar_step_t;

/* The UI only copies one latest command. No network/decode work takes this lock. */
static portMUX_TYPE radar_command_lock=portMUX_INITIALIZER_UNLOCKED;
static struct { bool pending; app_radar_action_t action; time_t time; } radar_command;

static struct {
  bool images_initialized, location_set, base_ready;
  double latitude, longitude;
  uint8_t *base, *image[2];
  unsigned slot;
  int tile_x, tile_y;
  radar_geometry_t geometry;
  int16_t source_x[APP_RADAR_WIDTH];
  radar_phase_t phase;
  time_t candidate_time;
  app_radar_snapshot_t published;
  radar_frame_t frames[RADAR_HISTORY_MAX];
  unsigned frame_count, tile_count, work_frame;
  size_t cache_bytes;
  bool cache_work, playing, paused, metadata_old;
  time_t held_time, slot_time[2];
  int64_t next_frame_us;
  int64_t request_times[100];
  unsigned request_times_count;
} radar;

esp_err_t weather_client_radar_control(app_radar_action_t action,time_t displayed_frame_time)
{
  if (action<APP_RADAR_LATEST || action>APP_RADAR_PAUSE) return ESP_ERR_INVALID_ARG;
  portENTER_CRITICAL(&radar_command_lock);
  radar_command.pending=true;
  radar_command.action=action;
  radar_command.time=displayed_frame_time;
  portEXIT_CRITICAL(&radar_command_lock);
  return ESP_OK;
}


#ifdef ESP_PLATFORM
/* LodePNG's finite intermediate allocations must not consume the internal heap.
 * Header dimensions are checked independently before decoding any PNG chunks. */
void *lodepng_malloc(size_t size)
{
  return size <= RADAR_PNG_ALLOC_MAX ? radar_alloc(size) : NULL;
}
void *lodepng_realloc(void *ptr, size_t size)
{
  if (size > RADAR_PNG_ALLOC_MAX) return NULL;
  if (!size) { free(ptr); return NULL; }
  /* PNG inflation can grow multi-megabyte buffers. Copy outside the heap lock. */
  void *data = radar_alloc(size);
  if (data) {
    if (ptr) {
      size_t previous = heap_caps_get_allocated_size(ptr);
      memcpy(data, ptr, previous < size ? previous : size);
    }
    free(ptr);
  }
  return data;
}
void lodepng_free(void *ptr) { heap_caps_free(ptr); }
static void *radar_draw_malloc(size_t size, lv_color_format_t color_format)
{
  (void)color_format;
  return size <= RADAR_PNG_ALLOC_MAX ? radar_alloc(size) : NULL;
}
#endif

esp_err_t weather_client_init_images(void)
{
  if (radar.images_initialized) return ESP_OK;
#ifdef ESP_PLATFORM
  lv_draw_buf_handlers_t *handlers = lv_draw_buf_get_image_handlers();
  handlers->buf_malloc_cb = radar_draw_malloc;
  handlers->buf_free_cb = lodepng_free;
#endif
  radar.images_initialized = true;
  return ESP_OK;
}

static bool radar_coordinate(const char *text, double limit, double *out)
{
  if (!text || !text[0]) return false;
  char *end;
  double value = strtod(text, &end);
  if (*end || !isfinite(value) || fabs(value) > limit) return false;
  *out = value;
  return true;
}
static double radar_world_y(double latitude, double world)
{
  double radians = latitude * RADAR_PI / 180;
  return (1.0 - asinh(tan(radians)) / RADAR_PI) * world / 2;
}
/* The complete 800px square shares the circular screen's 75-ground-mile
 * diameter. USGS exports directly at final resolution with the same transform. */
static bool radar_geometry(double latitude, double longitude, radar_geometry_t *out)
{
  double cosine=cos(latitude*RADAR_PI/180);
  double step=(RADAR_GROUND_M/APP_RADAR_HEIGHT)/(156543.033928*cosine/128);
  double left=(longitude+180)/360*RADAR_WORLD_PIXELS-APP_RADAR_WIDTH*step/2;
  double top=radar_world_y(latitude,RADAR_WORLD_PIXELS)-APP_RADAR_HEIGHT*step/2;
  if (top<0 || top+APP_RADAR_HEIGHT*step>RADAR_WORLD_PIXELS) return false;
  *out=(radar_geometry_t){.left=left,.top=top,.step=step,
    .first_x=(int)floor(left/256),
    .last_x=(int)floor((left+(APP_RADAR_WIDTH-0.5)*step)/256),
    .first_y=(int)floor(top/256),
    .last_y=(int)floor((top+(APP_RADAR_HEIGHT-0.5)*step)/256)};
  return true;
}
static int radar_wrapped_x(int x)
{
  return (x%128+128)%128;
}
/* History adds requests, so enforce RainViewer's published IP allowance with
 * a rolling minute. Waiting is an ordinary bounded step, not a failed frame. */
static bool radar_rainviewer_ready(void)
{
  int64_t now=esp_timer_get_time();
  unsigned expired=0;
  while (expired<radar.request_times_count && now-radar.request_times[expired]>=INT64_C(60000000)) ++expired;
  if (expired) {
    radar.request_times_count-=expired;
    memmove(radar.request_times,radar.request_times+expired,
      radar.request_times_count*sizeof(radar.request_times[0]));
  }
  return radar.request_times_count<100;
}
static esp_err_t radar_request(const char *url, bool background,
                               http_response_t *response)
{
  *response=(http_response_t){.external=true,
    .maximum=background ? RADAR_BASE_RESPONSE_MAX : WEATHER_RESPONSE_MAX};
  if (!background) {
    if (!radar_rainviewer_ready()) return ESP_ERR_INVALID_STATE;
    radar.request_times[radar.request_times_count++]=esp_timer_get_time();
  }
  esp_http_client_config_t config={.url=url,.event_handler=http_event,
    .user_data=response,.timeout_ms=background ? 30000 : WEATHER_HTTP_TIMEOUT_MS,
    .buffer_size=2048,.crt_bundle_attach=esp_crt_bundle_attach,
    .disable_auto_redirect=true,.user_agent="ESP32-P4-Weather-Alarm-Clock/1.0"};
  xSemaphoreTake(request_gate,portMAX_DELAY);
  esp_http_client_handle_t client=esp_http_client_init(&config);
  esp_err_t err=client ? esp_http_client_perform(client) : ESP_FAIL;
  if (client) {
    int status=esp_http_client_get_status_code(client);
    if (status<200 || status>=300) err=ESP_FAIL;
    esp_http_client_cleanup(client);
  }
  xSemaphoreGive(request_gate);
  return response->error!=ESP_OK ? response->error : err;
}
static esp_err_t radar_decode_png(const http_response_t *response, bool background, lv_draw_buf_t **out)
{
  *out = NULL;
  LodePNGState state;
  lodepng_state_init(&state);
  unsigned width=0, height=0;
  unsigned error = lodepng_inspect(&width, &height, &state,
    (const unsigned char *)response->data, response->length);
  unsigned expected_width=background ? APP_RADAR_WIDTH : RADAR_TILE_PIXELS;
  unsigned expected_height=background ? APP_RADAR_HEIGHT : RADAR_TILE_PIXELS;
  if (!error && (width!=expected_width || height!=expected_height || state.info_png.color.bitdepth>8)) error=93;
  state.decoder.read_text_chunks = 0;
  state.decoder.remember_unknown_chunks = 0;
  state.decoder.max_text_size = 1024;
  state.decoder.max_icc_size = 4096;
  /* Include a bounded allowance for Adam7 filter rows as well as RGBA bytes. */
  state.decoder.zlibsettings.max_output_size=(size_t)expected_width*expected_height*4+expected_height*4;
  unsigned char *decoded = NULL;
  if (!error) error = lodepng_decode(&decoded, &width, &height, &state,
    (const unsigned char *)response->data, response->length);
  lodepng_state_cleanup(&state);
  if (error) {
    if (decoded) lv_draw_buf_destroy((lv_draw_buf_t *)decoded);
    return ESP_ERR_INVALID_RESPONSE;
  }
  /* This bundled LodePNG returns a draw buffer containing unconverted RGBA,
   * unlike its upstream malloc-returning API and unlike LVGL's PNG frontend. */
  *out = (lv_draw_buf_t *)decoded;
  return *out ? ESP_OK : ESP_ERR_NO_MEM;
}

/* RainViewer's published Universal Blue rain table, rows -32..95 only.
 * The following snow rows are intentionally excluded (snow coloring is off).
 * Packed RGBA values document table alpha; RGB matching retains received alpha.
 * Repeated colors use the first/lower
 * represented strength in their band, rather than inventing exact dBZ. */
static const uint32_t radar_source_rgba[128] = {
  0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
  0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
  0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x63615914u, 0x66635A19u,
  0x69665C1Eu, 0x6C685D24u, 0x6F6B5F29u, 0x726E612Eu, 0x75706234u, 0x78736439u, 0x7C75653Eu, 0x7F786744u,
  0x827B6949u, 0x857D6A4Eu, 0x88806C54u, 0x8B826D59u, 0x8E856F5Eu, 0x92887164u, 0x9E93756Eu, 0xAA9E7978u,
  0xB6A97E82u, 0xC2B4828Cu, 0xCEC08796u, 0xD2C48BA0u, 0xD6C88FAAu, 0xDACC93B4u, 0xDED097BEu, 0x88DDEEFFu,
  0x6CD1EBFFu, 0x51C5E8FFu, 0x36BAE5FFu, 0x1BAEE2FFu, 0x00A3E0FFu, 0x009AD5FFu, 0x0091CAFFu, 0x0088BFFFu,
  0x007FB4FFu, 0x0077AAFFu, 0x0070A3FFu, 0x00699CFFu, 0x006295FFu, 0x005B8EFFu, 0x005588FFu, 0x005180FFu,
  0x004E78FFu, 0x004A70FFu, 0x004768FFu, 0xFFEE00FFu, 0xFFE000FFu, 0xFFD200FFu, 0xFFC500FFu, 0xFFB700FFu,
  0xFFAA00FFu, 0xFF9F00FFu, 0xFF9500FFu, 0xFF8B00FFu, 0xFF8100FFu, 0xFF4400FFu, 0xF23600FFu, 0xE62800FFu,
  0xD91B00FFu, 0xCD0D00FFu, 0xC10000FFu, 0xA80000FFu, 0x8F0000FFu, 0x760000FFu, 0x5D0000FFu, 0xFFAAFFFFu,
  0xFF9FFFFFu, 0xFF95FFFFu, 0xFF8BFFFFu, 0xFF81FFFFu, 0xFF77FFFFu, 0xFF6CFFFFu, 0xFF62FFFFu, 0xFF58FFFFu,
  0xFF4EFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
  0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0x00FF00FFu, 0x00FF00FFu, 0x00FF00FFu, 0x00FF00FFu, 0x00FF00FFu,
  0x00FF00FFu, 0x00FF00FFu, 0x00FF00FFu, 0x00FF00FFu, 0x00FF00FFu, 0x00FF00FFu, 0x00FF00FFu, 0x00FF00FFu,
  0x00FF00FFu, 0x00FF00FFu, 0x00FF00FFu, 0x00FF00FFu, 0x00FF00FFu, 0x00FF00FFu, 0x00FF00FFu, 0x00FF00FFu,
};
static void radar_color(int dbz, uint8_t rgb[3])
{
  static const struct { int dbz; uint8_t rgb[3]; } stops[] = {
    {-10,{195,230,189}}, {5,{153,217,141}}, {15,{82,194,79}},
    {25,{22,152,48}}, {34,{5,107,30}}, {35,{255,232,0}},
    {40,{255,194,0}}, {44,{255,153,0}}, {45,{240,49,36}},
    {55,{188,12,25}}, {75,{115,0,17}}, {95,{85,0,12}}
  };
  unsigned i=0;
  while (i+1 < sizeof(stops)/sizeof(stops[0]) && dbz > stops[i+1].dbz) ++i;
  if (dbz <= stops[0].dbz) { memcpy(rgb, stops[0].rgb, 3); return; }
  if (i+1 == sizeof(stops)/sizeof(stops[0])) { memcpy(rgb, stops[i].rgb, 3); return; }
  int range=stops[i+1].dbz-stops[i].dbz, delta=dbz-stops[i].dbz;
  for (int channel=0; channel<3; ++channel)
    rgb[channel]=(uint8_t)rounded(stops[i].rgb[channel] +
      (stops[i+1].rgb[channel]-stops[i].rgb[channel]) * (double)delta/range);
}
/* Build the exact RGB lookup once from the two authored provider/palette tables.
 * Insert only the first visible occurrence, preserving lower duplicate strengths.
 * Pixel mapping then needs only integer comparisons and three byte copies. */
static struct { uint32_t source; uint8_t rgb[3]; } radar_lookup[128];
static unsigned radar_lookup_count;
static void radar_init_lookup(void)
{
  if (radar_lookup_count) return;
  for (unsigned i=0; i<108; ++i) {
    if (!(radar_source_rgba[i]&255)) continue;
    uint32_t source=radar_source_rgba[i]>>8;
    unsigned at=0;
    while (at<radar_lookup_count && radar_lookup[at].source<source) ++at;
    if (at<radar_lookup_count && radar_lookup[at].source==source) continue;
    memmove(&radar_lookup[at+1],&radar_lookup[at],
      (radar_lookup_count-at)*sizeof(radar_lookup[0]));
    radar_lookup[at].source=source;
    radar_color((int)i-32,radar_lookup[at].rgb);
    ++radar_lookup_count;
  }
}
static bool radar_map_pixel(uint8_t rgba[4])
{
  if (!rgba[3]) return true;
  radar_init_lookup();
  uint32_t source=((uint32_t)rgba[0]<<16)|((uint32_t)rgba[1]<<8)|rgba[2];
  unsigned lo=0,hi=radar_lookup_count;
  while (lo<hi) {
    unsigned mid=lo+(hi-lo)/2;
    if (radar_lookup[mid].source<source) lo=mid+1;
    else hi=mid;
  }
  if (lo==radar_lookup_count || radar_lookup[lo].source!=source) return false;
  memcpy(rgba,radar_lookup[lo].rgb,3);
  return true;
}
static esp_err_t radar_background(void)
{
  const radar_geometry_t *geometry=&radar.geometry;
  double left=(geometry->left/RADAR_WORLD_PIXELS-0.5)*RADAR_WORLD_METERS;
  double right=((geometry->left+APP_RADAR_WIDTH*geometry->step)/RADAR_WORLD_PIXELS-0.5)*RADAR_WORLD_METERS;
  double top=(0.5-geometry->top/RADAR_WORLD_PIXELS)*RADAR_WORLD_METERS;
  double bottom=(0.5-(geometry->top+APP_RADAR_HEIGHT*geometry->step)/RADAR_WORLD_PIXELS)*RADAR_WORLD_METERS;
  char url[640];
  snprintf(url,sizeof(url),"https://basemap.nationalmap.gov/arcgis/rest/services/USGSTopo/MapServer/export?bbox=%.3f,%.3f,%.3f,%.3f&bboxSR=3857&imageSR=3857&size=%d,%d&format=png32&transparent=false&dpi=96&f=image",left,bottom,right,top,APP_RADAR_WIDTH,APP_RADAR_HEIGHT);
  http_response_t response;
  esp_err_t err=radar_request(url,true,&response);
  lv_draw_buf_t *image=NULL;
  if (err==ESP_OK) err=radar_decode_png(&response,true,&image);
  free(response.data);
  if (err!=ESP_OK) return err;
  for (unsigned y=0; y<APP_RADAR_HEIGHT && err==ESP_OK; ++y)
    for (unsigned x=0; x<APP_RADAR_WIDTH; ++x) {
      const uint8_t *rgba=image->data+y*image->header.stride+x*4;
      if (rgba[3]!=255) { err=ESP_ERR_INVALID_RESPONSE; break; }
      unsigned luminance=(54u*rgba[0]+183u*rgba[1]+19u*rgba[2])>>8;
      radar.base[(size_t)y*APP_RADAR_WIDTH+x]=(uint8_t)luminance;
    }
  lv_draw_buf_destroy(image);
  return err;
}
static esp_err_t radar_tile_image(radar_frame_t *frame,unsigned tile_index,bool compose)
{
  const radar_geometry_t *geometry=&radar.geometry;
  if (!frame->tiles) {
    size_t bytes=radar.tile_count*sizeof(*frame->tiles);
    if (bytes>RADAR_CACHE_MAX-radar.cache_bytes) return ESP_ERR_NO_MEM;
    frame->tiles=radar_alloc(bytes);
    if (!frame->tiles) return ESP_ERR_NO_MEM;
    memset(frame->tiles,0,bytes);
    frame->tile_count=radar.tile_count;
    radar.cache_bytes+=bytes;
  }
  radar_cached_tile_t *cached=&frame->tiles[tile_index];
  http_response_t response={0};
  esp_err_t err=ESP_OK;
  if (cached->png) {
    response.data=(char *)cached->png;
    response.length=cached->size;
  } else {
    char url[384];
    snprintf(url,sizeof(url),"https://tilecache.rainviewer.com%s/512/7/%d/%d/2/1_0.png",
      frame->path,radar_wrapped_x(radar.tile_x),radar.tile_y);
    err=radar_request(url,false,&response);
  }
  lv_draw_buf_t *tile=NULL;
  if (err==ESP_OK) err=radar_decode_png(&response,false,&tile);
  if (err!=ESP_OK) {
    if (!cached->png) free(response.data);
    return err;
  }
  /* Validate and recolor the entire provider tile before output sampling. */
  for (unsigned y=0; y<RADAR_TILE_PIXELS && err==ESP_OK; ++y)
    for (unsigned x=0; x<RADAR_TILE_PIXELS; ++x)
      if (!radar_map_pixel(tile->data+y*tile->header.stride+x*4)) {
        err=ESP_ERR_INVALID_RESPONSE;
        break;
      }
  if (err==ESP_OK && compose) {
    /* Keep geography in 256-native pixels; scale before rounding so odd
     * 512px source pixels are sampled. Horizontal samples repeat each row. */
    for (int x=0; x<APP_RADAR_WIDTH; ++x) {
      int source_x=(int)floor((geometry->left+(x+0.5)*geometry->step-radar.tile_x*256)*2);
      radar.source_x[x]=source_x<0 || source_x>=RADAR_TILE_PIXELS ? -1 : (int16_t)source_x;
    }
    for (int y=0; y<APP_RADAR_HEIGHT; ++y) {
      int source_y=(int)floor((geometry->top+(y+0.5)*geometry->step-radar.tile_y*256)*2);
      if (source_y<0 || source_y>=RADAR_TILE_PIXELS) continue;
      for (int x=0; x<APP_RADAR_WIDTH; ++x) {
        int source_x=radar.source_x[x];
        if (source_x<0) continue;
        const uint8_t *rgba=tile->data+source_y*tile->header.stride+source_x*4;
        if (rgba[3]==0) continue;
        uint8_t *bgra=radar.image[radar.slot]+((size_t)y*APP_RADAR_WIDTH+x)*4;
        for (unsigned c=0; c<3; ++c)
          bgra[c]=(uint8_t)((rgba[2-c]*rgba[3]+bgra[c]*(255-rgba[3])+127)/255);
      }
    }
  }
  lv_draw_buf_destroy(tile);
  if (!cached->png && err==ESP_OK) {
    if (response.capacity>RADAR_CACHE_MAX-radar.cache_bytes) err=ESP_ERR_NO_MEM;
    else {
      cached->png=(uint8_t *)response.data;
      cached->size=response.length;
      cached->capacity=response.capacity;
      radar.cache_bytes+=response.capacity;
      ++frame->loaded;
    }
  }
  if (cached->png!=(uint8_t *)response.data) free(response.data);
  return err;
}
static esp_err_t radar_tile(void)
{
  unsigned index=(unsigned)(radar.tile_y-radar.geometry.first_y)*
    (unsigned)(radar.geometry.last_x-radar.geometry.first_x+1)+
    (unsigned)(radar.tile_x-radar.geometry.first_x);
  return radar_tile_image(&radar.frames[radar.work_frame],index,true);
}
static void radar_first_tile(const radar_geometry_t *geometry)
{
  radar.tile_x=geometry->first_x;
  radar.tile_y=geometry->first_y;
}
static bool radar_next_tile(const radar_geometry_t *geometry)
{
  if (++radar.tile_x>geometry->last_x) {
    radar.tile_x=geometry->first_x;
    ++radar.tile_y;
  }
  return radar.tile_y<=geometry->last_y;
}
static void radar_free_frame(radar_frame_t *frame)
{
  for (unsigned i=0; i<frame->tile_count; ++i) {
    radar.cache_bytes-=frame->tiles[i].capacity;
    free(frame->tiles[i].png);
  }
  radar.cache_bytes-=frame->tile_count*sizeof(*frame->tiles);
  free(frame->tiles);
  memset(frame,0,sizeof(*frame));
}
static void radar_clear_history(void)
{
  for (unsigned i=0; i<RADAR_HISTORY_MAX; ++i) radar_free_frame(&radar.frames[i]);
  radar.frame_count=0;
}
static bool radar_safe_path(const char *path)
{
  if (!path || path[0]!='/' || strlen(path)>=RADAR_PATH_LEN || strstr(path,"..")) return false;
  for (; *path; ++path)
    if (!isalnum((unsigned char)*path) && !strchr("/-_.",*path)) return false;
  return true;
}
static bool radar_metadata(bool *unchanged)
{
  http_response_t response;
  esp_err_t err=radar_request("https://api.rainviewer.com/public/weather-maps.json",false,&response);
  cJSON *root=err==ESP_OK ? cJSON_Parse(response.data) : NULL;
  const char *host=string(field(root,"host"));
  const cJSON *frames=field(field(root,"radar"),"past");
  time_t latest=0;
  bool valid=host && !strcmp(host,"https://tilecache.rainviewer.com") && cJSON_IsArray(frames);
  if (valid) for (int i=0; i<cJSON_GetArraySize(frames); ++i) {
    const cJSON *frame=at(frames,i);
    double when=number(field(frame,"time"));
    if (radar_safe_path(string(field(frame,"path"))) && when>latest &&
        when<=INT64_C(4102444800) && floor(when)==when) latest=(time_t)when;
  }
  time_t previous=radar.frame_count ? radar.frames[radar.frame_count-1].time : radar.published.frame_time;
  valid=valid && latest>0 && latest>=previous;
  *unchanged=valid && radar.published.valid &&
    (latest==radar.published.frame_time || radar.playing || radar.paused);
  /* Select at most six distinct ten-minute samples from (latest-1h, latest].
   * Keep the first metadata path for duplicate timestamps. */
  struct { time_t time; const char *path; } selected[RADAR_HISTORY_MAX]={0};
  unsigned count=0;
  if (valid) for (int i=0; i<cJSON_GetArraySize(frames); ++i) {
    const cJSON *frame=at(frames,i);
    double when=number(field(frame,"time"));
    const char *path=string(field(frame,"path"));
    if (!radar_safe_path(path) || !isfinite(when) || when<=latest-3600 || when>latest ||
        floor(when)!=when || (latest-(time_t)when)%600) continue;
    unsigned pos=0;
    while (pos<count && selected[pos].time>(time_t)when) ++pos;
    if ((pos<count && selected[pos].time==(time_t)when) || pos==RADAR_HISTORY_MAX) continue;
    if (count<RADAR_HISTORY_MAX) ++count;
    memmove(&selected[pos+1],&selected[pos],(count-pos-1)*sizeof(selected[0]));
    selected[pos].time=(time_t)when;selected[pos].path=path;
  }
  radar_frame_t *next=valid ? radar_alloc(sizeof(radar.frames)) : NULL;
  if (valid && !next) valid=false;
  if (valid) {
    memset(next,0,sizeof(radar.frames));
    for (unsigned i=0; i<count; ++i) {
      unsigned chosen=count-1-i;
      next[i].time=selected[chosen].time;
      snprintf(next[i].path,sizeof(next[i].path),"%s",selected[chosen].path);
      for (unsigned old=0; old<radar.frame_count; ++old)
        if (radar.frames[old].time==next[i].time && !strcmp(radar.frames[old].path,next[i].path)) {
          next[i]=radar.frames[old];next[i].failed=false;
          memset(&radar.frames[old],0,sizeof(radar.frames[old]));
          break;
        }
    }
    radar_clear_history();
    memcpy(radar.frames,next,sizeof(radar.frames));
    radar.frame_count=count;
    radar.metadata_old=false;
    radar.candidate_time=latest;
  }
  free(next);cJSON_Delete(root);free(response.data);
  return valid;
}
static void radar_prepare_image(void)
{
  uint8_t *pixels=radar.image[radar.slot];
  for (int y=0; y<APP_RADAR_HEIGHT; ++y) for (int x=0; x<APP_RADAR_WIDTH; ++x) {
    size_t pixel=(size_t)y*APP_RADAR_WIDTH+x;
    uint8_t *bgra=pixels+pixel*4;
    unsigned luminance=radar.base[pixel];
    /* Match the approved brighter blue-gray map. Keep only source luminance
     * in the retained background; the two final images own their BGRA colors. */
    bgra[0]=(uint8_t)((18870-63*luminance+50)/100);
    bgra[1]=(uint8_t)((17340-60*luminance+50)/100);
    bgra[2]=(uint8_t)((15810-56*luminance+50)/100);
    bgra[3]=255;
  }
}
static bool radar_frame_complete(const radar_frame_t *frame)
{
  return radar.tile_count && frame->loaded==radar.tile_count && !frame->failed;
}
static unsigned radar_complete_count(void)
{
  unsigned count=0;
  for (unsigned i=0; i<radar.frame_count; ++i) if (radar_frame_complete(&radar.frames[i])) ++count;
  return count;
}
static bool radar_history_loading(void)
{
  for (unsigned i=0; i<radar.frame_count; ++i)
    if (!radar.frames[i].failed && !radar_frame_complete(&radar.frames[i])) return true;
  return false;
}
static bool radar_active(void)
{
  return radar.phase!=RADAR_IDLE || radar.playing || radar_history_loading();
}
static bool radar_publish(app_radar_snapshot_t *out)
{
  radar.published.history_count=(uint8_t)radar_complete_count();
  radar.published.history_loading=radar_history_loading();
  radar.published.playing=radar.playing;
  radar.published.paused=radar.paused;
  if (++radar.published.generation==0) ++radar.published.generation;
  *out=radar.published;
  return true;
}
static bool radar_failure(app_radar_snapshot_t *out, const char *stage)
{
  ESP_LOGW(TAG,"Radar %s failed; retaining the last same-location image",stage);
  radar.phase=RADAR_IDLE;
  if (radar.cache_work && radar.work_frame<radar.frame_count) radar.frames[radar.work_frame].failed=true;
  radar.cache_work=false;
  radar.published.stale=true;
  return radar_publish(out);
}
/* Apply controls before every step and recheck after slow work, before publishing
 * pixels. A pending command cancels obsolete preparation at the next short step. */
static bool weather_client_radar_control_pending(void)
{
  portENTER_CRITICAL(&radar_command_lock);
  bool pending=radar_command.pending;
  portEXIT_CRITICAL(&radar_command_lock);
  return pending;
}
static void radar_begin_frame(unsigned index);
static bool radar_apply_command(app_radar_snapshot_t *out)
{
  portENTER_CRITICAL(&radar_command_lock);
  if (!radar_command.pending) { portEXIT_CRITICAL(&radar_command_lock);return false; }
  app_radar_action_t action=radar_command.action;
  time_t when=radar_command.time;
  radar_command.pending=false;
  portEXIT_CRITICAL(&radar_command_lock);
  bool keep_metadata=radar.phase==RADAR_METADATA;
  bool keep_latest=action==APP_RADAR_LATEST && radar.cache_work && radar.frame_count &&
    radar.work_frame==radar.frame_count-1 &&
    (radar.phase==RADAR_BASE || radar.phase==RADAR_WAIT_SLOT ||
     radar.phase==RADAR_ECHO || radar.phase==RADAR_READY);
  if (!keep_metadata && !keep_latest) radar.phase=RADAR_IDLE;
  if (!keep_latest) radar.cache_work=false;
  if (action==APP_RADAR_PLAY) {
    bool resume=radar.paused;
    radar.playing=radar_complete_count()>=2;
    radar.paused=false;
    radar.next_frame_us=0;
    radar.held_time=resume ? when : 0;
  } else {
    radar.playing=false;
    radar.paused=action==APP_RADAR_PAUSE && radar.published.valid;
    radar.held_time=radar.paused ? when : 0;
    /* Both the displayed and pending image remain immutable until ack. The
     * requested time identifies the former if publication overtook the tap. */
    if (radar.paused && when!=radar.published.frame_time) {
      bool found=false;
      for (unsigned slot=0; slot<2; ++slot) if (radar.slot_time[slot]==when && radar.image[slot]) {
        radar.published.pixels=radar.image[slot];radar.published.frame_time=when;
        found=true;break;
      }
      if (!found) {
        /* An impossible/stale UI identity must complete the handoff explicitly. */
        radar.published.valid=false;radar.published.pixels=NULL;
        radar.published.frame_time=0;radar.paused=false;
      }
    }
  }
  if (action==APP_RADAR_LATEST && !keep_metadata && !keep_latest && radar.frame_count &&
      radar_frame_complete(&radar.frames[radar.frame_count-1]) &&
      radar.published.frame_time!=radar.frames[radar.frame_count-1].time) {
    time_t latest=radar.frames[radar.frame_count-1].time;
    bool restored=false;
    for (unsigned slot=0;slot<2;++slot) if (radar.slot_time[slot]==latest && radar.image[slot]) {
      radar.published.frame_time=latest;radar.published.pixels=radar.image[slot];restored=true;break;
    }
    if (!restored) radar_begin_frame(radar.frame_count-1);
  }
  return radar_publish(out);
}
static int radar_next_play_frame(time_t after)
{
  for (unsigned i=0; i<radar.frame_count; ++i)
    if (radar_frame_complete(&radar.frames[i]) && radar.frames[i].time>after) return (int)i;
  return -1;
}
static void radar_begin_frame(unsigned index)
{
  radar.work_frame=index;radar.cache_work=true;
  radar.candidate_time=radar.frames[index].time;
  radar.phase=radar.base_ready ? RADAR_WAIT_SLOT : RADAR_BASE;
}
/* Fill one missing history tile in a bounded step. Playback uses this spare
 * time only after the next output is ready; latest image always comes first. */
static bool radar_fill_history(app_radar_snapshot_t *out)
{
  for (unsigned reverse=radar.frame_count; reverse>0; --reverse) {
    radar_frame_t *frame=&radar.frames[reverse-1];
    if (frame->failed || radar_frame_complete(frame)) continue;
    unsigned index=0;
    while (frame->tiles && index<radar.tile_count && frame->tiles[index].png) ++index;
    int previous_x=radar.tile_x,previous_y=radar.tile_y;
    unsigned columns=(unsigned)(radar.geometry.last_x-radar.geometry.first_x+1);
    radar.tile_x=radar.geometry.first_x+(int)(index%columns);
    radar.tile_y=radar.geometry.first_y+(int)(index/columns);
    if (!radar_rainviewer_ready()) {
      radar.tile_x=previous_x;radar.tile_y=previous_y;return false;
    }
    esp_err_t err=radar_tile_image(frame,index,false);
    radar.tile_x=previous_x;radar.tile_y=previous_y;
    if (err!=ESP_OK) {
      frame->failed=true;
      if (reverse==radar.frame_count) {
        radar.metadata_old=true;
        radar.published.stale=true;
      }
      ESP_LOGW(TAG,"Radar history tile failed; retaining completed frames");
      return radar_publish(out);
    }
    return radar_frame_complete(frame) ? radar_publish(out) : false;
  }
  return false;
}
static bool weather_client_radar_step(const device_config_t *config, weather_radar_step_t action,
                               uint32_t displayed_generation,
                               app_radar_snapshot_t *out, bool *active)
{
  if (!active) return false;
  *active=false;
  if (!config || !out || !request_gate || !radar.images_initialized) return false;
  double latitude=0,longitude=0;
  bool location=config->location_ready && location_scoring_label_has_country(config->location,"US") &&
    radar_coordinate(config->latitude,85.05112878,&latitude) &&
    radar_coordinate(config->longitude,180,&longitude);
  bool changed=location!=radar.location_set ||
    (location && (latitude!=radar.latitude || longitude!=radar.longitude));
  if (changed) {
    radar_clear_history();
    radar.location_set=location;radar.latitude=latitude;radar.longitude=longitude;
    radar.base_ready=false;radar.playing=false;radar.paused=false;radar.cache_work=false;
    radar.tile_count=0;radar.slot_time[0]=radar.slot_time[1]=0;
    uint32_t generation=radar.published.generation;
    radar.published=(app_radar_snapshot_t){.generation=generation,.latitude=latitude,.longitude=longitude};
    radar.phase=location && action!=WEATHER_RADAR_OFFLINE ? RADAR_METADATA : RADAR_IDLE;
    portENTER_CRITICAL(&radar_command_lock);radar_command.pending=false;portEXIT_CRITICAL(&radar_command_lock);
    *active=radar.phase!=RADAR_IDLE;
    return radar_publish(out);
  }
  if (!location) return false;
  bool command_applied=radar_apply_command(out);
  if (action==WEATHER_RADAR_OFFLINE) {
    bool was_playing=radar.playing,was_stale=radar.published.stale;
    radar.playing=false;radar.metadata_old=true;radar.published.stale=true;
    for (unsigned i=0; i<radar.frame_count; ++i)
      if (!radar_frame_complete(&radar.frames[i])) radar.frames[i].failed=true;
    bool restore=!radar.paused && radar.base_ready && radar.frame_count &&
      radar_frame_complete(&radar.frames[radar.frame_count-1]) &&
      radar.published.frame_time!=radar.frames[radar.frame_count-1].time;
    if (restore) {
      unsigned latest=radar.frame_count-1;
      if (!radar.cache_work || radar.work_frame!=latest ||
          (radar.phase!=RADAR_WAIT_SLOT && radar.phase!=RADAR_ECHO && radar.phase!=RADAR_READY))
        radar_begin_frame(latest);
      *active=true;
      if (command_applied || was_playing || !was_stale) return radar_publish(out);
      /* Only a fully validated cached latest frame reaches the ordinary image
       * steps below. No offline metadata, background or missing-tile request. */
    } else {
      radar.phase=RADAR_IDLE;radar.cache_work=false;
      if (was_stale && !command_applied && !was_playing) return false;
      return radar_publish(out);
    }
  }
  if (action==WEATHER_RADAR_CHECK) radar.phase=RADAR_METADATA;
  if (command_applied) { *active=radar_active();return true; }
  *active=true;
  if (radar.phase==RADAR_METADATA) {
    if (!radar_rainviewer_ready()) return false;
    bool unchanged=false;
    radar.cache_work=false;
    if (!radar_geometry(latitude,longitude,&radar.geometry)) {
      *active=false;return radar_failure(out,"coverage");
    }
    unsigned columns=(unsigned)(radar.geometry.last_x-radar.geometry.first_x+1);
    unsigned rows=(unsigned)(radar.geometry.last_y-radar.geometry.first_y+1);
    if (columns>RADAR_TILES_MAX || rows>RADAR_TILES_MAX || columns*rows>RADAR_TILES_MAX) {
      *active=false;return radar_failure(out,"tile limit");
    }
    radar.tile_count=columns*rows;
    if (!radar_metadata(&unchanged)) {
      radar.metadata_old=true;
      for (unsigned i=0; i<radar.frame_count; ++i)
        if (!radar_frame_complete(&radar.frames[i])) radar.frames[i].failed=true;
      bool result=radar_failure(out,"metadata");*active=radar_active();return result;
    }
    radar.phase=RADAR_IDLE;
    bool old_stale=radar.published.stale;
    if (unchanged) radar.published.stale=false;
    if (!radar.paused && !radar.playing &&
        (!radar.published.valid || radar.published.frame_time!=radar.frames[radar.frame_count-1].time))
      radar_begin_frame(radar.frame_count-1);
    *active=radar_active();
    if (old_stale!=radar.published.stale ||
        ((radar.playing || radar.paused) && radar.published.history_count!=radar_complete_count()))
      return radar_publish(out);
    return false;
  }
  if (radar.phase==RADAR_IDLE) {
    if (!radar.paused && !radar.playing && radar.frame_count &&
        radar_frame_complete(&radar.frames[radar.frame_count-1]) &&
        radar.published.frame_time!=radar.frames[radar.frame_count-1].time)
      radar_begin_frame(radar.frame_count-1);
    else if (radar.playing) {
      int frame=radar_next_play_frame(radar.next_frame_us ? radar.published.frame_time : radar.held_time);
      if (frame>=0) radar_begin_frame((unsigned)frame);
      else {
        /* Finish in Latest mode and tell the UI to offer Play again. */
        radar.playing=false;radar.paused=false;radar.held_time=0;radar.next_frame_us=0;
        *active=radar_active();
        return radar_publish(out);
      }
    }
    if (radar.phase==RADAR_IDLE) {
      bool result=radar_fill_history(out);*active=radar_active();return result;
    }
  }
  if (radar.phase==RADAR_BASE) {
    /* Location invalidation must reach the UI before obsolete output storage
     * can be reclaimed for the large background decode. */
    if (!radar.published.valid && (radar.image[0] || radar.image[1])) {
      if (displayed_generation!=radar.published.generation) return false;
      for (unsigned slot=0; slot<2; ++slot) {
        free(radar.image[slot]);
        radar.image[slot]=NULL;
        radar.slot_time[slot]=0;
      }
    }
    if (!radar.base) radar.base=radar_alloc(RADAR_PIXELS);
    if (!radar.base || radar_background()!=ESP_OK) {
      bool result=radar_failure(out,"background image");*active=radar_active();return result;
    }
    radar.base_ready=true;radar.phase=RADAR_WAIT_SLOT;return false;
  }
  if (radar.phase==RADAR_WAIT_SLOT) {
    if (displayed_generation!=radar.published.generation) return false;
    radar.slot=radar.published.pixels==radar.image[0] ? 1 : 0;
    if (!radar.image[radar.slot]) radar.image[radar.slot]=radar_alloc(RADAR_PIXELS*4);
    if (!radar.image[radar.slot]) {
      bool result=radar_failure(out,"image allocation");*active=radar_active();return result;
    }
    radar.slot_time[radar.slot]=0;
    radar_prepare_image();radar_first_tile(&radar.geometry);radar.phase=RADAR_ECHO;return false;
  }
  if (radar.phase==RADAR_ECHO) {
    unsigned index=(unsigned)(radar.tile_y-radar.geometry.first_y)*
      (unsigned)(radar.geometry.last_x-radar.geometry.first_x+1)+
      (unsigned)(radar.tile_x-radar.geometry.first_x);
    radar_frame_t *frame=&radar.frames[radar.work_frame];
    if ((!frame->tiles || !frame->tiles[index].png) && !radar_rainviewer_ready()) return false;
    if (radar_tile()!=ESP_OK) {
      bool result=radar_failure(out,"echo tile");*active=radar_active();return result;
    }
    if (radar_next_tile(&radar.geometry)) return false;
    radar.phase=RADAR_READY;
  }
  if (weather_client_radar_control_pending()) return false;
  if (radar.phase==RADAR_READY) {
    if (radar.playing && radar.next_frame_us && esp_timer_get_time()<radar.next_frame_us) {
      bool result=radar_fill_history(out);*active=radar_active();return result;
    }
    portENTER_CRITICAL(&radar_command_lock);
    if (radar_command.pending) { portEXIT_CRITICAL(&radar_command_lock);return false; }
    radar.published.valid=true;radar.published.stale=radar.metadata_old;
    radar.published.frame_time=radar.candidate_time;
    radar.published.latitude=latitude;radar.published.longitude=longitude;
    radar.published.pixels=radar.image[radar.slot];
    radar.slot_time[radar.slot]=radar.candidate_time;
    radar.phase=RADAR_IDLE;radar.cache_work=false;
    radar.next_frame_us=esp_timer_get_time()+RADAR_FRAME_US;
    bool result=radar_publish(out);
    portEXIT_CRITICAL(&radar_command_lock);
    *active=radar_active();return result;
  }
  *active=radar_active();return false;
}

static void weather_update(
  const app_weather_snapshot_t *weather,
  bool mark_weather_stale
)
{
  if (weather_copy_lock == NULL) return;
  if (xSemaphoreTake(weather_copy_lock, pdMS_TO_TICKS(WEATHER_COPY_TIMEOUT_MS)) != pdTRUE) {
    ESP_LOGW(TAG, "Timed out waiting for weather copy lock");
    return;
  }
  if (weather != NULL) weather_completed.weather = *weather;
  if (mark_weather_stale && weather_completed.weather.valid)
    weather_completed.weather.stale = true;
  xSemaphoreGive(weather_copy_lock);
}

static bool weather_radar_publish(const app_radar_snapshot_t *radar)
{
  if (!weather_copy_lock || xSemaphoreTake(weather_copy_lock,
        pdMS_TO_TICKS(WEATHER_COPY_TIMEOUT_MS)) != pdTRUE) return false;
  weather_completed.radar = *radar;
  xSemaphoreGive(weather_copy_lock);
#ifdef ESP_PLATFORM
  /* Wake Radar updates independently of the clock's one-second polling. */
  if (weather_ui_task) xTaskNotifyGive(weather_ui_task);
#endif
  return true;
}

/* Only acknowledge after LVGL has replaced its image source under the display
 * lock. The client may then reuse the previous slot, never the displayed one. */
void weather_client_radar_acknowledge(uint32_t generation)
{
  if (!weather_copy_lock || xSemaphoreTake(weather_copy_lock,
        pdMS_TO_TICKS(WEATHER_COPY_TIMEOUT_MS)) != pdTRUE) return;
  radar_displayed_generation = generation;
  xSemaphoreGive(weather_copy_lock);
}

static uint32_t weather_radar_displayed(void)
{
  if (!weather_copy_lock || xSemaphoreTake(weather_copy_lock,
        pdMS_TO_TICKS(WEATHER_COPY_TIMEOUT_MS)) != pdTRUE) return 0;
  uint32_t displayed = radar_displayed_generation;
  xSemaphoreGive(weather_copy_lock);
  return displayed;
}

/* A copied descriptor is not an acknowledgment that LVGL replaced its image. */
bool weather_client_copy(app_weather_snapshot_t *weather, app_radar_snapshot_t *radar_copy)
{
  if (!weather || !radar_copy || !weather_copy_lock ||
      xSemaphoreTake(weather_copy_lock, pdMS_TO_TICKS(WEATHER_COPY_TIMEOUT_MS)) != pdTRUE)
    return false;
  *weather = weather_completed.weather;
  *radar_copy = weather_completed.radar;
  xSemaphoreGive(weather_copy_lock);
  return true;
}

static void weather_service_wait(int64_t weather_due_us, int64_t time_retry_due_us,
                                 int64_t radar_due_us, int64_t alerts_due_us)
{
  if (time_retry_due_us > 0 && time_retry_due_us < weather_due_us) {
    weather_due_us = time_retry_due_us;
  }
  if (radar_due_us < weather_due_us) weather_due_us = radar_due_us;
  if (alerts_due_us < weather_due_us) weather_due_us = alerts_due_us;
  int64_t remaining_us = weather_due_us - esp_timer_get_time();
  if (remaining_us > 0) {
    TickType_t ticks = pdMS_TO_TICKS((remaining_us + 999) / 1000);
#ifdef ESP_PLATFORM
    /* UI playback commands wake this existing owner without changing any
     * network deadline. A queued notification also covers the wait-entry race. */
    (void)ulTaskNotifyTake(pdTRUE, ticks > 0 ? ticks : 1);
#else
    vTaskDelay(ticks > 0 ? ticks : 1);
#endif
  }
}

static void weather_service_alerts(const device_config_t *config,
                                   int64_t *due_us, bool connected)
{
  weather_client_alerts_sync_location(config);
  weather_client_alerts_maintain(config);
  if (!config->location_ready || !config->wifi_ready) return;
  app_alert_snapshot_t alerts;
  weather_client_alerts_copy(&alerts);
  if (alerts.status == APP_ALERT_UNSUPPORTED) {
    *due_us = INT64_MAX;
    return;
  }
  /* The heartbeat may already have observed a new saved location. Its fresh
   * checking state, rather than a second location model, resets this deadline. */
  if (alerts.status == APP_ALERT_CHECKING) {
    *due_us = 0;
    if (!connected) return;
  }
  int64_t now = esp_timer_get_time();
  if (now < *due_us) return;
  *due_us = now + NWS_CHECK_INTERVAL_US;
  int64_t pause_until = 0;
  esp_err_t error = weather_client_alerts_check(config, connected, &pause_until);
  if (error != ESP_OK)
    ESP_LOGW(TAG, "NWS update failed: %s", esp_err_to_name(error));
  if (pause_until > *due_us) *due_us = pause_until;
  weather_client_alerts_maintain(config);
}

static void weather_service_radar(const device_config_t *config,
                                  weather_radar_work_t *work, bool connected)
{
  /* Freeze the exact candidate after a failed finite copy. Retry it before any
   * further Radar mutation, including commands, location changes or tile work.
   * The raw READY step releases radar_command_lock before we take this mutex. */
  if (work->publication_pending)
    work->publication_pending = !weather_radar_publish(&radar.published);
  if (work->publication_pending) return;
  /* Before the first image/attempt, wait for Wi-Fi readiness. An ordinary
   * outage must still mark an existing frame old when its check becomes due. */
  if (!connected && !work->active && !radar.published.valid) return;
  bool check = esp_timer_get_time() >= work->due_us;
  if (!check && !work->active && !weather_client_radar_control_pending()) return;
  if (check) work->due_us = esp_timer_get_time()
    + (int64_t) RADAR_REFRESH_INTERVAL_MS * 1000;
  weather_radar_step_t step = !connected ? WEATHER_RADAR_OFFLINE
    : check ? WEATHER_RADAR_CHECK : WEATHER_RADAR_CONTINUE;
  work->publication_pending = weather_client_radar_step(config, step,
    weather_radar_displayed(), &radar.published, &work->active);
  if (work->publication_pending)
    work->publication_pending = !weather_radar_publish(&radar.published);
}

void weather_client_task(void *arg)
{
  weather_ui_task = (TaskHandle_t) arg;

  const TickType_t retry_delay_ticks = pdMS_TO_TICKS(WEATHER_RETRY_INTERVAL_MS);
  const TickType_t wifi_timeout_ticks = pdMS_TO_TICKS(WIFI_CONNECT_TIMEOUT_MS);
  bool connectivity_started = false;
  bool reported_missing_wifi_profile = false;
  int64_t next_weather_attempt_us = 0;
  int64_t time_retry_due_us = 0;
  int64_t alerts_due_us = 0;
  weather_radar_work_t radar_work = {0};

  while (true) {
    device_config_t config;
    if (device_config_copy(&config) != ESP_OK) {
      vTaskDelay(retry_delay_ticks);
      continue;
    }
    weather_service_alerts(&config, &alerts_due_us,
      connectivity_started && connectivity_is_wifi_connected());

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
        weather_update(NULL, true);
        weather_service_radar(&config, &radar_work, false);
        vTaskDelay(retry_delay_ticks);
        continue;
      }

      connectivity_started = true;
    }

    if (!connectivity_is_wifi_connected()) {
      ESP_LOGI(TAG, "Waiting up to %u ms for Wi-Fi connection", WIFI_CONNECT_TIMEOUT_MS);
      weather_update(NULL, true);

      esp_err_t err = connectivity_wait_for_wifi(wifi_timeout_ticks);
      if (err != ESP_OK) {
        ESP_LOGW(TAG, "Wi-Fi connect timeout: %s", esp_err_to_name(err));
        weather_update(NULL, true);
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

    /* NWS has its own deadline and is unaffected by forecast-provider quotas.
     * Service it before a due forecast and between bounded Radar steps. */
    weather_service_alerts(&config, &alerts_due_us, true);

    int64_t quota_due = weather_client_next_allowed_us(&config);
    if (quota_due > next_weather_attempt_us) next_weather_attempt_us = quota_due;
    if (esp_timer_get_time() >= next_weather_attempt_us) {
      ESP_LOGI(TAG, "Requesting live weather for %s,%s (%s units)",
               config.latitude, config.longitude, config.units);
      app_weather_snapshot_t weather_snapshot = {0};
      esp_err_t weather_err = weather_client_fetch(&config, &weather_snapshot);
      if (weather_err != ESP_OK) {
        ESP_LOGW(TAG, "Weather refresh failed: %s", esp_err_to_name(weather_err));
        weather_update(NULL, true);
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
        weather_update(&weather_snapshot, false);
        ESP_LOGI(TAG, "Live weather updated (%d°, H:%d° L:%d°)",
                 weather_snapshot.temp, weather_snapshot.high, weather_snapshot.low);
        next_weather_attempt_us = esp_timer_get_time()
          + (int64_t) WEATHER_REFRESH_INTERVAL_MS * 1000;
      }
    }

    /* One bounded step leaves forecast/time eligible between Radar requests. */
    weather_service_radar(&config, &radar_work, true);
    weather_service_alerts(&config, &alerts_due_us, true);
    int64_t radar_due_us = (radar_work.active || radar_work.publication_pending)
      ? esp_timer_get_time() + (int64_t) RADAR_STEP_INTERVAL_MS * 1000
      : radar_work.due_us;
    weather_service_wait(next_weather_attempt_us, time_retry_due_us, radar_due_us,
      alerts_due_us);
  }
}

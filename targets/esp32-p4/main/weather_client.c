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
#include "forecast_representative.h"
#include "storm_conditions.h"
#include "weather_icon_names.h"
#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#include "freertos/task.h"
#endif

static const char *TAG = "rwd_weather";
#define WEATHER_HTTP_TIMEOUT_MS 10000
#define WEATHER_RESPONSE_MAX (192 * 1024)
#define WEATHER_HOURS_MAX 192
#define QUOTA_HOUR_US INT64_C(3600000000)

static SemaphoreHandle_t request_mutex;
static StaticSemaphore_t request_mutex_storage;
typedef struct {
  char key[DEVICE_CONFIG_WEATHER_KEY_LEN];
  bool paused;
  int64_t next_us;
} quota_record_t;
static quota_record_t quota[2];
static unsigned request_count;

#ifdef ESP_PLATFORM
static void log_request_resources(const char *phase)
{
  /* IDF reports the stack high-water mark in bytes. Sample while the response,
   * parsed JSON and normalized arrays coexist, then again after cleanup. */
  uint32_t internal=MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
  uint32_t external=MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
  ESP_LOGI(TAG,"request=%u phase=%s task=%s stack_free_min_bytes=%u "
    "internal_free=%u internal_largest=%u psram_free=%u psram_largest=%u",
    request_count,phase,pcTaskGetName(NULL),(unsigned)uxTaskGetStackHighWaterMark(NULL),
    (unsigned)heap_caps_get_free_size(internal),(unsigned)heap_caps_get_largest_free_block(internal),
    (unsigned)heap_caps_get_free_size(external),(unsigned)heap_caps_get_largest_free_block(external));
}
#endif

typedef struct {
  char *data;
  size_t length, capacity;
  esp_err_t error;
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
static bool put_number(cJSON *obj, const char *name, double value)
{
  return (isfinite(value) ? cJSON_AddNumberToObject(obj, name, value)
                         : cJSON_AddNullToObject(obj, name)) != NULL;
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
  if (request_mutex) return ESP_OK;
  request_mutex = xSemaphoreCreateMutexStatic(&request_mutex_storage);
#ifdef ESP_PLATFORM
  /* IDF's DEBUG request-header log includes the credential-bearing query.
   * VC uses host/path/query below, avoiding URL parse errors that echo a URL. */
  esp_log_level_set("HTTP_CLIENT", ESP_LOG_WARN);
#endif
  return request_mutex ? ESP_OK : ESP_ERR_NO_MEM;
}
static quota_record_t *record_for(const device_config_t *config)
{
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
int64_t weather_client_next_allowed_us(const device_config_t *config)
{
  if (!config || !request_mutex) return 0;
  xSemaphoreTake(request_mutex, portMAX_DELAY);
  quota_record_t *record = record_for(config);
  int64_t deadline = record && record->paused ? record->next_us : 0;
  xSemaphoreGive(request_mutex);
  return deadline;
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
  if (length > WEATHER_RESPONSE_MAX) return response->error = ESP_ERR_INVALID_SIZE;
  if (length + 1 > response->capacity) {
    size_t capacity = response->capacity ? response->capacity : 4096;
    while (capacity < length + 1) capacity *= 2;
    if (capacity > WEATHER_RESPONSE_MAX + 1) capacity = WEATHER_RESPONSE_MAX + 1;
    char *data = realloc(response->data, capacity);
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
  static const char *names[] = {"time","epoch","condition_flags","sky", "precipitation_probability","precipitation","rain","showers","snowfall","cloud_cover","pressure_msl","wind_speed_10m","wind_gusts_10m","temperature_2m"};
  cJSON *result=cJSON_CreateObject(), *columns[14];
  if (!result) return NULL;
  for (int i=0;i<14;++i) { columns[i]=cJSON_AddArrayToObject(result,names[i]); if (!columns[i]) goto fail; }
  int count=0; const cJSON *days=field(root,"days");
  double precip_factor=metric(config)?1:25.4, snow_factor=metric(config)?1:2.54, wind_factor=metric(config)?1:1.609344;
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
        NAN,NAN,snow,number(field(record,"cloudcover")),number(field(record,"pressure")),
        number(field(record,"windspeed"))*wind_factor,number(field(record,"windgust"))*wind_factor,
        number(field(record,"temp"))};
      for (int i=1;i<14;++i) if (!append_number(columns[i],values[i-1])) goto fail;
    }
  }
  return result;
fail:
  cJSON_Delete(result); return NULL;
}
static cJSON *normalize_vc_current(const device_config_t *config, const cJSON *root,
                                  const cJSON *current, weather_condition_facts_t facts)
{
  cJSON *result=cJSON_CreateObject(); char timestamp[40]="";
  if (!result) return NULL;
  /* The observation can belong to the preceding local day. Its actual epoch
   * and record-specific offset decide that date; today's day is not relabeled. */
  if (!local_timestamp(number(field(current,"datetimeEpoch")),vc_offset(root,NULL,current),timestamp,sizeof(timestamp)) ||
      !cJSON_AddStringToObject(result,"time",timestamp) ||
      !put_number(result,"epoch",number(field(current,"datetimeEpoch"))) ||
      !put_number(result,"condition_flags",facts.flags) ||
      !put_number(result,"temperature",number(field(current,"temp"))) ||
      !put_number(result,"windspeed",number(field(current,"windspeed"))*(metric(config)?1:1.609344)) ||
      !put_number(result,"winddirection",number(field(current,"winddir")))) {
    cJSON_Delete(result); return NULL;
  }
  return result;
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
static void build_conditions(const device_config_t *config,const cJSON *current,
                             const cJSON *hourly,const cJSON *daily,app_weather_snapshot_t *out)
{
  storm_conditions_config_t cc={.metric=metric(config),.time_24h=strcmp(config->time_format,"24")!=0};
  if (storm_conditions_build(&cc,current,hourly,daily,&out->conditions)) {
    snprintf(out->conditions_alternate_headline,sizeof(out->conditions_alternate_headline),"%s",out->conditions.headline);
    if (out->conditions.has_detail) snprintf(out->conditions_alternate_detail,sizeof(out->conditions_alternate_detail),"%s",out->conditions.detail);
  }
  cc.time_24h=!cc.time_24h; out->conditions_time_24h=cc.time_24h;
  out->has_conditions=storm_conditions_build(&cc,current,hourly,daily,&out->conditions);
}
static esp_err_t decode(const device_config_t *config,cJSON *root,app_weather_snapshot_t *out)
{
  bool vc=config->weather_provider==DEVICE_CONFIG_WEATHER_VISUAL_CROSSING;
  const cJSON *current=field(root,vc?"currentConditions":"current_weather"), *days=field(root,"days");
  cJSON *daily=(cJSON *)field(root,"daily"), *hourly=(cJSON *)field(root,"hourly"), *normalized_current=NULL;
  double temperature=number(field(current,vc?"temp":"temperature"));
  if (!isfinite(temperature)||fabs(temperature)>1000 || !cJSON_IsObject(current)) return ESP_FAIL;
  if (vc) {
    double epoch=number(field(current,"datetimeEpoch"));
    if (!isfinite(epoch) || epoch<0 || epoch>4102444800.0) return ESP_FAIL;
    hourly=normalize_vc_hourly(config,root); daily=cJSON_CreateObject();
    if (!hourly||!daily) { cJSON_Delete(hourly); cJSON_Delete(daily); return ESP_ERR_NO_MEM; }
    cJSON *highs=cJSON_AddArrayToObject(daily,"temperature_2m_max");
    if (!highs) goto allocation_failure;
    for (int i=0;i<6;++i) if (!append_number(highs,number(field(at(days,i),"tempmax")))) goto allocation_failure;
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
  if (vc) {
    facts=weather_facts_from_visual_crossing(current,number(field(current,"snow"))*(metric(config)?1:2.54));
    out->code=-1; out->is_day=vc_is_day(root,current); out->observed_at=(time_t)number(field(current,"datetimeEpoch"));
    normalized_current=normalize_vc_current(config,root,current,facts);
    if (!normalized_current) goto allocation_failure;
    current=normalized_current;
  } else {
    double code=number(field(current,"weathercode"));
    out->code=isfinite(code)?rounded(code):-1;
    out->is_day=number(field(current,"is_day"))==1;
    /* New OM `current` supplies same-interval amounts. Older stored samples
     * lack these optional fields; never borrow an earlier hourly interval. */
    const cJSON *amounts=field(root,"current");
    facts=weather_facts_from_open_meteo(out->code,number(field(amounts,"rain")),number(field(amounts,"showers")),number(field(amounts,"snowfall")));
    const char *timestamp=string(field(current,"time"));
    int y,m,d,h,min;
    if (timestamp && sscanf(timestamp,"%d-%d-%dT%d:%d",&y,&m,&d,&h,&min)==5) {
      double offset=number(field(root,"utc_offset_seconds"));
      if (isfinite(offset)) out->observed_at=(time_t)(civil_seconds(y,m,d,h,min,0)-offset);
    }
  }
  if (!vc && !put_number((cJSON *)current,"condition_flags",facts.flags)) goto allocation_failure;
  weather_picture_t picture=weather_picture_for_facts(&facts);
  out->thundersnow=picture==WEATHER_PICTURE_THUNDERSNOW;
  snprintf(out->summary,sizeof(out->summary),"%s",weather_caption_for_facts(&facts,out->is_day));
  snprintf(out->icon_name,sizeof(out->icon_name),"%s",weather_icon_name_for_picture(picture,out->is_day));
#ifdef ESP_PLATFORM
  log_request_resources("decode");
#endif
  build_conditions(config,current,hourly,daily,out);
  out->valid=true; out->updated_at=time(NULL);
  if (vc) { cJSON_Delete(hourly); cJSON_Delete(daily); cJSON_Delete(normalized_current); }
  return ESP_OK;
allocation_failure:
  if (vc) { cJSON_Delete(hourly); cJSON_Delete(daily); cJSON_Delete(normalized_current); }
  return ESP_ERR_NO_MEM;
invalid:
  if (vc) { cJSON_Delete(hourly); cJSON_Delete(daily); }
  return ESP_FAIL;
}

/* Keep request scratch out of the decoding/Conditions call stack. The target
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
      "&elements=datetime,datetimeEpoch,tzoffset,temp,tempmax,tempmin,windspeed,windgust,winddir,pressure,precip,precipprob,preciptype,snow,cloudcover,conditions,icon,sunrise,sunset,sunriseEpoch,sunsetEpoch&key=",
      metric(config)?"metric":"us");
    if (length<0 || (size_t)length>=sizeof(request.vc.query)) return ESP_ERR_INVALID_SIZE;
    encode(config->weather_api_key,request.vc.query+length,sizeof(request.vc.query)-(size_t)length);
    http_config.host="weather.visualcrossing.com"; http_config.path=request.vc.path; http_config.query=request.vc.query;
    http_config.transport_type=HTTP_TRANSPORT_OVER_SSL;
  } else {
    encode(config->timezone,request.om.timezone,sizeof(request.om.timezone));
    snprintf(request.om.url,sizeof(request.om.url),"https://api.open-meteo.com/v1/forecast?latitude=%s&longitude=%s&current_weather=true"
      "&current=rain,showers,snowfall&hourly=weathercode,precipitation_probability,precipitation,rain,showers,snowfall,cloud_cover,pressure_msl,wind_speed_10m,wind_gusts_10m,temperature_2m"
      "&daily=temperature_2m_max,temperature_2m_min,weathercode&temperature_unit=%s&wind_speed_unit=kmh&timezone=%s&forecast_days=6",
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
  if (!request_mutex || !config->latitude[0] || !config->longitude[0] ||
      (config->weather_provider!=DEVICE_CONFIG_WEATHER_OPEN_METEO && config->weather_provider!=DEVICE_CONFIG_WEATHER_VISUAL_CROSSING)) return ESP_ERR_INVALID_STATE;
  bool vc=config->weather_provider==DEVICE_CONFIG_WEATHER_VISUAL_CROSSING;
  if (vc && !config->weather_api_key[0]) return ESP_ERR_INVALID_ARG;
  xSemaphoreTake(request_mutex,portMAX_DELAY);
  quota_record_t *record=record_for(config);
  if (record&&record->paused&&esp_timer_get_time()<record->next_us) {
    xSemaphoreGive(request_mutex); return ESP_ERR_INVALID_STATE;
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
#ifdef ESP_PLATFORM
  log_request_resources("cleanup");
#endif
  xSemaphoreGive(request_mutex); return err;
}

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
#include "location_scoring.h"
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
typedef struct {
  char key[DEVICE_CONFIG_WEATHER_KEY_LEN];
  bool paused;
  int64_t next_us;
} quota_record_t;
static quota_record_t quota[2];
static unsigned request_count;

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

/* The weather worker owns this bounded pipeline and all mutable image memory.
 * A single grayscale basemap cache preserves the dark reference's terrain/roads
 * without retaining another full RGBA map. Final slots are immutable until the
 * display acknowledges their publication. There is no persistent tile cache. */
#define RADAR_PIXELS ((size_t)APP_RADAR_WIDTH * APP_RADAR_HEIGHT)
#define RADAR_GROUND_M 120700.8
#define RADAR_ZOOM 7
#define RADAR_WORLD_PIXELS 32768.0
#define RADAR_WORLD_METERS 40075016.68557849
#define RADAR_BASE_RESPONSE_MAX (3 * 1024 * 1024)
/* LodePNG reserves another 260 bytes while inflating, then grows its
 * exact-size scanline allocation by half. Valid 800px RGBA needs about
 * 3.84 MB of scratch even though its decoded pixels occupy 2.56 MB. */
#define RADAR_PNG_ALLOC_MAX (4 * 1024 * 1024)
#define RADAR_PATH_LEN 192
#define RADAR_PI 3.14159265358979323846

typedef struct {
  double left, top, step; /* Native RainViewer zoom-7 world pixels. */
  int first_x, last_x, first_y, last_y;
} radar_geometry_t;

typedef enum { RADAR_IDLE, RADAR_METADATA, RADAR_BASE,
               RADAR_WAIT_SLOT, RADAR_ECHO } radar_phase_t;
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
  char path[RADAR_PATH_LEN];
  app_radar_snapshot_t published;
} radar;

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
static esp_err_t radar_request(const char *url, bool background,
                               http_response_t *response)
{
  *response=(http_response_t){.external=true,
    .maximum=background ? RADAR_BASE_RESPONSE_MAX : WEATHER_RESPONSE_MAX};
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
  unsigned expected_width=background ? APP_RADAR_WIDTH : 256;
  unsigned expected_height=background ? APP_RADAR_HEIGHT : 256;
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
 * Packed RGBA values retain source alpha; repeated colors use the first/lower
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
static bool radar_map_pixel(uint8_t rgba[4])
{
  if (!rgba[3]) return true; /* A transparent valid tile means no echoes. */
  uint32_t source=((uint32_t)rgba[0]<<24)|((uint32_t)rgba[1]<<16)|((uint32_t)rgba[2]<<8)|rgba[3];
  for (unsigned i=0; i<128; ++i) if (radar_source_rgba[i] == source) {
    radar_color((int)i-32, rgba);
    return true;
  }
  return false; /* A changed/unknown provider palette must not imply strength. */
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
static esp_err_t radar_tile(void)
{
  const radar_geometry_t *geometry=&radar.geometry;
  char url[384];
  snprintf(url,sizeof(url),"https://tilecache.rainviewer.com%s/256/7/%d/%d/2/0_0.png",radar.path,radar_wrapped_x(radar.tile_x),radar.tile_y);
  http_response_t response;
  esp_err_t err=radar_request(url,false,&response);
  lv_draw_buf_t *tile=NULL;
  if (err==ESP_OK) err=radar_decode_png(&response,false,&tile);
  free(response.data);
  if (err!=ESP_OK) return err;
  /* Recolor original unsmoothed pixels before any output sampling. */
  for (unsigned y=0; y<256 && err==ESP_OK; ++y)
    for (unsigned x=0; x<256; ++x)
      if (!radar_map_pixel(tile->data+y*tile->header.stride+x*4)) {
        err=ESP_ERR_INVALID_RESPONSE;
        break;
      }
  if (err==ESP_OK) {
    /* The horizontal sample is identical on every row of this tile. */
    for (int x=0; x<APP_RADAR_WIDTH; ++x) {
      int source_x=(int)floor(geometry->left+(x+0.5)*geometry->step)-radar.tile_x*256;
      radar.source_x[x]=source_x<0 || source_x>=256 ? -1 : (int16_t)source_x;
    }
    for (int y=0; y<APP_RADAR_HEIGHT; ++y) {
      int source_y=(int)floor(geometry->top+(y+0.5)*geometry->step)-radar.tile_y*256;
      if (source_y<0 || source_y>=256) continue;
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
  return err;
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
static bool radar_metadata(bool *unchanged)
{
  http_response_t response;
  esp_err_t err=radar_request("https://api.rainviewer.com/public/weather-maps.json",false,&response);
  cJSON *root=err==ESP_OK ? cJSON_Parse(response.data) : NULL;
  const char *host=string(field(root,"host"));
  const cJSON *frames=field(field(root,"radar"),"past"), *latest=NULL;
  double latest_time=0;
  if (host && strcmp(host,"https://tilecache.rainviewer.com")==0 && cJSON_IsArray(frames)) {
    for (int i=0; i<cJSON_GetArraySize(frames); ++i) {
      const cJSON *frame=at(frames,i);
      double when=number(field(frame,"time"));
      const char *path=string(field(frame,"path"));
      bool safe=path && path[0]=='/' && strlen(path)<sizeof(radar.path) && !strstr(path,"..");
      for (const char *p=path; safe && *p; ++p)
        if (!isalnum((unsigned char)*p) && !strchr("/-_.",*p)) safe=false;
      if (safe && when>latest_time && when<=INT64_C(4102444800) && floor(when)==when) {
        latest=frame;
        latest_time=when;
      }
    }
  }
  bool ok=latest!=NULL;
  /* An older/cached provider list cannot confirm the displayed frame. */
  if (radar.published.valid && latest_time<radar.published.frame_time) ok=false;
  *unchanged=ok && radar.published.valid && latest_time==radar.published.frame_time;
  if (ok && !*unchanged) {
    radar.candidate_time=(time_t)latest_time;
    snprintf(radar.path,sizeof(radar.path),"%s",string(field(latest,"path")));
  }
  cJSON_Delete(root);
  free(response.data);
  return ok;
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
static bool radar_publish(app_radar_snapshot_t *out)
{
  if (++radar.published.generation==0) ++radar.published.generation;
  *out=radar.published;
  return true;
}
static bool radar_failure(app_radar_snapshot_t *out, const char *stage)
{
  ESP_LOGW(TAG,"Radar %s failed; retaining the last same-location image",stage);
  radar.phase=RADAR_IDLE;
  radar.published.stale=true;
  return radar_publish(out);
}
bool weather_client_radar_step(const device_config_t *config, weather_radar_step_t action,
                               uint32_t displayed_generation,
                               app_radar_snapshot_t *out, bool *active)
{
  if (!active) return false;
  *active=false;
  if (!config || !out || !request_gate || !radar.images_initialized) return false;
  double latitude=0, longitude=0;
  bool location=config->location_ready && location_scoring_label_has_country(config->location,"US") &&
    radar_coordinate(config->latitude,85.05112878,&latitude) &&
    radar_coordinate(config->longitude,180,&longitude);
  bool changed=location!=radar.location_set ||
    (location && (latitude!=radar.latitude || longitude!=radar.longitude));
  if (changed) {
    radar.location_set=location;
    radar.latitude=latitude; radar.longitude=longitude;
    radar.base_ready=false;
    uint32_t generation=radar.published.generation;
    radar.published=(app_radar_snapshot_t){.generation=generation,
      .latitude=latitude,.longitude=longitude};
    radar.phase=location && action!=WEATHER_RADAR_OFFLINE ? RADAR_METADATA : RADAR_IDLE;
    *active=radar.phase!=RADAR_IDLE;
    /* Publish invalidation before touching an old displayed/pending slot. */
    return radar_publish(out);
  }
  if (!location) return false;
  if (action==WEATHER_RADAR_OFFLINE) {
    radar.phase=RADAR_IDLE;
    if (radar.published.stale) return false;
    radar.published.stale=true;
    return radar_publish(out);
  }
  if (radar.phase==RADAR_IDLE) {
    if (action!=WEATHER_RADAR_CHECK) return false;
    radar.phase=RADAR_METADATA;
  }
  *active=true;
  if (radar.phase==RADAR_METADATA) {
    bool unchanged=false;
    if (!radar_metadata(&unchanged)) {
      *active=false;
      return radar_failure(out,"metadata");
    }
    if (unchanged) {
      radar.phase=RADAR_IDLE;
      *active=false;
      if (!radar.published.stale) return false;
      radar.published.stale=false;
      return radar_publish(out);
    }
    if (!radar_geometry(latitude,longitude,&radar.geometry)) {
      *active=false;
      return radar_failure(out,"coverage");
    }
    radar.phase=radar.base_ready ? RADAR_WAIT_SLOT : RADAR_BASE;
    return false;
  }
  if (radar.phase==RADAR_BASE) {
    if (!radar.base) radar.base=radar_alloc(RADAR_PIXELS);
    if (!radar.base || radar_background()!=ESP_OK) {
      *active=false;
      return radar_failure(out,"background image");
    }
    radar.base_ready=true;
    radar.phase=RADAR_WAIT_SLOT;
    return false;
  }
  if (radar.phase==RADAR_WAIT_SLOT) {
    if (displayed_generation!=radar.published.generation) return false;
    radar.slot=radar.published.pixels==radar.image[0] ? 1 : 0;
    if (!radar.image[radar.slot]) radar.image[radar.slot]=radar_alloc(RADAR_PIXELS*4);
    if (!radar.image[radar.slot]) {
      *active=false;
      return radar_failure(out,"image allocation");
    }
    radar_prepare_image();
    radar_first_tile(&radar.geometry);
    radar.phase=RADAR_ECHO;
    return false;
  }
  if (radar_tile()!=ESP_OK) {
    *active=false;
    return radar_failure(out,"echo tile");
  }
  if (radar_next_tile(&radar.geometry)) return false;
  radar.published.valid=true;
  radar.published.stale=false;
  radar.published.frame_time=radar.candidate_time;
  radar.published.latitude=latitude;
  radar.published.longitude=longitude;
  radar.published.pixels=radar.image[radar.slot];
  radar.phase=RADAR_IDLE;
  *active=false;
  return radar_publish(out);
}

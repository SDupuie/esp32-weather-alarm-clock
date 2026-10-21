/* Runs the ESP32-P4 C forecast calculation against the JSON cases in
 * tests/data/forecast-representative-cases.json. The fixture cases
 * provide inputs and expected representative pictures; this test executes the
 * shared C implementation without an ESP-IDF toolchain.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "forecast_representative.h"

static char *read_file(const char *path)
{
  FILE *file = fopen(path, "rb");
  if (file == NULL) {
    fprintf(stderr, "Could not open %s\n", path);
    return NULL;
  }

  fseek(file, 0, SEEK_END);
  long size = ftell(file);
  fseek(file, 0, SEEK_SET);

  char *buffer = malloc((size_t) size + 1);
  if (buffer == NULL) {
    fclose(file);
    return NULL;
  }

  size_t read = fread(buffer, 1, (size_t) size, file);
  buffer[read] = '\0';
  fclose(file);
  return buffer;
}

static int get_int(const cJSON *object, const char *key, int fallback)
{
  cJSON *value = cJSON_GetObjectItemCaseSensitive(object, key);
  return cJSON_IsNumber(value) ? (int) cJSON_GetNumberValue(value) : fallback;
}


/* These are constructed mapping inputs, not records observed from the API. */
static int check_mapping(const char *label, weather_condition_facts_t f,
  const char *icon, const char *caption, uint32_t present, uint32_t absent)
{
  const char *actual_icon = weather_icon_name_for_picture(weather_picture_for_facts(&f), true);
  const char *actual_caption = weather_caption_for_facts(&f, true);
  if (strcmp(actual_icon, icon) || (caption && strcmp(actual_caption, caption))
      || (f.flags & present) != present || (f.flags & absent)) {
    printf("FAIL mapping %s: icon %s caption %s flags %u\n", label, actual_icon, actual_caption, f.flags);
    return 1;
  }
  return 0;
}

static int mapping_checks(void)
{
  int failures = 0;
  const char *native_icons[43] = {
    "snow", "rain", "rain", "rain", "rain", "rain", "fog", "fog",
    "freezing-rain", "freezing-rain", "freezing-rain", "fog", "freezing-rain", "freezing-rain",
    "tornado", "hail", "ice", "thunderstorm", "fog", "", "rain", "rain-snow", "rain-snow",
    "showers-day", "rain", "rain", "", "", "", "fog", "snow", "rain-snow", "snow", "snow",
    "snow", "wind", "thunderstorm", "thunderstorm", "snow", "hail", "cloudy", "partlycloudy-day", "clear-day"
  };
  const char *native_captions[43] = {
    "Blowing / Drifting Snow", "Drizzle", "Heavy Drizzle", "Light Drizzle",
    "Heavy Drizzle & Rain", "Light Drizzle & Rain", "Dust Storm", "Fog",
    "Freezing Precipitation", "Heavy Freezing Precipitation", "Light Freezing Precipitation",
    "Freezing Fog", "Heavy Freezing Rain", "Light Freezing Rain", "Funnel Cloud / Tornado",
    "Hail Showers", "Ice", "Lightning", "Mist", "Unknown", "Rain", "Heavy Rain & Snow",
    "Light Rain & Snow", "Showers", "Heavy Rain", "Light Rain", "Unknown", "Unknown", "Unknown",
    "Smoke / Haze", "Snow", "Rain & Snow Showers", "Snow Showers", "Heavy Snow", "Light Snow",
    "Squalls", "Thunderstorm", "Thunderstorm", "Diamond Dust / Ice Crystals", "Hail", "Cloudy", "Partly Cloudy", "Clear"
  };
  for (int i = 1; i <= 43; ++i) {
    char json[80], label[24];
    snprintf(json, sizeof(json), "{\"conditions\":\" type_%d \"}", i);
    snprintf(label, sizeof(label), "VC type_%d", i);
    cJSON *record = cJSON_Parse(json);
    failures += check_mapping(label, weather_facts_from_visual_crossing(record, NAN), native_icons[i-1], native_captions[i-1], 0, 0);
    cJSON_Delete(record);
  }
  const char *vc_icons[16] = {"clear-day", "clear-night", "partly-cloudy-day", "partly-cloudy-night",
    "cloudy", "fog", "wind", "rain", "showers-day", "showers-night", "snow", "snow-showers-day",
    "snow-showers-night", "thunder-rain", "thunder-showers-day", "thunder-showers-night"};
  const char *expected_icons[16] = {"clear-day", "clear-day", "partlycloudy-day", "partlycloudy-day",
    "cloudy", "fog", "wind", "rain", "showers-day", "showers-day", "snow", "snow", "snow",
    "thunderstorm", "thunderstorm", "thunderstorm"};
  for (int i = 0; i < 16; ++i) {
    cJSON *record = cJSON_CreateObject(); cJSON_AddStringToObject(record, "icon", vc_icons[i]);
    failures += check_mapping(vc_icons[i], weather_facts_from_visual_crossing(record, NAN), expected_icons[i], NULL, 0, 0);
    cJSON_Delete(record);
  }
  const struct { int code; const char *icon; } om[] = {
    {0,"clear-day"},{1,"partlycloudy-day"},{2,"partlycloudy-day"},{3,"cloudy"},
    {45,"fog"},{48,"fog"},{51,"rain"},{53,"rain"},{55,"rain"},
    {56,"freezing-rain"},{57,"freezing-rain"},{61,"rain"},{63,"rain"},{65,"rain"},
    {66,"freezing-rain"},{67,"freezing-rain"},{71,"snow"},{73,"snow"},{75,"snow"},
    {77,"snow"},{80,"showers-day"},{81,"showers-day"},{82,"showers-day"},{85,"snow"},{86,"snow"},
    {95,"thunderstorm"},{96,"hail"},{97,"thunderstorm"},{99,"hail"},{999,""},{-1,""}
  };
  for (size_t i = 0; i < sizeof(om)/sizeof(om[0]); ++i) {
    char label[20]; snprintf(label,sizeof(label),"OM %d",om[i].code);
    failures += check_mapping(label, weather_facts_from_open_meteo(om[i].code,NAN,NAN,NAN),om[i].icon,NULL,0,0);
  }
  const struct { const char *json, *icon, *caption; uint32_t present, absent; } compounds[] = {
    {"{\"conditions\":\" type_40 , type_38, type_40 \"}","hail","Hail & Thunder",WEATHER_FACT_HAIL|WEATHER_FACT_THUNDER,WEATHER_FACT_LIQUID},
    {"{\"conditions\":\"type_15,type_40,type_13,type_37,type_31\"}","tornado","Funnel Cloud / Tornado",WEATHER_FACT_HAIL|WEATHER_FACT_THUNDER|WEATHER_FACT_FREEZING|WEATHER_FACT_FALLING_SNOW,0},
    {"{\"conditions\":\"type_40,type_13,type_37,type_31\"}","hail","Hail & Thunder",WEATHER_FACT_FREEZING|WEATHER_FACT_THUNDER|WEATHER_FACT_FALLING_SNOW,0},
    {"{\"conditions\":\"type_13,type_37,type_31\"}","freezing-rain","Heavy Freezing Rain",WEATHER_FACT_THUNDER|WEATHER_FACT_FALLING_SNOW,0},
    {"{\"conditions\":\"type_37,type_31\"}","thundersnow","Thundersnow",WEATHER_FACT_THUNDER|WEATHER_FACT_FALLING_SNOW,0},
    {"{\"conditions\":\"type_37,type_1\"}","thunderstorm","Thunderstorm",WEATHER_FACT_THUNDER|WEATHER_FACT_SNOW,WEATHER_FACT_FALLING_SNOW},
    {"{\"conditions\":\"type_18\"}","thunderstorm","Lightning",WEATHER_FACT_LIGHTNING,WEATHER_FACT_THUNDER|WEATHER_FACT_LIQUID},
    {"{\"conditions\":\"type_38\",\"icon\":\"thunder-rain\"}","thunderstorm","Thunderstorm",WEATHER_FACT_THUNDER,WEATHER_FACT_LIQUID},
    {"{\"conditions\":\"type_21,type_31\"}","rain-snow","Rain & Snow",WEATHER_FACT_MIXED,0},
    {"{\"conditions\":\"type_21,type_1\",\"icon\":\"snow\"}","snow","Blowing / Drifting Snow",WEATHER_FACT_LIQUID|WEATHER_FACT_SNOW,WEATHER_FACT_MIXED|WEATHER_FACT_FALLING_SNOW},
    {"{\"conditions\":\"type_39\",\"icon\":\"snow\"}","snow","Diamond Dust / Ice Crystals",WEATHER_FACT_SNOW,WEATHER_FACT_FALLING_SNOW},
    {"{\"conditions\":\"type_17\",\"icon\":\"rain\",\"preciptype\":[\"ice\"]}","ice","Ice",WEATHER_FACT_ICE,WEATHER_FACT_FREEZING|WEATHER_FACT_HAIL|WEATHER_FACT_SNOW|WEATHER_FACT_LIQUID},
    {"{\"preciptype\":[\"ice\",\"rain\",\"snow\",\"freezingrain\"]}","freezing-rain","Freezing Rain",WEATHER_FACT_ICE|WEATHER_FACT_LIQUID|WEATHER_FACT_FALLING_SNOW|WEATHER_FACT_MIXED,WEATHER_FACT_HAIL|WEATHER_FACT_THUNDER},
    {"{\"conditions\":\"type_20,type_27,type_999\",\"icon\":\"cloudy\"}","cloudy","Cloudy",0,WEATHER_FACT_LIQUID},
    {"{\"conditions\":\"type_999\",\"icon\":\"new\",\"preciptype\":[\"new\"]}","","Unknown",0,UINT32_MAX},
    {"{\"conditions\":\"type_30,type_36\"}","fog","Smoke / Haze",WEATHER_FACT_WIND|WEATHER_FACT_FOG,0},
    {"{\"conditions\":\"type_17,type_31\"}","snow","Snow",WEATHER_FACT_ICE|WEATHER_FACT_FALLING_SNOW,0},
    {"{\"conditions\":\"type_27\",\"cloudcover\":80}","cloudy","Cloudy",0,0}
  };
  for (size_t i = 0; i < sizeof(compounds)/sizeof(compounds[0]); ++i) {
    cJSON *record = cJSON_Parse(compounds[i].json);
    failures += check_mapping(compounds[i].json,weather_facts_from_visual_crossing(record,NAN),compounds[i].icon,compounds[i].caption,compounds[i].present,compounds[i].absent);
    cJSON_Delete(record);
  }
  failures += check_mapping("OM quantitative mixed",weather_facts_from_open_meteo(61,.1,0,.1),"rain-snow","Rain & Snow",WEATHER_FACT_MIXED,0);
  failures += check_mapping("OM snow grains do not mean falling snow",weather_facts_from_open_meteo(77,.1,0,0),"snow","Snow Grains",WEATHER_FACT_SNOW,WEATHER_FACT_MIXED|WEATHER_FACT_FALLING_SNOW);
  failures += check_mapping("OM thunder snow same interval",weather_facts_from_open_meteo(97,0,0,.1),"thundersnow","Thundersnow",WEATHER_FACT_THUNDER,0);
  failures += check_mapping("OM hail retains thunder",weather_facts_from_open_meteo(99,0,0,.1),"hail","Hail & Thunder",WEATHER_FACT_THUNDER|WEATHER_FACT_FALLING_SNOW,0);
  weather_condition_facts_t mostly = weather_facts_from_open_meteo(1,NAN,NAN,NAN);
  if (strcmp(weather_caption_for_facts(&mostly,false),"Mostly Clear") || strcmp(weather_caption_for_facts(&mostly,true),"Mostly Sunny")) failures++;
  if (strcmp(weather_icon_name_for_picture(WEATHER_PICTURE_CLEAR,false),"clear-night") || strcmp(weather_icon_name_for_picture(WEATHER_PICTURE_SHOWERS,false),"showers-night") || strcmp(weather_icon_name_for_picture(WEATHER_PICTURE_PARTLY,false),"partlycloudy-night")) failures++;
  if (weather_icon_name_is_known("unknown") || strcmp(weather_icon_name_or_fallback("unknown"),"") || !weather_icon_name_is_known("HAIL")) failures++;
  printf("Mapping checks: %d failures (constructed cases)\n",failures);
  return failures;
}

int main(int argc, char **argv)
{
  const char *fixture_path = argc > 1 ? argv[1] : "tests/data/forecast-representative-cases.json";

  char *json_text = read_file(fixture_path);
  if (json_text == NULL) {
    return 1;
  }

  cJSON *cases = cJSON_Parse(json_text);
  free(json_text);
  if (!cJSON_IsArray(cases)) {
    fprintf(stderr, "Fixture file did not parse as a JSON array: %s\n", fixture_path);
    return 1;
  }

  int failures = mapping_checks();
  int total = 0;
  cJSON *test_case = NULL;

  cJSON_ArrayForEach(test_case, cases) {
    total++;
    const char *name = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(test_case, "name"));
    const char *date = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(test_case, "date"));
    cJSON *hourly = cJSON_GetObjectItemCaseSensitive(test_case, "hourly");
    cJSON *daily = cJSON_GetObjectItemCaseSensitive(test_case, "dailyFacts");
    weather_condition_facts_t facts = {.flags = (uint32_t)get_int(daily,"flags",0), .sky = (weather_sky_t)get_int(daily,"sky",0)};
    double daily_high = cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(test_case, "dailyHigh"));
    double daily_low = cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(test_case, "dailyLow"));
    int snow_threshold = get_int(test_case, "snowTempThreshold", 34);
    const char *expected = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(test_case, "expectedPicture"));
    weather_picture_t actual = forecast_pick_representative_picture(
      hourly, date, &facts, daily_high, daily_low, snow_threshold);
    const char *actual_icon = weather_icon_name_for_picture(actual, true);
    if (expected && strcmp(actual_icon, expected) == 0) {
      printf("PASS %s: %s\n", name ? name : "(unnamed)", actual_icon);
    } else {
      printf("FAIL %s: expected %s, got %s\n", name ? name : "(unnamed)", expected ? expected : "(missing)", actual_icon);
      failures++;
    }
  }

  cJSON_Delete(cases);

  printf("%d/%d forecast cases passed\n", total - failures, total);
  return failures > 0 ? 1 : 0;
}

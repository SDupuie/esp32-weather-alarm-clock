/* Native checks for the retained storm indicator. Fixtures exercise only its
 * boolean decision; constructed boundaries below are not live observations. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "storm_conditions.h"
#include "weather_icon_names.h"

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

static void check_boundaries(void)
{
  cJSON *current = cJSON_Parse("{\"time\":\"2026-03-14T12:30\"}");
  cJSON *hourly = cJSON_Parse("{\"time\":[\"2026-03-14T12:00\",\"2026-03-14T13:00\",\"2026-03-14T14:00\",\"2026-03-14T15:00\",\"2026-03-14T16:00\"]}");
  assert(current && hourly);
  const char *fields[] = {"wind_gusts_10m", "wind_speed_10m", "precipitation"};
  for (int metric = 0; metric < 2; ++metric) {
    double limits[] = {metric ? 64.0 : 40.0 * 1.609344,
                       metric ? 40.0 : 25.0 * 1.609344, 7.6};
    for (int f = 0; f < 3; ++f) {
      cJSON *values = cJSON_AddArrayToObject(hourly, fields[f]); assert(values);
      for (int i = 0; i < 5; ++i) cJSON_AddItemToArray(values, cJSON_CreateNumber(0));
      for (int i = 0; i < 5; ++i) {
        assert(cJSON_ReplaceItemInArray(values, i, cJSON_CreateNumber(limits[f])));
        assert(storm_conditions_alert(metric, current, hourly) == (i <= 3));
        assert(cJSON_ReplaceItemInArray(values, i, cJSON_CreateNumber(nextafter(limits[f], 0))));
        assert(!storm_conditions_alert(metric, current, hourly));
        cJSON_GetArrayItem(values, i)->valuedouble = NAN;
        assert(!storm_conditions_alert(metric, current, hourly));
        cJSON_GetArrayItem(values, i)->valuedouble = INFINITY;
        assert(!storm_conditions_alert(metric, current, hourly));
        assert(cJSON_ReplaceItemInArray(values, i, cJSON_CreateNumber(0)));
      }
      cJSON_DeleteItemFromObjectCaseSensitive(hourly, fields[f]);
    }
    cJSON *speed = cJSON_AddNumberToObject(current, "windspeed", limits[1]); assert(speed);
    assert(storm_conditions_alert(metric, current, hourly));
    cJSON_SetNumberValue(speed, nextafter(limits[1], 0));
    assert(!storm_conditions_alert(metric, current, hourly));
    speed->valuedouble = NAN;
    assert(!storm_conditions_alert(metric, current, hourly));
    speed->valuedouble = INFINITY;
    assert(!storm_conditions_alert(metric, current, hourly));
    cJSON_DeleteItemFromObjectCaseSensitive(current, "windspeed");
  }
  cJSON *flags = cJSON_AddNumberToObject(current, "condition_flags", WEATHER_FACT_THUNDER); assert(flags);
  assert(storm_conditions_alert(false, current, hourly));
  flags->valuedouble = INFINITY;
  assert(!storm_conditions_alert(false, current, hourly));
  flags->valuedouble = NAN;
  assert(!storm_conditions_alert(false, current, hourly));
  cJSON_Delete(hourly); cJSON_Delete(current);
  puts("PASS constructed inclusive unit thresholds, now through next three hours, current wind and nonfinite data");
}

int main(int argc, char **argv)
{
  const char *path = argc > 1 ? argv[1] : "shared/test-data/storm-conditions-cases.json";
  char *text = read_file(path);
  if (!text) return 1;
  cJSON *cases = cJSON_Parse(text); free(text);
  if (!cJSON_IsArray(cases)) { cJSON_Delete(cases); return 1; }
  int total = 0, failures = 0;
  const cJSON *test_case = NULL;
  cJSON_ArrayForEach(test_case, cases) {
    const cJSON *cfg = cJSON_GetObjectItemCaseSensitive(test_case, "cfg");
    const char *units = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(cfg, "units"));
    const char *name = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(test_case, "name"));
    const cJSON *expected = cJSON_GetObjectItemCaseSensitive(test_case, "expected");
    bool actual = storm_conditions_alert(units && strcmp(units, "metric") == 0,
      cJSON_GetObjectItemCaseSensitive(test_case, "current"),
      cJSON_GetObjectItemCaseSensitive(test_case, "hourly"));
    bool ok = cJSON_IsBool(expected) && actual == cJSON_IsTrue(expected);
    printf("%s %s\n", ok ? "PASS" : "FAIL", name ? name : "(unnamed)");
    ++total; if (!ok) ++failures;
  }
  cJSON_Delete(cases);
  check_boundaries();
  printf("%d/%d storm-alert cases passed\n", total - failures, total);
  return failures ? 1 : 0;
}

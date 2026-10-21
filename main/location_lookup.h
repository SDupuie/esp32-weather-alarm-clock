#pragma once

#include "esp_err.h"

#include "device_config.h"

typedef struct {
  char location[DEVICE_CONFIG_STR_LEN];
  char timezone[DEVICE_CONFIG_STR_LEN];
  char latitude[DEVICE_CONFIG_COORD_LEN];
  char longitude[DEVICE_CONFIG_COORD_LEN];
} location_lookup_result_t;

/* Blocking provider lookup; use background work. The returned strings belong
 * to the caller and are not saved here. Settings validation owns whether these
 * coordinates and timezone can be applied to the clock. */
esp_err_t location_lookup_resolve(const char *query, location_lookup_result_t *out_result);

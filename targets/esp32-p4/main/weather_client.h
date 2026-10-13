#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "app_runtime.h"
#include "device_config.h"

/* Initialize once before starting workers/settings tasks. Requests share one
 * mutex and two RAM-only quota records: the applied key and latest draft key. */
esp_err_t weather_client_init(void);
/* Monotonic microseconds; zero means no quota pause. Scheduling remains in main. */
int64_t weather_client_next_allowed_us(const device_config_t *config);
esp_err_t weather_client_fetch(const device_config_t *config,
                              app_weather_snapshot_t *out_snapshot);

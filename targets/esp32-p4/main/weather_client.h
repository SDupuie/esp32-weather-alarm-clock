#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "app_runtime.h"
#include "device_config.h"

/* Initialize once before starting workers/settings tasks. Requests share one
 * binary request gate and two RAM-only quota records: the applied key and latest draft key. */
esp_err_t weather_client_init(void);
/* Monotonic microseconds; zero means no quota pause. Scheduling remains in main. */
int64_t weather_client_next_allowed_us(const device_config_t *config);
esp_err_t weather_client_fetch(const device_config_t *config,
                              app_weather_snapshot_t *out_snapshot);

/* Call once after lv_init, while holding the initial display lock. Only the
 * PNG image allocator is changed; display/layer/font buffers keep their handlers. */
esp_err_t weather_client_init_images(void);
typedef enum {
  WEATHER_RADAR_CONTINUE = 0,
  WEATHER_RADAR_CHECK = 1,
  WEATHER_RADAR_OFFLINE = 2,
} weather_radar_step_t;
/* Weather-worker owned. At most one bounded request/tile decode per call. The
 * UI acknowledges the latest generation before either final image can be reused.
 * OFFLINE cancels preparation and marks retained same-location data old without
 * network access. true means replace the published snapshot; active requests
 * another short step. */
bool weather_client_radar_step(const device_config_t *config, weather_radar_step_t action,
                               uint32_t displayed_generation,
                               app_radar_snapshot_t *out, bool *active);

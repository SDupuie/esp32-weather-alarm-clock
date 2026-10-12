#pragma once

#include <stdbool.h>

#include "clock_time.h"
#include "device_config.h"
#include "esp_err.h"
#include "solar_times.h"

/* Initialize before tasks. Storage errors still leave the selected appearance
 * usable. */
esp_err_t wac_night_init(const device_config_t *config, const wac_time_snapshot_t *clock);
/* Single ongoing writer: the clock-control task, outside the display lock.
 * Failed saves still publish the new appearance, without tick-based retries. */
esp_err_t wac_night_update(const device_config_t *config, const wac_time_snapshot_t *clock);
/* Short copied-state read, safe while the writer performs storage or BSP work. */
bool wac_night_active(void);
/* Copied raw events for today's sunset and tomorrow's sunrise, before saved
 * offsets. Unavailable inputs/events clear the copy without changing appearance. */
void wac_night_solar_times(solar_times_t *out);
/* Signal only, after BSP display initialization and the initial draw.
 * Subsequent clock-control updates perform the gradual backlight transition. */
void wac_night_start_backlight(void);

#pragma once

#include <stdbool.h>

#include "clock_time.h"
#include "device_config.h"

/* Called serially while assembling startup and heartbeat snapshots, before the
 * display lock. Retains the current appearance when Fixed lacks usable time;
 * initial appearance is Day. Solar, persistence and backlight work are pending. */
bool wac_night_update(const device_config_t *config, const wac_time_snapshot_t *clock);

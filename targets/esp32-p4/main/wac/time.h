#pragma once

#include "esp_err.h"

/* Empty timezone is permitted before first-use location setup and uses UTC. */
esp_err_t wac_time_apply_timezone(const char *timezone);

#pragma once

#include <stdbool.h>
#include <time.h>

#include "esp_err.h"

/* Called serially by the time owner; the BSP owns the shared I2C bus.
 * Initializes one device handle and enables battery-backed oscillation without
 * clearing the oscillator stop flag. An access error leaves the handle usable
 * for a subsequent read, so lost-time evidence can still be inspected. */
esp_err_t wac_ds3231_init(void);

/* Reads UTC in 2000–2099, accepting the hardware's 12- or 24-hour format.
 * Returns ESP_ERR_INVALID_RESPONSE for malformed or unsupported calendar data.
 * Sets oscillator_stopped before calendar validation when the register read
 * succeeds, including when calendar validation fails. On an access failure it
 * is false (no stop flag was observed). utc changes only on ESP_OK. */
esp_err_t wac_ds3231_read(time_t *utc, bool *oscillator_stopped);

/* Writes UTC in 2000–2099 using 24-hour format. Enables battery oscillation,
 * writes the complete calendar, then clears the hardware stop flag. A failed
 * calendar write never attempts to clear the flag. Returns any access error,
 * including a failure to clear the flag after a successful calendar write. */
esp_err_t wac_ds3231_write(time_t utc);

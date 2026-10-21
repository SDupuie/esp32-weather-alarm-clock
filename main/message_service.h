#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#include "app_runtime.h"
#include "device_config.h"

esp_err_t message_service_init(const device_config_t *config);
/* Copies local records or the worker's shared-client cache; performs no HTTP.
 * An empty cache is a successful snapshot with no available message. */
esp_err_t message_service_get_snapshot(
  const char *device_id,
  app_message_snapshot_t *out_snapshot,
  uint32_t *out_unread_count,
  bool *out_has_important
);
// Copies IDs for the existing worker. An occupied slot returns ESP_ERR_TIMEOUT.
esp_err_t message_service_submit_acknowledgment(
  const char *device_id,
  const char *message_id
);
// Completion remains available, and blocks another submission, until consumed.
bool message_service_take_acknowledgment_result(esp_err_t *out_result);

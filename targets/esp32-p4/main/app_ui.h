#pragma once

#include <stdbool.h>

#include "esp_err.h"

#include "app_runtime.h"
#include "device_config.h"

esp_err_t app_ui_show_boot_screen(const device_config_t *config,
                                  const app_runtime_state_t *runtime, bool touch_ready);
/* Register a nonblocking command submission callback before UI startup. */
void app_ui_set_clock_warning_dismiss_handler(esp_err_t (*handler)(wac_time_warning_t warning));
esp_err_t app_ui_update_runtime(
  const device_config_t *config,
  const app_runtime_state_t *runtime
);
/* Deliver copied worker completion while holding the display lock. */
void app_ui_complete_message_acknowledgment(esp_err_t result);

void app_ui_show_storage_error(esp_err_t error);

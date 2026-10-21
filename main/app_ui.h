#pragma once

#include <stdbool.h>

#include "esp_err.h"

#include "app_runtime.h"
#include "device_config.h"

esp_err_t app_ui_show_boot_screen(const device_config_t *config,
                                  const app_runtime_state_t *runtime, bool touch_ready);
/* Register a nonblocking command submission callback before UI startup. */
void app_ui_set_clock_warning_dismiss_handler(esp_err_t (*handler)(wac_time_warning_t warning));
/* Submit a copied command to the weather worker; never perform network I/O. */
void app_ui_set_radar_action_handler(esp_err_t (*handler)(app_radar_action_t action,
  time_t displayed_frame_time));
/* Under the display lock, acknowledge only the publication actually consumed. */
uint32_t app_ui_radar_displayed_generation(void);
typedef enum {
  APP_ALARM_SNOOZE, APP_ALARM_DISMISS, APP_ALARM_DISMISS_MISSED
} app_alarm_action_t;
/* Submission copies the displayed occurrence; one alarm action remains pending
 * until its copied completion is delivered. It never performs storage I/O. */
void app_ui_set_alarm_action_handler(esp_err_t (*handler)(app_alarm_action_t action,
  const wac_alarm_occurrence_t *occurrence));
void app_ui_complete_alarm_action(app_alarm_action_t action,
  const wac_alarm_occurrence_t *occurrence, esp_err_t result);
esp_err_t app_ui_update_runtime(
  const device_config_t *config,
  const app_runtime_state_t *runtime
);
/* Deliver copied worker completion while holding the display lock. */
void app_ui_complete_message_acknowledgment(esp_err_t result);

void app_ui_show_storage_error(esp_err_t error);

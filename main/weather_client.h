#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
#include "app_runtime.h"
#include "device_config.h"

/* Initialize once before starting workers/settings tasks. Requests share one
 * binary request gate and two RAM-only quota records: the applied key and latest draft key. */
esp_err_t weather_client_init(void);
/* Raw request for a caller, including unsaved Settings validation. It does not
 * replace applied weather or its trusted download timestamp. */
esp_err_t weather_client_fetch(const device_config_t *config,
                              app_weather_snapshot_t *out_snapshot);

/* Call once after lv_init, while holding the initial display lock. Only the
 * PNG image allocator is changed; display/layer/font buffers keep their handlers. */
esp_err_t weather_client_init_images(void);
/* Copies the newest UI command without waiting for HTTP or image processing.
 * PAUSE identifies the actual displayed frame, including a pending replacement. */
esp_err_t weather_client_radar_control(app_radar_action_t action,time_t displayed_frame_time);
/* The existing weather worker owns refresh deadlines and retained results.
 * main creates it with the UI task handle as its argument. */
void weather_client_task(void *ui_task);
/* Short finite copy. Failure leaves outputs unchanged. Copying is not display
 * acceptance; acknowledge only after LVGL has safely replaced/detached pixels. */
bool weather_client_copy(app_weather_snapshot_t *weather, app_radar_snapshot_t *radar);
void weather_client_radar_acknowledge(uint32_t generation);

/* One catalog for parsing, ordering and settings. Stable indices: 23 watches
 * alphabetically, followed by 32 warnings alphabetically. bit is within family. */
typedef struct {
  const char *name;
  bool warning;
  uint8_t bit;
  uint16_t priority;
} weather_alert_type_t;
const weather_alert_type_t *weather_client_alert_catalog(size_t *count);
typedef struct {
  uint32_t revision;
  uint64_t id;
  uint8_t type;
  time_t issued, cutoff;
} weather_alert_row_t;
typedef struct {
  weather_alert_row_t row;
  size_t area_offset, headline_offset, description_offset, instruction_offset;
} weather_alert_detail_t;
/* Init reads retained records, with ordinary fresh-check fallback for missing
 * or unusable alert data. All subsequent slow work belongs to the weather worker. */
esp_err_t weather_client_alerts_init(const device_config_t *config);
/* Immediate short operation, including when HTTP/storage is blocked. A changed
 * saved location invalidates old results and records before their next use. */
bool weather_client_alerts_sync_location(const device_config_t *config);
void weather_client_alerts_copy(app_alert_snapshot_t *out);
/* A tap covers every event present when the owner takes its short lock.
 * Arrivals after that action remain new. */
void weather_client_alerts_dismiss(void);
esp_err_t weather_client_alerts_row_copy(size_t index, weather_alert_row_t *out);
/* All four strings (including null instruction as empty) are copied into the
 * caller's buffer; offsets refer to it. needed includes their terminators.
 * INVALID_SIZE returns needed without exposing owner pointers. */
esp_err_t weather_client_alerts_detail_copy(uint64_t id, weather_alert_detail_t *out,
  char *buffer, size_t capacity, size_t *needed);

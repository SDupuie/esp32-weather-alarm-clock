#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"

#include "esp_err.h"

#include "device_config.h"

typedef struct {
  char ssid[DEVICE_CONFIG_WIFI_LEN];
  int8_t rssi;
  bool requires_password;
} connectivity_scan_result_t;

typedef enum {
  CONNECTIVITY_WIFI_DISCONNECTED,
  CONNECTIVITY_WIFI_CONNECTING,
  CONNECTIVITY_WIFI_RETRYING,
  CONNECTIVITY_WIFI_CONNECTED,
} connectivity_wifi_state_t;

/* Preparation, initialization and scans wait for each other. Call from startup
 * or background work; Wi-Fi event callbacks never wait for this operation lock. */
esp_err_t connectivity_prepare_runtime(void);
esp_err_t connectivity_init(const device_config_t *config);
esp_err_t connectivity_scan_networks(
  connectivity_scan_result_t *results,
  size_t max_results,
  size_t *out_result_count
);
esp_err_t connectivity_wait_for_wifi(TickType_t timeout_ticks);
bool connectivity_is_wifi_connected(void);
/* Observes existing attempts without changing their timing or retry behavior.
 * Connected uses the same acquired-IP bit as the existing wait/boolean APIs. */
connectivity_wifi_state_t connectivity_get_wifi_state(void);
bool connectivity_get_local_ipv4(char *buffer, size_t buffer_size);
bool connectivity_is_time_synced(void);
/* Changes only when the network-time callback reports an actual correction. */
uint32_t connectivity_get_time_sync_generation(void);
esp_err_t connectivity_sync_time(TickType_t timeout_ticks);

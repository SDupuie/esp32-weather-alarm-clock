#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <pthread.h>

typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NO_MEM 0x101
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_NOT_FOUND 0x105
#define ESP_ERR_TIMEOUT 0x107
#define ESP_ERR_WIFI_NOT_CONNECT 0x300f
#define ESP_ERR_NVS_NOT_FOUND 0x1102
#define ESP_ERR_NVS_TYPE_MISMATCH 0x1103
#define ESP_ERR_NVS_INVALID_LENGTH 0x110c
#define ESP_ERR_NVS_INVALID_HANDLE 0x1107

typedef unsigned nvs_handle_t;
typedef enum { NVS_READONLY, NVS_READWRITE } nvs_open_mode_t;
esp_err_t nvs_open(const char *, nvs_open_mode_t, nvs_handle_t *);
void nvs_close(nvs_handle_t);
esp_err_t nvs_commit(nvs_handle_t);
esp_err_t nvs_get_str(nvs_handle_t, const char *, char *, size_t *);
esp_err_t nvs_set_str(nvs_handle_t, const char *, const char *);
esp_err_t nvs_get_u8(nvs_handle_t, const char *, uint8_t *);
esp_err_t nvs_set_u8(nvs_handle_t, const char *, uint8_t);
esp_err_t nvs_get_i16(nvs_handle_t, const char *, int16_t *);
esp_err_t nvs_set_i16(nvs_handle_t, const char *, int16_t);

typedef int BaseType_t;
typedef unsigned TickType_t;
typedef pthread_mutex_t *SemaphoreHandle_t;
#define pdTRUE 1
#define pdFALSE 0
#define pdPASS pdTRUE
#define portMAX_DELAY UINT32_MAX
SemaphoreHandle_t xSemaphoreCreateMutex(void);
BaseType_t xSemaphoreTake(SemaphoreHandle_t, TickType_t);
BaseType_t xSemaphoreGive(SemaphoreHandle_t);

typedef pthread_mutex_t portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED PTHREAD_MUTEX_INITIALIZER
#define portENTER_CRITICAL(lock) pthread_mutex_lock(lock)
#define portEXIT_CRITICAL(lock) pthread_mutex_unlock(lock)
typedef void *TaskHandle_t;
typedef unsigned UBaseType_t;
BaseType_t xTaskCreate(void (*)(void *), const char *, uint32_t, void *, UBaseType_t, TaskHandle_t *);
void vTaskDelete(TaskHandle_t);
void esp_restart(void);
const char *esp_err_to_name(esp_err_t);

typedef enum { FAKE_OPEN, FAKE_READ, FAKE_SET, FAKE_COMMIT, FAKE_OP_COUNT } fake_op_t;
/* Successful sets become readable immediately. A later set/commit failure does
 * not roll them back, deliberately exercising accepted partial-save outcomes. */
void fake_nvs_reset(void);
void fake_nvs_clear_activity(void);
void fake_nvs_fail(fake_op_t, unsigned call);
unsigned fake_nvs_calls(fake_op_t);
unsigned fake_nvs_key_count(void);
const char *fake_nvs_key(unsigned index);
void fake_nvs_remove(const char *key);
void fake_nvs_put_str(const char *key, const char *value);
void fake_nvs_put_u8(const char *key, uint8_t value);
void fake_nvs_block_next_set(void);
bool fake_nvs_wait_blocked(void);
void fake_nvs_release_set(void);

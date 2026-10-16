#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <pthread.h>

typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NO_MEM 0x101
#define ESP_ERR_INVALID_SIZE 0x104
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
esp_err_t nvs_get_blob(nvs_handle_t, const char *, void *, size_t *);
esp_err_t nvs_set_blob(nvs_handle_t, const char *, const void *, size_t);

typedef int BaseType_t;
typedef unsigned UBaseType_t;
typedef unsigned TickType_t;
typedef pthread_mutex_t *SemaphoreHandle_t;
typedef pthread_mutex_t StaticSemaphore_t;
#define pdTRUE 1
#define pdFALSE 0
#define pdPASS pdTRUE
#define portMAX_DELAY UINT32_MAX
SemaphoreHandle_t xSemaphoreCreateMutex(void);
SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t *);
SemaphoreHandle_t xSemaphoreCreateBinaryStatic(StaticSemaphore_t *);
BaseType_t xSemaphoreTake(SemaphoreHandle_t, TickType_t);
BaseType_t xSemaphoreGive(SemaphoreHandle_t);
/* Observe an actual blocked native mutex caller, without timing sleeps. */
bool fake_semaphore_wait_for_waiter(void);

typedef pthread_mutex_t portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED PTHREAD_MUTEX_INITIALIZER
#define portENTER_CRITICAL(lock) pthread_mutex_lock(lock)
#define portEXIT_CRITICAL(lock) pthread_mutex_unlock(lock)
/* Queue substitutions are supplied by the checks that use them. */
typedef struct fake_queue *QueueHandle_t;
QueueHandle_t xQueueCreate(UBaseType_t, UBaseType_t);
void vQueueDelete(QueueHandle_t);
BaseType_t xQueueOverwrite(QueueHandle_t, const void *);
BaseType_t xQueuePeek(QueueHandle_t, void *, TickType_t);
BaseType_t xQueueSend(QueueHandle_t, const void *, TickType_t);
BaseType_t xQueueReceive(QueueHandle_t, void *, TickType_t);

typedef void *TaskHandle_t;
typedef void (*TaskFunction_t)(void *);
typedef uint32_t configSTACK_DEPTH_TYPE;
BaseType_t xTaskCreate(void (*)(void *), const char *, uint32_t, void *, UBaseType_t, TaskHandle_t *);
BaseType_t xTaskCreatePinnedToCore(TaskFunction_t, const char * const, const configSTACK_DEPTH_TYPE,
                                  void * const, UBaseType_t, TaskHandle_t * const, const BaseType_t);
void vTaskDelete(TaskHandle_t);
void esp_restart(void);
const char *esp_err_to_name(esp_err_t);

/* Public BSP/codec substitutions are implemented only in the audio check. */
typedef void *esp_codec_dev_handle_t;
typedef struct {
  uint8_t bits_per_sample, channel;
  uint16_t channel_mask;
  uint32_t sample_rate;
  int mclk_multiple;
} esp_codec_dev_sample_info_t;
esp_codec_dev_handle_t bsp_audio_codec_speaker_init(void);
int esp_codec_dev_open(esp_codec_dev_handle_t, esp_codec_dev_sample_info_t *);
int esp_codec_dev_set_out_vol(esp_codec_dev_handle_t, int);
int esp_codec_dev_write(esp_codec_dev_handle_t, void *, int);
int esp_codec_dev_close(esp_codec_dev_handle_t);

typedef enum { FAKE_OPEN, FAKE_READ, FAKE_SET, FAKE_COMMIT, FAKE_OP_COUNT } fake_op_t;
/* Successful sets become readable immediately. A later set/commit failure does
 * not roll them back, deliberately exercising accepted partial-save outcomes. */
void fake_nvs_reset(void);
void fake_nvs_clear_activity(void);
void fake_nvs_fail(fake_op_t, unsigned call);
void fake_nvs_fail_with_error(fake_op_t, unsigned call, esp_err_t error);
unsigned fake_nvs_calls(fake_op_t);
unsigned fake_nvs_key_count(void);
const char *fake_nvs_key(unsigned index);
const char *fake_nvs_namespace(unsigned index);
int bsp_display_brightness_get(void);
esp_err_t bsp_display_brightness_set(int percent);
void fake_nvs_remove(const char *key);
void fake_nvs_put_str(const char *key, const char *value);
void fake_nvs_put_u8(const char *key, uint8_t value);
void fake_nvs_block_next_set(void);
bool fake_nvs_wait_blocked(void);
void fake_nvs_release_set(void);

#define ESP_ERR_INVALID_RESPONSE 0x108
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
TickType_t xTaskGetTickCount(void);
void vTaskDelay(TickType_t ticks);
int64_t esp_timer_get_time(void);

/* Only the time test target replaces libc clock access. Other native tests
 * retain their real clock for condition-variable timeouts. */
#include <time.h>
#include <sys/time.h>
time_t wac_test_time(time_t *out);
int wac_test_settimeofday(const struct timeval *value, const struct timezone *zone);
void fake_clock_reset(time_t utc);
void fake_clock_advance(unsigned milliseconds);
void fake_clock_set(time_t utc);
void fake_clock_fail_set(bool fail);
unsigned fake_clock_set_calls(void);

typedef void *i2c_master_bus_handle_t;
typedef void *i2c_master_dev_handle_t;
typedef enum { I2C_ADDR_BIT_LEN_7 } i2c_addr_bit_len_t;
typedef struct {
  i2c_addr_bit_len_t dev_addr_length;
  uint16_t device_address;
  uint32_t scl_speed_hz;
} i2c_device_config_t;
esp_err_t bsp_i2c_init(void);
i2c_master_bus_handle_t bsp_i2c_get_handle(void);
esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t,
                                  const i2c_device_config_t *, i2c_master_dev_handle_t *);
esp_err_t i2c_master_transmit_receive(i2c_master_dev_handle_t, const uint8_t *,
                                    size_t, uint8_t *, size_t, int);
esp_err_t i2c_master_transmit(i2c_master_dev_handle_t, const uint8_t *, size_t, int);

typedef struct {
  bool write;
  uint8_t reg;
  size_t length;
  esp_err_t result;
} fake_i2c_event_t;
void fake_i2c_reset(void);
uint8_t *fake_i2c_registers(void);
void fake_i2c_clear_activity(void);
void fake_i2c_fail(bool write, uint8_t reg, unsigned matching_call);
void fake_i2c_fail_add(bool fail);
unsigned fake_i2c_event_count(void);
fake_i2c_event_t fake_i2c_event(unsigned index);
void fake_i2c_block_next_read(void);
bool fake_i2c_wait_blocked(void);
void fake_i2c_release_read(void);

/* Connectivity platform declarations. Their implementations stay in the
 * connectivity check, so other targets do not acquire network substitutes. */
#define BIT0 (1u << 0)
#define BIT1 (1u << 1)
typedef uint32_t EventBits_t;
typedef struct fake_event_group *EventGroupHandle_t;
EventGroupHandle_t xEventGroupCreate(void);
EventBits_t xEventGroupClearBits(EventGroupHandle_t, EventBits_t);
EventBits_t xEventGroupSetBits(EventGroupHandle_t, EventBits_t);
EventBits_t xEventGroupGetBits(EventGroupHandle_t);
EventBits_t xEventGroupWaitBits(EventGroupHandle_t, EventBits_t, BaseType_t,
                              BaseType_t, TickType_t);

typedef const char *esp_event_base_t;
typedef void (*esp_event_handler_t)(void *, esp_event_base_t, int32_t, void *);
extern const char fake_wifi_event_base[], fake_ip_event_base[];
#define WIFI_EVENT fake_wifi_event_base
#define IP_EVENT fake_ip_event_base
#define ESP_EVENT_ANY_ID -1
#define WIFI_EVENT_STA_START 1
#define WIFI_EVENT_STA_DISCONNECTED 2
#define WIFI_EVENT_SCAN_DONE 3
#define IP_EVENT_STA_GOT_IP 4
esp_err_t esp_event_loop_create_default(void);
esp_err_t esp_event_handler_register(esp_event_base_t, int32_t,
                                     esp_event_handler_t, void *);

typedef struct { unsigned unused; } esp_netif_t;
typedef struct { uint32_t addr; } esp_ip4_addr_t;
typedef struct { esp_ip4_addr_t ip; } esp_netif_ip_info_t;
esp_err_t esp_netif_init(void);
esp_netif_t *esp_netif_create_default_wifi_sta(void);
esp_err_t esp_netif_get_ip_info(esp_netif_t *, esp_netif_ip_info_t *);
#define IPSTR "%u.%u.%u.%u"
#define IP2STR(ip) (unsigned)((ip)->addr & 255u), \
  (unsigned)(((ip)->addr >> 8) & 255u), (unsigned)(((ip)->addr >> 16) & 255u), \
  (unsigned)(((ip)->addr >> 24) & 255u)

typedef enum { WIFI_AUTH_OPEN, WIFI_AUTH_WPA2_PSK } wifi_auth_mode_t;
typedef enum { WIFI_STORAGE_RAM } wifi_storage_t;
typedef enum { WIFI_MODE_STA } wifi_mode_t;
typedef enum { WIFI_IF_STA } wifi_interface_t;
typedef struct { unsigned unused; } wifi_init_config_t;
#define WIFI_INIT_CONFIG_DEFAULT() ((wifi_init_config_t){0})
typedef struct {
  struct {
    uint8_t ssid[32], password[64];
    struct { wifi_auth_mode_t authmode; } threshold;
    struct { bool capable, required; } pmf_cfg;
  } sta;
} wifi_config_t;
typedef struct { uint8_t ssid[33]; int8_t rssi; wifi_auth_mode_t authmode; } wifi_ap_record_t;
typedef struct { uint32_t status; uint8_t number, scan_id; } wifi_event_sta_scan_done_t;
typedef struct { unsigned unused; } wifi_scan_config_t;
esp_err_t esp_wifi_init(const wifi_init_config_t *);
esp_err_t esp_wifi_set_storage(wifi_storage_t);
esp_err_t esp_wifi_set_mode(wifi_mode_t);
esp_err_t esp_wifi_set_config(wifi_interface_t, const wifi_config_t *);
esp_err_t esp_wifi_start(void);
esp_err_t esp_wifi_connect(void);
esp_err_t esp_wifi_disconnect(void);
esp_err_t esp_wifi_scan_start(const wifi_scan_config_t *, bool);
esp_err_t esp_wifi_scan_get_ap_num(uint16_t *);
esp_err_t esp_wifi_scan_get_ap_records(uint16_t *, wifi_ap_record_t *);
typedef struct {
  bool start, wait_for_sync;
  void (*sync_cb)(struct timeval *);
} esp_sntp_config_t;
#define ESP_NETIF_SNTP_DEFAULT_CONFIG(server) ((esp_sntp_config_t){0})
esp_err_t esp_netif_sntp_init(const esp_sntp_config_t *);

/* HTTP substitutions are implemented only by the weather service check. */
enum { HTTP_TRANSPORT_OVER_SSL };
enum { HTTP_METHOD_GET };
typedef void *esp_http_client_handle_t;
typedef enum { HTTP_EVENT_ON_DATA, HTTP_EVENT_ON_HEADER } esp_http_client_event_id_t;
typedef struct {
  esp_http_client_event_id_t event_id;
  void *data;
  int data_len;
  void *user_data;
  char *header_key;
  char *header_value;
} esp_http_client_event_t;
typedef struct {
  const char *url;
  const char *host, *path, *query;
  const char *user_agent;
  int method;
  int transport_type;
  bool disable_auto_redirect;
  esp_err_t (*event_handler)(esp_http_client_event_t *);
  void *user_data;
  int timeout_ms;
  int buffer_size;
  esp_err_t (*crt_bundle_attach)(void *);
} esp_http_client_config_t;
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *);
esp_err_t esp_http_client_perform(esp_http_client_handle_t);
int esp_http_client_get_status_code(esp_http_client_handle_t);
esp_err_t esp_http_client_cleanup(esp_http_client_handle_t);
esp_err_t esp_crt_bundle_attach(void *);
#define ESP_RETURN_ON_ERROR(expression, tag, ...) do { \
  (void)(tag); esp_err_t test_error = (expression); \
  if (test_error != ESP_OK) return test_error; \
} while (0)

#ifdef WAC_TOUCH_TEST
/* Opaque platform types required by the retained real touch header. */
typedef int gpio_num_t;
typedef struct esp_lcd_panel_io_t *esp_lcd_panel_io_handle_t;
#endif

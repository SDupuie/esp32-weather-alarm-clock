#include "test_support.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef enum { STRING, U8, I16, BLOB } value_type_t;
typedef struct {
  char namespace[16];
  char key[16];
  value_type_t type;
  char string[256];
  int number;
  size_t blob_size;
} entry_t;

static entry_t entries[64];
static char namespaces[8][16];
static unsigned namespace_count;
static unsigned count, calls[FAKE_OP_COUNT], failure[FAKE_OP_COUNT];
static esp_err_t failure_error[FAKE_OP_COUNT];
static pthread_mutex_t fake_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t fake_condition = PTHREAD_COND_INITIALIZER;
static bool block_set, blocked, released;

static struct timespec deadline(void)
{
  struct timespec until;
  clock_gettime(CLOCK_REALTIME, &until);
  until.tv_sec += 3;
  return until;
}

static esp_err_t operation(fake_op_t op)
{
  pthread_mutex_lock(&fake_lock);
  ++calls[op];
  if (op == FAKE_SET && block_set) {
    block_set = false;
    blocked = true;
    pthread_cond_broadcast(&fake_condition);
    struct timespec until = deadline();
    while (!released) {
      int result = pthread_cond_timedwait(&fake_condition, &fake_lock, &until);
      assert(result != ETIMEDOUT && "test did not release blocked storage write");
    }
    blocked = false;
  }
  bool failed = failure[op] && calls[op] == failure[op];
  pthread_mutex_unlock(&fake_lock);
  return failed ? failure_error[op] : ESP_OK;
}

static const char *namespace_for(nvs_handle_t handle)
{
  assert(handle > 0 && handle <= namespace_count);
  return namespaces[handle - 1];
}

static entry_t *find(const char *name, const char *key)
{
  for (unsigned i = 0; i < count; ++i) {
    if (!strcmp(entries[i].namespace, name) && !strcmp(entries[i].key, key)) return &entries[i];
  }
  return NULL;
}

static entry_t *put(const char *name, const char *key, value_type_t type)
{
  entry_t *entry = find(name, key);
  if (!entry) {
    assert(count < 64);
    entry = &entries[count++];
    assert(strlen(key) < sizeof(entry->key));
    strcpy(entry->key, key);
    assert(strlen(name) < sizeof(entry->namespace));
    strcpy(entry->namespace, name);
  }
  entry->type = type;
  return entry;
}

void fake_nvs_clear_activity(void)
{
  memset(calls, 0, sizeof(calls));
  memset(failure, 0, sizeof(failure));
  for (unsigned i = 0; i < FAKE_OP_COUNT; ++i) failure_error[i] = ESP_FAIL;
}

void fake_nvs_reset(void)
{
  memset(entries, 0, sizeof(entries));
  count = namespace_count = 0;
  memset(namespaces, 0, sizeof(namespaces));
  fake_nvs_clear_activity();
  block_set = blocked = released = false;
}

void fake_nvs_fail_with_error(fake_op_t op, unsigned call, esp_err_t error)
{
  assert(error != ESP_OK); failure[op] = call; failure_error[op] = error;
}
void fake_nvs_fail(fake_op_t op, unsigned call)
{
  fake_nvs_fail_with_error(op, call, ESP_FAIL);
}
unsigned fake_nvs_calls(fake_op_t op)
{
  pthread_mutex_lock(&fake_lock);
  unsigned result = calls[op];
  pthread_mutex_unlock(&fake_lock);
  return result;
}
unsigned fake_nvs_key_count(void) { return count; }
const char *fake_nvs_namespace(unsigned index) { assert(index < count); return entries[index].namespace; }
const char *fake_nvs_key(unsigned index) { assert(index < count); return entries[index].key; }
void fake_nvs_remove(const char *key)
{
  entry_t *entry = find("wac_cfg", key);
  assert(entry);
  *entry = entries[--count];
}
void fake_nvs_put_str(const char *key, const char *value)
{
  entry_t *entry = put("wac_cfg", key, STRING);
  assert(strlen(value) < sizeof(entry->string));
  strcpy(entry->string, value);
}
void fake_nvs_put_u8(const char *key, uint8_t value) { put("wac_cfg", key, U8)->number = value; }

esp_err_t nvs_open(const char *name, nvs_open_mode_t mode, nvs_handle_t *handle)
{
  assert(name && (mode == NVS_READWRITE || mode == NVS_READONLY) && handle);
  esp_err_t error = operation(FAKE_OPEN);
  if (error != ESP_OK) return error;
  unsigned index = 0;
  while (index < namespace_count && strcmp(namespaces[index], name)) ++index;
  if (index == namespace_count && mode == NVS_READONLY) return ESP_ERR_NVS_NOT_FOUND;
  if (index == namespace_count) {
    assert(namespace_count < 8 && strlen(name) < sizeof(namespaces[0]));
    strcpy(namespaces[namespace_count++], name);
  }
  *handle = index + 1;
  return ESP_OK;
}
void nvs_close(nvs_handle_t handle) { (void)namespace_for(handle); }
esp_err_t nvs_commit(nvs_handle_t handle)
{
  (void)namespace_for(handle);
  return operation(FAKE_COMMIT);
}
esp_err_t nvs_get_str(nvs_handle_t handle, const char *key, char *value, size_t *size)
{
  assert(key && size);
  esp_err_t error = operation(FAKE_READ);
  if (error != ESP_OK) return error;
  entry_t *entry = find(namespace_for(handle), key);
  if (!entry) return ESP_ERR_NVS_NOT_FOUND;
  if (entry->type != STRING) return ESP_ERR_NVS_TYPE_MISMATCH;
  size_t needed = strlen(entry->string) + 1;
  if (value && *size < needed) { *size = needed; return ESP_ERR_NVS_INVALID_LENGTH; }
  *size = needed;
  if (value) memcpy(value, entry->string, needed);
  return ESP_OK;
}
esp_err_t nvs_set_str(nvs_handle_t handle, const char *key, const char *value)
{
  (void)namespace_for(handle);
  esp_err_t error = operation(FAKE_SET);
  if (error != ESP_OK) return error;
  entry_t *entry = put(namespace_for(handle), key, STRING);
  assert(strlen(value) < sizeof(entry->string));
  strcpy(entry->string, value);
  return ESP_OK;
}

#define NUMBER_ACCESSORS(suffix, ctype, kind) \
  esp_err_t nvs_get_##suffix(nvs_handle_t handle, const char *key, ctype *value) \
  { \
    assert(value); \
    esp_err_t error = operation(FAKE_READ); \
    if (error != ESP_OK) return error; \
    entry_t *entry = find(namespace_for(handle), key); \
    if (!entry) return ESP_ERR_NVS_NOT_FOUND; \
    if (entry->type != kind) return ESP_ERR_NVS_TYPE_MISMATCH; \
    *value = (ctype)entry->number; \
    return ESP_OK; \
  } \
  esp_err_t nvs_set_##suffix(nvs_handle_t handle, const char *key, ctype value) \
  { \
    (void)namespace_for(handle); \
    esp_err_t error = operation(FAKE_SET); \
    if (error != ESP_OK) return error; \
    put(namespace_for(handle), key, kind)->number = value; \
    return ESP_OK; \
  }
NUMBER_ACCESSORS(u8, uint8_t, U8)
NUMBER_ACCESSORS(i16, int16_t, I16)

esp_err_t nvs_get_blob(nvs_handle_t handle, const char *key, void *value, size_t *size)
{
  assert(key && size);
  esp_err_t error = operation(FAKE_READ);
  if (error != ESP_OK) return error;
  entry_t *entry = find(namespace_for(handle), key);
  if (!entry) return ESP_ERR_NVS_NOT_FOUND;
  if (entry->type != BLOB) return ESP_ERR_NVS_TYPE_MISMATCH;
  size_t needed = entry->blob_size;
  if (value && *size < needed) { *size = needed; return ESP_ERR_NVS_INVALID_LENGTH; }
  *size = needed;
  if (value) memcpy(value, entry->string, needed);
  return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t handle, const char *key, const void *value, size_t size)
{
  assert(key && value && size <= sizeof(entries[0].string));
  esp_err_t error = operation(FAKE_SET);
  if (error != ESP_OK) return error;
  entry_t *entry = put(namespace_for(handle), key, BLOB);
  memcpy(entry->string, value, size);
  entry->blob_size = size;
  return ESP_OK;
}

SemaphoreHandle_t xSemaphoreCreateMutex(void)
{
  pthread_mutex_t *mutex = malloc(sizeof(*mutex));
  if (!mutex) return NULL;
  assert(pthread_mutex_init(mutex, NULL) == 0);
  return mutex;
}
SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t *storage)
{
  assert(storage && pthread_mutex_init(storage, NULL) == 0);
  return storage;
}
static unsigned semaphore_waiters;
bool fake_semaphore_wait_for_waiter(void)
{
  pthread_mutex_lock(&fake_lock);
  struct timespec until = deadline();
  while (!semaphore_waiters) {
    if (pthread_cond_timedwait(&fake_condition, &fake_lock, &until) == ETIMEDOUT) break;
  }
  bool result = semaphore_waiters != 0;
  pthread_mutex_unlock(&fake_lock);
  return result;
}
BaseType_t xSemaphoreTake(SemaphoreHandle_t mutex, TickType_t ticks)
{
  int result = pthread_mutex_trylock(mutex);
  if (result == 0) return pdTRUE;
  if (ticks == 0 || result != EBUSY) return pdFALSE;
  pthread_mutex_lock(&fake_lock);
  ++semaphore_waiters;
  pthread_cond_broadcast(&fake_condition);
  pthread_mutex_unlock(&fake_lock);
  result = pthread_mutex_lock(mutex);
  pthread_mutex_lock(&fake_lock);
  --semaphore_waiters;
  pthread_mutex_unlock(&fake_lock);
  return result == 0 ? pdTRUE : pdFALSE;
}
BaseType_t xSemaphoreGive(SemaphoreHandle_t mutex)
{
  return pthread_mutex_unlock(mutex) == 0 ? pdTRUE : pdFALSE;
}

void fake_nvs_block_next_set(void)
{
  pthread_mutex_lock(&fake_lock);
  block_set = true;
  released = false;
  pthread_mutex_unlock(&fake_lock);
}
bool fake_nvs_wait_blocked(void)
{
  pthread_mutex_lock(&fake_lock);
  struct timespec until = deadline();
  while (!blocked) {
    if (pthread_cond_timedwait(&fake_condition, &fake_lock, &until) == ETIMEDOUT) break;
  }
  bool result = blocked;
  pthread_mutex_unlock(&fake_lock);
  return result;
}
void fake_nvs_release_set(void)
{
  pthread_mutex_lock(&fake_lock);
  released = true;
  pthread_cond_broadcast(&fake_condition);
  pthread_mutex_unlock(&fake_lock);
}

static time_t clock_utc;
static TickType_t clock_ticks;
static bool clock_set_failure;
static unsigned clock_sets;

void fake_clock_reset(time_t utc)
{
  clock_utc = utc;
  clock_ticks = clock_sets = 0;
  clock_set_failure = false;
}
void fake_clock_set(time_t utc) { clock_utc = utc; }
void fake_clock_advance(unsigned milliseconds)
{
  clock_ticks += milliseconds;
  clock_utc += milliseconds / 1000;
}
void fake_clock_fail_set(bool fail) { clock_set_failure = fail; }
unsigned fake_clock_set_calls(void) { return clock_sets; }
time_t wac_test_time(time_t *out)
{
  if (out) *out = clock_utc;
  return clock_utc;
}
int wac_test_settimeofday(const struct timeval *value, const struct timezone *zone)
{
  assert(value && !zone);
  ++clock_sets;
  if (clock_set_failure) return -1;
  clock_utc = value->tv_sec;
  return 0;
}
TickType_t xTaskGetTickCount(void) { return clock_ticks; }

static uint8_t rtc_registers[0x20];
static fake_i2c_event_t i2c_events[256];
static unsigned i2c_event_count, i2c_matching_calls, i2c_failure_call;
static uint8_t i2c_failure_reg;
static bool i2c_add_failure;
static bool i2c_failure_write, i2c_block_read, i2c_blocked, i2c_released;
static pthread_mutex_t i2c_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t i2c_condition = PTHREAD_COND_INITIALIZER;

void fake_i2c_clear_activity(void)
{
  i2c_event_count = i2c_matching_calls = i2c_failure_call = 0;
}
void fake_i2c_reset(void)
{
  memset(rtc_registers, 0, sizeof(rtc_registers));
  fake_i2c_clear_activity();
  i2c_block_read = i2c_blocked = i2c_released = i2c_add_failure = false;
}
uint8_t *fake_i2c_registers(void) { return rtc_registers; }
void fake_i2c_fail(bool write, uint8_t reg, unsigned matching_call)
{
  i2c_failure_write = write;
  i2c_failure_reg = reg;
  i2c_failure_call = matching_call;
  i2c_matching_calls = 0;
}
void fake_i2c_fail_add(bool fail) { i2c_add_failure = fail; }
unsigned fake_i2c_event_count(void) { return i2c_event_count; }
fake_i2c_event_t fake_i2c_event(unsigned index)
{
  assert(index < i2c_event_count);
  return i2c_events[index];
}
esp_err_t bsp_i2c_init(void) { return ESP_OK; }
i2c_master_bus_handle_t bsp_i2c_get_handle(void) { return rtc_registers; }
esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t bus,
                                  const i2c_device_config_t *config,
                                  i2c_master_dev_handle_t *out)
{
  assert(bus == rtc_registers && config && out);
  assert(config->device_address == 0x68);
  assert(config->dev_addr_length == I2C_ADDR_BIT_LEN_7);
  assert(config->scl_speed_hz > 0 && config->scl_speed_hz <= 400000);
  if (i2c_add_failure) return ESP_ERR_TIMEOUT;
  *out = rtc_registers;
  return ESP_OK;
}
static esp_err_t i2c_operation(bool write, uint8_t reg, size_t length)
{
  pthread_mutex_lock(&i2c_lock);
  if (!write && i2c_block_read) {
    i2c_block_read = false;
    i2c_blocked = true;
    pthread_cond_broadcast(&i2c_condition);
    struct timespec until = deadline();
    while (!i2c_released) {
      int result = pthread_cond_timedwait(&i2c_condition, &i2c_lock, &until);
      assert(result != ETIMEDOUT && "test did not release RTC read");
    }
    i2c_blocked = false;
  }
  esp_err_t result = ESP_OK;
  if (write == i2c_failure_write && reg == i2c_failure_reg && i2c_failure_call) {
    ++i2c_matching_calls;
    if (i2c_matching_calls == i2c_failure_call) result = ESP_ERR_TIMEOUT;
  }
  assert(i2c_event_count < sizeof(i2c_events) / sizeof(i2c_events[0]));
  i2c_events[i2c_event_count++] = (fake_i2c_event_t){ write, reg, length, result };
  pthread_mutex_unlock(&i2c_lock);
  return result;
}
esp_err_t i2c_master_transmit_receive(i2c_master_dev_handle_t device,
                                    const uint8_t *tx, size_t tx_size,
                                    uint8_t *rx, size_t rx_size, int timeout)
{
  assert(device == rtc_registers && tx && tx_size == 1 && rx && rx_size);
  assert(timeout > 0 && timeout <= 100 && tx[0] + rx_size <= sizeof(rtc_registers));
  esp_err_t result = i2c_operation(false, tx[0], rx_size);
  if (result == ESP_OK) memcpy(rx, rtc_registers + tx[0], rx_size);
  return result;
}
esp_err_t i2c_master_transmit(i2c_master_dev_handle_t device,
                            const uint8_t *tx, size_t tx_size, int timeout)
{
  assert(device == rtc_registers && tx && tx_size > 1);
  assert(timeout > 0 && timeout <= 100 && tx[0] + tx_size - 1 <= sizeof(rtc_registers));
  esp_err_t result = i2c_operation(true, tx[0], tx_size - 1);
  if (result == ESP_OK) {
    for (size_t i = 1; i < tx_size; ++i) {
      uint8_t reg = tx[0] + i - 1;
      /* Alarm flags are cleared by zero writes; writing one preserves them. */
      rtc_registers[reg] = reg == 0x0f
        ? (tx[i] & 0xfc) | (rtc_registers[reg] & tx[i] & 3) : tx[i];
    }
  }
  return result;
}
void fake_i2c_block_next_read(void)
{
  pthread_mutex_lock(&i2c_lock);
  i2c_block_read = true;
  i2c_released = false;
  pthread_mutex_unlock(&i2c_lock);
}
bool fake_i2c_wait_blocked(void)
{
  pthread_mutex_lock(&i2c_lock);
  struct timespec until = deadline();
  while (!i2c_blocked) {
    if (pthread_cond_timedwait(&i2c_condition, &i2c_lock, &until) == ETIMEDOUT) break;
  }
  bool result = i2c_blocked;
  pthread_mutex_unlock(&i2c_lock);
  return result;
}
void fake_i2c_release_read(void)
{
  pthread_mutex_lock(&i2c_lock);
  i2c_released = true;
  pthread_cond_broadcast(&i2c_condition);
  pthread_mutex_unlock(&i2c_lock);
}

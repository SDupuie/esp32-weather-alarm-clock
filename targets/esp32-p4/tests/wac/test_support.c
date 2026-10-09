#include "test_support.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef enum { STRING, U8, I16 } value_type_t;
typedef struct {
  char key[16];
  value_type_t type;
  char string[256];
  int number;
} entry_t;

static entry_t entries[64];
static unsigned count, calls[FAKE_OP_COUNT], failure[FAKE_OP_COUNT];
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

static bool operation(fake_op_t op)
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
  return failed;
}

static entry_t *find(const char *key)
{
  for (unsigned i = 0; i < count; ++i) {
    if (!strcmp(entries[i].key, key)) return &entries[i];
  }
  return NULL;
}

static entry_t *put(const char *key, value_type_t type)
{
  entry_t *entry = find(key);
  if (!entry) {
    assert(count < 64);
    entry = &entries[count++];
    assert(strlen(key) < sizeof(entry->key));
    strcpy(entry->key, key);
  }
  entry->type = type;
  return entry;
}

void fake_nvs_clear_activity(void)
{
  memset(calls, 0, sizeof(calls));
  memset(failure, 0, sizeof(failure));
}

void fake_nvs_reset(void)
{
  memset(entries, 0, sizeof(entries));
  count = 0;
  fake_nvs_clear_activity();
  block_set = blocked = released = false;
}

void fake_nvs_fail(fake_op_t op, unsigned call) { failure[op] = call; }
unsigned fake_nvs_calls(fake_op_t op)
{
  pthread_mutex_lock(&fake_lock);
  unsigned result = calls[op];
  pthread_mutex_unlock(&fake_lock);
  return result;
}
unsigned fake_nvs_key_count(void) { return count; }
const char *fake_nvs_key(unsigned index) { assert(index < count); return entries[index].key; }
void fake_nvs_remove(const char *key)
{
  entry_t *entry = find(key);
  assert(entry);
  *entry = entries[--count];
}
void fake_nvs_put_str(const char *key, const char *value)
{
  entry_t *entry = put(key, STRING);
  assert(strlen(value) < sizeof(entry->string));
  strcpy(entry->string, value);
}
void fake_nvs_put_u8(const char *key, uint8_t value) { put(key, U8)->number = value; }

esp_err_t nvs_open(const char *name, nvs_open_mode_t mode, nvs_handle_t *handle)
{
  assert(name && !strcmp(name, "wac_cfg") && mode == NVS_READWRITE && handle);
  if (operation(FAKE_OPEN)) return ESP_FAIL;
  *handle = 1;
  return ESP_OK;
}
void nvs_close(nvs_handle_t handle) { assert(handle == 1); }
esp_err_t nvs_commit(nvs_handle_t handle)
{
  assert(handle == 1);
  return operation(FAKE_COMMIT) ? ESP_FAIL : ESP_OK;
}
esp_err_t nvs_get_str(nvs_handle_t handle, const char *key, char *value, size_t *size)
{
  assert(handle == 1 && key && size);
  if (operation(FAKE_READ)) return ESP_FAIL;
  entry_t *entry = find(key);
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
  assert(handle == 1);
  if (operation(FAKE_SET)) return ESP_FAIL;
  fake_nvs_put_str(key, value);
  return ESP_OK;
}

#define NUMBER_ACCESSORS(suffix, ctype, kind) \
  esp_err_t nvs_get_##suffix(nvs_handle_t handle, const char *key, ctype *value) \
  { \
    assert(handle == 1 && value); \
    if (operation(FAKE_READ)) return ESP_FAIL; \
    entry_t *entry = find(key); \
    if (!entry) return ESP_ERR_NVS_NOT_FOUND; \
    if (entry->type != kind) return ESP_ERR_NVS_TYPE_MISMATCH; \
    *value = (ctype)entry->number; \
    return ESP_OK; \
  } \
  esp_err_t nvs_set_##suffix(nvs_handle_t handle, const char *key, ctype value) \
  { \
    assert(handle == 1); \
    if (operation(FAKE_SET)) return ESP_FAIL; \
    put(key, kind)->number = value; \
    return ESP_OK; \
  }
NUMBER_ACCESSORS(u8, uint8_t, U8)
NUMBER_ACCESSORS(i16, int16_t, I16)

SemaphoreHandle_t xSemaphoreCreateMutex(void)
{
  pthread_mutex_t *mutex = malloc(sizeof(*mutex));
  if (!mutex) return NULL;
  assert(pthread_mutex_init(mutex, NULL) == 0);
  return mutex;
}
BaseType_t xSemaphoreTake(SemaphoreHandle_t mutex, TickType_t ticks)
{
  (void)ticks;
  return pthread_mutex_lock(mutex) == 0 ? pdTRUE : pdFALSE;
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

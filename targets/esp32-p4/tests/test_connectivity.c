#include "test_support.h"
#include "connectivity.h"

#include <assert.h>
#include <errno.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

/* These controlled SDK examples execute production connectivity.c. They prove
 * ordering and returned-error behavior, not radio or Hosted transport timing. */
typedef enum {
  NETIF_INIT, LOOP_CREATE, GROUP_CREATE, STA_CREATE, DRIVER_INIT, STORAGE,
  WIFI_HANDLER, IP_HANDLER, MODE, CONFIG, START, CONNECT, DISCONNECT,
  SCAN, AP_COUNT, AP_RECORDS, SNTP_INIT, OP_COUNT
} operation_t;
static unsigned calls[OP_COUNT], failure_call[OP_COUNT];
static pthread_mutex_t sdk_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t sdk_condition = PTHREAD_COND_INITIALIZER;
static operation_t blocked_operation;
static unsigned blocked_call;
static bool blocked, released;
static esp_event_handler_t wifi_handler, ip_handler;
static esp_netif_t station;
static unsigned empty_scans;
static void (*time_sync_callback)(struct timeval *);
static TickType_t last_wait_timeout;
const char fake_wifi_event_base[] = "wifi";
const char fake_ip_event_base[] = "ip";

static struct timespec deadline(void)
{
  struct timespec result;
  assert(clock_gettime(CLOCK_REALTIME, &result) == 0);
  result.tv_sec += 3;
  return result;
}
static esp_err_t visit(operation_t operation)
{
  pthread_mutex_lock(&sdk_lock);
  unsigned call = ++calls[operation];
  if (operation == blocked_operation && call == blocked_call) {
    blocked = true;
    pthread_cond_broadcast(&sdk_condition);
    struct timespec until = deadline();
    while (!released) {
      assert(pthread_cond_timedwait(&sdk_condition, &sdk_lock, &until) != ETIMEDOUT
             && "test did not release blocked SDK operation");
    }
    blocked = false;
  }
  esp_err_t result = call == failure_call[operation] ? ESP_FAIL : ESP_OK;
  pthread_mutex_unlock(&sdk_lock);
  return result;
}
static unsigned call_count(operation_t operation)
{
  pthread_mutex_lock(&sdk_lock);
  unsigned result = calls[operation];
  pthread_mutex_unlock(&sdk_lock);
  return result;
}
static void block_next(operation_t operation)
{
  pthread_mutex_lock(&sdk_lock);
  blocked_operation = operation;
  blocked_call = calls[operation] + 1;
  blocked = released = false;
  pthread_mutex_unlock(&sdk_lock);
}
static void wait_blocked(void)
{
  pthread_mutex_lock(&sdk_lock);
  struct timespec until = deadline();
  while (!blocked) {
    assert(pthread_cond_timedwait(&sdk_condition, &sdk_lock, &until) != ETIMEDOUT);
  }
  pthread_mutex_unlock(&sdk_lock);
}
static void release_operation(void)
{
  pthread_mutex_lock(&sdk_lock);
  released = true;
  pthread_cond_broadcast(&sdk_condition);
  pthread_mutex_unlock(&sdk_lock);
}
static void fail_next(operation_t operation)
{
  failure_call[operation] = call_count(operation) + 1;
}
static void emit(esp_event_base_t base, int32_t event)
{
  esp_event_handler_t handler = base == WIFI_EVENT ? wifi_handler : ip_handler;
  assert(handler);
  handler(NULL, base, event, NULL);
}

struct fake_event_group { atomic_uint bits; };
static struct fake_event_group event_group;
EventGroupHandle_t xEventGroupCreate(void)
{
  return visit(GROUP_CREATE) == ESP_OK ? &event_group : NULL;
}
EventBits_t xEventGroupClearBits(EventGroupHandle_t group, EventBits_t bits)
{
  assert(group == &event_group);
  return atomic_fetch_and(&group->bits, ~bits);
}
EventBits_t xEventGroupSetBits(EventGroupHandle_t group, EventBits_t bits)
{
  assert(group == &event_group);
  return atomic_fetch_or(&group->bits, bits) | bits;
}
EventBits_t xEventGroupGetBits(EventGroupHandle_t group)
{
  assert(group == &event_group);
  return atomic_load(&group->bits);
}
EventBits_t xEventGroupWaitBits(EventGroupHandle_t group, EventBits_t bits,
                              BaseType_t clear, BaseType_t all, TickType_t timeout)
{
  (void)bits;
  last_wait_timeout = timeout;
  assert(!clear && !all);
  return xEventGroupGetBits(group);
}
esp_err_t esp_netif_init(void) { return visit(NETIF_INIT); }
esp_err_t esp_event_loop_create_default(void) { return visit(LOOP_CREATE); }
esp_netif_t *esp_netif_create_default_wifi_sta(void)
{
  return visit(STA_CREATE) == ESP_OK ? &station : NULL;
}
esp_err_t esp_event_handler_register(esp_event_base_t base, int32_t event,
                                     esp_event_handler_t handler, void *arg)
{
  assert(handler && !arg);
  if (base == WIFI_EVENT) {
    assert(event == ESP_EVENT_ANY_ID);
    esp_err_t err = visit(WIFI_HANDLER);
    if (err == ESP_OK) wifi_handler = handler;
    return err;
  }
  assert(base == IP_EVENT && event == IP_EVENT_STA_GOT_IP);
  esp_err_t err = visit(IP_HANDLER);
  if (err == ESP_OK) ip_handler = handler;
  return err;
}
esp_err_t esp_netif_get_ip_info(esp_netif_t *netif, esp_netif_ip_info_t *info)
{
  assert(netif == &station && info);
  info->ip.addr = 0x0100007f;
  return ESP_OK;
}
esp_err_t esp_wifi_init(const wifi_init_config_t *config)
{
  assert(config);
  return visit(DRIVER_INIT);
}
esp_err_t esp_wifi_set_storage(wifi_storage_t storage)
{
  assert(storage == WIFI_STORAGE_RAM);
  return visit(STORAGE);
}
esp_err_t esp_wifi_set_mode(wifi_mode_t mode)
{
  assert(mode == WIFI_MODE_STA);
  return visit(MODE);
}
esp_err_t esp_wifi_set_config(wifi_interface_t interface, const wifi_config_t *config)
{
  assert(interface == WIFI_IF_STA && config);
  assert(!strcmp((const char *)config->sta.ssid, "Home network"));
  assert(!strcmp((const char *)config->sta.password, "example-password"));
  assert(config->sta.threshold.authmode == WIFI_AUTH_OPEN);
  assert(config->sta.pmf_cfg.capable && !config->sta.pmf_cfg.required);
  return visit(CONFIG);
}
esp_err_t esp_wifi_start(void)
{
  esp_err_t err = visit(START);
  if (err == ESP_OK) emit(WIFI_EVENT, WIFI_EVENT_STA_START);
  return err;
}
esp_err_t esp_wifi_connect(void)
{
  connectivity_wifi_state_t phase = connectivity_get_wifi_state();
  assert(phase == CONNECTIVITY_WIFI_CONNECTING || phase == CONNECTIVITY_WIFI_RETRYING);
  return visit(CONNECT);
}
esp_err_t esp_wifi_disconnect(void)
{
  esp_err_t err = visit(DISCONNECT);
  if (err == ESP_OK) emit(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED);
  return err;
}
void vTaskDelay(TickType_t ticks) { assert(ticks == pdMS_TO_TICKS(1500)); }
esp_err_t esp_wifi_scan_start(const wifi_scan_config_t *config, bool block)
{
  assert(!config && block);
  esp_err_t err = visit(SCAN);
  if (err == ESP_OK) emit(WIFI_EVENT, WIFI_EVENT_SCAN_DONE);
  return err;
}
esp_err_t esp_wifi_scan_get_ap_num(uint16_t *count)
{
  assert(count);
  esp_err_t err = visit(AP_COUNT);
  if (err == ESP_OK) *count = call_count(SCAN) <= empty_scans ? 0 : 4;
  return err;
}
esp_err_t esp_wifi_scan_get_ap_records(uint16_t *count, wifi_ap_record_t *records)
{
  assert(count && *count == 4 && records);
  esp_err_t err = visit(AP_RECORDS);
  if (err != ESP_OK) return err;
  const wifi_ap_record_t data[] = {
    { .ssid = "Home network", .rssi = -35, .authmode = WIFI_AUTH_WPA2_PSK },
    { .ssid = "Home network", .rssi = -65, .authmode = WIFI_AUTH_WPA2_PSK },
    { .ssid = "", .rssi = -45, .authmode = WIFI_AUTH_OPEN },
    { .ssid = "Guest", .rssi = -70, .authmode = WIFI_AUTH_OPEN },
  };
  memcpy(records, data, sizeof(data));
  return ESP_OK;
}
esp_err_t esp_netif_sntp_init(const esp_sntp_config_t *config)
{
  assert(config && config->start && !config->wait_for_sync && config->sync_cb);
  esp_err_t error = visit(SNTP_INIT);
  if (error == ESP_OK) time_sync_callback = config->sync_cb;
  return error;
}

static void time_sync_pending(bool fail_initialization)
{
  device_config_t config = { .wifi_ready = true };
  strcpy(config.wifi_ssid, "Home network");
  strcpy(config.wifi_password, "example-password");
  assert(connectivity_init(&config) == ESP_OK);
  assert(!connectivity_is_time_synced());
  assert(connectivity_get_time_sync_generation() == 0);
  if (fail_initialization) {
    fail_next(SNTP_INIT);
    assert(connectivity_sync_time(0) == ESP_FAIL);
    assert(!time_sync_callback && !connectivity_is_time_synced());
  }
  assert(connectivity_sync_time(0) == ESP_ERR_TIMEOUT);
  assert(last_wait_timeout == 0);
  unsigned starts = fail_initialization ? 2 : 1;
  assert(call_count(SNTP_INIT) == starts && time_sync_callback);
  assert(connectivity_sync_time(0) == ESP_ERR_TIMEOUT);
  assert(call_count(SNTP_INIT) == starts);
  assert(connectivity_get_time_sync_generation() == 0);
  struct timeval correction = { .tv_sec = 0 };
  time_sync_callback(&correction);
  assert(!connectivity_is_time_synced());
  correction.tv_sec = 1770000000;
  time_sync_callback(&correction);
  assert(connectivity_is_time_synced());
  assert(connectivity_get_time_sync_generation() == 1);
  assert(connectivity_sync_time(0) == ESP_OK);
  assert(call_count(SNTP_INIT) == starts);
  time_sync_callback(&correction);
  assert(connectivity_get_time_sync_generation() == 2);
}

static device_config_t profile(void)
{
  device_config_t result = { .wifi_ready = true };
  strcpy(result.wifi_ssid, "Home network");
  strcpy(result.wifi_password, "example-password");
  return result;
}
typedef struct {
  bool scan;
  esp_err_t error;
  size_t count;
  connectivity_scan_result_t networks[4];
} request_t;
static void *request(void *arg)
{
  request_t *result = arg;
  device_config_t config = profile();
  result->error = result->scan
    ? connectivity_scan_networks(result->networks, 4, &result->count)
    : connectivity_init(&config);
  return NULL;
}
static void assert_results(const request_t *scan)
{
  assert(scan->error == ESP_OK && scan->count == 2);
  assert(!strcmp(scan->networks[0].ssid, "Home network"));
  assert(scan->networks[0].rssi == -35 && scan->networks[0].requires_password);
  assert(!strcmp(scan->networks[1].ssid, "Guest"));
  assert(!scan->networks[1].requires_password);
}
static void assert_shared_setup(void)
{
  assert(call_count(NETIF_INIT) == 1 && call_count(LOOP_CREATE) == 1);
  assert(call_count(GROUP_CREATE) == 1 && call_count(STA_CREATE) == 1);
  assert(call_count(DRIVER_INIT) == 1 && call_count(START) == 1);
  assert(call_count(WIFI_HANDLER) == 1 && call_count(IP_HANDLER) == 1);
}
static void overlapping_requests(bool scan_first, operation_t pause)
{
  request_t first = { .scan = scan_first }, second = { .scan = !scan_first };
  pthread_t first_thread, second_thread;
  block_next(pause);
  assert(pthread_create(&first_thread, NULL, request, &first) == 0);
  wait_blocked();
  assert(pthread_create(&second_thread, NULL, request, &second) == 0);
  assert(fake_semaphore_wait_for_waiter());
  /* The second caller reached the operation mutex. During station creation it
   * cannot create another interface; during scanning it cannot apply a profile. */
  assert(call_count(STA_CREATE) == 1 && call_count(CONFIG) == 0);
  assert(call_count(DRIVER_INIT) == (pause == STA_CREATE ? 0u : 1u));
  assert(connectivity_get_wifi_state() == CONNECTIVITY_WIFI_DISCONNECTED);
  assert(call_count(CONNECT) == 0);
  release_operation();
  assert(pthread_join(first_thread, NULL) == 0);
  assert(pthread_join(second_thread, NULL) == 0);
  assert(first.error == ESP_OK && second.error == ESP_OK);
  assert_results(scan_first ? &first : &second);
  assert_shared_setup();
  assert(call_count(STORAGE) == 2 && call_count(CONFIG) == 1);
  assert(connectivity_get_wifi_state() == (scan_first
    ? CONNECTIVITY_WIFI_CONNECTING : CONNECTIVITY_WIFI_RETRYING));
  unsigned connections = call_count(CONNECT);
  emit(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED);
  assert(call_count(CONNECT) == connections + 1);
  assert(connectivity_get_wifi_state() == CONNECTIVITY_WIFI_RETRYING);
  /* Successful init stays idempotent and public preparation shares the lock. */
  device_config_t config = profile();
  assert(connectivity_prepare_runtime() == ESP_OK);
  assert(connectivity_init(&config) == ESP_OK);
  assert(call_count(CONFIG) == 1);
  assert(connectivity_get_wifi_state() == CONNECTIVITY_WIFI_RETRYING);
  emit(IP_EVENT, IP_EVENT_STA_GOT_IP);
  assert(connectivity_get_wifi_state() == CONNECTIVITY_WIFI_CONNECTED);
  assert(connectivity_is_wifi_connected() && connectivity_wait_for_wifi(0) == ESP_OK);
}
static void init_errors(void)
{
  device_config_t config = profile();
  fail_next(LOOP_CREATE);
  assert(connectivity_prepare_runtime() == ESP_FAIL);
  assert(connectivity_prepare_runtime() == ESP_OK);
  assert(call_count(NETIF_INIT) == 2 && call_count(LOOP_CREATE) == 2);
  fail_next(DRIVER_INIT);
  assert(connectivity_init(&config) == ESP_FAIL);
  assert(call_count(STA_CREATE) == 1 && call_count(DRIVER_INIT) == 1);
  fail_next(STORAGE);
  assert(connectivity_init(&config) == ESP_FAIL);
  fail_next(CONFIG);
  assert(connectivity_init(&config) == ESP_FAIL);
  fail_next(START);
  assert(connectivity_init(&config) == ESP_FAIL);
  assert(connectivity_get_wifi_state() == CONNECTIVITY_WIFI_DISCONNECTED);
  /* A failed driver connect from its START callback leaves init's historical
   * success return intact, but must not claim an attempt is still pending. */
  fail_next(CONNECT);
  assert(connectivity_init(&config) == ESP_OK);
  assert(connectivity_get_wifi_state() == CONNECTIVITY_WIFI_DISCONNECTED);
  assert(!connectivity_is_wifi_connected());
  assert(connectivity_wait_for_wifi(0) == ESP_ERR_TIMEOUT);
  emit(WIFI_EVENT, WIFI_EVENT_STA_START);
  assert(connectivity_get_wifi_state() == CONNECTIVITY_WIFI_CONNECTING);
  fail_next(CONNECT);
  emit(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED);
  assert(connectivity_get_wifi_state() == CONNECTIVITY_WIFI_DISCONNECTED);
  emit(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED);
  assert(connectivity_get_wifi_state() == CONNECTIVITY_WIFI_RETRYING);
  assert(call_count(STA_CREATE) == 1 && call_count(DRIVER_INIT) == 2);
  assert(call_count(WIFI_HANDLER) == 1 && call_count(IP_HANDLER) == 1);
  request_t scan = { .scan = true };
  request(&scan); assert_results(&scan);
  assert(connectivity_init(NULL) == ESP_ERR_INVALID_ARG);
  config.wifi_ready = false;
  assert(connectivity_init(&config) == ESP_ERR_INVALID_ARG);
}
static void scan_errors(void)
{
  device_config_t config = profile();
  assert(connectivity_init(&config) == ESP_OK);
  assert(connectivity_get_wifi_state() == CONNECTIVITY_WIFI_CONNECTING);
  const operation_t failures[] = { STORAGE, MODE, SCAN, AP_COUNT, AP_RECORDS };
  for (unsigned i = 0; i < sizeof(failures) / sizeof(failures[0]); ++i) {
    unsigned connections = call_count(CONNECT);
    fail_next(failures[i]);
    request_t scan = { .scan = true, .count = 99 };
    request(&scan);
    assert(scan.error == ESP_FAIL && scan.count == 0);
    emit(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED);
    assert(call_count(CONNECT) > connections);
    request(&scan); assert_results(&scan);
  }
  assert_shared_setup();
  empty_scans = call_count(SCAN) + 5;
  unsigned before = call_count(SCAN);
  request_t scan = { .scan = true, .count = 99 };
  request(&scan);
  assert(scan.error == ESP_OK && scan.count == 0 && call_count(SCAN) == before + 5);
  empty_scans = 0;
  size_t count = 99;
  assert(connectivity_scan_networks(NULL, 0, &count) == ESP_OK && count == 0);
  assert(connectivity_scan_networks(NULL, 1, &count) == ESP_ERR_INVALID_ARG);
  assert(connectivity_scan_networks(scan.networks, 4, NULL) == ESP_ERR_INVALID_ARG);
  request(&scan); assert_results(&scan);
  /* The scan result remains successful when its historical cleanup connect
   * returns an error; only the displayed connection phase changes. */
  fail_next(CONNECT);
  request(&scan); assert_results(&scan);
  assert(connectivity_get_wifi_state() == CONNECTIVITY_WIFI_DISCONNECTED);
  assert(!connectivity_is_wifi_connected());
  emit(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED);
  assert(connectivity_get_wifi_state() == CONNECTIVITY_WIFI_RETRYING);
}
static void disconnect_during_cleanup(void)
{
  device_config_t config = profile();
  assert(connectivity_init(&config) == ESP_OK);
  request_t scan = { .scan = true };
  pthread_t thread;
  /* A disconnect during scanning remains suppressed, as before the fix. */
  block_next(SCAN);
  assert(pthread_create(&thread, NULL, request, &scan) == 0);
  wait_blocked();
  unsigned before = call_count(CONNECT);
  emit(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED);
  assert(call_count(CONNECT) == before);
  assert(connectivity_get_wifi_state() == CONNECTIVITY_WIFI_DISCONNECTED);
  release_operation();
  assert(pthread_join(thread, NULL) == 0);
  assert_results(&scan);
  assert(call_count(CONNECT) == before + 1);
  assert(connectivity_get_wifi_state() == CONNECTIVITY_WIFI_RETRYING);

  /* Once cleanup restores intent, its still-held operation mutex cannot drop
   * a disconnect retry. The callback completes while cleanup remains blocked. */
  fail_next(CONNECT);
  block_next(CONNECT);
  assert(pthread_create(&thread, NULL, request, &scan) == 0);
  wait_blocked();
  assert(connectivity_get_wifi_state() == CONNECTIVITY_WIFI_RETRYING);
  before = call_count(CONNECT);
  emit(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED);
  assert(call_count(CONNECT) == before + 1);
  assert(connectivity_get_wifi_state() == CONNECTIVITY_WIFI_RETRYING);
  release_operation();
  assert(pthread_join(thread, NULL) == 0);
  assert_results(&scan);
  /* The older failed cleanup attempt cannot overwrite the newer retry. */
  assert(connectivity_get_wifi_state() == CONNECTIVITY_WIFI_RETRYING);
  emit(IP_EVENT, IP_EVENT_STA_GOT_IP);
  assert(connectivity_get_wifi_state() == CONNECTIVITY_WIFI_CONNECTED);
  assert(connectivity_is_wifi_connected());
  assert(connectivity_wait_for_wifi(0) == ESP_OK);

  /* An IP callback can also complete while cleanup holds its operation lock. */
  emit(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED);
  fail_next(CONNECT);
  block_next(CONNECT);
  assert(pthread_create(&thread, NULL, request, &scan) == 0);
  wait_blocked();
  emit(IP_EVENT, IP_EVENT_STA_GOT_IP);
  assert(connectivity_get_wifi_state() == CONNECTIVITY_WIFI_CONNECTED);
  release_operation();
  assert(pthread_join(thread, NULL) == 0);
  assert_results(&scan);
  assert(connectivity_get_wifi_state() == CONNECTIVITY_WIFI_CONNECTED);
  assert(connectivity_wait_for_wifi(0) == ESP_OK);

  /* A scan without a disconnect retains an already connected phase and does
   * not introduce another connection attempt. */
  before = call_count(CONNECT);
  request(&scan);
  assert_results(&scan);
  assert(call_count(CONNECT) == before);
  assert(connectivity_get_wifi_state() == CONNECTIVITY_WIFI_CONNECTED);
}
int main(int argc, char **argv)
{
  assert(argc == 2);
  assert(connectivity_get_wifi_state() == CONNECTIVITY_WIFI_DISCONNECTED);
  assert(!connectivity_is_wifi_connected());
  assert(connectivity_wait_for_wifi(0) == ESP_ERR_INVALID_STATE);
  if (!strcmp(argv[1], "init_first")) overlapping_requests(false, STA_CREATE);
  else if (!strcmp(argv[1], "scan_first")) overlapping_requests(true, STA_CREATE);
  else if (!strcmp(argv[1], "scan_busy")) overlapping_requests(true, SCAN);
  else if (!strcmp(argv[1], "init_error")) init_errors();
  else if (!strcmp(argv[1], "scan_error")) scan_errors();
  else if (!strcmp(argv[1], "cleanup_disconnect")) disconnect_during_cleanup();
  else if (!strcmp(argv[1], "time_pending")) time_sync_pending(false);
  else if (!strcmp(argv[1], "time_init_retry")) time_sync_pending(true);
  else assert(!"unknown connectivity scenario");
  printf("Connectivity %s checks passed\n", argv[1]);
  return 0;
}

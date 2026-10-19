"""Test actual acknowledgement and retained-store handoff with native network/RTOS substitutes. Run from repo root with IDF_PATH set."""
from pathlib import Path
import subprocess, tempfile, os
source=Path('targets/esp32-p4/main/message_service.c').read_text()
def function(signature):
 start=source.index(signature); end=source.index('\n}',start)+2
 return source[start:end]
prefix=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "cJSON.h"
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_ERR_INVALID_ARG 1
#define ESP_ERR_NO_MEM 2
#define ESP_ERR_NOT_FOUND 3
#define ESP_FAIL 4
#define ESP_ERR_INVALID_STATE 5
#define ESP_ERR_TIMEOUT 6
#define MESSAGE_MAX_RECORDS 3
#define MESSAGE_MAX_ACKS 4
#define MESSAGE_HUB_URL_LEN 128
#define MESSAGE_URL_LEN 256
#define DEVICE_CONFIG_STR_LEN 64
#define APP_MESSAGE_ID_LEN 40
#define HTTP_POST 1
#define portMAX_DELAY 0
#define ESP_LOGI(...) ((void)0)
typedef struct {bool in_use,active,has_expires_at; time_t expires_at_epoch; char id[40],target[64],acknowledged_by[4][64];size_t acknowledged_count;} message_record_t;
static message_record_t s_messages[3];
static void *s_message_mutex=(void*)1;
static void *s_runtime_task_handle=(void*)1;
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
static int critical;
#define portENTER_CRITICAL(m) ((void)(m), assert(critical==0), critical++)
#define portEXIT_CRITICAL(m) ((void)(m), assert(critical==1), critical--)
static esp_err_t message_service_acknowledge(const char *,const char *);
esp_err_t message_service_submit_acknowledgment(const char *,const char *);
bool message_service_take_acknowledgment_result(esp_err_t *);
static struct {char hub_url[128],device_id[64],room_name[64];bool store_sync_pending;int64_t next_store_sync_ms;} s_runtime={.hub_url="http://hub",.device_id="clock",.room_name="Room"};
static uint64_t s_store_revision;
typedef struct {int unused;} http_response_buffer_t;
static bool proxy;
static int locks, requests, response_status=200, request_error=ESP_OK;
static bool inspect_pending;
static char requested_url[256];
static void xSemaphoreTake(void *m,int t){(void)m;(void)t;assert(locks==0 && critical==0);locks++;}
static void xSemaphoreGive(void *m){(void)m;assert(locks==1);locks--;}
static bool runtime_should_proxy_locked(void){return proxy;}
static void copy_text(char *out,size_t n,const char *in){snprintf(out,n,"%s",in);}
static void update_known_target(const char *a,const char *b){(void)a;(void)b;}
static void free_http_response_buffer(http_response_buffer_t *r){(void)r;}
static int refresh_remote_client_cache(void){return 0;}
static int perform_http_request(const char *url,int method,const char *body,const char *device,const char *room,int *status,http_response_buffer_t *response){
 (void)method;(void)device;(void)room;(void)response;assert(locks==0 && critical==0);requests++;
 if(inspect_pending){
  esp_err_t result;
  assert(message_service_submit_acknowledgment("clock","other")==ESP_ERR_TIMEOUT);
  assert(!message_service_take_acknowledgment_result(&result));
 }
 snprintf(requested_url,sizeof(requested_url),"%s",url);
 cJSON *json=cJSON_Parse(body);assert(strcmp(cJSON_GetStringValue(cJSON_GetObjectItem(json,"deviceId")),"clock")==0);cJSON_Delete(json);*status=response_status;return request_error;
}
'''
tests=r'''
int main(void){
 // The later arrival occupies the first slot; the UI still displays the other ID.
 s_messages[0]=(message_record_t){.in_use=true,.active=true,.id="new-important",.target="all"};
 s_messages[1]=(message_record_t){.in_use=true,.active=true,.id="displayed",.target="all"};
 assert(message_service_acknowledge("clock","displayed")==ESP_OK);
 assert(s_messages[0].acknowledged_count==0 && s_messages[1].acknowledged_count==1);
 assert(s_runtime.store_sync_pending && s_store_revision==1);
 assert(message_service_acknowledge("clock","displayed")==ESP_OK && s_messages[1].acknowledged_count==1);
 assert(s_store_revision==1); // Duplicate dismissal does not schedule another change.
 assert(message_service_acknowledge("clock","removed")==ESP_ERR_NOT_FOUND);
 s_messages[1].active=false;
 assert(message_service_acknowledge("clock","displayed")==ESP_ERR_NOT_FOUND);
 s_messages[1].active=true;s_messages[1].has_expires_at=true;s_messages[1].expires_at_epoch=1;
 assert(message_service_acknowledge("clock","displayed")==ESP_ERR_NOT_FOUND);
 s_messages[1].has_expires_at=false;strcpy(s_messages[1].target,"another-clock");
 assert(message_service_acknowledge("clock","displayed")==ESP_ERR_NOT_FOUND);
 assert(message_service_acknowledge("clock","")==ESP_ERR_INVALID_ARG);
 assert(message_service_acknowledge("clock",NULL)==ESP_ERR_INVALID_ARG);
 proxy=true;
 assert(message_service_acknowledge("clock","displayed")==ESP_OK);
 assert(strcmp(requested_url,"http://hub/api/messages/displayed/ack")==0 && requests==1);
 response_status=404;
 assert(message_service_acknowledge("clock","removed")==ESP_FAIL);
 assert(strcmp(requested_url,"http://hub/api/messages/removed/ack")==0 && requests==2);
 assert(s_messages[0].acknowledged_count==0 && locks==0);

 // Submission only copies values, even while the service/network owner is busy.
 proxy=false;strcpy(s_messages[1].target,"all");s_messages[1].acknowledged_count=0;
 esp_err_t result=ESP_FAIL;
 assert(!message_service_take_acknowledgment_result(&result));
 assert(message_service_submit_acknowledgment(NULL,"displayed")==ESP_ERR_INVALID_ARG);
 assert(message_service_submit_acknowledgment("clock","")==ESP_ERR_INVALID_ARG);
 char too_long[DEVICE_CONFIG_STR_LEN+1];memset(too_long,'a',sizeof(too_long)-1);too_long[sizeof(too_long)-1]='\0';
 assert(message_service_submit_acknowledgment(too_long,"displayed")==ESP_ERR_INVALID_ARG);
 assert(message_service_submit_acknowledgment("clock",too_long)==ESP_ERR_INVALID_ARG);
 s_runtime_task_handle=NULL;
 assert(message_service_submit_acknowledgment("clock","displayed")==ESP_ERR_INVALID_STATE);
 s_runtime_task_handle=(void*)1;
 char device[]="clock",message[]="displayed";
 locks=1;
 assert(message_service_submit_acknowledgment(device,message)==ESP_OK);
 assert(requests==2 && s_messages[1].acknowledged_count==0 && locks==1);
 locks=0;
 strcpy(device,"other");strcpy(message,"changed");
 assert(message_service_submit_acknowledgment("clock","displayed")==ESP_ERR_TIMEOUT);
 assert(!message_service_take_acknowledgment_result(&result));
 process_pending_acknowledgment();
 assert(s_messages[1].acknowledged_count==1 && strcmp(s_messages[1].acknowledged_by[0],"clock")==0);
 assert(s_messages[0].acknowledged_count==0);
 // Worker polls and duplicate taps cannot overwrite a completion before the UI reads it.
 process_pending_acknowledgment();process_pending_acknowledgment();
 assert(message_service_submit_acknowledgment("clock","other")==ESP_ERR_TIMEOUT);
 assert(!message_service_take_acknowledgment_result(NULL));
 assert(message_service_take_acknowledgment_result(&result) && result==ESP_OK);
 assert(!message_service_take_acknowledgment_result(&result));
 assert(message_service_submit_acknowledgment("clock","removed")==ESP_OK);
 process_pending_acknowledgment();
 assert(message_service_take_acknowledgment_result(&result) && result==ESP_ERR_NOT_FOUND);

 proxy=true;inspect_pending=true;response_status=200;
 assert(message_service_submit_acknowledgment("clock","displayed")==ESP_OK && requests==2);
 process_pending_acknowledgment();
 assert(requests==3 && strcmp(requested_url,"http://hub/api/messages/displayed/ack")==0);
 process_pending_acknowledgment();process_pending_acknowledgment();
 assert(requests==3); // Repeated worker polls cannot repeat completed network work.
 assert(message_service_take_acknowledgment_result(&result) && result==ESP_OK);
 response_status=404;
 assert(message_service_submit_acknowledgment("clock","removed")==ESP_OK);
 process_pending_acknowledgment();
 assert(message_service_take_acknowledgment_result(&result) && result==ESP_FAIL);
 request_error=ESP_ERR_TIMEOUT;
 assert(message_service_submit_acknowledgment("clock","displayed")==ESP_OK);
 process_pending_acknowledgment();
 assert(message_service_take_acknowledgment_result(&result) && result==ESP_ERR_TIMEOUT);
 assert(locks==0 && critical==0);
 puts("PASS: displayed ID and existing acknowledgment cases; copied submission, no synchronous execution, busy pending/unconsumed result, retained completion, local/shared success and failure.");
}
'''
handoff=source[source.index('typedef enum {\n  MESSAGE_ACK_IDLE'):source.index('\nstatic void copy_text')]
body=handoff+'\n'+'\n'.join(function(f) for f in ['static void local_store_changed_locked(','static bool is_message_expired(','static bool is_visible_to_device(','static bool is_acknowledged_for_device(','static int find_message_index_by_id(','static esp_err_t message_service_acknowledge(const char *device_id, const char *message_id)\n{','esp_err_t message_service_submit_acknowledgment(','bool message_service_take_acknowledgment_result(','static void process_pending_acknowledgment('])
assert '    process_pending_acknowledgment();' in function('static void message_runtime_task(')
cjson=Path(os.environ['IDF_PATH'])/'components/json/cJSON'
with tempfile.TemporaryDirectory(prefix='wac-message-ack-') as temp:
 p=Path(temp)/'test.c';p.write_text(prefix+body+tests);binary=Path(temp)/'test'
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-I',str(cjson),str(p),str(cjson/'cJSON.c'),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)

# Run the actual runtime loop one iteration at a time. Network/RTOS operations
# are substitutes; discovery, scheduling, serialization and merge are production.
store_prefix=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <setjmp.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "cJSON.h"
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_ERR_INVALID_ARG 1
#define ESP_ERR_NO_MEM 2
#define ESP_FAIL 4
#define ESP_ERR_INVALID_STATE 5
#define ESP_ERR_INVALID_SIZE 7
#define DEVICE_CONFIG_STR_LEN 64
#define DEVICE_CONFIG_SHORT_STR_LEN 16
#define APP_MESSAGE_ID_LEN 40
#define APP_MESSAGE_TEXT_LEN 181
#define APP_MESSAGE_SENDER_LEN 41
#define HTTP_POST 1
#define portMAX_DELAY 0
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define pdMS_TO_TICKS(ms) (ms)
typedef struct {bool available;char id[40],text[181],meta[96];} app_message_snapshot_t;
'''
store_defines='\n'.join(line for line in source.splitlines() if line.startswith('#define MESSAGE_'))
store_types=source[source.index('typedef struct {\n  bool in_use;'):source.index('typedef struct {\n  char device_id[DEVICE_CONFIG_STR_LEN];\n  char room_name[DEVICE_CONFIG_STR_LEN];\n} request_known_device_t;')]
store_substitutes=r'''
static message_record_t s_messages[MESSAGE_MAX_RECORDS], remote[MESSAGE_MAX_RECORDS];
static message_runtime_state_t s_runtime;
static message_client_cache_t s_client_cache;
static uint64_t s_store_revision;
static void *s_message_mutex=(void*)1;
typedef struct {char *data;} http_response_buffer_t;
#define HTTP_GET 0
static int locks, requests, refreshes, acknowledgments, response_status=200, request_error;
static int64_t clock_ms;
static bool wifi=true, apply_request=true, mutate_during_request;
static int poll_status=200, poll_error;
static const char *poll_reply="{\"unreadCount\":0,\"messages\":[]}";
static char packet[384], requested_url[MESSAGE_URL_LEN];
static char sent_payload[384];
static size_t sent_payload_length;
static int discovery_send_calls;
static bool discovery_send_fails;
static size_t cjson_allocation_attempts, cjson_free_calls, cjson_live_allocations, cjson_fail_at;
static jmp_buf loop_end;
static void xSemaphoreTake(void *m,int t){(void)m;(void)t;assert(locks==0);locks++;}
static void xSemaphoreGive(void *m){(void)m;assert(locks==1);locks--;}
static int64_t now_ms(void){return clock_ms;}
static bool connectivity_is_wifi_connected(void){return wifi;}
static bool connectivity_get_local_ipv4(char *out,size_t n){snprintf(out,n,"10.0.0.20");return wifi;}
static void process_pending_acknowledgment(void){assert(locks==0);acknowledgments++;}
static void free_http_response_buffer(http_response_buffer_t *r){free(r->data);r->data=NULL;}
static void update_known_target(const char *id,const char *room){(void)id;(void)room;}
static int fake_close(int fd){(void)fd;assert(locks==1);return 0;}
#define close fake_close
static ssize_t fake_sendto(int fd,const void *buf,size_t n,int flags,const struct sockaddr *addr,socklen_t addrlen){
 (void)flags;assert(locks==1 && fd==42 && addr!=NULL && addrlen==sizeof(struct sockaddr_in));discovery_send_calls++;
 assert(n<sizeof(sent_payload));memcpy(sent_payload,buf,n);sent_payload[n]='\0';sent_payload_length=n;
 return discovery_send_fails ? -1 : (ssize_t)n;
}
#define sendto fake_sendto
static void *test_cjson_malloc(size_t n){
 cjson_allocation_attempts++;if(cjson_fail_at!=0 && cjson_allocation_attempts==cjson_fail_at)return NULL;
 void *p=malloc(n);if(p!=NULL)cjson_live_allocations++;return p;
}
static void test_cjson_free(void *p){
 if(p!=NULL){assert(cjson_live_allocations>0);cjson_live_allocations--;cjson_free_calls++;free(p);}
}
static ssize_t fake_recvfrom(int fd,void *buf,size_t n,int flags,struct sockaddr *addr,socklen_t *len){
 (void)fd;(void)flags;(void)addr;(void)len;assert(locks==0);
 size_t size=strlen(packet);assert(size<n);memcpy(buf,packet,size);packet[0]='\0';return (ssize_t)size;
}
#define recvfrom fake_recvfrom
static void vTaskDelay(int ticks){(void)ticks;assert(locks==0);longjmp(loop_end,1);}
static esp_err_t runtime_ensure_socket_locked(int64_t ms){(void)ms;assert(locks==1);s_runtime.socket_fd=42;return ESP_OK;}
static esp_err_t perform_http_request(const char *,int,const char *,const char *,const char *,int *,http_response_buffer_t *);
'''
store_functions='\n'.join(function(f) for f in [
 'static void copy_text(', 'static bool strings_equal_ignore_case(',
 'static int compare_hub_priority(', 'static bool runtime_is_shared_locked(',
 'static bool runtime_should_proxy_locked(', 'static void clear_client_cache_locked(',
 'static void local_store_changed_locked(', 'static int64_t days_from_civil(',
 'static bool parse_iso_time(', 'static int find_message_index_by_id(',
 'static int find_insertion_slot(',
 'static bool parse_message_json(', 'static void merge_message_record_locked(',
 'static cJSON *message_record_to_json(', 'static cJSON *build_store_messages_array_locked(',
 'static esp_err_t sync_local_store_to_hub(', 'static void process_pending_store_sync(',
 'static void runtime_close_socket_locked(', 'static void runtime_update_local_address_locked(',
 'static esp_err_t runtime_send_discovery_payload_locked(',
 'static void runtime_start_discovery_locked(', 'static void runtime_become_shared_hub_locked(',
 'static void runtime_become_shared_client_locked(', 'static void runtime_reset_network_locked(',
 'static void handle_hub_signal_locked(', 'static void process_discovery_packet(',
 'static void format_message_meta_values(', 'static esp_err_t refresh_remote_client_cache(',
 'static void message_runtime_task('
])
store_tests=r'''
static message_record_t record(const char *id){
 message_record_t r={.in_use=true,.active=true,.important=true,.created_at_epoch=100,
  .has_expires_at=true,.expires_at_epoch=4102444800LL,.acknowledged_count=1};
 copy_text(r.id,sizeof(r.id),id);strcpy(r.text,"Remember the appointment");strcpy(r.sender,"Family");
 strcpy(r.target,"all");strcpy(r.created_at,"2026-10-09T10:00:00Z");
 strcpy(r.expires_at,"2100-01-01T00:00:00Z");strcpy(r.acknowledged_by[0],"bedroom");return r;
}
static esp_err_t perform_http_request(const char *url,int method,const char *body,const char *device,const char *room,int *status,http_response_buffer_t *response){
 assert(locks==0 && acknowledgments>0);
 if(method==HTTP_GET){
  refreshes++;*status=poll_status;
  response->data=malloc(strlen(poll_reply)+1);assert(response->data);strcpy(response->data,poll_reply);
  return poll_error;
 }
 assert(method==HTTP_POST);
 assert(strcmp(device,"z-clock")==0 && strcmp(room,"Bedroom")==0);requests++;
 copy_text(requested_url,sizeof(requested_url),url);
 cJSON *json=cJSON_Parse(body);assert(json);cJSON *messages=cJSON_GetObjectItem(json,"messages");assert(cJSON_IsArray(messages));
 if(apply_request){
  message_record_t sender[MESSAGE_MAX_RECORDS];memcpy(sender,s_messages,sizeof(sender));
  message_runtime_state_t sender_runtime=s_runtime;uint64_t revision=s_store_revision;
  memcpy(s_messages,remote,sizeof(remote));xSemaphoreTake(s_message_mutex,0);
  cJSON *item=NULL;cJSON_ArrayForEach(item,messages){message_record_t incoming;assert(parse_message_json(item,&incoming));merge_message_record_locked(&incoming);}
  xSemaphoreGive(s_message_mutex);memcpy(remote,s_messages,sizeof(remote));
  memcpy(s_messages,sender,sizeof(sender));s_runtime=sender_runtime;s_store_revision=revision;
 }
 cJSON_Delete(json);
 if(mutate_during_request){
  mutate_during_request=false;message_record_t arriving=record("late-arrival");
  xSemaphoreTake(s_message_mutex,0);merge_message_record_locked(&arriving);xSemaphoreGive(s_message_mutex);
 }
 *status=response_status;return request_error;
}
static void iteration(int64_t ms){clock_ms=ms;if(setjmp(loop_end)==0)message_runtime_task(NULL);assert(locks==0);}
static void heartbeat(const char *id,const char *url){
 snprintf(packet,sizeof(packet),"{\"type\":\"weather-clock-message-heartbeat\",\"version\":1,\"deviceId\":\"%s\",\"roomName\":\"Kitchen\",\"hubUrl\":\"%s\"}",id,url);
}
static void initialize(void){
 memset(s_messages,0,sizeof(s_messages));memset(remote,0,sizeof(remote));memset(&s_runtime,0,sizeof(s_runtime));
 memset(&s_client_cache,0,sizeof(s_client_cache));poll_status=200;poll_error=ESP_OK;poll_reply="{\"unreadCount\":0,\"messages\":[]}";
 s_store_revision=0;requests=refreshes=acknowledgments=0;response_status=200;request_error=ESP_OK;
 wifi=apply_request=true;mutate_during_request=false;packet[0]='\0';
 strcpy(s_runtime.sharing_mode,"shared");strcpy(s_runtime.device_id,"z-clock");strcpy(s_runtime.room_name,"Bedroom");
 s_runtime.socket_fd=42;s_runtime.role=MESSAGE_ROLE_SHARED_HUB;
 xSemaphoreTake(s_message_mutex,0);runtime_update_local_address_locked("10.0.0.20");
 message_record_t local=record("retained");merge_message_record_locked(&local);xSemaphoreGive(s_message_mutex);
}
static esp_err_t send_discovery_for_test(const char *type){
 struct sockaddr_in destination={.sin_family=AF_INET};
 xSemaphoreTake(s_message_mutex,0);esp_err_t result=runtime_send_discovery_payload_locked(type,&destination);xSemaphoreGive(s_message_mutex);return result;
}
static void reset_discovery_capture(void){discovery_send_calls=0;sent_payload_length=0;sent_payload[0]='\0';discovery_send_fails=false;}
static void test_discovery_sender(void){
 cJSON_Hooks hooks={.malloc_fn=test_cjson_malloc,.free_fn=test_cjson_free};cJSON_InitHooks(&hooks);
 initialize();copy_text(s_runtime.local_hub_url,sizeof(s_runtime.local_hub_url),"http://10.0.0.20:80");
 reset_discovery_capture();cjson_allocation_attempts=0;cjson_fail_at=0;
 assert(send_discovery_for_test("weather-clock-message-heartbeat")==ESP_OK);
 assert(discovery_send_calls==1 && sent_payload_length>0 && sent_payload_length<320);
 size_t sender_allocations=cjson_allocation_attempts;assert(sender_allocations>0 && cjson_live_allocations==0);
 cJSON *json=cJSON_Parse(sent_payload);assert(cJSON_IsObject(json));
 int field_count=0;for(cJSON *field=json->child;field!=NULL;field=field->next)field_count++;
 assert(field_count==5);
 assert(strcmp(cJSON_GetStringValue(cJSON_GetObjectItem(json,"type")),"weather-clock-message-heartbeat")==0);
 cJSON *version=cJSON_GetObjectItem(json,"version");assert(cJSON_IsNumber(version) && version->valuedouble==MESSAGE_DISCOVERY_VERSION);
 assert(strcmp(cJSON_GetStringValue(cJSON_GetObjectItem(json,"deviceId")),"z-clock")==0);
 assert(strcmp(cJSON_GetStringValue(cJSON_GetObjectItem(json,"roomName")),"Bedroom")==0);
 assert(strcmp(cJSON_GetStringValue(cJSON_GetObjectItem(json,"hubUrl")),"http://10.0.0.20:80")==0);
 cJSON_Delete(json);assert(cjson_live_allocations==0);

 copy_text(s_runtime.device_id,sizeof(s_runtime.device_id),"clock \"one\"\\gateway");
 copy_text(s_runtime.room_name,sizeof(s_runtime.room_name),"Lab \"West\"\\south");
 reset_discovery_capture();cjson_allocation_attempts=0;
 assert(send_discovery_for_test("weather-clock-message-hub")==ESP_OK && discovery_send_calls==1);
 json=cJSON_Parse(sent_payload);assert(json!=NULL);
 assert(strcmp(cJSON_GetStringValue(cJSON_GetObjectItem(json,"deviceId")),"clock \"one\"\\gateway")==0);
 assert(strcmp(cJSON_GetStringValue(cJSON_GetObjectItem(json,"roomName")),"Lab \"West\"\\south")==0);
 cJSON_Delete(json);assert(cjson_live_allocations==0);

 memset(s_runtime.device_id,'D',sizeof(s_runtime.device_id)-1);s_runtime.device_id[sizeof(s_runtime.device_id)-1]='\0';
 memset(s_runtime.room_name,'R',sizeof(s_runtime.room_name)-1);s_runtime.room_name[sizeof(s_runtime.room_name)-1]='\0';
 copy_text(s_runtime.local_hub_url,sizeof(s_runtime.local_hub_url),"http://10.0.0.20:80");
 reset_discovery_capture();cjson_allocation_attempts=0;
 assert(send_discovery_for_test("weather-clock-message-heartbeat")==ESP_OK && discovery_send_calls==1);
 assert(sent_payload_length<319 && cjson_live_allocations==0);
 size_t quote_count=319-sent_payload_length;assert(quote_count<2*(DEVICE_CONFIG_STR_LEN-1));
 for(size_t i=0;i<quote_count && i<DEVICE_CONFIG_STR_LEN-1;i++)s_runtime.device_id[i]='"';
 for(size_t i=DEVICE_CONFIG_STR_LEN-1;i<quote_count;i++)s_runtime.room_name[i-(DEVICE_CONFIG_STR_LEN-1)]='"';
 reset_discovery_capture();cjson_allocation_attempts=0;
 assert(send_discovery_for_test("weather-clock-message-heartbeat")==ESP_OK);
 assert(discovery_send_calls==1 && sent_payload_length==319 && cjson_live_allocations==0);
 if(quote_count<DEVICE_CONFIG_STR_LEN-1)s_runtime.device_id[quote_count]='"';
 else s_runtime.room_name[quote_count-(DEVICE_CONFIG_STR_LEN-1)]='"';
 reset_discovery_capture();cjson_allocation_attempts=0;
 assert(send_discovery_for_test("weather-clock-message-heartbeat")==ESP_ERR_INVALID_SIZE);
 assert(discovery_send_calls==0 && cjson_live_allocations==0);

 copy_text(s_runtime.device_id,sizeof(s_runtime.device_id),"z-clock");
 copy_text(s_runtime.room_name,sizeof(s_runtime.room_name),"Bedroom");
 copy_text(s_runtime.local_hub_url,sizeof(s_runtime.local_hub_url),"http://10.0.0.20:80");
 for(size_t fail=1;fail<=sender_allocations;fail++){
  reset_discovery_capture();cjson_allocation_attempts=0;cjson_fail_at=fail;size_t frees_before=cjson_free_calls;
  assert(send_discovery_for_test("weather-clock-message-heartbeat")==ESP_ERR_NO_MEM);
  assert(discovery_send_calls==0 && cjson_live_allocations==0);
  assert(cjson_free_calls-frees_before==fail-1);
 }
 cjson_fail_at=0;cjson_allocation_attempts=0;reset_discovery_capture();discovery_send_fails=true;
 assert(send_discovery_for_test("weather-clock-message-heartbeat")==ESP_FAIL);
 assert(discovery_send_calls==1 && cjson_live_allocations==0);
 discovery_send_fails=false;cjson_allocation_attempts=0;
 cJSON_InitHooks(NULL);
 puts("PASS: production discovery sender schema/escaping, escaped-size rejection, cJSON allocation cleanup, and send failure cleanup.");
}
static int remote_count(void){int n=0;for(size_t i=0;i<MESSAGE_MAX_RECORDS;i++)n+=remote[i].in_use;return n;}
int main(void){
 test_discovery_sender();
 // Ordinary Wi-Fi interruption resets the hub role, but not its undelivered data.
 initialize();wifi=false;iteration(1000);
 assert(s_runtime.role==MESSAGE_ROLE_SHARED_DISCOVERING && s_runtime.store_sync_pending && requests==0 && s_messages[0].in_use);
 wifi=true;heartbeat("a-clock","http://10.0.0.1:80");iteration(2000);
 assert(requests==1 && remote_count()==1 && !s_runtime.store_sync_pending && s_messages[0].in_use);
 assert(strcmp(requested_url,"http://10.0.0.1:80/api/messages/sync")==0);
 assert(strcmp(remote[0].text,"Remember the appointment")==0 && strcmp(remote[0].sender,"Family")==0);
 assert(remote[0].important && remote[0].has_expires_at && remote[0].acknowledged_count==1);
 heartbeat("a-clock","http://10.0.0.1:80");iteration(3000);assert(requests==1);
 // Already-delivered copies must not replay merely because of another reconnect.
 memset(remote,0,sizeof(remote));wifi=false;iteration(4000);wifi=true;
 heartbeat("a-clock","http://10.0.0.1:80");iteration(5000);assert(requests==1 && remote_count()==0);

 // The receiver can accept a request whose reply is lost. Retry remains idempotent.
 initialize();remote[0]=record("retained");remote[0].active=false;strcpy(remote[0].acknowledged_by[0],"kitchen");
 request_error=6;heartbeat("a-clock","http://10.0.0.1:80");iteration(1000);
 assert(requests==1 && s_runtime.store_sync_pending && remote_count()==1);
 assert(!remote[0].active && remote[0].acknowledged_count==2);
 request_error=ESP_OK;heartbeat("a-clock","http://10.0.0.1:80");iteration(15999);assert(requests==1);
 heartbeat("a-clock","http://10.0.0.1:80");iteration(16000);
 assert(requests==2 && !s_runtime.store_sync_pending && remote_count()==1);
 assert(!remote[0].active && remote[0].acknowledged_count==2);

 // HTTP rejection does not count as delivery. A changed coordinator is used.
 initialize();response_status=409;apply_request=false;heartbeat("b-clock","http://10.0.0.2:80");iteration(1000);
 assert(s_runtime.store_sync_pending && requests==1 && remote_count()==0);
 response_status=200;apply_request=true;heartbeat("a-clock","http://10.0.0.1:80");iteration(2000);
 assert(requests==2 && !s_runtime.store_sync_pending && strstr(requested_url,"10.0.0.1"));
 // A DHCP address change of the same coordinator redirects pending work.
 initialize();request_error=6;heartbeat("a-clock","http://10.0.0.1:80");iteration(1000);
 request_error=ESP_OK;heartbeat("a-clock","http://10.0.0.9:80");iteration(2000);
 assert(requests==2 && !s_runtime.store_sync_pending && strstr(requested_url,"10.0.0.9"));

 // A local operation finishing during HTTP cannot be erased by older success.
 initialize();mutate_during_request=true;heartbeat("a-clock","http://10.0.0.1:80");iteration(1000);
 assert(requests==1 && s_runtime.store_sync_pending && remote_count()==1);
 heartbeat("a-clock","http://10.0.0.1:80");iteration(16000);
 assert(requests==2 && !s_runtime.store_sync_pending && remote_count()==2);

 // An empty startup, Single, and still-discovering stores do not transfer.
 initialize();memset(s_messages,0,sizeof(s_messages));s_runtime.store_sync_pending=false;s_store_revision=0;
 heartbeat("a-clock","http://10.0.0.1:80");iteration(1000);
 assert(requests==0 && !s_runtime.store_sync_pending);
 initialize();strcpy(s_runtime.sharing_mode,"single");iteration(1000);assert(requests==0);
 initialize();s_runtime.role=MESSAGE_ROLE_SHARED_DISCOVERING;s_runtime.hub_url[0]='\0';
 iteration(1000);assert(requests==0 && s_runtime.store_sync_pending);
 // A selected hub can heartbeat normally while HTTP fails. Run the actual
 // worker and cache refresh; keep its last useful message through each failure.
 initialize();s_runtime.store_sync_pending=false;
 heartbeat("a-clock","http://10.0.0.1:80");iteration(1000);
 poll_reply="{\"unreadCount\":1,\"messages\":[{\"id\":\"cached\",\"text\":\"Call home\",\"priority\":\"important\"}]}";
 iteration(1250);assert(refreshes==1 && s_client_cache.valid);
 assert(strcmp(s_client_cache.snapshot.id,"cached")==0);
 heartbeat("a-clock","http://10.0.0.1:80");iteration(15000);assert(refreshes==1);
 // Transport, HTTP and invalid JSON failures each wait the complete interval.
 for(int failure=0;failure<3;failure++){
  int64_t due=16250+failure*15000;
  poll_error=failure==0?6:ESP_OK;poll_status=failure==1?502:200;
  poll_reply=failure==2?"invalid JSON":"{\"error\":\"hub unavailable\"}";
  heartbeat("a-clock","http://10.0.0.1:80");iteration(due);
  assert(refreshes==failure+2);
  assert(s_client_cache.valid && strcmp(s_client_cache.snapshot.id,"cached")==0);
  heartbeat("a-clock","http://10.0.0.1:80");iteration(due+250);
  heartbeat("a-clock","http://10.0.0.1:80");iteration(due+14999);
  assert(refreshes==failure+2 && s_client_cache.unread_count==1 && s_client_cache.has_important);
 }
 poll_error=ESP_OK;poll_status=200;poll_reply="{\"unreadCount\":0,\"messages\":[]}";
 heartbeat("a-clock","http://10.0.0.1:80");iteration(61250);
 assert(refreshes==5 && s_client_cache.valid && !s_client_cache.snapshot.available);
 // Explicit refreshes remain immediate (the acknowledgment path calls this).
 assert(refresh_remote_client_cache()==ESP_OK && refreshes==6);
 assert(locks==0);
 puts("PASS: failed shared polling uses 15-second spacing, retains cache, recovers, and permits explicit refresh.");
 puts("PASS: actual worker reconnect, bounded retries, 2xx delivery, current hub/address, no unchanged replay, concurrent arrival, duplicate/ack/deactivation merge, Single/discovery/empty cases.");
}
'''
with tempfile.TemporaryDirectory(prefix='wac-message-transfer-') as temp:
 p=Path(temp)/'test.c';p.write_text(store_prefix+store_defines+'\n'+store_types+store_substitutes+store_functions+store_tests);binary=Path(temp)/'test'
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-I',str(cjson),str(p),str(cjson/'cJSON.c'),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)

# Browser listing and forwarding use actual handlers. The request URI includes
# its query, as in ESP-IDF; transport and HTTP server I/O are native substitutes.
browser_prefix=store_prefix+r'''
#define HTTP_GET 0
#define HTTP_HEAD 2
#define HTTPD_RESP_USE_STRLEN -1
#define HTTPD_500_INTERNAL_SERVER_ERROR 500
#define ESP_ERR_NOT_FOUND 3
typedef int httpd_method_t;
typedef struct {const char *uri,*body;httpd_method_t method;size_t content_len,received;} httpd_req_t;
'''
browser_types=source[source.index('typedef struct {\n  bool in_use;'):source.index('\nstatic SemaphoreHandle_t')]
browser_substitutes=r'''
static message_record_t s_messages[MESSAGE_MAX_RECORDS];
static message_runtime_state_t s_runtime;
static message_client_cache_t s_client_cache;
static uint64_t s_store_revision;
static void *s_message_mutex=(void*)1;
static int locks, response_status, requests, forwarded_method;
static bool forwarded_body_present, serve_hub_response;
static const char *shared_poll_reply;
static esp_err_t messages_get_handler(httpd_req_t *req);
static int64_t now_ms(void){return 1234;}
static char response_body[16384], forwarded_url[MESSAGE_URL_LEN], forwarded_body[1024];
static void xSemaphoreTake(void *m,int t){(void)m;(void)t;assert(locks==0);locks++;}
static void xSemaphoreGive(void *m){(void)m;assert(locks==1);locks--;}
static void update_known_target(const char *id,const char *room){(void)id;(void)room;assert(locks==1);}
static int httpd_req_get_hdr_value_str(httpd_req_t *req,const char *key,char *out,size_t n){
 (void)req;(void)key;assert(n>0);out[0]='\0';return ESP_ERR_NOT_FOUND;
}
static int httpd_req_get_url_query_len(httpd_req_t *req){const char *q=strchr(req->uri,'?');return q?(int)strlen(q+1):0;}
static int httpd_req_get_url_query_str(httpd_req_t *req,char *out,size_t n){
 const char *q=strchr(req->uri,'?');if(!q)return ESP_ERR_NOT_FOUND;
 if(strlen(q+1)>=n)return ESP_ERR_INVALID_SIZE;
 snprintf(out,n,"%s",q+1);return ESP_OK;
}
static int httpd_query_key_value(const char *query,const char *key,char *out,size_t n){
 const char *part=query;size_t key_len=strlen(key);
 while(*part){
  const char *end=strchr(part,'&');if(!end)end=part+strlen(part);
  if((size_t)(end-part)>key_len && strncmp(part,key,key_len)==0 && part[key_len]=='='){
   size_t len=(size_t)(end-part)-key_len-1;if(len>=n)return ESP_ERR_INVALID_SIZE;
   memcpy(out,part+key_len+1,len);out[len]='\0';return ESP_OK;
  }
  part=*end?end+1:end;
 }
 return ESP_ERR_NOT_FOUND;
}
static int httpd_req_recv(httpd_req_t *req,char *out,size_t n){
 assert(locks==0);size_t left=req->content_len-req->received;if(n>left)n=left;
 if(n){assert(req->body);memcpy(out,req->body+req->received,n);req->received+=n;}return (int)n;
}
static int httpd_resp_set_status(httpd_req_t *req,const char *status){(void)req;response_status=atoi(status);return ESP_OK;}
static int httpd_resp_set_type(httpd_req_t *req,const char *type){(void)req;assert(strcmp(type,"application/json")==0);return ESP_OK;}
static int httpd_resp_send(httpd_req_t *req,const char *body,int n){
 (void)req;assert(locks==0);if(n==HTTPD_RESP_USE_STRLEN)n=(int)strlen(body);
 assert(n>=0 && (size_t)n<sizeof(response_body));memcpy(response_body,body,(size_t)n);response_body[n]='\0';return ESP_OK;
}
static int httpd_resp_send_err(httpd_req_t *req,int status,const char *body){
 response_status=status;return httpd_resp_send(req,body,HTTPD_RESP_USE_STRLEN);
}
static int perform_http_request(const char *url,int method,const char *body,const char *device,const char *room,int *status,http_response_buffer_t *response){
 assert(locks==0);requests++;snprintf(forwarded_url,sizeof(forwarded_url),"%s",url);
 forwarded_method=method;forwarded_body_present=body!=NULL;snprintf(forwarded_body,sizeof(forwarded_body),"%s",body?body:"");
 const char *reply;
 if(shared_poll_reply){
  assert(method==HTTP_GET && body==NULL);reply=shared_poll_reply;*status=200;
 }else if(serve_hub_response){
  assert(method==HTTP_GET && body==NULL && strcmp(device,s_runtime.device_id)==0 && strcmp(room,s_runtime.room_name)==0);
  assert(strncmp(url,"http://hub",10)==0);
  httpd_req_t req={.uri=url+10,.method=HTTP_GET};
  message_runtime_role_t saved_role=s_runtime.role;s_runtime.role=MESSAGE_ROLE_SHARED_HUB;
  assert(messages_get_handler(&req)==ESP_OK);s_runtime.role=saved_role;
  reply=response_body;*status=response_status;
 }else{
  assert(strcmp(device,"hall")==0 && strcmp(room,"Hall")==0);*status=200;reply="{\"ok\":true}";
 }
 response->data=malloc(strlen(reply)+1);assert(response->data);strcpy(response->data,reply);return ESP_OK;
}
'''
browser_functions='\n'.join(function(f) for f in [
 'static void copy_text(', 'static bool strings_equal_ignore_case(',
 'static bool runtime_is_shared_locked(', 'static bool runtime_should_proxy_locked(',
 'static void local_store_changed_locked(', 'static void clear_client_cache_locked(',
 'static int64_t days_from_civil(',
 'static bool parse_iso_time(', 'static void format_message_meta_values(',
 'static void format_message_meta(', 'static bool is_message_expired(',
 'static bool is_visible_to_device(', 'static bool is_acknowledged_for_device(',
 'static int compare_message_order(', 'static int collect_visible_message_indexes(',
 'static int find_message_index_by_id(', 'static cJSON *message_record_to_json(',
 'static esp_err_t read_request_body_alloc(', 'static const char *http_status_string(',
 'static esp_err_t send_json_response(', 'static esp_err_t send_error_response(',
 'static bool request_method_has_body(', 'static void free_http_response_buffer(',
 'static void read_request_known_device(', 'static esp_err_t proxy_request_to_hub(',
 'static esp_err_t messages_get_handler(', 'static esp_err_t message_action_handler(',
 'esp_err_t message_service_get_snapshot(', 'static esp_err_t refresh_remote_client_cache('
])
browser_tests=r'''
static message_record_t browser_record(const char *id,const char *target,bool important,time_t created){
 message_record_t r={.in_use=true,.active=true,.important=important,.created_at_epoch=created};
 copy_text(r.id,sizeof(r.id),id);copy_text(r.target,sizeof(r.target),target);copy_text(r.text,sizeof(r.text),"Remember the appointment");return r;
}
static void seed_browser_records(void){
 memset(s_messages,0,sizeof(s_messages));memset(&s_runtime,0,sizeof(s_runtime));memset(&s_client_cache,0,sizeof(s_client_cache));
 strcpy(s_runtime.device_id,"hall");strcpy(s_runtime.room_name,"Hall");s_store_revision=0;
 // Realistic management mix: broadcast, two rooms, dismissed, expired and removed.
 s_messages[0]=browser_record("broadcast","all",false,100);
 s_messages[1]=browser_record("kitchen-old","kitchen",true,200);
 s_messages[2]=browser_record("hall-new","hall",true,300);
 s_messages[3]=browser_record("hall-dismissed","hall",false,400);
 strcpy(s_messages[3].acknowledged_by[0],"hall");s_messages[3].acknowledged_count=1;
 s_messages[4]=browser_record("inactive","kitchen",true,500);s_messages[4].active=false;
 s_messages[5]=browser_record("expired","hall",true,600);s_messages[5].has_expires_at=true;s_messages[5].expires_at_epoch=1;
 s_messages[6]=browser_record("unused","hall",true,700);s_messages[6].in_use=false;
 s_messages[7]=browser_record("hall-tie-a","hall",false,100);
 s_messages[8]=browser_record("hall-tie-b","hall",false,100);
}
static cJSON *get_messages(const char *uri){
 httpd_req_t req={.uri=uri,.method=HTTP_GET};assert(messages_get_handler(&req)==ESP_OK && response_status==200);
 cJSON *json=cJSON_Parse(response_body);assert(json);return json;
}
static void expect_ids(cJSON *json,const char *device,uint32_t unread,const char *const *ids,int count){
 assert(strcmp(cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(json,"deviceId")),device)==0);
 assert(cJSON_GetObjectItemCaseSensitive(json,"unreadCount")->valueint==(int)unread);
 cJSON *messages=cJSON_GetObjectItemCaseSensitive(json,"messages");assert(cJSON_GetArraySize(messages)==count);
 for(int i=0;i<count;i++)assert(strcmp(cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(messages,i),"id")),ids[i])==0);
 cJSON_Delete(json);
}
int main(void){
 seed_browser_records();
 const char *all_ids[]={"hall-new","kitchen-old","hall-dismissed","broadcast","hall-tie-a","hall-tie-b"};
 expect_ids(get_messages("/api/messages?deviceId=all&includeDismissed=true"),"all",6,all_ids,6);
 const char *hall_ids[]={"hall-new","broadcast","hall-tie-a","hall-tie-b"};
 expect_ids(get_messages("/api/messages?deviceId=hall"),"hall",4,hall_ids,4);
 const char *hall_including_dismissed[]={"hall-new","hall-dismissed","broadcast","hall-tie-a","hall-tie-b"};
 expect_ids(get_messages("/api/messages?deviceId=hall&includeDismissed=true"),"hall",4,hall_including_dismissed,5);
 const char *kitchen_ids[]={"kitchen-old","broadcast"};
 expect_ids(get_messages("/api/messages?deviceId=kitchen&includeDismissed=true"),"kitchen",2,kitchen_ids,2);
 expect_ids(get_messages("/api/messages"),"hall",4,hall_ids,4);
 // Preserve acknowledgments by ID: the browser's 'all' has no new ack meaning.
 strcpy(s_messages[3].acknowledged_by[1],"all");s_messages[3].acknowledged_count=2;
 const char *broadcast_only[]={"broadcast"};
 expect_ids(get_messages("/api/messages?deviceId=all"),"all",1,broadcast_only,1);
 expect_ids(get_messages("/api/messages?deviceId=all&includeDismissed=false"),"all",1,broadcast_only,1);
 expect_ids(get_messages("/api/messages?deviceId=all&includeDismissed=TRUE"),"all",5,all_ids,6);
 // Execute collector directly to check unread/important outputs and truncation.
 int indexes[MESSAGE_MAX_RECORDS];uint32_t unread;bool important;
 assert(collect_visible_message_indexes("kitchen",false,false,indexes,MESSAGE_MAX_RECORDS,&unread,&important)==2);
 assert(unread==2 && important && strcmp(s_messages[indexes[0]].id,"kitchen-old")==0);
 assert(collect_visible_message_indexes("all",true,true,indexes,1,&unread,&important)==1 && unread==5 && important);
 app_message_snapshot_t snapshot;
 assert(message_service_get_snapshot("hall",&snapshot,&unread,&important)==ESP_OK);
 assert(snapshot.available && strcmp(snapshot.id,"hall-new")==0 && unread==4 && important);
 // A clock configured as 'all' is still a clock: only broadcasts appear locally.
 strcpy(s_runtime.device_id,"all");
 assert(message_service_get_snapshot(NULL,&snapshot,&unread,&important)==ESP_OK);
 assert(snapshot.available && strcmp(snapshot.id,"broadcast")==0 && unread==1 && !important);
 assert(message_service_get_snapshot("all",&snapshot,&unread,&important)==ESP_OK);
 assert(strcmp(snapshot.id,"broadcast")==0 && unread==1 && !important);
 expect_ids(get_messages("/api/messages?includeDismissed=true"),"all",5,all_ids,6);

 seed_browser_records();
 httpd_req_t deactivate={.uri="/api/messages/kitchen-old/deactivate",.method=HTTP_POST};
 assert(message_action_handler(&deactivate)==ESP_OK && response_status==200);
 assert(!s_messages[1].active && s_messages[1].acknowledged_count==0 && s_runtime.store_sync_pending && s_store_revision==1);
 const char *after_deactivate[]={"hall-new","hall-dismissed","broadcast","hall-tie-a","hall-tie-b"};
 expect_ids(get_messages("/api/messages?deviceId=all&includeDismissed=true"),"all",5,after_deactivate,5);
 expect_ids(get_messages("/api/messages?deviceId=kitchen&includeDismissed=true"),"kitchen",1,broadcast_only,1);
 assert(message_action_handler(&deactivate)==ESP_OK && s_store_revision==1);
 // Ordinary HTTP ack still changes the supplied device's ack, without global deletion.
 const char *ack_body="{\"deviceId\":\"hall\"}";
 httpd_req_t ack={.uri="/api/messages/hall-new/ack",.method=HTTP_POST,.body=ack_body,.content_len=strlen(ack_body)};
 assert(message_action_handler(&ack)==ESP_OK && response_status==200 && s_messages[2].active);
 assert(is_acknowledged_for_device(&s_messages[2],"hall") && s_store_revision==2);

 seed_browser_records();strcpy(s_runtime.sharing_mode,"shared");s_runtime.role=MESSAGE_ROLE_SHARED_CLIENT;strcpy(s_runtime.hub_url,"http://hub");
 httpd_req_t forwarded_get={.uri="/api/messages?deviceId=all&includeDismissed=true",.method=HTTP_GET};
 assert(messages_get_handler(&forwarded_get)==ESP_OK && requests==1);
 assert(strcmp(forwarded_url,"http://hub/api/messages?deviceId=all&includeDismissed=true")==0);
 assert(forwarded_method==HTTP_GET && !forwarded_body_present && response_status==200 && strcmp(response_body,"{\"ok\":true}")==0);
 // Query bytes and body must both survive proxying exactly once.
 httpd_req_t forwarded_post={.uri="/api/messages?source=House%20Messages&deviceId=hall",.method=HTTP_POST,.body=ack_body,.content_len=strlen(ack_body)};
 assert(proxy_request_to_hub(&forwarded_post)==ESP_OK && requests==2);
 assert(strcmp(forwarded_url,"http://hub/api/messages?source=House%20Messages&deviceId=hall")==0);
 assert(forwarded_method==HTTP_POST && forwarded_body_present && strcmp(forwarded_body,ack_body)==0);
 deactivate.received=0;
 assert(message_action_handler(&deactivate)==ESP_OK && requests==3);
 assert(strcmp(forwarded_url,"http://hub/api/messages/kitchen-old/deactivate")==0 && forwarded_method==HTTP_POST);
 assert(forwarded_body_present && forwarded_body[0]=='\0' && s_messages[1].active);
 ack.received=0;
 assert(message_action_handler(&ack)==ESP_OK && requests==4);
 assert(strcmp(forwarded_url,"http://hub/api/messages/hall-new/ack")==0 && strcmp(forwarded_body,ack_body)==0);
 // Execute actual shared polling against the actual hub GET handler. A configured
 // ID 'all' sends no browser includeDismissed flag and retains clock filtering.
 seed_browser_records();strcpy(s_runtime.device_id,"all");strcpy(s_runtime.sharing_mode,"shared");
 s_runtime.role=MESSAGE_ROLE_SHARED_CLIENT;strcpy(s_runtime.hub_url,"http://hub");serve_hub_response=true;
 assert(refresh_remote_client_cache()==ESP_OK && requests==5);
 assert(strcmp(forwarded_url,"http://hub/api/messages?deviceId=all")==0);
 assert(s_client_cache.valid && s_runtime.last_client_refresh_ms==1234);
 assert(message_service_get_snapshot(NULL,&snapshot,&unread,&important)==ESP_OK);
 assert(snapshot.available && strcmp(snapshot.id,"broadcast")==0 && unread==1 && !important);
 strcpy(s_runtime.device_id,"hall");
 assert(refresh_remote_client_cache()==ESP_OK && requests==6);
 assert(strcmp(forwarded_url,"http://hub/api/messages?deviceId=hall")==0);
 assert(message_service_get_snapshot(NULL,&snapshot,&unread,&important)==ESP_OK);
 assert(strcmp(snapshot.id,"hall-new")==0 && unread==4 && important);
 // A controlled remote response checks that trimming the snapshot keeps the
 // existing 40-character sender limit before metadata is formatted.
 shared_poll_reply="{\"unreadCount\":1,\"messages\":[{\"id\":\"long-sender\",\"text\":\"Call home\",\"priority\":\"important\",\"sender\":\"1234567890123456789012345678901234567890extra\",\"createdAt\":\"\"}]}";
 assert(refresh_remote_client_cache()==ESP_OK && requests==7);
 assert(message_service_get_snapshot(NULL,&snapshot,&unread,&important)==ESP_OK);
 assert(snapshot.available && strcmp(snapshot.id,"long-sender")==0 && unread==1 && important);
 assert(strcmp(snapshot.meta,"-1234567890123456789012345678901234567890")==0);
 shared_poll_reply=NULL;
 assert(locks==0);
 puts("PASS: browser all-target and per-clock listing, expired/inactive exclusion, dismissed records, ordering, local/shared clock 'all' isolation, global deactivate, and exact shared query/body forwarding.");
}
'''
with tempfile.TemporaryDirectory(prefix='wac-message-browser-') as temp:
 p=Path(temp)/'test.c';p.write_text(browser_prefix+store_defines+'\n'+browser_types+browser_substitutes+browser_functions+browser_tests);binary=Path(temp)/'test'
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-I',str(cjson),str(p),str(cjson/'cJSON.c'),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)

# Execute the page's actual refresh and button handlers with controlled fetch
# results and only the DOM operations they use. No browser or server is started.
page=Path('targets/esp32-p4/main/web/messages.html').read_text()
page_refresh=page[page.index('    async function loadTargets('):page.index('    async function initializeMessagesAdmin(')]
javascript_tests=r'''
const assert = require('node:assert/strict');
class Element {
  constructor() { this.children = []; this.style = {}; this.dataset = {}; this.listeners = {}; }
  replaceChildren() { this.children = []; }
  appendChild(child) { this.children.push(child); }
  set innerHTML(html) {
    this.text = new Element(); this.meta = new Element(); this.button = new Element();
    this.button.dataset.id = html.match(/data-id="([^"]*)"/)[1];
  }
  querySelector(selector) { return selector === '.admin-message-text' ? this.text : this.meta; }
  querySelectorAll() { return this.children.filter(child => child.button).map(child => child.button); }
  addEventListener(event, handler) { this.listeners[event] = handler; }
}
class Select extends Element {
  get value() { return this.selectedOptions[0]?.value || ''; }
  set value(value) { this.selected = this.children.find(option => option.value === value); }
  get selectedOptions() {
    const option = this.children.includes(this.selected) ? this.selected : this.children[0];
    return option ? [option] : [];
  }
}
const list = new Element(), status = new Element(), target = new Select(), send = new Element();
const fields = {'msg-target': target, 'send-message': send, 'msg-text': {value: 'Call home'}};
const document = {
  getElementById: id => id === 'active-messages' ? list : id === 'compose-status' ? status : fields[id] || null,
  createElement: () => new Element()
};
let replies = [], requests = [], errors = [];
const console = { error: (...args) => errors.push(args) };
async function fetch(url, options) {
  requests.push({url, options}); assert.ok(replies.length, 'unexpected request');
  const reply = replies.shift(); if (reply instanceof Error) throw reply;
  return reply;
}
function response(data, ok = true) { return {ok, json: async () => data}; }
function formatAdminMessageTime(value) { throw Error('unexpected timestamp in controlled data'); }
async function refresh(reply) {
  replies.push(reply); await loadActiveMessages(); assert.equal(replies.length, 0);
  assert.equal(requests.at(-1).url, '/api/messages?deviceId=all&includeDismissed=true');
  assert.deepEqual(requests.at(-1).options, {cache: 'no-store'});
}
async function targets(reply) {
  replies.push(reply); await loadTargets(); assert.equal(replies.length, 0);
  assert.equal(requests.at(-1).url, '/api/message-targets');
  assert.deepEqual(requests.at(-1).options, {cache: 'no-store'});
}
async function sentTo(expected) {
  replies.push(response({ok: true}), response({messages: []}));
  await send.listeners.click();
  assert.equal(requests.at(-2).url, '/api/messages');
  assert.equal(JSON.parse(requests.at(-2).options.body).target, expected);
  assert.equal(replies.length, 0);
}
const all = {id: 'all', label: 'All clocks'}, kitchen = {id: 'kitchen', label: 'Kitchen'}, hall = {id: 'hall', label: 'Hall'};
(async () => {
  setupMessageForm();
  await targets(response({targets: [all, kitchen, hall]}));
  assert.equal(target.value, 'all');
  target.value = 'kitchen';
  const originalOptions = target.children.slice();
  for (const failed of [
    response({error: 'Message hub unavailable'}, false),
    response({targets: [all]}, false),
    response({error: 'Message hub unavailable', targets: [all]}),
    response({}), response(null), response({targets: 'invalid'}),
    new Error('network unavailable'),
    {ok: true, json: async () => { throw Error('invalid JSON'); }}
  ]) {
    await targets(failed);
    assert.equal(target.children.length, originalOptions.length);
    originalOptions.forEach((option, index) => assert.equal(target.children[index], option));
    assert.equal(target.value, 'kitchen');
    await sentTo('kitchen');
  }
  assert.equal(errors.length, 8);
  errors = [];
  // A valid replacement hub list can temporarily lack the chosen room.
  await targets(response({targets: [all, hall]}));
  assert.equal(target.value, 'kitchen');
  assert.equal(target.selectedOptions[0].textContent, 'Kitchen');
  await sentTo('kitchen');
  await targets(response({targets: [all, {id: 'kitchen', label: 'Kitchen updated'}, hall]}));
  assert.equal(target.value, 'kitchen');
  assert.equal(target.selectedOptions[0].textContent, 'Kitchen updated');
  assert.equal(target.children.filter(option => option.value === 'kitchen').length, 1);
  // Preserve choices made during either awaited operation, including when absent.
  let resolveFetch;
  replies.push(new Promise(resolve => { resolveFetch = resolve; }));
  const pendingFetch = loadTargets();
  target.value = 'hall';
  resolveFetch(response({targets: [all, kitchen]}));
  await pendingFetch;
  assert.equal(target.value, 'hall');
  assert.equal(target.selectedOptions[0].textContent, 'Hall');
  await sentTo('hall');
  let resolveJson, jsonStarted = false;
  const pendingJsonData = new Promise(resolve => { resolveJson = resolve; });
  replies.push({ok: true, json: () => { jsonStarted = true; return pendingJsonData; }});
  const pendingJson = loadTargets();
  await Promise.resolve();
  assert.equal(jsonStarted, true);
  target.value = 'kitchen';
  resolveJson({targets: [all, hall]});
  await pendingJson;
  assert.equal(target.value, 'kitchen');
  assert.equal(target.selectedOptions[0].textContent, 'Kitchen');
  await sentTo('kitchen');
  // Ordinary explicit choices still apply; stale retained rooms disappear once unselected.
  target.value = 'all';
  await targets(response({targets: [all, hall]}));
  assert.equal(target.value, 'all');
  assert.deepEqual(target.children.map(option => option.value), ['all', 'hall']);
  await sentTo('all');
  target.value = 'hall';
  await targets(response({targets: [all, {id: 'hall', label: 'Hall updated'}]}));
  assert.equal(target.value, 'hall');
  assert.equal(target.selectedOptions[0].textContent, 'Hall updated');
  await sentTo('hall');
  await refresh(response({messages: [{id: 'room-message', text: 'Call home', target: 'bedroom', sender: 'Family'}]}));
  const original = list.children[0];
  assert.equal(original.text.textContent, 'Call home');
  assert.equal(original.meta.textContent, 'bedroom · from Family');
  assert.equal(typeof original.button.listeners.click, 'function');
  for (const failed of [
    response({error: 'Message hub unavailable'}, false),
    response({messages: []}, false),
    response({error: 'Message hub unavailable', messages: []}),
    response({}), response(null), response({messages: 'invalid'}),
    new Error('network unavailable'),
    {ok: true, json: async () => { throw Error('invalid JSON'); }}
  ]) {
    await refresh(failed);
    assert.equal(list.children.length, 1); assert.equal(list.children[0], original);
    assert.equal(status.textContent, 'Failed to load active messages');
    assert.equal(status.style.color, 'rgba(255, 164, 164, 0.96)');
  }
  assert.equal(errors.length, 8);
  await refresh(response({messages: [{id: 'broadcast', text: 'Dinner ready', target: 'all'}]}));
  assert.notEqual(list.children[0], original);
  assert.equal(list.children[0].text.textContent, 'Dinner ready');
  assert.equal(list.children[0].meta.textContent, 'All clocks');
  // Deactivation still posts the global action, then refreshes the same list.
  replies.push(response({ok: true}), response({messages: []}));
  await list.children[0].button.listeners.click();
  assert.equal(requests.at(-2).url, '/api/messages/broadcast/deactivate');
  assert.deepEqual(requests.at(-2).options, {
    method: 'POST', headers: {'Content-Type': 'application/json'}, body: '{}'
  });
  assert.equal(requests.at(-1).url, '/api/messages?deviceId=all&includeDismissed=true');
  assert.equal(replies.length, 0);
  assert.equal(list.children.length, 1);
  assert.equal(list.children[0].textContent, 'No active messages.');
  // On first load, failure must not manufacture an empty-list success either.
  list.replaceChildren(); await refresh(response({error: 'Message hub unavailable'}, false));
  assert.equal(list.children.length, 0);
  process.stdout.write('PASS: actual browser target refresh preserves chosen ID/label and Send destination on failures, incomplete lists and choices during awaits; recovers and preserves normal selection, Active Messages and Deactivate behavior.\n');
})().catch(error => { process.stderr.write(error.stack + '\n'); process.exitCode = 1; });
'''
subprocess.run(['node','-'],input=page_refresh+javascript_tests,text=True,check=True)

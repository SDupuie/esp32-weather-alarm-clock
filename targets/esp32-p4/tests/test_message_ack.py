"""Test actual acknowledgement functions with native network/RTOS substitutes. Run from repo root with IDF_PATH set."""
from pathlib import Path
import subprocess, tempfile, os
source=Path('targets/esp32-p4/main/message_service.c').read_text()
def function(signature):
 start=source.index(signature); end=source.index('\n}',start)+2
 return source[start:end]
prefix=r'''
#include <assert.h>
#include <stdbool.h>
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
#define MESSAGE_MAX_RECORDS 3
#define MESSAGE_MAX_ACKS 4
#define MESSAGE_HUB_URL_LEN 128
#define MESSAGE_URL_LEN 256
#define DEVICE_CONFIG_STR_LEN 64
#define HTTP_POST 1
#define portMAX_DELAY 0
#define ESP_LOGI(...) ((void)0)
typedef struct {bool in_use,active,has_expires_at; time_t expires_at_epoch; char id[40],target[64],acknowledged_by[4][64];size_t acknowledged_count;} message_record_t;
static message_record_t s_messages[3];
static void *s_message_mutex=(void*)1;
static struct {char hub_url[128],device_id[64],room_name[64];} s_runtime={"http://hub","clock","Room"};
typedef struct {int unused;} http_response_buffer_t;
static bool proxy;
static int locks, requests, response_status=200;
static char requested_url[256];
static void xSemaphoreTake(void *m,int t){(void)m;(void)t;assert(locks==0);locks++;}
static void xSemaphoreGive(void *m){(void)m;assert(locks==1);locks--;}
static bool runtime_should_proxy_locked(void){return proxy;}
static void copy_text(char *out,size_t n,const char *in){snprintf(out,n,"%s",in);}
static void update_known_target(const char *a,const char *b){(void)a;(void)b;}
static void free_http_response_buffer(http_response_buffer_t *r){(void)r;}
static int refresh_remote_client_cache(void){return 0;}
static int perform_http_request(const char *url,int method,const char *body,const char *device,const char *room,int *status,http_response_buffer_t *response){
 (void)method;(void)device;(void)room;(void)response;assert(locks==0);requests++;
 snprintf(requested_url,sizeof(requested_url),"%s",url);
 cJSON *json=cJSON_Parse(body);assert(strcmp(cJSON_GetStringValue(cJSON_GetObjectItem(json,"deviceId")),"clock")==0);cJSON_Delete(json);*status=response_status;return ESP_OK;
}
'''
tests=r'''
int main(void){
 // The later arrival occupies the first slot; the UI still displays the other ID.
 s_messages[0]=(message_record_t){.in_use=true,.active=true,.id="new-important",.target="all"};
 s_messages[1]=(message_record_t){.in_use=true,.active=true,.id="displayed",.target="all"};
 assert(message_service_acknowledge("clock","displayed")==ESP_OK);
 assert(s_messages[0].acknowledged_count==0 && s_messages[1].acknowledged_count==1);
 assert(message_service_acknowledge("clock","displayed")==ESP_OK && s_messages[1].acknowledged_count==1);
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
 puts("PASS: displayed ID, newer arrival untouched, repeat/missing/inactive/expired/targeted messages, invalid IDs, shared URL and remote failure.");
}
'''
body='\n'.join(function(f) for f in ['static bool is_message_expired(','static bool is_visible_to_device(','static bool is_acknowledged_for_device(','static int find_message_index_by_id(','esp_err_t message_service_acknowledge('])
cjson=Path(os.environ['IDF_PATH'])/'components/json/cJSON'
with tempfile.TemporaryDirectory(prefix='wac-message-ack-') as temp:
 p=Path(temp)/'test.c';p.write_text(prefix+body+tests);binary=Path(temp)/'test'
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-I',str(cjson),str(p),str(cjson/'cJSON.c'),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)

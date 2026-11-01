#include <assert.h>
#include <errno.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include "bepio_wifi.h"
#include "bepio_store.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
const char wifi_event_base[] = "wifi", ip_event_base[] = "ip";
static void (*worker_fn)(void *);
static esp_event_handler_t wifi_handler, ip_handler;
static unsigned event_bits, item_size;
static unsigned char queued[512], storage[1024];
static size_t storage_size;
static bool has_command, awaiting_ip, cancel_during_connect;
static int outcome, scan_error, store_error, driver_error;
static unsigned starts, scans, connects, writes;
static wifi_config_t captured_config;
static jmp_buf drained;
static void pump(void) { if (!setjmp(drained)) { worker_fn(NULL); } }
SemaphoreHandle_t xSemaphoreCreateMutex(void) { return (void *)1; }
int xSemaphoreTake(SemaphoreHandle_t m, TickType_t t) { (void)m; (void)t; return 1; }
int xSemaphoreGive(SemaphoreHandle_t m) { (void)m; return 1; }
void vSemaphoreDelete(SemaphoreHandle_t m) { (void)m; }
EventGroupHandle_t xEventGroupCreate(void) { return (void *)2; }
void vEventGroupDelete(EventGroupHandle_t e) { (void)e; }
unsigned xEventGroupSetBits(EventGroupHandle_t e, unsigned b) { (void)e; return event_bits |= b; }
unsigned xEventGroupClearBits(EventGroupHandle_t e, unsigned b) { (void)e; unsigned old = event_bits; event_bits &= ~b; return old; }
unsigned xEventGroupWaitBits(EventGroupHandle_t e, unsigned b, int clear, int all, unsigned timeout)
{
	(void)e; (void)all; (void)timeout;
	if (awaiting_ip) {
		awaiting_ip = false;
		if (cancel_during_connect) { cancel_during_connect = false; assert(bepio_wifi_set_enabled(false) == 0); }
		else if (!outcome) { ip_event_got_ip_t ip = {0}; ip_handler(NULL, IP_EVENT, IP_EVENT_STA_GOT_IP, &ip); }
		else if (outcome == 1) { wifi_event_sta_disconnected_t d = {.reason = 202}; wifi_handler(NULL, WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &d); }
	}
	unsigned value = event_bits; if (clear) { event_bits &= ~b; } return value;
}
QueueHandle_t xQueueCreate(unsigned count, unsigned size) { assert(count == 1 && size <= sizeof(queued)); item_size = size; return (void *)3; }
int xQueueOverwrite(QueueHandle_t q, const void *v) { (void)q; memcpy(queued, v, item_size); has_command = true; return 1; }
int xQueueReceive(QueueHandle_t q, void *v, TickType_t t)
{
	(void)q; (void)t; if (!has_command) { longjmp(drained, 1); }
	memcpy(v, queued, item_size); has_command = false; return 1;
}
void vQueueDelete(QueueHandle_t q) { (void)q; }
BaseType_t xTaskCreate(void (*fn)(void *), const char *n, unsigned stack, void *a, unsigned p, void *h)
{ (void)n; (void)stack; (void)a; (void)p; (void)h; worker_fn = fn; return pdPASS; }
int esp_event_loop_create_default(void) { return 0; }
int esp_event_handler_register(esp_event_base_t b, int32_t id, esp_event_handler_t cb, void *arg)
{ (void)id; (void)arg; if (b == WIFI_EVENT) { wifi_handler = cb; } else { ip_handler = cb; } return 0; }
int esp_event_handler_unregister(esp_event_base_t b, int32_t id, esp_event_handler_t cb) { (void)b; (void)id; (void)cb; return 0; }
int esp_netif_init(void) { return 0; }
esp_netif_t *esp_netif_create_default_wifi_sta(void) { static esp_netif_t n; return &n; }
int esp_wifi_init(const wifi_init_config_t *c) { (void)c; return driver_error; }
int esp_wifi_set_storage(int v) { (void)v; return 0; }
int esp_wifi_set_mode(int v) { (void)v; return 0; }
int esp_wifi_deinit(void) { return 0; }
int esp_wifi_start(void) { starts++; return 0; }
int esp_wifi_stop(void) { return esp_wifi_disconnect(); }
int esp_wifi_disconnect(void)
{
	wifi_event_sta_disconnected_t d = {.reason = 3};
	if (wifi_handler) { wifi_handler(NULL, WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &d); }
	return 0;
}
int esp_wifi_connect(void) { connects++; awaiting_ip = true; return 0; }
int esp_wifi_set_config(int i, const wifi_config_t *c) { (void)i; captured_config = *c; return 0; }
int esp_wifi_scan_start(const wifi_scan_config_t *c, bool blocking) { (void)c; assert(blocking); scans++; return scan_error; }
int esp_wifi_scan_get_ap_records(uint16_t *count, wifi_ap_record_t *records)
{
	assert(*count >= 4); *count = 4;
	records[0] = (wifi_ap_record_t){.ssid = "Home", .bssid = {1,2,3,4,5,6}, .primary = 6, .rssi = -40, .authmode = WIFI_AUTH_WPA2_PSK};
	records[1] = records[0]; records[1].rssi = -60;
	records[2] = (wifi_ap_record_t){.ssid = "Guest", .rssi = -70, .authmode = WIFI_AUTH_OPEN};
	records[3] = (wifi_ap_record_t){0}; return 0;
}
int esp_wifi_clear_ap_list(void) { return 0; }
int bepio_store_read(const char *space, const char *key, void *data, size_t *size)
{
	(void)space; (void)key; if (!storage_size) { return -ENOENT; }
	assert(*size >= storage_size); memcpy(data, storage, storage_size); *size = storage_size; return 0;
}
int bepio_store_write(const char *space, const char *key, const void *data, size_t size)
{
	(void)space; (void)key; writes++; if (store_error) { return store_error; }
	assert(size <= sizeof(storage)); memcpy(storage, data, size); storage_size = size; return 0;
}
int main(void)
{
	bepio_wifi_snapshot_t snapshot;
	bepio_wifi_snapshot(&snapshot); assert(!snapshot.enabled);
	assert(bepio_wifi_scan() == -ENETDOWN);
	assert(bepio_wifi_set_enabled(true) == 0);
	bepio_wifi_snapshot(&snapshot); assert(snapshot.busy && snapshot.scanning && !starts);
	assert(bepio_wifi_scan() == -EBUSY);
	pump(); bepio_wifi_snapshot(&snapshot);
	assert(starts == 1 && scans == 1 && snapshot.count == 2 && !snapshot.busy);
	assert(snapshot.aps[0].rssi == -40 && !snapshot.aps[0].saved);
	bepio_wifi_ap_t home = snapshot.aps[0];
	assert(bepio_wifi_connect(&home, "short") == -EINVAL);
	assert(bepio_wifi_connect(&home, "correctpass") == 0);
	assert(connects == 0 && writes == 0); pump();
	bepio_wifi_snapshot(&snapshot); assert(snapshot.connected && writes == 1);
	assert(!strcmp(snapshot.ip, "192.0.2.42") && snapshot.aps[0].saved);
	assert(captured_config.sta.channel == 6 && captured_config.sta.bssid_set);
	assert(!memcmp(captured_config.sta.bssid, home.bssid, 6));
	assert(!memcmp(captured_config.sta.password, "correctpass", 11));
	outcome = 1;
	assert(bepio_wifi_connect(&home, "wrongpass") == 0); pump();
	bepio_wifi_snapshot(&snapshot); assert(!snapshot.connected && snapshot.error == -ECONNREFUSED && writes == 1);
	outcome = 2;
	assert(bepio_wifi_connect(&home, "correctpass") == 0); pump();
	bepio_wifi_snapshot(&snapshot); assert(snapshot.error == -ETIMEDOUT && writes == 1);
	outcome = 0; cancel_during_connect = true;
	assert(bepio_wifi_connect(&home, "correctpass") == 0); pump();
	bepio_wifi_snapshot(&snapshot); assert(!snapshot.enabled && !snapshot.connected && !snapshot.busy && writes == 1);
	assert(bepio_wifi_set_enabled(true) == 0); pump();
	bepio_wifi_snapshot(&snapshot); assert(snapshot.connected && writes == 2); /* saved auto-connect */
	assert(!memcmp(captured_config.sta.password, "correctpass", 11));
	unsigned old_connects = connects;
	assert(bepio_wifi_connect(&home, "correctpass") == 0);
	assert(bepio_wifi_set_enabled(false) == 0); pump();
	assert(connects == old_connects);
	assert(bepio_wifi_set_enabled(true) == 0); pump();
	scan_error = 1; assert(bepio_wifi_scan() == 0); pump();
	bepio_wifi_snapshot(&snapshot); assert(snapshot.error == -EIO && !snapshot.busy);
	scan_error = 0;
	store_error = -EIO; assert(bepio_wifi_connect(&home, "correctpass") == 0); pump();
	bepio_wifi_snapshot(&snapshot); assert(snapshot.connected && snapshot.storage_error == -EIO);
	bepio_wifi_ap_t guest = snapshot.aps[1];
	assert(!strcmp(guest.ssid, "Guest") && !guest.saved);
	assert(bepio_wifi_connect(&guest, "") == 0); pump();
	bepio_wifi_snapshot(&snapshot);
	assert(snapshot.connected && snapshot.storage_error == -EIO && !snapshot.aps[1].saved);
	ip_handler(NULL, IP_EVENT, IP_EVENT_STA_LOST_IP, NULL);
	bepio_wifi_snapshot(&snapshot); assert(!snapshot.connected && !snapshot.ip[0]);
	ip_event_got_ip_t renewed = {0}; ip_handler(NULL, IP_EVENT, IP_EVENT_STA_GOT_IP, &renewed);
	bepio_wifi_snapshot(&snapshot); assert(snapshot.connected && snapshot.ip[0]);
	assert(bepio_wifi_disconnect() == 0); pump();
	bepio_wifi_snapshot(&snapshot); assert(!snapshot.connected);
	puts("PASS Wi-Fi service: queued radio work, dedup/BSSID, validation, DHCP success, no-save on failure, timeout/cancel, saved auto-connect, scan/storage errors");
}

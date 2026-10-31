/* Wi-Fi flow informed by NomadCast Settings; native ESP-IDF background service. */
#include "bepio_wifi.h"
#include "bepio_store.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#define CONNECTED_BIT BIT0
#define FAILED_BIT BIT1
#define CANCEL_BIT BIT2
#define SAVED_MAX 4
typedef enum { ENABLE, DISABLE, SCAN, CONNECT, DISCONNECT } operation_t;
typedef struct {
	operation_t operation;
	uint32_t generation;
	bepio_wifi_ap_t ap;
	bool use_saved;
	char password[BEPIO_WIFI_PASSWORD_MAX + 1];
} command_t;
typedef struct { char ssid[33], password[65]; } credential_t;
static struct { uint32_t version, count; credential_t entries[SAVED_MAX]; } saved;
static SemaphoreHandle_t lock;
static QueueHandle_t commands;
static EventGroupHandle_t events;
static esp_netif_t *station;
static bool driver_ready, started;
static uint32_t generation;
static bepio_wifi_snapshot_t state;
static int saved_index(const char *ssid)
{
	for (unsigned i = 0; i < saved.count; i++) {
		if (!strcmp(saved.entries[i].ssid, ssid)) { return (int)i; }
	}
	return -1;
}
static bool current(uint32_t value)
{
	xSemaphoreTake(lock, portMAX_DELAY);
	bool result = generation == value;
	xSemaphoreGive(lock);
	return result;
}
static void result(uint32_t value, int error, const char *message)
{
	xSemaphoreTake(lock, portMAX_DELAY);
	if (generation == value) {
		state.busy = state.scanning = state.connecting = false;
		state.error = error;
		snprintf(state.status, sizeof(state.status), "%s", message);
		state.revision++;
	}
	xSemaphoreGive(lock);
}
static void on_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
	(void)arg;
	if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
		wifi_event_sta_disconnected_t *event = data;
		xSemaphoreTake(lock, portMAX_DELAY);
		state.connected = false; state.ip[0] = 0;
		if (!state.busy && state.enabled && !state.error) {
			snprintf(state.status, sizeof(state.status), "Disconnected (reason %u)", event->reason);
			state.revision++;
		}
		xSemaphoreGive(lock);
		xEventGroupSetBits(events, FAILED_BIT);
	} else if (base == IP_EVENT && id == IP_EVENT_STA_LOST_IP) {
		xSemaphoreTake(lock, portMAX_DELAY);
		state.connected = false; state.ip[0] = 0;
		if (state.enabled && !state.busy) {
			snprintf(state.status, sizeof(state.status), "Wi-Fi has no IP address"); state.revision++;
		}
		xSemaphoreGive(lock);
		xEventGroupSetBits(events, FAILED_BIT);
	} else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
		ip_event_got_ip_t *event = data;
		xSemaphoreTake(lock, portMAX_DELAY);
		if (state.enabled) {
			state.connected = true; state.error = 0;
			snprintf(state.ip, sizeof(state.ip), IPSTR, IP2STR(&event->ip_info.ip));
			if (!state.busy) { snprintf(state.status, sizeof(state.status), "Connected"); }
			state.revision++;
		}
		xSemaphoreGive(lock);
		xEventGroupSetBits(events, CONNECTED_BIT);
	}
}
static esp_err_t initialize_driver(void)
{
	if (driver_ready) { return ESP_OK; }
	esp_err_t ret = esp_netif_init();
	if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) { return ret; }
	ret = esp_event_loop_create_default();
	if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) { return ret; }
	if (!station) { station = esp_netif_create_default_wifi_sta(); }
	if (!station) { return ESP_ERR_NO_MEM; }
	wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
	ret = esp_wifi_init(&config);
	if (ret) { return ret; }
	ret = esp_wifi_set_storage(WIFI_STORAGE_RAM);
	if (!ret) { ret = esp_wifi_set_mode(WIFI_MODE_STA); }
	if (!ret) { ret = esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, on_event, NULL); }
	if (!ret) { ret = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_event, NULL); }
	if (!ret) { ret = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_LOST_IP, on_event, NULL); }
	if (ret) {
		esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, on_event);
		esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, on_event);
		esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_LOST_IP, on_event);
		esp_wifi_deinit();
		return ret;
	}
	driver_ready = true;
	return ESP_OK;
}
static void load_saved(void)
{
	size_t size = sizeof(saved);
	int error = bepio_store_read("bepio_wifi", "networks", &saved, &size);
	if (error || size != sizeof(saved) || saved.version != 1 || saved.count > SAVED_MAX) {
		if (!error) { error = -EINVAL; }
		memset(&saved, 0, sizeof(saved)); saved.version = 1;
	} else {
		for (unsigned i = 0; i < saved.count; i++) {
			if (!memchr(saved.entries[i].ssid, 0, sizeof(saved.entries[i].ssid)) ||
			    !memchr(saved.entries[i].password, 0, sizeof(saved.entries[i].password))) {
				memset(&saved, 0, sizeof(saved)); saved.version = 1; error = -EINVAL; break;
			}
		}
	}
	xSemaphoreTake(lock, portMAX_DELAY);
	state.storage_error = error == -ENOENT ? 0 : error;
	xSemaphoreGive(lock);
}
static void save_credential(const command_t *command, const char *password)
{
	int old = saved_index(command->ap.ssid);
	if (old < 0) { old = saved.count < SAVED_MAX ? (int)saved.count++ : SAVED_MAX - 1; }
	for (int i = old; i > 0; i--) { saved.entries[i] = saved.entries[i - 1]; }
	snprintf(saved.entries[0].ssid, sizeof(saved.entries[0].ssid), "%s", command->ap.ssid);
	snprintf(saved.entries[0].password, sizeof(saved.entries[0].password), "%s", password);
	int error = bepio_store_write("bepio_wifi", "networks", &saved, sizeof(saved));
	xSemaphoreTake(lock, portMAX_DELAY);
	state.storage_error = error;
	for (size_t i = 0; i < state.count; i++) { state.aps[i].saved = saved_index(state.aps[i].ssid) >= 0; }
	xSemaphoreGive(lock);
}
static void connect_network(const command_t *command)
{
	char password[65] = {0};
	if (command->use_saved) {
		int at = saved_index(command->ap.ssid);
		if (at < 0) { result(command->generation, -ENOENT, "No saved password; enter it below"); return; }
		snprintf(password, sizeof(password), "%s", saved.entries[at].password);
	} else { snprintf(password, sizeof(password), "%s", command->password); }
	/* Drain the intentional disconnect before arming the next association. */
	xEventGroupClearBits(events, CONNECTED_BIT | FAILED_BIT | CANCEL_BIT);
	esp_wifi_disconnect();
	xEventGroupWaitBits(events, FAILED_BIT | CANCEL_BIT, pdTRUE, pdFALSE, pdMS_TO_TICKS(500));
	if (!current(command->generation)) { memset(password, 0, sizeof(password)); return; }
	xEventGroupClearBits(events, CONNECTED_BIT | FAILED_BIT | CANCEL_BIT);
	wifi_config_t config = {0};
	memcpy(config.sta.ssid, command->ap.ssid, strlen(command->ap.ssid));
	memcpy(config.sta.password, password, strlen(password));
	memcpy(config.sta.bssid, command->ap.bssid, 6);
	config.sta.bssid_set = true;
	config.sta.channel = command->ap.channel;
	config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
	config.sta.failure_retry_cnt = 2;
	config.sta.pmf_cfg.capable = true;
	config.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
	xSemaphoreTake(lock, portMAX_DELAY);
	state.busy = state.connecting = true;
	state.scanning = false; state.connected = false; state.ip[0] = 0;
	snprintf(state.ssid, sizeof(state.ssid), "%s", command->ap.ssid);
	snprintf(state.status, sizeof(state.status), "Connecting to %s...", command->ap.ssid);
	state.revision++;
	xSemaphoreGive(lock);
	esp_err_t ret = esp_wifi_set_config(WIFI_IF_STA, &config);
	memset(&config, 0, sizeof(config));
	if (!ret) { ret = esp_wifi_connect(); }
	EventBits_t bits = ret ? 0 : xEventGroupWaitBits(events, CONNECTED_BIT | FAILED_BIT | CANCEL_BIT,
		pdTRUE, pdFALSE, pdMS_TO_TICKS(15000));
	if (current(command->generation)) {
		xSemaphoreTake(lock, portMAX_DELAY); bool has_ip = state.connected; xSemaphoreGive(lock);
		if (!ret && has_ip && (bits & CONNECTED_BIT) && !(bits & CANCEL_BIT)) {
			save_credential(command, password);
			result(command->generation, 0, "Connected");
		} else {
			esp_wifi_disconnect();
			result(command->generation, ret ? -EIO : bits & FAILED_BIT ? -ECONNREFUSED : -ETIMEDOUT,
				"Failed: password or signal");
		}
	}
	memset(password, 0, sizeof(password));
}
static bool supported(wifi_auth_mode_t auth)
{
	return auth == WIFI_AUTH_OPEN || auth == WIFI_AUTH_WPA_PSK || auth == WIFI_AUTH_WPA2_PSK ||
		auth == WIFI_AUTH_WPA_WPA2_PSK || auth == WIFI_AUTH_WPA3_PSK || auth == WIFI_AUTH_WPA2_WPA3_PSK;
}
static void scan_networks(const command_t *command, bool auto_connect)
{
	wifi_scan_config_t config = {.show_hidden = false, .scan_type = WIFI_SCAN_TYPE_ACTIVE};
	esp_err_t ret = esp_wifi_scan_start(&config, true);
	wifi_ap_record_t records[BEPIO_WIFI_AP_MAX];
	uint16_t count = BEPIO_WIFI_AP_MAX;
	if (!ret) { ret = esp_wifi_scan_get_ap_records(&count, records); }
	else { esp_wifi_clear_ap_list(); }
	if (!current(command->generation)) { return; }
	if (ret) { result(command->generation, -EIO, "Scan failed; toggle Wi-Fi off/on to retry"); return; }
	xSemaphoreTake(lock, portMAX_DELAY);
	state.count = 0;
	for (unsigned i = 0; i < count; i++) {
		char ssid[33] = {0}; memcpy(ssid, records[i].ssid, 32);
		if (!ssid[0]) { continue; }
		bool duplicate = false;
		for (size_t j = 0; j < state.count; j++) { if (!strcmp(state.aps[j].ssid, ssid)) { duplicate = true; break; } }
		if (duplicate) { continue; }
		bepio_wifi_ap_t *ap = &state.aps[state.count++];
		*ap = (bepio_wifi_ap_t){.channel = records[i].primary, .rssi = records[i].rssi,
			.secured = records[i].authmode != WIFI_AUTH_OPEN, .supported = supported(records[i].authmode),
			.saved = saved_index(ssid) >= 0};
		memcpy(ap->ssid, ssid, sizeof(ssid)); memcpy(ap->bssid, records[i].bssid, 6);
	}
	bepio_wifi_ap_t chosen = {0};
	if (auto_connect) {
		for (unsigned i = 0; i < saved.count && !chosen.ssid[0]; i++) {
			for (size_t j = 0; j < state.count; j++) {
				if (state.aps[j].supported && !strcmp(saved.entries[i].ssid, state.aps[j].ssid)) { chosen = state.aps[j]; break; }
			}
		}
	}
	xSemaphoreGive(lock);
	if (chosen.ssid[0]) {
		command_t connect = {.operation = CONNECT, .generation = command->generation, .ap = chosen, .use_saved = true};
		connect_network(&connect);
	} else {
		xSemaphoreTake(lock, portMAX_DELAY); bool found = state.count != 0; xSemaphoreGive(lock);
		result(command->generation, 0, found ? "Select a network" : "No networks; toggle Wi-Fi off/on to retry");
	}
}
static void worker(void *unused)
{
	(void)unused;
	load_saved();
	command_t command;
	for (;;) {
		if (xQueueReceive(commands, &command, portMAX_DELAY) != pdTRUE) { continue; }
		if (!current(command.generation)) { memset(&command, 0, sizeof(command)); continue; }
		if (command.operation == DISABLE) {
			if (started) { esp_wifi_stop(); started = false; }
			xSemaphoreTake(lock, portMAX_DELAY);
			state.connected = false; state.count = 0; state.ip[0] = state.ssid[0] = 0;
			xSemaphoreGive(lock);
			result(command.generation, 0, "Wi-Fi is off");
		} else {
			esp_err_t ret = initialize_driver();
			if (!ret && !started) { ret = esp_wifi_start(); if (!ret) { started = true; } }
			if (ret) { result(command.generation, -EIO, "Wi-Fi unavailable; toggle off and retry"); }
			else if (!current(command.generation)) { /* New request supersedes this one. */ }
			else if (command.operation == ENABLE || command.operation == SCAN) { scan_networks(&command, command.operation == ENABLE); }
			else if (command.operation == CONNECT) { connect_network(&command); }
			else {
				esp_wifi_disconnect();
				xSemaphoreTake(lock, portMAX_DELAY);
				state.connected = false; state.ip[0] = 0;
				xSemaphoreGive(lock);
				result(command.generation, 0, "Disconnected");
			}
		}
		memset(&command, 0, sizeof(command));
	}
}
static int initialize(void)
{
	if (commands) { return 0; }
	lock = xSemaphoreCreateMutex();
	events = xEventGroupCreate();
	QueueHandle_t queue = xQueueCreate(1, sizeof(command_t));
	if (!lock || !events || !queue) {
		if (lock) { vSemaphoreDelete(lock); lock = NULL; }
		if (events) { vEventGroupDelete(events); events = NULL; }
		if (queue) { vQueueDelete(queue); }
		return -ENOMEM;
	}
	commands = queue;
	if (xTaskCreate(worker, "bepio_wifi", 6144, NULL, 3, NULL) != pdPASS) {
		commands = NULL; vQueueDelete(queue); vSemaphoreDelete(lock); vEventGroupDelete(events);
		lock = NULL; events = NULL; return -ENOMEM;
	}
	return 0;
}
static int request(command_t *command)
{
	xSemaphoreTake(lock, portMAX_DELAY);
	if (command->operation != ENABLE && command->operation != DISABLE && !state.enabled) {
		xSemaphoreGive(lock); return -ENETDOWN;
	}
	if (state.busy && command->operation != DISABLE) { xSemaphoreGive(lock); return -EBUSY; }
	command->generation = ++generation;
	if (command->operation == ENABLE || command->operation == DISABLE) { state.enabled = command->operation == ENABLE; }
	state.busy = true; state.error = 0;
	state.scanning = command->operation == ENABLE || command->operation == SCAN;
	state.connecting = command->operation == CONNECT;
	if (state.scanning) { state.count = 0; }
	snprintf(state.status, sizeof(state.status), "%s", command->operation == DISABLE ? "Turning Wi-Fi off..." :
		state.scanning ? "Scanning..." : state.connecting ? "Connecting..." : "Disconnecting...");
	state.revision++;
	xQueueOverwrite(commands, command);
	xSemaphoreGive(lock);
	if (command->operation == DISABLE) { xEventGroupSetBits(events, CANCEL_BIT); }
	return 0;
}
int bepio_wifi_set_enabled(bool enabled)
{
	int error = initialize(); if (error) { return error; }
	command_t command = {.operation = enabled ? ENABLE : DISABLE};
	return request(&command);
}
int bepio_wifi_scan(void)
{
	if (!commands) { return -ENETDOWN; }
	command_t command = {.operation = SCAN}; return request(&command);
}
int bepio_wifi_connect(const bepio_wifi_ap_t *ap, const char *password)
{
	if (!ap || !ap->ssid[0] || !memchr(ap->ssid, 0, sizeof(ap->ssid))) { return -EINVAL; }
	if (!ap->supported) { return -ENOTSUP; }
	if (password) {
		size_t length = strnlen(password, 65);
		if (length > 64 || (ap->secured && length < 8)) { return -EINVAL; }
		if (length == 64) {
			for (size_t i = 0; i < length; i++) {
				char c = password[i];
				if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) { return -EINVAL; }
			}
		}
	}
	if (!commands) { return -ENETDOWN; }
	command_t command = {.operation = CONNECT, .ap = *ap, .use_saved = password == NULL};
	if (password) { snprintf(command.password, sizeof(command.password), "%s", password); }
	int error = request(&command); memset(&command, 0, sizeof(command)); return error;
}
int bepio_wifi_disconnect(void)
{
	if (!commands) { return -ENETDOWN; }
	command_t command = {.operation = DISCONNECT}; return request(&command);
}
void bepio_wifi_snapshot(bepio_wifi_snapshot_t *snapshot)
{
	if (!snapshot) { return; }
	if (!lock) { *snapshot = (bepio_wifi_snapshot_t){0}; snprintf(snapshot->status, sizeof(snapshot->status), "Wi-Fi is off"); return; }
	xSemaphoreTake(lock, portMAX_DELAY); *snapshot = state; xSemaphoreGive(lock);
}

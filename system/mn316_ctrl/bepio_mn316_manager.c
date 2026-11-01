#include "bepio_mn316_manager.h"
#include "bepio_mn316_config.h"
#include "mn316.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_mac.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#if defined(CONFIG_BEPIO_APP_MESSAGES)
#include "messaging.h"
#include "bepio_account.h"
#include "../../modules/pager_id.h"
#endif

#if __has_include("emqx_local.h")
#include "emqx_local.h"
#endif
#ifndef BEPIO_EMQX_USERNAME
#define BEPIO_EMQX_USERNAME ""
#endif
#ifndef BEPIO_EMQX_PASSWORD
#define BEPIO_EMQX_PASSWORD ""
#endif


static const char *TAG __attribute__((unused)) = "mn316_manager";
static char candidate_device_id[33];

static int load_device_id(void)
{
#if defined(CONFIG_BEPIO_APP_MESSAGES)
	bepio_account_snapshot_t account;
	bepio_account_snapshot(&account);
	if (!bepio_pager_id_valid(account.id, BEPIO_MESSAGE_PHONE_MAX)) return -EINVAL;
	strcpy(candidate_device_id, account.id);
#else
	uint8_t mac[6];
	if (esp_efuse_mac_get_default(mac) != ESP_OK) return -EIO;
	uint64_t number = 0;
	for (size_t i = 0; i < sizeof(mac); i++) number = (number << 8) | mac[i];
	snprintf(candidate_device_id, sizeof(candidate_device_id), "%llu", (unsigned long long)number);
#endif
	return 0;
}

static bepio_mn316_tls_credentials_t default_credentials(void)
{
	const bepio_mn316_tls_credentials_t credentials = {
		.username = BEPIO_EMQX_USERNAME, .password = BEPIO_EMQX_PASSWORD,
		.device_id = candidate_device_id,
	};
	return credentials;
}

#if defined(CONFIG_BEPIO_APP_MESSAGES)
#include <stdio.h>
#include <time.h>
#include "mbedtls/sha256.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static bepio_message_t sending;
static bool emqx_started;
static portMUX_TYPE emqx_start_lock = portMUX_INITIALIZER_UNLOCKED;
static TaskHandle_t emqx_task;
static bepio_mn316_tls_credentials_t emqx_credentials;
static char emqx_device_id[33];


static int emqx_take(struct bepio_pager_message *message)
{
	int ret = bepio_messaging_begin(false, &sending);
	if (ret == -ENOENT || ret == -EBUSY) return 0;
	if (ret) return ret;
	memset(message, 0, sizeof(*message));
	strcpy(message->from, emqx_device_id);
	strcpy(message->to, sending.payload.phone);
	strcpy(message->text, sending.payload.body);
	return 1;
}

static void emqx_complete(bool acknowledged)
{
	int ret = bepio_messaging_complete(sending.id, sending.attempt, false, acknowledged);
	ESP_LOGI(TAG, "EMQX message %lu: %s (store=%d)", (unsigned long)sending.id,
		acknowledged ? "broker PUBACK" : "failed/unconfirmed", ret);
}

static int emqx_receive(const struct bepio_pager_message *message)
{
	if (strlen(message->from) > BEPIO_MESSAGE_PHONE_MAX ||
	    strlen(message->text) > BEPIO_MESSAGE_BODY_MAX) return 0;
	bepio_message_payload_t payload = {0};
	strcpy(payload.phone, message->from);
	strcpy(payload.name, message->from);
	strcpy(payload.body, message->text);
	/* Legacy wire format has no message ID. Hash canonical JSON for dedup. */
	char json[BEPIO_PAGER_WIRE_MAX], remote_id[41];
	unsigned char digest[32];
	if (bepio_pager_message_encode(message, json, sizeof(json)) ||
	    mbedtls_sha256((const unsigned char *)json, strlen(json), digest, 0)) return -EIO;
	for (size_t i = 0; i < 20; i++) snprintf(remote_id + i * 2, 3, "%02x", digest[i]);
	uint32_t id;
	int ret = bepio_messaging_receive(remote_id, &payload, (int64_t)time(NULL) * 1000, false, &id);
	ESP_LOGI(TAG, "EMQX inbox from %s: %d", message->from, ret);
	return ret == -EALREADY || ret == -EINVAL ? 0 : ret;
}

static void emqx_online(bool connected)
{
	bepio_messaging_set_online(connected);
	ESP_LOGI(TAG, "EMQX %s; Client ID=%s inbox=" BEPIO_MN316_TOPIC_PREFIX "%s/inbox",
		connected ? "online" : "offline", emqx_device_id, emqx_device_id);
}

static const bepio_mn316_mqtt_hooks_t emqx_hooks = {
	.online = emqx_online,
	.take = emqx_take,
	.complete = emqx_complete,
	.receive = emqx_receive,
	.record = bepio_account_record,
};

static void emqx_worker(void *context)
{
	(void)context;
	const bepio_mn316_tls_credentials_t *credentials = &emqx_credentials;
	for (;;) {
		int ret = bepio_mn316_driver_init();
		if (!ret) ret = bepio_mn316_driver_network_wait(120000);
		if (!ret) ret = bepio_mn316_manager_tls_test(credentials, 600);
		bepio_messaging_set_online(false);
		unsigned delay_ms = ret == -EACCES ? 60000 : 10000;
		ESP_LOGW(TAG, "EMQX session ended (%d); retry in %us", ret, delay_ms / 1000);
		ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(delay_ms));
	}
}

static int start_worker(const bepio_mn316_tls_credentials_t *credentials)
{
	if (!credentials || !credentials->username || !credentials->password ||
	    !credentials->username[0] || !credentials->password[0]) return -EACCES;
	if (!credentials->device_id ||
	    !bepio_pager_id_valid(credentials->device_id, BEPIO_MESSAGE_PHONE_MAX)) return -EINVAL;
	portENTER_CRITICAL(&emqx_start_lock);
	if (emqx_started) {
		TaskHandle_t task = emqx_task;
		portEXIT_CRITICAL(&emqx_start_lock);
		if (task) xTaskNotifyGive(task);
		return 0;
	}
	emqx_started = true;
	portEXIT_CRITICAL(&emqx_start_lock);
	emqx_credentials = *credentials;
	strcpy(emqx_device_id, credentials->device_id);
	emqx_credentials.device_id = emqx_device_id;
	emqx_credentials.hooks = &emqx_hooks;
	if (xTaskCreate(emqx_worker, "mn316_emqx", 16384, NULL, 5, &emqx_task) != pdPASS) {
		portENTER_CRITICAL(&emqx_start_lock);
		emqx_task = NULL;
		emqx_started = false;
		portEXIT_CRITICAL(&emqx_start_lock);
		return -ENOMEM;
	}
	return 0;
}


#endif

int bepio_mn316_manager_emqx_start(void)
{
#if defined(CONFIG_BEPIO_APP_MESSAGES)
	int ret = load_device_id();
	if (ret) return ret;
	const bepio_mn316_tls_credentials_t credentials = default_credentials();
	return start_worker(&credentials);
#else
	return -ENOTSUP;
#endif
}

int bepio_mn316_manager_init(void)
{
	return bepio_mn316_driver_init();
}

int bepio_mn316_manager_device_id(char *out, size_t capacity)
{
	if (!out || capacity < sizeof(candidate_device_id)) return -EINVAL;
	int ret = load_device_id();
	if (!ret) strcpy(out, candidate_device_id);
	return ret;
}

int bepio_mn316_manager_at(const char *command, char *reply, size_t capacity, int timeout_ms)
{
	return bepio_mn316_driver_at(command, reply, capacity, timeout_ms);
}

int bepio_mn316_manager_network_wait(int timeout_ms)
{
	return bepio_mn316_driver_network_wait(timeout_ms);
}

int bepio_mn316_manager_tls_test(const bepio_mn316_tls_credentials_t *credentials, int listen_seconds)
{
	return bepio_mn316_session_run(credentials, listen_seconds);
}

int bepio_mn316_manager_test(int listen_seconds, const char *text)
{
	int identity_ret = load_device_id();
	if (identity_ret) return identity_ret;
	bepio_mn316_tls_credentials_t credentials = default_credentials();
	credentials.test_text = text;
	if (!credentials.username[0] || !credentials.password[0]) return -EACCES;
	int ret = bepio_mn316_driver_network_wait(120000);
	if (!ret) ret = bepio_mn316_manager_tls_test(&credentials, listen_seconds);
	return ret;
}

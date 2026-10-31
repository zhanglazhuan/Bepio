#include "bepio_account.h"
#include "bepio_mutex.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#define DAY 86400
BEPIO_MUTEX_DEFINE(account_lock);
static struct {
	uint32_t version;
	char id[BEPIO_ACCOUNT_ID_MAX + 1];
	uint64_t tx, rx, seconds;
	int64_t next_change;
	bool confirmed;
} saved;
static nvs_handle_t store;
static bool ready, online, taken;
static char pending[BEPIO_ACCOUNT_ID_MAX + 1];
static int last_result, storage_error;
static uint32_t attempt;
static int64_t last_save_us;
static int64_t online_us, trusted_epoch, trusted_us, request_us;
static esp_timer_handle_t timer;
static int64_t now_us(void) { return esp_timer_get_time(); }
static void accrue(void)
{
	if (online) {
		int64_t now = now_us();
		uint64_t seconds = (now - online_us) / 1000000;
		saved.seconds += seconds;
		online_us += seconds * 1000000;
	}
}
static void persist(void)
{
	storage_error = nvs_set_blob(store, "state", &saved, sizeof(saved));
	if (!storage_error) { storage_error = nvs_commit(store); }
}
static void checkpoint(void *arg)
{
	(void)arg;
	bepio_mutex_lock(account_lock);
	accrue();
	if (pending[0] && now_us() - request_us >= 30000000) {
		pending[0] = 0; taken = false; last_result = -ETIMEDOUT;
	}
	if (now_us() - last_save_us >= 60000000) { persist(); last_save_us = now_us(); }
	bepio_mutex_unlock(account_lock);
}
int bepio_account_init(void)
{
	esp_err_t ret = nvs_flash_init();
	/* Never erase unrelated NVS data on an initialization error. */
	if (ret) { return ret; }
	ret = nvs_open("bepio_account", NVS_READWRITE, &store);
	if (ret) { return ret; }
	size_t size = sizeof(saved);
	ret = nvs_get_blob(store, "state", &saved, &size);
	if (ret == ESP_ERR_NVS_NOT_FOUND) {
		uint8_t mac[6];
		ret = esp_efuse_mac_get_default(mac);
		if (ret) { return ret; }
		saved.version = 1;
		snprintf(saved.id, sizeof(saved.id), "bepio-%02x%02x%02x%02x%02x%02x",
			 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		persist();
		if (storage_error) { return storage_error; }
	} else if (ret || size != sizeof(saved) || saved.version != 1 ||
		   !memchr(saved.id, 0, sizeof(saved.id))) {
		return ret ? ret : ESP_ERR_INVALID_STATE;
	}
	esp_timer_create_args_t args = {.callback = checkpoint, .name = "account_save"};
	ret = esp_timer_create(&args, &timer);
	if (!ret) { ret = esp_timer_start_periodic(timer, 5000000); }
	ready = !ret;
	return ret;
}
void bepio_account_snapshot(bepio_account_snapshot_t *out)
{
	if (!out) { return; }
	bepio_mutex_lock(account_lock);
	accrue();
	*out = (bepio_account_snapshot_t){.tx_bytes = saved.tx, .rx_bytes = saved.rx,
		.connected_seconds = saved.seconds, .online = online,
		.confirmed = saved.confirmed, .busy = pending[0] != 0,
		.last_result = last_result, .storage_error = storage_error};
	strcpy(out->id, saved.id); strcpy(out->pending, pending);
	int64_t remaining = saved.next_change - (trusted_epoch + (now_us() - trusted_us) / 1000000);
	if (saved.next_change && (!trusted_epoch || remaining > 0)) {
		out->cooldown_seconds = trusted_epoch ? (uint32_t)remaining : DAY;
	}
	bepio_mutex_unlock(account_lock);
}
int bepio_account_request(const char *id)
{
	if (!id) { return -EINVAL; }
	size_t n = strnlen(id, BEPIO_ACCOUNT_ID_MAX + 1);
	if (n < 4 || n > BEPIO_ACCOUNT_ID_MAX || !strncmp(id, "bepio-", 6)) { return -EINVAL; }
	for (size_t i = 0; i < n; i++) {
		if (!((id[i] >= 'a' && id[i] <= 'z') || (id[i] >= '0' && id[i] <= '9') || id[i] == '_')) { return -EINVAL; }
	}
	bepio_mutex_lock(account_lock);
	int ret = !ready ? -EIO : pending[0] ? -EBUSY : !online ? -ENETDOWN :
		!strcmp(id, saved.id) ? -EALREADY : 0;
	/* After reboot the server remains authoritative when UTC is unknown. */
	if (!ret && trusted_epoch && saved.next_change > trusted_epoch + (now_us() - trusted_us) / 1000000) { ret = -EAGAIN; }
	if (!ret) { if (++attempt == 0) { ++attempt; } strcpy(pending, id); taken = false; request_us = now_us(); }
	last_result = ret;
	bepio_mutex_unlock(account_lock);
	return ret;
}
int bepio_account_take(char id[BEPIO_ACCOUNT_ID_MAX + 1], uint32_t *token)
{
	if (!id || !token) { return -EINVAL; }
	bepio_mutex_lock(account_lock);
	int ret = pending[0] && !taken ? 0 : -ENOENT;
	if (!ret) { strcpy(id, pending); *token = attempt; taken = true; }
	bepio_mutex_unlock(account_lock);
	return ret;
}
int bepio_account_complete(const char *id, uint32_t token, int result, int64_t epoch, int64_t next)
{
	bepio_mutex_lock(account_lock);
	if (!id || !pending[0] || !taken || token != attempt || now_us() - request_us >= 30000000 || strcmp(id, pending)) {
		bepio_mutex_unlock(account_lock); return -ESTALE;
	}
	if (epoch <= 0 || epoch > INT64_MAX - DAY || (!result && next < epoch + DAY)) {
		bepio_mutex_unlock(account_lock); return -EINVAL;
	}
	trusted_epoch = epoch; trusted_us = now_us();
	if (!result) {
		/* Server has committed: retain confirmed identity in RAM even on NVS failure. */
		strcpy(saved.id, id); saved.confirmed = true; saved.next_change = next;
		persist();
	} else if (result == -EAGAIN && next > epoch) { saved.next_change = next; persist(); }
	pending[0] = 0; taken = false; last_result = result;
	bepio_mutex_unlock(account_lock);
	return storage_error ? -EIO : 0;
}
void bepio_account_record(size_t tx, size_t rx)
{
	bepio_mutex_lock(account_lock);
	if (ready) { saved.tx += tx; saved.rx += rx; }
	bepio_mutex_unlock(account_lock);
}
void bepio_account_set_online(bool available)
{
	bepio_mutex_lock(account_lock);
	if (ready && available != online) {
		accrue(); online = available; online_us = now_us();
		if (!online) { persist(); }
	}
	bepio_mutex_unlock(account_lock);
}

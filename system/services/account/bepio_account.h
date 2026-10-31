#ifndef BEPIO_ACCOUNT_H_
#define BEPIO_ACCOUNT_H_
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define BEPIO_ACCOUNT_ID_MAX 32
typedef struct {
	char id[BEPIO_ACCOUNT_ID_MAX + 1];
	char pending[BEPIO_ACCOUNT_ID_MAX + 1];
	uint64_t tx_bytes, rx_bytes, connected_seconds;
	uint32_t cooldown_seconds;
	bool online, confirmed, busy;
	int last_result, storage_error;
} bepio_account_snapshot_t;
int bepio_account_init(void);
void bepio_account_snapshot(bepio_account_snapshot_t *out);
int bepio_account_request(const char *id);
/* Transport polls take; authenticate device by immutable factory identity.
 * Server MUST atomically reserve ID, enforce 24h, and support idempotent retries.
 * Complete only authenticated replies, correlated with the exact pending ID.
 * result: 0 accepted, -EEXIST occupied, -EINVAL invalid, -EAGAIN rate limited.
 * server_epoch is trusted UTC seconds; next_change >= server_epoch + 86400
 * on acceptance. Timeout/error preserves the old ID. */
int bepio_account_take(char id[BEPIO_ACCOUNT_ID_MAX + 1], uint32_t *attempt);
int bepio_account_complete(const char *id, uint32_t attempt, int result, int64_t server_epoch,
			   int64_t next_change);
/* Call for actual JSON UTF-8 bytes at transport boundary, including retries,
 * rejected/duplicate packets and control messages; exclude simulated traffic.
 * Not carrier billing: IP/TLS/modem overhead is excluded. */
void bepio_account_record(size_t tx, size_t rx);
void bepio_account_set_online(bool online);
#endif

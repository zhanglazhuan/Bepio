#ifdef ESP_PLATFORM
#include "sdkconfig.h"
#endif
#ifndef BEPIO_MESSAGING_H_
#define BEPIO_MESSAGING_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BEPIO_MESSAGE_BODY_MAX      256
#define BEPIO_MESSAGE_NAME_MAX      48
#define BEPIO_MESSAGE_PHONE_MAX     24
#define BEPIO_MESSAGE_REMOTE_ID_MAX 48

typedef enum {
	BEPIO_MESSAGE_INBOX,
	BEPIO_MESSAGE_OUTBOX
} bepio_message_box_t;
typedef enum {
	BEPIO_MESSAGE_RECEIVED,
	BEPIO_MESSAGE_DRAFT,
	BEPIO_MESSAGE_QUEUED,
	BEPIO_MESSAGE_SENDING,
	BEPIO_MESSAGE_SENT,
	BEPIO_MESSAGE_FAILED,
} bepio_message_state_t;

typedef struct {
	char name[BEPIO_MESSAGE_NAME_MAX + 1];
	char phone[BEPIO_MESSAGE_PHONE_MAX + 1];
	char body[BEPIO_MESSAGE_BODY_MAX + 1];
} bepio_message_payload_t;

typedef struct {
	uint32_t id; /* Monotonic local ID, restored across boots; never reused. */
	uint32_t attempt;
	bepio_message_state_t state;
	bool read;
	bool simulated;
	int64_t timestamp_ms; /* Epoch time supplied by transport; 0 = unsynchronized. */
	char remote_id[BEPIO_MESSAGE_REMOTE_ID_MAX + 1]; /* Inbox only, for dedup. */
	bepio_message_payload_t payload;
} bepio_message_t;

typedef struct {
	size_t inbox, outbox, unread, queued;
	bool online;
} bepio_messaging_stats_t;

/* Service owns storage independently of app lifetime; all access is locked.
 * Reads copy records, newest first. Opening an inbox detail atomically marks it read.
 * Full mailboxes reject inserts without evicting any message.
 */
int bepio_messaging_validate(const bepio_message_payload_t *payload, bool for_send);
void bepio_messaging_stats(bepio_messaging_stats_t *stats);
int bepio_messaging_get_at(bepio_message_box_t box, size_t index, bepio_message_t *message);
int bepio_messaging_get(bepio_message_box_t box, uint32_t id, bool mark_read,
			bepio_message_t *message);
int bepio_messaging_save(uint32_t draft_id, const bepio_message_payload_t *payload, uint32_t *id);
int bepio_messaging_submit(uint32_t draft_id, const bepio_message_payload_t *payload, uint32_t *id);
int bepio_messaging_retry(uint32_t id);
int bepio_messaging_delete(bepio_message_box_t box, uint32_t id);
/* Receiver ACKs only after success; -EALREADY means recently seen sender+remote ID.
 * Deduplication covers all retained messages and the latest inbox-capacity
 * receive IDs, including deleted messages. Simulated and real IDs are isolated.
 */
int bepio_messaging_receive(const char *remote_id, const bepio_message_payload_t *payload,
			    int64_t timestamp_ms, bool simulated, uint32_t *id);
/* Transport adapter contract: no backend is installed yet. Report connectivity,
 * take a queued record, then complete only after server ACK/error/timeout.
 * Correlate both ID and attempt; retry preserves ID and immutable content.
 * Only one message may be SENDING. Simulation bypasses offline solely for tests
 * and is retained on the record; a simulated result never appears as real SENT.
 */
void bepio_messaging_set_online(bool online);
int bepio_messaging_begin(bool simulated, bepio_message_t *message);
int bepio_messaging_complete(uint32_t id, uint32_t attempt, bool simulated, bool acknowledged);
const char *bepio_messaging_state_label(const bepio_message_t *message);

int bepio_messaging_init(void);
unsigned bepio_messaging_retention_days(void);
int bepio_messaging_set_retention_days(unsigned days);
int bepio_messaging_prune(int64_t now_ms);

#endif

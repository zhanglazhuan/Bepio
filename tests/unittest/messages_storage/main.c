#include "../../../modules/message_uuid.h"
#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "messaging.h"
#include "bepio_message_file.h"
#include "bepio_store.h"

static unsigned char flash[8192];
static size_t flash_size;
static int write_error;
static unsigned saved_days;
typedef struct {
	char remote_id[BEPIO_MESSAGE_REMOTE_ID_MAX + 1];
	char phone[BEPIO_MESSAGE_PHONE_MAX + 1];
	bool simulated;
} seen_entry_t;
typedef struct {
	uint32_t magic, next_id, inbox_count, outbox_count, seen_count, seen_next;
	bepio_message_t inbox[CONFIG_BEPIO_MESSAGES_INBOX_CAPACITY];
	bepio_message_t outbox[CONFIG_BEPIO_MESSAGES_OUTBOX_CAPACITY];
	seen_entry_t seen[CONFIG_BEPIO_MESSAGES_INBOX_CAPACITY];
} snapshot_t;

int bepio_message_file_mount(void) { return 0; }
int bepio_message_file_read(void *data, size_t size)
{
	if (!flash_size) { return -ENOENT; }
	if (size != flash_size) { return -EIO; }
	memcpy(data, flash, size); return 0;
}
int bepio_message_file_write(const void *data, size_t size)
{
	if (write_error) { return write_error; }
	assert(size <= sizeof(flash));
	memcpy(flash, data, size); flash_size = size; return 0;
}
int bepio_store_read(const char *space, const char *key, void *data, size_t *size)
{
	assert(!strcmp(space, "bepio_msgs") && !strcmp(key, "days"));
	if (!saved_days) { return -ENOENT; }
	assert(*size == 1); *(unsigned char *)data = saved_days; return 0;
}
int bepio_store_write(const char *space, const char *key, const void *data, size_t size)
{
	assert(!strcmp(space, "bepio_msgs") && !strcmp(key, "days") && size == 1);
	saved_days = *(const unsigned char *)data; return 0;
}

int main(void)
{
	assert(bepio_messaging_init() == 0);
	assert(bepio_messaging_set_retention_days(14) == 0);
	bepio_message_payload_t payload = {.name = "Alice", .phone = "13800138000", .body = "Hello"};
	uint32_t incoming, outgoing;
	const int64_t now = 1800000000000LL;
	assert(bepio_messaging_receive("remote-1", &payload, now - 5LL * 86400000, false, &incoming) == 0);
	assert(bepio_messaging_submit(0, &payload, &outgoing) == 0);
	snapshot_t legacy;
	memcpy(&legacy, flash, sizeof(legacy));
	strcpy(legacy.inbox[0].payload.phone, "Pager_PC");
	strcpy(legacy.outbox[0].payload.phone, "+8613800000000");
	/* Emulate an old snapshot whose outgoing remote_id was unused. */
	memset(legacy.outbox[0].remote_id, 0, sizeof(legacy.outbox[0].remote_id));
	memcpy(flash, &legacy, sizeof(legacy));
	assert(bepio_messaging_init() == 0);
	assert(bepio_messaging_retention_days() == 14);
	bepio_message_t message;
	assert(bepio_messaging_get(BEPIO_MESSAGE_INBOX, incoming, false, &message) == 0);
	assert(!strcmp(message.payload.body, "Hello"));
	assert(!strcmp(message.payload.phone, "Pager_PC"));
	assert(bepio_messaging_get(BEPIO_MESSAGE_OUTBOX, outgoing, false, &message) == 0);
	assert(message.state == BEPIO_MESSAGE_QUEUED);
	assert(!strcmp(message.payload.phone, "8613800000000"));

	bepio_message_t first, restored;
	bepio_messaging_set_online(true);
	write_error = -EIO;
	assert(bepio_messaging_begin(false, &first) == -EIO);
	assert(bepio_messaging_get(BEPIO_MESSAGE_OUTBOX, outgoing, false, &message) == 0);
	assert(message.state == BEPIO_MESSAGE_QUEUED && !message.remote_id[0]);
	write_error = 0;
	assert(bepio_messaging_begin(false, &first) == 0);
	assert(bepio_message_uuid_valid(first.remote_id));
	/* Interrupted sending is restored to queued with the same wire identity. */
	assert(bepio_messaging_init() == 0);
	bepio_messaging_set_online(true);
	assert(bepio_messaging_begin(false, &restored) == 0);
	assert(!strcmp(first.remote_id, restored.remote_id));
	assert(first.timestamp_ms == restored.timestamp_ms);
	assert(bepio_messaging_complete(outgoing, restored.attempt, false, false) == 0);
	assert(bepio_messaging_retry(outgoing) == 0);
	assert(bepio_messaging_set_retention_days(3) == 0);
	write_error = -EIO;
	assert(bepio_messaging_prune(now) == -EIO);
	assert(bepio_messaging_get(BEPIO_MESSAGE_INBOX, incoming, false, &message) == 0);
	write_error = 0;
	assert(bepio_messaging_prune(now) == 0);
	assert(bepio_messaging_get(BEPIO_MESSAGE_INBOX, incoming, false, &message) == -ENOENT);
	assert(bepio_messaging_init() == 0);
	assert(bepio_messaging_retention_days() == 3);
	assert(bepio_messaging_get(BEPIO_MESSAGE_INBOX, incoming, false, &message) == -ENOENT);
	write_error = -EIO;
	assert(bepio_messaging_delete(BEPIO_MESSAGE_OUTBOX, outgoing) == -EIO);
	assert(bepio_messaging_get(BEPIO_MESSAGE_OUTBOX, outgoing, false, &message) == 0);
	write_error = 0;
	assert(bepio_messaging_delete(BEPIO_MESSAGE_OUTBOX, outgoing) == 0);
	assert(bepio_messaging_init() == 0);
	assert(bepio_messaging_get(BEPIO_MESSAGE_OUTBOX, outgoing, false, &message) == -ENOENT);
	assert(flash_size == sizeof(snapshot_t));
	uint32_t seen_count;
	memcpy(&seen_count, flash + offsetof(snapshot_t, seen_count), sizeof(seen_count));
	assert(seen_count);
	unsigned char *remote = flash + offsetof(snapshot_t, seen) + offsetof(seen_entry_t, remote_id);
	char remote_backup[BEPIO_MESSAGE_REMOTE_ID_MAX + 1];
	memcpy(remote_backup, remote, sizeof(remote_backup));
	memset(remote, 'X', sizeof(remote_backup));
	assert(bepio_messaging_init() == -EIO);
	memcpy(remote, remote_backup, sizeof(remote_backup));
	uint32_t exhausted = UINT32_MAX;
	assert(flash_size >= 8);
	memcpy(flash + 4, &exhausted, sizeof(exhausted));
	assert(bepio_messaging_init() == 0);
	assert(bepio_messaging_submit(0, &payload, &outgoing) == -EOVERFLOW);
	assert(bepio_messaging_receive("remote-2", &payload, now, false, &incoming) == -EOVERFLOW);
	puts("PASS: message text restore, retention, rollback, and ID exhaustion");
	return 0;
}

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "messaging.h"
#include "bepio_message_file.h"
#include "bepio_store.h"

static unsigned char flash[8192];
static size_t flash_size;
static int write_error;
static unsigned saved_days;

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
	assert(bepio_messaging_init() == 0);
	assert(bepio_messaging_retention_days() == 14);
	bepio_message_t message;
	assert(bepio_messaging_get(BEPIO_MESSAGE_INBOX, incoming, false, &message) == 0);
	assert(!strcmp(message.payload.body, "Hello"));
	assert(bepio_messaging_get(BEPIO_MESSAGE_OUTBOX, outgoing, false, &message) == 0);
	assert(message.state == BEPIO_MESSAGE_QUEUED);
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
	puts("PASS: message text snapshot restore, retention policy, deletion rollback");
	return 0;
}

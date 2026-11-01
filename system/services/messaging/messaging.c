#include "messaging.h"
#include "../../../modules/pager_id.h"
#ifdef ESP_PLATFORM
#include "bepio_account.h"
#endif
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "bepio_mutex.h"
#if defined(ESP_PLATFORM) || defined(BEPIO_MESSAGES_STORAGE)
#include "bepio_message_file.h"
#include "bepio_store.h"
#endif
#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif

static bepio_message_t inbox[CONFIG_BEPIO_MESSAGES_INBOX_CAPACITY];
static bepio_message_t outbox[CONFIG_BEPIO_MESSAGES_OUTBOX_CAPACITY];
static size_t inbox_count, outbox_count, seen_count, seen_next;
static uint32_t next_id;
static bool online;
typedef struct {
	char remote_id[BEPIO_MESSAGE_REMOTE_ID_MAX + 1];
	char phone[BEPIO_MESSAGE_PHONE_MAX + 1];
	bool simulated;
} seen_entry_t;
static seen_entry_t seen[CONFIG_BEPIO_MESSAGES_INBOX_CAPACITY];
static unsigned retention_days = 7;
BEPIO_MUTEX_DEFINE(messaging_lock);

typedef struct {
	uint32_t magic, next_id, inbox_count, outbox_count, seen_count, seen_next;
	bepio_message_t inbox[CONFIG_BEPIO_MESSAGES_INBOX_CAPACITY];
	bepio_message_t outbox[CONFIG_BEPIO_MESSAGES_OUTBOX_CAPACITY];
	seen_entry_t seen[CONFIG_BEPIO_MESSAGES_INBOX_CAPACITY];
} message_snapshot_t;

static int persist_locked(void)
{
#if defined(ESP_PLATFORM) || defined(BEPIO_MESSAGES_STORAGE)
	message_snapshot_t *copy = calloc(1, sizeof(*copy));
	if (!copy) { return -ENOMEM; }
	copy->magic = 0x3147534d;
	copy->next_id = next_id;
	copy->inbox_count = (uint32_t)inbox_count;
	copy->outbox_count = (uint32_t)outbox_count;
	copy->seen_count = (uint32_t)seen_count;
	copy->seen_next = (uint32_t)seen_next;
	memcpy(copy->inbox, inbox, sizeof(inbox));
	memcpy(copy->outbox, outbox, sizeof(outbox));
	memcpy(copy->seen, seen, sizeof(seen));
	int ret = bepio_message_file_write(copy, sizeof(*copy));
	free(copy);
	return ret;
#else
	return 0;
#endif
}

static int64_t clock_ms(void)
{
	time_t now = time(NULL);
	return now >= 1704067200 ? (int64_t)now * 1000 : 0;
}

#ifdef ESP_PLATFORM
static void retention_worker(void *context)
{
	(void)context;
	for (;;) {
		vTaskDelay(pdMS_TO_TICKS(3600000));
		(void)bepio_messaging_prune(clock_ms());
	}
}

static int start_retention_worker(void)
{
	return xTaskCreate(retention_worker, "message_expiry", 3072, NULL, 2, NULL) == pdPASS ? 0 : -ENOMEM;
}
#elif defined(BEPIO_MESSAGES_STORAGE)
static int start_retention_worker(void) { return 0; }
#endif

unsigned bepio_messaging_retention_days(void)
{
	return retention_days;
}

int bepio_messaging_prune(int64_t now_ms)
{
	if (now_ms < 1704067200000LL) { return 0; }
	bepio_mutex_lock(messaging_lock);
	int64_t cutoff = now_ms - (int64_t)retention_days * 86400000;
	bool changed = false;
	for (size_t i = 0; i < inbox_count; i++) {
		changed |= inbox[i].timestamp_ms > 0 && inbox[i].timestamp_ms < cutoff;
	}
	for (size_t i = 0; i < outbox_count; i++) {
		changed |= outbox[i].timestamp_ms > 0 && outbox[i].timestamp_ms < cutoff &&
			outbox[i].state != BEPIO_MESSAGE_SENDING && outbox[i].state != BEPIO_MESSAGE_QUEUED;
	}
	if (!changed) { bepio_mutex_unlock(messaging_lock); return 0; }
	message_snapshot_t *before = malloc(sizeof(*before));
	if (!before) { bepio_mutex_unlock(messaging_lock); return -ENOMEM; }
	before->inbox_count = inbox_count;
	before->outbox_count = outbox_count;
	memcpy(before->inbox, inbox, sizeof(inbox));
	memcpy(before->outbox, outbox, sizeof(outbox));
	for (int box = BEPIO_MESSAGE_INBOX; box <= BEPIO_MESSAGE_OUTBOX; box++) {
		bepio_message_t *items = box == BEPIO_MESSAGE_INBOX ? inbox : outbox;
		size_t *count = box == BEPIO_MESSAGE_INBOX ? &inbox_count : &outbox_count;
		for (size_t i = 0; i < *count;) {
			if (items[i].timestamp_ms > 0 && items[i].timestamp_ms < cutoff &&
			    items[i].state != BEPIO_MESSAGE_SENDING && items[i].state != BEPIO_MESSAGE_QUEUED) {
				memmove(&items[i], &items[i + 1], (*count - i - 1) * sizeof(*items));
				memset(&items[--*count], 0, sizeof(*items));
			} else { i++; }
		}
	}
	int ret = persist_locked();
	if (ret) {
		inbox_count = before->inbox_count;
		outbox_count = before->outbox_count;
		memcpy(inbox, before->inbox, sizeof(inbox));
		memcpy(outbox, before->outbox, sizeof(outbox));
	}
	free(before);
	bepio_mutex_unlock(messaging_lock);
	return ret;
}

int bepio_messaging_set_retention_days(unsigned days)
{
	if (days != 3 && days != 7 && days != 14) { return -EINVAL; }
#if defined(ESP_PLATFORM) || defined(BEPIO_MESSAGES_STORAGE)
	uint8_t value = (uint8_t)days;
	int ret = bepio_store_write("bepio_msgs", "days", &value, sizeof(value));
	if (ret) { return ret; }
#endif
	retention_days = days;
	return bepio_messaging_prune(clock_ms());
}

/* Bounded canonical UTF-8 validation, including overlong/surrogate rejection. */
static int text_valid(const char *text, size_t max, bool multiline, bool required)
{
	size_t n = 0;
	bool content = false;

	if (!text) {
		return -EINVAL;
	}
	while (n <= max && text[n]) {
		n++;
	}
	if (n > max) {
		return -ENAMETOOLONG;
	}
	for (size_t i = 0; i < n;) {
		unsigned char c = (unsigned char)text[i];
		size_t width;

		if (c < 0x80) {
			if (c == 0x7f || (c < 0x20 && !(multiline && (c == '\n' || c == '\t')))) {
				return -EINVAL;
			}
			content |= c != ' ' && c != '\n' && c != '\t';
			i++;
			continue;
		}
		width = c >= 0xc2 && c <= 0xdf   ? 2
			: c >= 0xe0 && c <= 0xef ? 3
			: c >= 0xf0 && c <= 0xf4 ? 4
						 : 0;
		if (!width || i + width > n) {
			return -EINVAL;
		}
		for (size_t j = 1; j < width; j++) {
			unsigned char t = (unsigned char)text[i + j];
			if (t < 0x80 || t > 0xbf) {
				return -EINVAL;
			}
		}
		unsigned char second = (unsigned char)text[i + 1];
		if ((c == 0xe0 && second < 0xa0) || (c == 0xed && second > 0x9f) ||
		    (c == 0xf0 && second < 0x90) || (c == 0xf4 && second > 0x8f)) {
			return -EINVAL;
		}
		content = true;
		i += width;
	}
	return required && !content ? -EINVAL : 0;
}

int bepio_messaging_validate(const bepio_message_payload_t *p, bool for_send)
{
	int ret;
	size_t n = 0;

	if (!p) {
		return -EINVAL;
	}
	ret = text_valid(p->name, BEPIO_MESSAGE_NAME_MAX, false, false);
	if (ret < 0) {
		return ret;
	}
	while (n <= BEPIO_MESSAGE_PHONE_MAX && p->phone[n]) {
		n++;
	}
	if (n > BEPIO_MESSAGE_PHONE_MAX) {
		return -ENAMETOOLONG;
	}
	if (!n && for_send) {
		return -EINVAL;
	}
	if (!n || bepio_pager_id_valid(p->phone, BEPIO_MESSAGE_PHONE_MAX)) {
		return text_valid(p->body, BEPIO_MESSAGE_BODY_MAX, true, for_send);
	}
	return -EINVAL;
}

static int find(bepio_message_box_t box, uint32_t id, bepio_message_t **entry)
{
	bepio_message_t *items;
	size_t count;

	if (box != BEPIO_MESSAGE_INBOX && box != BEPIO_MESSAGE_OUTBOX) {
		return -EINVAL;
	}
	items = box == BEPIO_MESSAGE_INBOX ? inbox : outbox;
	count = box == BEPIO_MESSAGE_INBOX ? inbox_count : outbox_count;
	for (size_t i = 0; i < count; i++) {
		if (items[i].id == id) {
			*entry = &items[i];
			return (int)i;
		}
	}
	return -ENOENT;
}

void bepio_messaging_stats(bepio_messaging_stats_t *stats)
{
	if (!stats) {
		return;
	}
	bepio_mutex_lock(messaging_lock);
	*stats = (bepio_messaging_stats_t){
		.inbox = inbox_count, .outbox = outbox_count, .online = online};
	for (size_t i = 0; i < inbox_count; i++) {
		stats->unread += !inbox[i].read;
	}
	for (size_t i = 0; i < outbox_count; i++) {
		stats->queued += outbox[i].state == BEPIO_MESSAGE_QUEUED;
	}
	bepio_mutex_unlock(messaging_lock);
}

int bepio_messaging_get_at(bepio_message_box_t box, size_t index, bepio_message_t *message)
{
	size_t count;
	int ret = 0;

	if (!message || (box != BEPIO_MESSAGE_INBOX && box != BEPIO_MESSAGE_OUTBOX)) {
		return -EINVAL;
	}
	bepio_mutex_lock(messaging_lock);
	count = box == BEPIO_MESSAGE_INBOX ? inbox_count : outbox_count;
	if (index >= count) {
		ret = -ENOENT;
	} else {
		*message = (box == BEPIO_MESSAGE_INBOX ? inbox : outbox)[count - index - 1];
	}
	bepio_mutex_unlock(messaging_lock);
	return ret;
}

int bepio_messaging_get(bepio_message_box_t box, uint32_t id, bool mark_read,
			bepio_message_t *message)
{
	bepio_message_t *entry;
	int ret;

	if (!message) {
		return -EINVAL;
	}
	bepio_mutex_lock(messaging_lock);
	ret = find(box, id, &entry);
	if (ret >= 0) {
		if (mark_read && box == BEPIO_MESSAGE_INBOX) {
			bool previous = entry->read;
			entry->read = true;
			if (!previous && persist_locked()) { entry->read = previous; ret = -EIO; goto out; }
		}
		*message = *entry;
		ret = 0;
	}
out:
	bepio_mutex_unlock(messaging_lock);
	return ret;
}

static int store(uint32_t draft_id, const bepio_message_payload_t *payload, uint32_t *id,
		 bool submit)
{
	bepio_message_t *entry;
	int ret = bepio_messaging_validate(payload, submit);

	if (ret < 0 || !id) {
		return ret < 0 ? ret : -EINVAL;
	}
	bepio_mutex_lock(messaging_lock);
	bepio_message_t previous = {0};
	bool updating = draft_id != 0;
	if (draft_id) {
		ret = find(BEPIO_MESSAGE_OUTBOX, draft_id, &entry);
		if (ret < 0) {
			goto out;
		}
		if (entry->state != BEPIO_MESSAGE_DRAFT) {
			ret = -EPERM;
			goto out;
		}
		previous = *entry;
	} else {
		if (outbox_count == CONFIG_BEPIO_MESSAGES_OUTBOX_CAPACITY) {
			ret = -ENOSPC;
			goto out;
		}
		if (!next_id || next_id == UINT32_MAX) {
			ret = -EOVERFLOW;
			goto out;
		}
		entry = &outbox[outbox_count++];
		*entry = (bepio_message_t){.id = next_id++};
	}
	entry->payload = *payload;
	entry->state = submit ? BEPIO_MESSAGE_QUEUED : BEPIO_MESSAGE_DRAFT;
	if (!entry->timestamp_ms) { entry->timestamp_ms = clock_ms(); }
	*id = entry->id;
	ret = persist_locked();
	if (ret) {
		if (updating) { *entry = previous; }
		else { memset(entry, 0, sizeof(*entry)); outbox_count--; next_id--; }
	}
out:
	bepio_mutex_unlock(messaging_lock);
	return ret;
}

int bepio_messaging_save(uint32_t draft_id, const bepio_message_payload_t *payload, uint32_t *id)
{
	return store(draft_id, payload, id, false);
}

int bepio_messaging_submit(uint32_t draft_id, const bepio_message_payload_t *payload, uint32_t *id)
{
	return store(draft_id, payload, id, true);
}

int bepio_messaging_retry(uint32_t id)
{
	bepio_message_t *entry;
	int ret;

	bepio_mutex_lock(messaging_lock);
	ret = find(BEPIO_MESSAGE_OUTBOX, id, &entry);
	if (ret >= 0) {
		if (entry->state != BEPIO_MESSAGE_FAILED) {
			ret = -EPERM;
		} else {
			bepio_message_t previous = *entry;
			entry->state = BEPIO_MESSAGE_QUEUED;
			entry->simulated = false;
			ret = persist_locked();
			if (ret) { *entry = previous; }
		}
	}
	bepio_mutex_unlock(messaging_lock);
	return ret;
}

int bepio_messaging_delete(bepio_message_box_t box, uint32_t id)
{
	bepio_message_t *entry, *items;
	size_t *count;
	int ret;

	bepio_mutex_lock(messaging_lock);
	ret = find(box, id, &entry);
	if (ret >= 0) {
		if (entry->state == BEPIO_MESSAGE_SENDING) {
			ret = -EBUSY;
			goto out;
		}
		items = box == BEPIO_MESSAGE_INBOX ? inbox : outbox;
		count = box == BEPIO_MESSAGE_INBOX ? &inbox_count : &outbox_count;
		bepio_message_t removed = *entry;
		size_t index = (size_t)ret;
		memmove(entry, entry + 1, (*count - (size_t)ret - 1) * sizeof(*entry));
		memset(&items[--(*count)], 0, sizeof(*items));
		ret = persist_locked();
		if (ret) {
			memmove(&items[index + 1], &items[index], (*count - index) * sizeof(*entry));
			items[index] = removed; (*count)++;
		}
	}
out:
	bepio_mutex_unlock(messaging_lock);
	return ret;
}

int bepio_messaging_receive(const char *remote_id, const bepio_message_payload_t *payload,
			    int64_t timestamp_ms, bool simulated, uint32_t *id)
{
	int ret = bepio_messaging_validate(payload, true);
	bepio_message_t *entry;

	if (ret < 0) {
		return ret;
	}
	if (!id || timestamp_ms < 0) {
		return -EINVAL;
	}
	ret = text_valid(remote_id, BEPIO_MESSAGE_REMOTE_ID_MAX, false, true);
	if (ret < 0) {
		return ret;
	}
	bepio_mutex_lock(messaging_lock);
	for (size_t i = 0; i < inbox_count; i++) {
		if (inbox[i].simulated == simulated && !strcmp(inbox[i].remote_id, remote_id) &&
		    !strcmp(inbox[i].payload.phone, payload->phone)) {
			ret = -EALREADY;
			goto out;
		}
	}
	for (size_t i = 0; i < seen_count; i++) {
		if (seen[i].simulated == simulated && !strcmp(seen[i].remote_id, remote_id) &&
		    !strcmp(seen[i].phone, payload->phone)) {
			ret = -EALREADY;
			goto out;
		}
	}
	if (inbox_count == CONFIG_BEPIO_MESSAGES_INBOX_CAPACITY) {
		ret = -ENOSPC;
		goto out;
	}
	if (!next_id || next_id == UINT32_MAX) {
		ret = -EOVERFLOW;
		goto out;
	}
	entry = &inbox[inbox_count++];
	*entry = (bepio_message_t){.id = next_id++,
				   .state = BEPIO_MESSAGE_RECEIVED,
				   .payload = *payload,
				   .timestamp_ms = timestamp_ms,
				   .simulated = simulated};
	strcpy(entry->remote_id, remote_id);
	if (!entry->timestamp_ms) { entry->timestamp_ms = clock_ms(); }
	*id = entry->id;
	seen_entry_t previous_seen = seen[seen_next];
	size_t previous_seen_count = seen_count, previous_seen_next = seen_next;
	strcpy(seen[seen_next].remote_id, remote_id);
	strcpy(seen[seen_next].phone, payload->phone);
	seen[seen_next].simulated = simulated;
	seen_next = (seen_next + 1) % CONFIG_BEPIO_MESSAGES_INBOX_CAPACITY;
	if (seen_count < CONFIG_BEPIO_MESSAGES_INBOX_CAPACITY) {
		seen_count++;
	}
	ret = persist_locked();
	if (ret) {
		memset(entry, 0, sizeof(*entry)); inbox_count--; next_id--;
		seen[previous_seen_next] = previous_seen;
		seen_next = previous_seen_next;
		seen_count = previous_seen_count;
	}
out:
	bepio_mutex_unlock(messaging_lock);
	return ret;
}

void bepio_messaging_set_online(bool available)
{
#ifdef ESP_PLATFORM
	bepio_account_set_online(available);
#endif
	bepio_mutex_lock(messaging_lock);
	online = available;
	bepio_mutex_unlock(messaging_lock);
}

int bepio_messaging_begin(bool simulated, bepio_message_t *message)
{
	int ret = -ENOENT;

	if (!message) {
		return -EINVAL;
	}
	bepio_mutex_lock(messaging_lock);
	if (!simulated && !online) {
		ret = -ENETDOWN;
		goto out;
	}
	for (size_t i = 0; i < outbox_count; i++) {
		if (outbox[i].state == BEPIO_MESSAGE_SENDING) {
			ret = -EBUSY;
			goto out;
		}
	}
	for (size_t i = 0; i < outbox_count; i++) {
		if (outbox[i].state == BEPIO_MESSAGE_QUEUED) {
			if (outbox[i].attempt == UINT32_MAX) {
				ret = -EOVERFLOW;
				break;
			}
			bepio_message_t previous = outbox[i];
			outbox[i].state = BEPIO_MESSAGE_SENDING;
			outbox[i].attempt++;
			outbox[i].simulated = simulated;
			int saved = persist_locked();
			if (saved) { outbox[i] = previous; ret = saved; break; }
			*message = outbox[i];
			ret = 0;
			break;
		}
	}
out:
	bepio_mutex_unlock(messaging_lock);
	return ret;
}

int bepio_messaging_complete(uint32_t id, uint32_t attempt, bool simulated, bool acknowledged)
{
	bepio_message_t *entry;
	int ret;

	bepio_mutex_lock(messaging_lock);
	ret = find(BEPIO_MESSAGE_OUTBOX, id, &entry);
	if (ret >= 0) {
		if (entry->state != BEPIO_MESSAGE_SENDING || entry->attempt != attempt ||
		    entry->simulated != simulated) {
			ret = -ESTALE;
		} else {
			bepio_message_t previous = *entry;
			entry->state = acknowledged ? BEPIO_MESSAGE_SENT : BEPIO_MESSAGE_FAILED;
			ret = persist_locked();
			if (ret) { *entry = previous; }
		}
	}
	bepio_mutex_unlock(messaging_lock);
	return ret;
}

const char *bepio_messaging_state_label(const bepio_message_t *message)
{
	switch (message->state) {
	case BEPIO_MESSAGE_RECEIVED:
		return message->simulated ? "SIMULATED-received" : "received";
	case BEPIO_MESSAGE_DRAFT:
		return "draft";
	case BEPIO_MESSAGE_QUEUED:
		return "queued";
	case BEPIO_MESSAGE_SENDING:
		return message->simulated ? "SIMULATED-sending" : "sending";
	case BEPIO_MESSAGE_SENT:
		return message->simulated ? "SIMULATED-sent" : "sent(server-ack)";
	case BEPIO_MESSAGE_FAILED:
		return message->simulated ? "SIMULATED-failed" : "failed/unconfirmed";
	default:
		return "unknown";
	}
}

#if defined(ESP_PLATFORM) || defined(BEPIO_MESSAGES_STORAGE)
static bool restore_payload_valid(bepio_message_payload_t *payload, bool required)
{
	if (payload->phone[0] == '+' &&
	    bepio_pager_id_valid(payload->phone + 1, BEPIO_MESSAGE_PHONE_MAX - 1)) {
		memmove(payload->phone, payload->phone + 1, strlen(payload->phone));
	}
	if (!bepio_messaging_validate(payload, required)) return true;
	/* Historical demo addresses remain visible in old conversations. They are
	 * never accepted for a new send under the numeric-only validation above. */
	if (strncmp(payload->phone, "Pager_", 6) || !payload->phone[6] ||
	    strspn(payload->phone + 6,
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-") !=
	    strlen(payload->phone + 6)) return false;
	char old_number[sizeof(payload->phone)];
	strcpy(old_number, payload->phone);
	strcpy(payload->phone, "1");
	bool valid = !bepio_messaging_validate(payload, required);
	strcpy(payload->phone, old_number);
	return valid;
}
#endif

int bepio_messaging_init(void)
{
	memset(inbox, 0, sizeof(inbox));
	memset(outbox, 0, sizeof(outbox));
	memset(seen, 0, sizeof(seen));
	inbox_count = outbox_count = seen_count = seen_next = 0;
	next_id = 1;
	online = false;
	retention_days = 7;
#if defined(ESP_PLATFORM) || defined(BEPIO_MESSAGES_STORAGE)
	int ret = bepio_message_file_mount();
	if (ret) { return ret; }
	uint8_t days;
	size_t size = sizeof(days);
	ret = bepio_store_read("bepio_msgs", "days", &days, &size);
	if (!ret && size == sizeof(days) && (days == 3 || days == 7 || days == 14)) { retention_days = days; }
	else if (ret && ret != -ENOENT) { return ret; }
	message_snapshot_t *copy = calloc(1, sizeof(*copy));
	if (!copy) { return -ENOMEM; }
	ret = bepio_message_file_read(copy, sizeof(*copy));
	if (ret == -ENOENT) { free(copy); return start_retention_worker(); }
	if (ret || copy->magic != 0x3147534d || !copy->next_id ||
	    copy->inbox_count > CONFIG_BEPIO_MESSAGES_INBOX_CAPACITY ||
	    copy->outbox_count > CONFIG_BEPIO_MESSAGES_OUTBOX_CAPACITY ||
	    copy->seen_count > CONFIG_BEPIO_MESSAGES_INBOX_CAPACITY ||
	    copy->seen_next >= CONFIG_BEPIO_MESSAGES_INBOX_CAPACITY) {
		free(copy); return -EIO;
	}
	for (size_t i = 0; i < copy->inbox_count; i++) {
		if (!copy->inbox[i].id || copy->inbox[i].id >= copy->next_id ||
		    copy->inbox[i].state != BEPIO_MESSAGE_RECEIVED ||
		    !memchr(copy->inbox[i].remote_id, 0, sizeof(copy->inbox[i].remote_id)) ||
		    !copy->inbox[i].remote_id[0] ||
		    !memchr(copy->inbox[i].payload.name, 0, sizeof(copy->inbox[i].payload.name)) ||
		    !memchr(copy->inbox[i].payload.phone, 0, sizeof(copy->inbox[i].payload.phone)) ||
		    !memchr(copy->inbox[i].payload.body, 0, sizeof(copy->inbox[i].payload.body)) ||
		    !restore_payload_valid(&copy->inbox[i].payload, true)) { free(copy); return -EIO; }
	}
	for (size_t i = 0; i < copy->outbox_count; i++) {
		if (!copy->outbox[i].id || copy->outbox[i].id >= copy->next_id ||
		    copy->outbox[i].state < BEPIO_MESSAGE_DRAFT ||
		    copy->outbox[i].state > BEPIO_MESSAGE_FAILED ||
		    !memchr(copy->outbox[i].payload.name, 0, sizeof(copy->outbox[i].payload.name)) ||
		    !memchr(copy->outbox[i].payload.phone, 0, sizeof(copy->outbox[i].payload.phone)) ||
		    !memchr(copy->outbox[i].payload.body, 0, sizeof(copy->outbox[i].payload.body)) ||
		    !restore_payload_valid(&copy->outbox[i].payload, false)) { free(copy); return -EIO; }
		if (copy->outbox[i].state == BEPIO_MESSAGE_SENDING) { copy->outbox[i].state = BEPIO_MESSAGE_QUEUED; }
	}
	for (size_t i = 0; i < copy->seen_count; i++) {
		if (!memchr(copy->seen[i].remote_id, 0, sizeof(copy->seen[i].remote_id)) ||
		    !memchr(copy->seen[i].phone, 0, sizeof(copy->seen[i].phone)) ||
		    !copy->seen[i].remote_id[0] || !copy->seen[i].phone[0]) {
			free(copy); return -EIO;
		}
	}
	inbox_count = copy->inbox_count;
	outbox_count = copy->outbox_count;
	seen_count = copy->seen_count;
	seen_next = copy->seen_next;
	next_id = copy->next_id;
	memcpy(inbox, copy->inbox, sizeof(inbox));
	memcpy(outbox, copy->outbox, sizeof(outbox));
	memcpy(seen, copy->seen, sizeof(seen));
	free(copy);
	ret = bepio_messaging_prune(clock_ms());
	return ret ? ret : start_retention_worker();
#endif
	return 0;
}


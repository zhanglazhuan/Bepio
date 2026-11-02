#include "model.h"
#include "bepio_mutex.h"
#include <errno.h>
#include <string.h>

static bepio_notification_t items[BEPIO_NOTIFICATIONS_CAPACITY];
static size_t count;
static uint32_t next_id = 1, revision;
BEPIO_MUTEX_DEFINE(notifications_lock);

void bepio_notifications_init(void)
{
	bepio_mutex_lock(notifications_lock);
	memset(items, 0, sizeof(items));
	count = 0;
	next_id = 1;
	revision++;
	bepio_mutex_unlock(notifications_lock);
}

static bool text_valid(const char *text, size_t capacity, bool required)
{
	const char *end = memchr(text, 0, capacity);
	return end && (!required || end != text);
}

int bepio_notifications_receive(const bepio_notification_payload_t *p)
{
	if (!p || !text_valid(p->id, sizeof(p->id), true) ||
	    !text_valid(p->from, sizeof(p->from), true) ||
	    !text_valid(p->app, sizeof(p->app), true) ||
	    !text_valid(p->title, sizeof(p->title), false) ||
	    !text_valid(p->text, sizeof(p->text), false) ||
	    !text_valid(p->datetime, sizeof(p->datetime), true) ||
	    (!p->title[0] && !p->text[0])) return -EINVAL;
	bepio_mutex_lock(notifications_lock);
	size_t at;
	for (at = 0; at < count; at++) {
		if (!strcmp(items[at].payload.id, p->id) &&
		    !strcmp(items[at].payload.from, p->from) && !strcmp(items[at].payload.app, p->app)) break;
	}
	bepio_notification_t updated;
	if (at < count) {
		updated = items[at];
		if (!strcmp(updated.payload.title, p->title) && !strcmp(updated.payload.text, p->text) &&
		    !strcmp(updated.payload.datetime, p->datetime)) {
			bepio_mutex_unlock(notifications_lock);
			return 0;
		}
	} else {
		if (next_id == UINT32_MAX) {
			bepio_mutex_unlock(notifications_lock);
			return -EOVERFLOW;
		}
		updated = (bepio_notification_t){.id = next_id++};
		if (count < BEPIO_NOTIFICATIONS_CAPACITY) count++;
		at = count - 1;
	}
	updated.payload = *p;
	updated.read = false;
	memmove(items + 1, items, at * sizeof(*items));
	items[0] = updated;
	revision++;
	bepio_mutex_unlock(notifications_lock);
	return 0;
}

int bepio_notifications_get_at(size_t index, bepio_notification_t *item)
{
	if (!item) return -EINVAL;
	bepio_mutex_lock(notifications_lock);
	int ret = index < count ? 0 : -ENOENT;
	if (!ret) *item = items[index];
	bepio_mutex_unlock(notifications_lock);
	return ret;
}

int bepio_notifications_get(uint32_t id, bool mark_read, bepio_notification_t *item)
{
	if (!item) return -EINVAL;
	int ret = -ENOENT;
	bepio_mutex_lock(notifications_lock);
	for (size_t i = 0; i < count; i++) {
		if (items[i].id != id) continue;
		if (mark_read && !items[i].read) { items[i].read = true; revision++; }
		*item = items[i]; ret = 0; break;
	}
	bepio_mutex_unlock(notifications_lock);
	return ret;
}

int bepio_notifications_delete(uint32_t id)
{
	int ret = -ENOENT;
	bepio_mutex_lock(notifications_lock);
	for (size_t i = 0; i < count; i++) {
		if (items[i].id != id) continue;
		memmove(items + i, items + i + 1, (count - i - 1) * sizeof(*items));
		memset(items + --count, 0, sizeof(*items));
		revision++; ret = 0; break;
	}
	bepio_mutex_unlock(notifications_lock);
	return ret;
}

uint32_t bepio_notifications_revision(void)
{
	bepio_mutex_lock(notifications_lock);
	uint32_t value = revision;
	bepio_mutex_unlock(notifications_lock);
	return value;
}

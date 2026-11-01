#include "model.h"
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#if defined(ESP_PLATFORM) || defined(BEPIO_CONTACTS_STORAGE)
#include "system/platform/bepio_store.h"
#include <stdlib.h>

/* Versioned fixed-width records, independent of compiler struct padding. */
#define STORE_HEADER 12
#define STORE_RECORD (4 + CONTACTS_NAME_MAX + 1 + CONTACTS_PHONE_MAX + 1 + 1)
static void put_u32(unsigned char *p, uint32_t value)
{
	for (int i = 0; i < 4; i++) { p[i] = (unsigned char)(value >> (i * 8)); }
}
static uint32_t get_u32(const unsigned char *p)
{
	return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static int persist(const contacts_model_t *model)
{
	size_t size = STORE_HEADER + model->count * STORE_RECORD;
	unsigned char *data = calloc(1, size);
	if (!data) { return -ENOMEM; }
	put_u32(data, 0x31435442); /* BTC1 */
	put_u32(data + 4, (uint32_t)model->count);
	put_u32(data + 8, model->next_id);
	for (size_t i = 0; i < model->count; i++) {
		unsigned char *p = data + STORE_HEADER + i * STORE_RECORD;
		const contacts_entry_t *e = &model->entries[i];
		put_u32(p, e->id);
		memcpy(p + 4, e->name, sizeof(e->name));
		memcpy(p + 4 + sizeof(e->name), e->phone, sizeof(e->phone));
		p[STORE_RECORD - 1] = e->favorite;
	}
	int ret = bepio_store_write("bepio_contacts", "state", data, size);
	free(data);
	return ret;
}
#else
static int persist(const contacts_model_t *model) { (void)model; return 0; }
#endif

static int find_index(const contacts_model_t *model, uint32_t id)
{
	for (size_t i = 0; i < model->count; i++) {
		if (model->entries[i].id == id) {
			return (int)i;
		}
	}
	return -ENOENT;
}

#include "../../modules/pager_id.h"

static int validate(const char *name, const char *phone)
{
	bool has_name = false;
	size_t length;

	if (!name || !phone || !name[0] || !phone[0]) {
		return -EINVAL;
	}
	for (length = 0; name[length]; length++) {
		unsigned char ch = (unsigned char)name[length];

		if (length == CONTACTS_NAME_MAX) {
			return -ENAMETOOLONG;
		}
		if (ch < 0x20 || ch == 0x7f) {
			return -EINVAL;
		}
		has_name |= ch != ' ';
	}
	if (!has_name) {
		return -EINVAL;
	}
	if (strnlen(phone, CONTACTS_PHONE_MAX + 1) > CONTACTS_PHONE_MAX) return -ENAMETOOLONG;
	return bepio_pager_id_valid(phone, CONTACTS_PHONE_MAX) ? 0 : -EINVAL;
}

static bool duplicate_phone(const contacts_model_t *model, const char *phone, uint32_t except)
{
	for (size_t i = 0; i < model->count; i++) {
		if (model->entries[i].id != except &&
		    strcmp(model->entries[i].phone, phone) == 0) {
			return true;
		}
	}
	return false;
}

void contacts_model_init(contacts_model_t *model)
{
	memset(model, 0, sizeof(*model));
	model->next_id = 1;
}

int bepio_contacts_model_restore(contacts_model_t *model)
{
#if defined(ESP_PLATFORM) || defined(BEPIO_CONTACTS_STORAGE)
	if (!model) { return -EINVAL; }
	size_t size = STORE_HEADER + CONFIG_BEPIO_CONTACTS_CAPACITY * STORE_RECORD;
	unsigned char *data = malloc(size);
	contacts_model_t *loaded = calloc(1, sizeof(*loaded));
	if (!data || !loaded) { free(data); free(loaded); return -ENOMEM; }
	int ret = bepio_store_read("bepio_contacts", "state", data, &size);
	if (ret) { goto done; }
	ret = -EINVAL;
	if (size < STORE_HEADER || get_u32(data) != 0x31435442) { goto done; }
	uint32_t count = get_u32(data + 4);
	if (count > CONFIG_BEPIO_CONTACTS_CAPACITY || size != STORE_HEADER + count * STORE_RECORD) { goto done; }
	loaded->count = count; loaded->next_id = get_u32(data + 8);
	for (size_t i = 0; i < count; i++) {
		const unsigned char *p = data + STORE_HEADER + i * STORE_RECORD;
		contacts_entry_t *e = &loaded->entries[i];
		e->id = get_u32(p);
		memcpy(e->name, p + 4, sizeof(e->name));
		memcpy(e->phone, p + 4 + sizeof(e->name), sizeof(e->phone));
		if (!memchr(e->name, 0, sizeof(e->name)) || !memchr(e->phone, 0, sizeof(e->phone)) ||
		    !e->id || (loaded->next_id && e->id >= loaded->next_id) || p[STORE_RECORD - 1] > 1) { goto done; }
		/* Existing international numbers become decimal IDs without losing the
		 * contact. Old nonnumeric demo IDs remain editable, but cannot be sent. */
		if (e->phone[0] == '+' && bepio_pager_id_valid(e->phone + 1, CONTACTS_PHONE_MAX - 1)) {
			memmove(e->phone, e->phone + 1, strlen(e->phone));
		}
		if (validate(e->name, e->phone) &&
		    !(validate(e->name, "1") == 0 && strncmp(e->phone, "Pager_", 6) == 0 && e->phone[6] &&
		      strspn(e->phone + 6, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-") ==
		      strlen(e->phone + 6))) { goto done; }
		e->favorite = p[STORE_RECORD - 1];
		for (size_t j = 0; j < i; j++) {
			if (loaded->entries[j].id == e->id || !strcmp(loaded->entries[j].phone, e->phone)) { goto done; }
		}
	}
	*model = *loaded; ret = 0;
done:
	free(loaded); free(data); return ret;
#else
	(void)model; return -ENOENT;
#endif
}

int contacts_model_get(const contacts_model_t *model, uint32_t id, contacts_entry_t *entry)
{
	int index;

	if (!model || !entry) {
		return -EINVAL;
	}
	index = find_index(model, id);
	if (index < 0) {
		return index;
	}
	*entry = model->entries[index];
	return 0;
}

int contacts_model_add(contacts_model_t *model, const char *name, const char *phone,
		       uint32_t *id)
{
	contacts_entry_t entry = {0};
	int ret;

	if (!model || !id) {
		return -EINVAL;
	}
	ret = validate(name, phone);
	if (ret < 0) {
		return ret;
	}
	if (duplicate_phone(model, phone, 0)) {
		return -EEXIST;
	}
	if (model->count == CONFIG_BEPIO_CONTACTS_CAPACITY) {
		return -ENOSPC;
	}
	if (model->next_id == 0) {
		return -EOVERFLOW;
	}
	entry.id = model->next_id;
	strcpy(entry.name, name);
	strcpy(entry.phone, phone);
	model->entries[model->count++] = entry;
	model->next_id++;
	ret = persist(model);
	if (ret) {
		model->next_id--; memset(&model->entries[--model->count], 0, sizeof(entry)); return ret;
	}
	*id = entry.id;
	return 0;
}

int contacts_model_update(contacts_model_t *model, uint32_t id,
			  const char *name, const char *phone)
{
	contacts_entry_t entry = { .id = id };
	int index;
	int ret;

	if (!model) {
		return -EINVAL;
	}
	index = find_index(model, id);
	if (index < 0) {
		return index;
	}
	ret = validate(name, phone);
	if (ret < 0) {
		return ret;
	}
	if (duplicate_phone(model, phone, id)) {
		return -EEXIST;
	}
	strcpy(entry.name, name);
	strcpy(entry.phone, phone);
	entry.favorite = model->entries[index].favorite;
	contacts_entry_t previous = model->entries[index];
	model->entries[index] = entry;
	ret = persist(model);
	if (ret) { model->entries[index] = previous; }
	return ret;
}

int bepio_contacts_model_toggle_favorite(contacts_model_t *model, uint32_t id)
{
	if (!model) { return -EINVAL; }
	int index = find_index(model, id);
	if (index < 0) { return index; }
	model->entries[index].favorite = !model->entries[index].favorite;
	int ret = persist(model);
	if (ret) { model->entries[index].favorite = !model->entries[index].favorite; }
	return ret;
}

int contacts_model_delete(contacts_model_t *model, uint32_t id)
{
	int index;

	if (!model) {
		return -EINVAL;
	}
	index = find_index(model, id);
	if (index < 0) {
		return index;
	}
	contacts_entry_t previous = model->entries[index];
	memmove(&model->entries[index], &model->entries[index + 1],
		(model->count - (size_t)index - 1) * sizeof(model->entries[0]));
	memset(&model->entries[--model->count], 0, sizeof(model->entries[0]));
	int ret = persist(model);
	if (ret) {
		memmove(&model->entries[index + 1], &model->entries[index],
			(model->count - (size_t)index) * sizeof(model->entries[0]));
		model->entries[index] = previous; model->count++;
	}
	return ret;
}

static unsigned char lower_ascii(unsigned char c)
{
	return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
}

static bool contains(const char *text, const char *query)
{
	for (; *text; text++) {
		size_t n = 0;
		while (query[n] && text[n] && lower_ascii((unsigned char)text[n]) == lower_ascii((unsigned char)query[n])) { n++; }
		if (!query[n]) { return true; }
	}
	return !*query;
}

bool contacts_model_matches(const contacts_entry_t *entry, const char *query)
{
	return entry && query && (contains(entry->name, query) || contains(entry->phone, query));
}


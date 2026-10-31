#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "apps/contacts/model.h"
#include "system/platform/bepio_store.h"

static unsigned char flash[4096];
static size_t flash_size;
static int write_error;
int bepio_store_read(const char *space, const char *key, void *data, size_t *size)
{
	assert(!strcmp(space, "bepio_contacts") && !strcmp(key, "state"));
	if (!flash_size) { return -ENOENT; }
	if (*size < flash_size) { return -EIO; }
	memcpy(data, flash, flash_size); *size = flash_size; return 0;
}
int bepio_store_write(const char *space, const char *key, const void *data, size_t size)
{
	assert(!strcmp(space, "bepio_contacts") && !strcmp(key, "state"));
	if (write_error) { return write_error; }
	assert(size <= sizeof(flash)); memcpy(flash, data, size); flash_size = size; return 0;
}
int main(void)
{
	contacts_model_t model, reboot;
	contacts_entry_t entry;
	uint32_t id, second;
	contacts_model_init(&model);
	assert(bepio_contacts_model_restore(&model) == -ENOENT);
	assert(contacts_model_add(&model, "Ava", "123", &id) == 0);
	assert(bepio_contacts_model_toggle_favorite(&model, id) == 0);
	contacts_model_init(&reboot);
	assert(bepio_contacts_model_restore(&reboot) == 0);
	assert(contacts_model_get(&reboot, id, &entry) == 0 && entry.favorite);
	assert(!strcmp(entry.name, "Ava") && !strcmp(entry.phone, "123"));
	assert(contacts_model_update(&reboot, id, "Ava Chen", "456") == 0);
	assert(bepio_contacts_model_restore(&model) == 0);
	assert(contacts_model_get(&model, id, &entry) == 0 && entry.favorite && !strcmp(entry.phone, "456"));
	write_error = -EIO;
	contacts_model_t before = model;
	assert(bepio_contacts_model_toggle_favorite(&model, id) == -EIO);
	assert(contacts_model_update(&model, id, "Lost", "789") == -EIO);
	assert(contacts_model_add(&model, "New", "789", &second) == -EIO);
	assert(contacts_model_delete(&model, id) == -EIO);
	assert(!memcmp(&model, &before, sizeof(model)));
	assert(bepio_contacts_model_restore(&reboot) == 0);
	assert(!memcmp(&model, &reboot, sizeof(model)));
	write_error = 0;
	assert(bepio_contacts_model_toggle_favorite(&model, id) == 0);
	assert(bepio_contacts_model_restore(&reboot) == 0);
	assert(contacts_model_get(&reboot, id, &entry) == 0 && !entry.favorite);
	assert(contacts_model_add(&model, "Ben", "789", &second) == 0 && second > id);
	before = model; write_error = -EIO;
	assert(contacts_model_delete(&model, id) == -EIO);
	assert(!memcmp(&model, &before, sizeof(model)));
	write_error = 0;
	assert(contacts_model_delete(&model, id) == 0);
	assert(bepio_contacts_model_restore(&reboot) == 0 && reboot.count == 1);
	assert(contacts_model_get(&reboot, id, &entry) == -ENOENT);
	assert(contacts_model_get(&reboot, second, &entry) == 0 && !entry.favorite);
	assert(contacts_model_delete(&reboot, second) == 0);
	assert(bepio_contacts_model_restore(&model) == 0 && model.count == 0);
	assert(model.next_id > second); /* Empty snapshot must not reseed demo contacts. */
	before = model;
	flash[0] ^= 1;
	assert(bepio_contacts_model_restore(&model) == -EINVAL);
	assert(!memcmp(&model, &before, sizeof(model)));
	flash[0] ^= 1; flash_size--;
	assert(bepio_contacts_model_restore(&model) == -EINVAL);
	puts("PASS: contacts Flash restore, favorites, edits, deletes, stable IDs, failed-write rollback and corrupt snapshot rejection");
	return 0;
}

#ifdef ESP_PLATFORM
#include "sdkconfig.h"
#endif
#ifndef BEPIO_CONTACTS_MODEL_H_
#define BEPIO_CONTACTS_MODEL_H_

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#define CONTACTS_NAME_MAX 48
#define CONTACTS_PHONE_MAX 24

typedef struct {
	uint32_t id;
	char name[CONTACTS_NAME_MAX + 1];
	char phone[CONTACTS_PHONE_MAX + 1];
	bool favorite;
} contacts_entry_t;

typedef struct {
	contacts_entry_t entries[CONFIG_BEPIO_CONTACTS_CAPACITY];
	size_t count;
	uint32_t next_id;
} contacts_model_t;

/* Model access is serialized by the controller. IDs are never reused during a
 * boot session. Phones contain digits with an optional leading '+'. Names are
 * UTF-8 byte strings without ASCII controls, with at least one non-space byte.
 * All validation completes before mutation. Duplicate phone strings are rejected.
 */
void contacts_model_init(contacts_model_t *model);
/* Restore Flash snapshot; -ENOENT means first boot. Does not replace on error. */
int bepio_contacts_model_restore(contacts_model_t *model);
int contacts_model_get(const contacts_model_t *model, uint32_t id, contacts_entry_t *entry);
int contacts_model_add(contacts_model_t *model, const char *name, const char *phone,
		       uint32_t *id);
int contacts_model_update(contacts_model_t *model, uint32_t id,
			  const char *name, const char *phone);
int contacts_model_delete(contacts_model_t *model, uint32_t id);
int bepio_contacts_model_toggle_favorite(contacts_model_t *model, uint32_t id);

/* Explicit opt-in demo population; normal model_init remains empty. */
bool contacts_model_matches(const contacts_entry_t *entry, const char *query);

#endif

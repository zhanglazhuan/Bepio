#ifndef BEPIO_CONTACTS_CONTROLLER_H_
#define BEPIO_CONTACTS_CONTROLLER_H_

#include "model.h"
#include "view.h"

typedef struct {
	contacts_model_t *model;
	contacts_view_t *view;
	bool active;
} contacts_controller_t;

void contacts_controller_init(contacts_controller_t *controller,
			      contacts_model_t *model, contacts_view_t *view);
void contacts_controller_deinit(contacts_controller_t *controller);
bool contacts_controller_back(contacts_controller_t *controller);
void contacts_controller_set_ui_available(contacts_controller_t *controller, bool available);
/* Read-only service access; does not require an active UI or change navigation. */
int contacts_controller_lookup(contacts_controller_t *controller, uint32_t id,
			       contacts_entry_t *entry);
int contacts_controller_lookup_at(contacts_controller_t *controller, size_t index,
				  contacts_entry_t *entry);
int contacts_controller_list(contacts_controller_t *controller, size_t *count);
int contacts_controller_get_at(contacts_controller_t *controller, size_t index,
			       contacts_entry_t *entry);
int contacts_controller_show(contacts_controller_t *controller, uint32_t id,
			     contacts_entry_t *entry);
int contacts_controller_add(contacts_controller_t *controller, const char *name,
			    const char *phone, uint32_t *id);
int contacts_controller_update(contacts_controller_t *controller, uint32_t id,
			       const char *name, const char *phone);
int contacts_controller_delete(contacts_controller_t *controller, uint32_t id);
int bepio_contacts_controller_toggle_favorite(contacts_controller_t *controller, uint32_t id);

/* Touch-editor navigation; draft changes never mutate the model until Save. */
int contacts_controller_edit(contacts_controller_t *controller, uint32_t id);
int contacts_controller_draft(contacts_controller_t *controller, const char *name, const char *phone);
int contacts_controller_save(contacts_controller_t *controller);
int contacts_controller_discard(contacts_controller_t *controller);
int contacts_controller_request_delete(contacts_controller_t *controller);
int contacts_controller_confirm_delete(contacts_controller_t *controller);
int contacts_controller_search(contacts_controller_t *controller, const char *query);

#endif

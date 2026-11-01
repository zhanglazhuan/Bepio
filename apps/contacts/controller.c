#include "controller.h"
#include <errno.h>
#include <string.h>
#include "bepio_mutex.h"

BEPIO_MUTEX_DEFINE(contacts_lock);

static void show_page(contacts_controller_t *controller)
{
	contacts_view_t *view = controller->view;
	view->model = controller->model;
	if (view->available && view->page_nav.registry) {
		page_navigator_navigate_to(&view->page_nav, view,
			controller->model->flow.page + 1, NULL);
	}
}

void contacts_controller_set_compose_handler(contacts_controller_t *controller,
	int (*handler)(const char *phone, const char *name))
{
	bepio_mutex_lock(contacts_lock);
	controller->compose_message = handler;
	bepio_mutex_unlock(contacts_lock);
}

int bepio_contacts_controller_toggle_favorite(contacts_controller_t *controller, uint32_t id)
{
	bepio_mutex_lock(contacts_lock);
	int ret = controller->active ? bepio_contacts_model_toggle_favorite(controller->model, id) : -EACCES;
	if (!ret) { show_page(controller); }
	bepio_mutex_unlock(contacts_lock);
	return ret;
}

int contacts_controller_lookup_at(contacts_controller_t *controller, size_t index,
				  contacts_entry_t *entry)
{
	int ret = 0;

	if (!entry) {
		return -EINVAL;
	}
	bepio_mutex_lock(contacts_lock);
	if (!controller->model) {
		ret = -ENODEV;
	} else if (index >= controller->model->count) {
		ret = -ENOENT;
	} else {
		*entry = controller->model->entries[index];
	}
	bepio_mutex_unlock(contacts_lock);
	return ret;
}

int contacts_controller_lookup(contacts_controller_t *controller, uint32_t id,
			       contacts_entry_t *entry)
{
	int ret;

	bepio_mutex_lock(contacts_lock);
	ret = controller->model ? contacts_model_get(controller->model, id, entry) : -ENODEV;
	bepio_mutex_unlock(contacts_lock);
	return ret;
}

void contacts_controller_init(contacts_controller_t *controller,
			      contacts_model_t *model, contacts_view_t *view)
{
	bepio_mutex_lock(contacts_lock);
	controller->model = model;
	controller->view = view;
	controller->active = true;
	model->flow = (contacts_flow_t){.page = CONTACTS_PAGE_LIST};
	show_page(controller);
	bepio_mutex_unlock(contacts_lock);
}

void contacts_controller_deinit(contacts_controller_t *controller)
{
	bepio_mutex_lock(contacts_lock);
	controller->active = false;
	contacts_view_deinit(controller->view);
	bepio_mutex_unlock(contacts_lock);
}

bool contacts_controller_back(contacts_controller_t *controller)
{
	bool consumed = false;

	bepio_mutex_lock(contacts_lock);
	if (controller->active && controller->model->flow.page == CONTACTS_PAGE_LIST && controller->model->flow.search_editing) {
		controller->model->flow.search_editing = false;
		show_page(controller);
		consumed = true;
	} else if (controller->active && controller->model->flow.page != CONTACTS_PAGE_LIST) {
		switch (controller->model->flow.page) {
		case CONTACTS_PAGE_EDIT:
			controller->model->flow.page = controller->model->flow.dirty ? CONTACTS_PAGE_DISCARD_CONFIRM : controller->model->flow.edit_return;
			break;
		case CONTACTS_PAGE_DELETE_CONFIRM: controller->model->flow.page = CONTACTS_PAGE_DETAIL; break;
		case CONTACTS_PAGE_DISCARD_CONFIRM: controller->model->flow.page = CONTACTS_PAGE_EDIT; break;
		default: controller->model->flow.page = CONTACTS_PAGE_LIST; controller->model->flow.selected_id = 0; break;
		}
		show_page(controller);
		consumed = true;
	}
	bepio_mutex_unlock(contacts_lock);
	return consumed;
}

void contacts_controller_set_ui_available(contacts_controller_t *controller, bool available)
{
	bepio_mutex_lock(contacts_lock);
	if (controller->active) {
		controller->view->available = available;
		show_page(controller);
	}
	bepio_mutex_unlock(contacts_lock);
}

int contacts_controller_list(contacts_controller_t *controller, size_t *count)
{
	int ret = 0;

	if (!count) {
		return -EINVAL;
	}
	bepio_mutex_lock(contacts_lock);
	if (!controller->active) {
		ret = -EACCES;
	} else {
		*count = controller->model->count;
		controller->model->flow.page = CONTACTS_PAGE_LIST;
		controller->model->flow.selected_id = 0;
		show_page(controller);
	}
	bepio_mutex_unlock(contacts_lock);
	return ret;
}

int contacts_controller_get_at(contacts_controller_t *controller, size_t index,
			       contacts_entry_t *entry)
{
	int ret = 0;

	if (!entry) {
		return -EINVAL;
	}
	bepio_mutex_lock(contacts_lock);
	if (!controller->active) {
		ret = -EACCES;
	} else if (index >= controller->model->count) {
		ret = -ENOENT;
	} else {
		*entry = controller->model->entries[index];
	}
	bepio_mutex_unlock(contacts_lock);
	return ret;
}

int contacts_controller_show(contacts_controller_t *controller, uint32_t id,
			     contacts_entry_t *entry)
{
	int ret;

	bepio_mutex_lock(contacts_lock);
	ret = controller->active ? contacts_model_get(controller->model, id, entry) : -EACCES;
	if (ret == 0) {
		controller->model->flow.page = CONTACTS_PAGE_DETAIL;
		controller->model->flow.selected_id = id;
		show_page(controller);
	}
	bepio_mutex_unlock(contacts_lock);
	return ret;
}

int contacts_controller_add(contacts_controller_t *controller, const char *name,
			    const char *phone, uint32_t *id)
{
	int ret;

	bepio_mutex_lock(contacts_lock);
	ret = controller->active ? contacts_model_add(controller->model, name, phone, id) : -EACCES;
	if (ret == 0) {
		controller->model->flow.page = CONTACTS_PAGE_DETAIL;
		controller->model->flow.selected_id = *id;
		show_page(controller);
	}
	bepio_mutex_unlock(contacts_lock);
	return ret;
}

int contacts_controller_update(contacts_controller_t *controller, uint32_t id,
			       const char *name, const char *phone)
{
	int ret;

	bepio_mutex_lock(contacts_lock);
	ret = controller->active ? contacts_model_update(controller->model, id, name, phone) : -EACCES;
	if (ret == 0) {
		controller->model->flow.page = CONTACTS_PAGE_DETAIL;
		controller->model->flow.selected_id = id;
		show_page(controller);
	}
	bepio_mutex_unlock(contacts_lock);
	return ret;
}

int contacts_controller_delete(contacts_controller_t *controller, uint32_t id)
{
	int ret;

	bepio_mutex_lock(contacts_lock);
	ret = controller->active ? contacts_model_delete(controller->model, id) : -EACCES;
	if (ret == 0) {
		if (controller->model->flow.selected_id == id) {
			controller->model->flow.page = CONTACTS_PAGE_LIST;
			controller->model->flow.selected_id = 0;
		}
		show_page(controller);
	}
	bepio_mutex_unlock(contacts_lock);
	return ret;
}

int contacts_controller_edit(contacts_controller_t *controller, uint32_t id)
{
	int ret = 0;
	bepio_mutex_lock(contacts_lock);
	if (!controller->active) { ret = -EACCES; }
	else {
		contacts_entry_t draft = {0};
		ret = id ? contacts_model_get(controller->model, id, &draft) : 0;
		if (!ret) {
			controller->model->flow.draft = draft;
			controller->model->flow.edit_return = id ? CONTACTS_PAGE_DETAIL : CONTACTS_PAGE_LIST;
			controller->model->flow.selected_id = id;
			controller->model->flow.dirty = false;
			controller->model->flow.page = CONTACTS_PAGE_EDIT;
			show_page(controller);
		}
	}
	bepio_mutex_unlock(contacts_lock);
	return ret;
}

int contacts_controller_draft(contacts_controller_t *controller, const char *name, const char *phone)
{
	if (!name || !phone || strlen(name) > CONTACTS_NAME_MAX || strlen(phone) > CONTACTS_PHONE_MAX) {
		return -EINVAL;
	}
	int ret = 0;
	bepio_mutex_lock(contacts_lock);
	if (!controller->active || controller->model->flow.page != CONTACTS_PAGE_EDIT) { ret = -EACCES; }
	else {
		contacts_entry_t original = {0};
		if (controller->model->flow.draft.id) { ret = contacts_model_get(controller->model, controller->model->flow.draft.id, &original); }
		if (!ret) {
			strcpy(controller->model->flow.draft.name, name);
			strcpy(controller->model->flow.draft.phone, phone);
			controller->model->flow.dirty = strcmp(name, original.name) || strcmp(phone, original.phone);
		}
	}
	bepio_mutex_unlock(contacts_lock);
	return ret;
}

int contacts_controller_save(contacts_controller_t *controller)
{
	int ret = 0;
	bepio_mutex_lock(contacts_lock);
	if (!controller->active || controller->model->flow.page != CONTACTS_PAGE_EDIT) { ret = -EACCES; }
	else {
		uint32_t id = controller->model->flow.draft.id;
		ret = id ? contacts_model_update(controller->model, id, controller->model->flow.draft.name, controller->model->flow.draft.phone) :
			contacts_model_add(controller->model, controller->model->flow.draft.name, controller->model->flow.draft.phone, &id);
		if (!ret) {
			controller->model->flow.selected_id = id;
			controller->model->flow.page = CONTACTS_PAGE_DETAIL;
			controller->model->flow.dirty = false;
			show_page(controller);
		}
	}
	bepio_mutex_unlock(contacts_lock);
	return ret;
}

int contacts_controller_discard(contacts_controller_t *controller)
{
	int ret = 0;
	bepio_mutex_lock(contacts_lock);
	if (!controller->active || controller->model->flow.page != CONTACTS_PAGE_DISCARD_CONFIRM) { ret = -EACCES; }
	else {
		controller->model->flow.page = controller->model->flow.edit_return;
		controller->model->flow.dirty = false;
		memset(&controller->model->flow.draft, 0, sizeof(controller->model->flow.draft));
		show_page(controller);
	}
	bepio_mutex_unlock(contacts_lock);
	return ret;
}

int contacts_controller_request_delete(contacts_controller_t *controller)
{
	int ret = 0;
	bepio_mutex_lock(contacts_lock);
	if (!controller->active || controller->model->flow.page != CONTACTS_PAGE_DETAIL) { ret = -EACCES; }
	else {
		controller->model->flow.page = CONTACTS_PAGE_DELETE_CONFIRM;
		show_page(controller);
	}
	bepio_mutex_unlock(contacts_lock);
	return ret;
}

int contacts_controller_confirm_delete(contacts_controller_t *controller)
{
	int ret = 0;
	bepio_mutex_lock(contacts_lock);
	if (!controller->active || controller->model->flow.page != CONTACTS_PAGE_DELETE_CONFIRM) { ret = -EACCES; }
	else {
		ret = contacts_model_delete(controller->model, controller->model->flow.selected_id);
		if (!ret) {
			controller->model->flow.page = CONTACTS_PAGE_LIST;
			controller->model->flow.selected_id = 0;
			show_page(controller);
		}
	}
	bepio_mutex_unlock(contacts_lock);
	return ret;
}

int contacts_controller_search(contacts_controller_t *controller, const char *query)
{
	if (!query || strlen(query) > CONTACTS_NAME_MAX) { return -EINVAL; }
	int ret = 0;
	bepio_mutex_lock(contacts_lock);
	if (!controller->active || controller->model->flow.page != CONTACTS_PAGE_LIST) { ret = -EACCES; }
	else { strcpy(controller->model->flow.search, query); }
	/* GUI refreshes list only; leave the focused text area and keyboard intact. */
	bepio_mutex_unlock(contacts_lock);
	return ret;
}

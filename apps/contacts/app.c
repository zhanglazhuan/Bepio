#include "app.h"
#include "bepio_app_manager.h"
#include <errno.h>
#include <string.h>
#ifdef ESP_PLATFORM
#include "bepio_account.h"
#include "system/platform/bepio_store.h"
#include "esp_log.h"

/* A saved marker distinguishes first-time setup from a user-deleted Self entry. */
static int ensure_self_contact(contacts_model_t *model)
{
	bepio_account_snapshot_t account;
	bepio_account_snapshot(&account);
	if (!account.id[0] || strlen(account.id) > CONTACTS_PHONE_MAX) { return -EINVAL; }
	char previous[BEPIO_ACCOUNT_ID_MAX + 1] = {0};
	size_t size = sizeof(previous);
	int ret = bepio_store_read("bepio_contacts", "self_id", previous, &size);
	if (ret && ret != -ENOENT) { return ret; }
	if (!ret && (size < 2 || size > sizeof(previous) || previous[size - 1] ||
	    !memchr(previous, 0, size))) { return -EINVAL; }
	if (!ret && !strcmp(previous, account.id)) { return 0; }
	bool own_id_exists = false;
	for (size_t i = 0; i < model->count; i++) {
		contacts_entry_t *entry = &model->entries[i];
		if (!strcmp(entry->phone, account.id)) { own_id_exists = true; }
		if (!ret && !own_id_exists && !strcmp(entry->name, "Self") &&
		    !strcmp(entry->phone, previous)) {
			int updated = contacts_model_update(model, entry->id, "Self", account.id);
			if (updated && updated != -EEXIST) { return updated; }
			if (!updated) { own_id_exists = true; }
		}
	}
	if (ret == -ENOENT && !own_id_exists) {
		uint32_t id;
		ret = contacts_model_add(model, "Self", account.id, &id);
		if (ret) { return ret; }
		ESP_LOGI("contacts", "Created Self contact for device ID %s", account.id);
	}
	return bepio_store_write("bepio_contacts", "self_id", account.id, strlen(account.id) + 1);
}
#endif

static bepio_contacts_app_t contacts_app;

static int contacts_start(void *root, void *group)
{
	contacts_view_init(&contacts_app.view, root, group);
	contacts_controller_init(&contacts_app.controller, &contacts_app.model, &contacts_app.view);
	return 0;
}

static void contacts_stop(void)
{
	contacts_controller_deinit(&contacts_app.controller);
}

static bool contacts_back(void)
{
	return contacts_controller_back(&contacts_app.controller);
}

static void contacts_ui_unavailable(void)
{
	contacts_controller_set_ui_available(&contacts_app.controller, false);
}

static void contacts_ui_available(void)
{
	contacts_controller_set_ui_available(&contacts_app.controller, true);
}

static bepio_application_t application = {
	.name = BEPIO_CONTACTS_APP_NAME,
	.category = BEPIO_APP_CATEGORY_TOOLS,
	.start_func = contacts_start,
	.stop_func = contacts_stop,
	.back_func = contacts_back,
	.ui_unavailable_func = contacts_ui_unavailable,
	.ui_available_func = contacts_ui_available,
};

contacts_controller_t *contacts_app_get_controller(void)
{
	return &contacts_app.controller;
}

int contacts_app_close_request(void)
{
	return bepio_app_manager_app_close_request(&application);
}

int bepio_contacts_register(void)
{
	contacts_model_init(&contacts_app.model);
	int ret = bepio_contacts_model_restore(&contacts_app.model);
	if (ret && ret != -ENOENT) { return ret; }
#ifdef ESP_PLATFORM
	ret = ensure_self_contact(&contacts_app.model);
	if (ret) { return ret; }
#endif
	contacts_app.controller.model = &contacts_app.model;
	return bepio_app_manager_add_application(&application);
}



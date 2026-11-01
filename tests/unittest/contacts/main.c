#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "bepio_console.h"
#include "apps/contacts/app.h"
#include "apps/settings/app.h"
#include "bepio_app_manager.h"

int bepio_contacts_register(void);
int bepio_settings_register(void);
extern const bepio_console_command_t *host_commands_contacts;
char host_shell_output[4096];
int host_shell_errors;

static void test_model_crud(void)
{
	contacts_model_t model;
	contacts_entry_t entry;
	uint32_t first, second, third;

	contacts_model_init(&model);
	assert(model.count == 0);
	assert(contacts_model_add(&model, "妈妈", "8613800000000", &first) == 0);
	assert(contacts_model_add(&model, "Dad", "13900000000", &second) == 0);
	assert(first == 1 && second == 2);
	assert(contacts_model_get(&model, first, &entry) == 0);
	assert(strcmp(entry.name, "妈妈") == 0);
	assert(!entry.favorite);
	assert(bepio_contacts_model_toggle_favorite(&model, first) == 0);
	assert(contacts_model_update(&model, first, "Mother", "8613800000001") == 0);
	assert(contacts_model_get(&model, first, &entry) == 0);
	assert(entry.favorite);
	assert(bepio_contacts_model_toggle_favorite(&model, first) == 0);
	assert(contacts_model_get(&model, first, &entry) == 0 && !entry.favorite);
	assert(bepio_contacts_model_toggle_favorite(&model, 999) == -ENOENT);
	assert(entry.id == first && strcmp(entry.name, "Mother") == 0);
	assert(contacts_model_update(&model, first, "Bad change", "13900000000") == -EEXIST);
	assert(contacts_model_update(&model, first, "Bad change", "12x") == -EINVAL);
	assert(contacts_model_get(&model, first, &entry) == 0);
	assert(strcmp(entry.name, "Mother") == 0);
	assert(strcmp(entry.phone, "8613800000001") == 0);
	assert(contacts_model_delete(&model, first) == 0);
	assert(contacts_model_get(&model, first, &entry) == -ENOENT);
	assert(contacts_model_get(&model, second, &entry) == 0 && entry.id == second);
	assert(contacts_model_add(&model, "Dad", "110", &third) == 0);
	assert(third == 3); /* Same names are allowed; deleted IDs are not reused. */
	assert(contacts_model_delete(&model, first) == -ENOENT);
	assert(contacts_model_update(&model, 999, "Nobody", "119") == -ENOENT);
	assert(contacts_model_delete(&model, third) == 0);
	assert(contacts_model_delete(&model, second) == 0);
	assert(model.count == 0);
}

static void test_validation_and_capacity(void)
{
	contacts_model_t model;
	contacts_entry_t entry;
	char name[CONTACTS_NAME_MAX + 2];
	char phone[CONTACTS_PHONE_MAX + 2];
	uint32_t id = 999;

	contacts_model_init(&model);
	assert(contacts_model_add(&model, "", "110", &id) == -EINVAL);
	assert(contacts_model_add(&model, "   ", "110", &id) == -EINVAL);
	assert(contacts_model_add(&model, "A\nB", "110", &id) == -EINVAL);
	assert(contacts_model_add(&model, NULL, "110", &id) == -EINVAL);
	assert(contacts_model_add(&model, "Name", "", &id) == -EINVAL);
	assert(contacts_model_add(&model, "Name", "+", &id) == -EINVAL);
	assert(contacts_model_add(&model, "Name", "12+3", &id) == -EINVAL);
	assert(contacts_model_add(&model, "Name", "12 3", &id) == -EINVAL);
	assert(contacts_model_add(&model, "Name", "123", NULL) == -EINVAL);
	assert(id == 999 && model.count == 0 && model.next_id == 1);
	memset(name, 'N', sizeof(name));
	name[sizeof(name) - 1] = 0;
	memset(phone, '1', sizeof(phone));
	phone[sizeof(phone) - 1] = 0;
	assert(contacts_model_add(&model, name, "110", &id) == -ENAMETOOLONG);
	assert(contacts_model_add(&model, "Name", phone, &id) == -ENAMETOOLONG);
	name[CONTACTS_NAME_MAX] = 0;
	phone[CONTACTS_PHONE_MAX] = 0;
	assert(contacts_model_add(&model, name, phone, &id) == 0);
	assert(contacts_model_update(&model, id, name, phone) == 0);
	assert(contacts_model_add(&model, "Another", phone, &id) == -EEXIST);
	assert(contacts_model_get(&model, 1, NULL) == -EINVAL);
	assert(contacts_model_get(NULL, 1, &entry) == -EINVAL);
	for (unsigned int i = 1; i < CONFIG_BEPIO_CONTACTS_CAPACITY; i++) {
		snprintf(phone, sizeof(phone), "%u", i);
		assert(contacts_model_add(&model, "Name", phone, &id) == 0);
	}
	assert(contacts_model_add(&model, "Full", "99999", &id) == -ENOSPC);
	assert(model.count == CONFIG_BEPIO_CONTACTS_CAPACITY);
	assert(model.next_id == CONFIG_BEPIO_CONTACTS_CAPACITY + 1);
	assert(contacts_model_delete(&model, 1) == 0);
	assert(contacts_model_add(&model, "Reuse slot", "99999", &id) == 0);
	assert(id == CONFIG_BEPIO_CONTACTS_CAPACITY + 1);
	contacts_model_init(&model);
	model.next_id = UINT32_MAX;
	assert(contacts_model_add(&model, "Last", "1", &id) == 0 && id == UINT32_MAX);
	assert(contacts_model_add(&model, "Overflow", "2", &id) == -EOVERFLOW);
	assert(model.count == 1 && model.next_id == 0);
}

static int command(size_t argc, char **argv)
{
	static const bepio_console_context_t shell;

	host_shell_output[0] = 0;
	host_shell_errors = 0;
	for (const bepio_console_command_t *cmd = host_commands_contacts; cmd->name; cmd++) {
		if (strcmp(cmd->name, argv[0]) == 0) {
			assert((size_t)cmd->required == argc);
			return cmd->handler(&shell, argc, argv);
		}
	}
	assert(0 && "Missing shell command");
	return -EINVAL;
}

#define COMMAND(...) command(sizeof((char *[]){__VA_ARGS__}) / sizeof(char *), (char *[]){__VA_ARGS__})

static void test_app_and_shell(void)
{
	contacts_controller_t *controller = contacts_app_get_controller();
	contacts_entry_t entry;
	size_t count;

	assert(bepio_settings_register() == 0);
	assert(bepio_contacts_register() == 0);
	assert(bepio_app_manager_get_num_apps() == 2);
	assert(contacts_controller_list(controller, &count) == -EACCES);
	assert(bepio_app_manager_show(NULL, NULL, NULL, "Settings") == 0);
	assert(COMMAND("list") == -EBUSY);
	assert(strcmp(bepio_app_manager_get_current_app_name(), "Settings") == 0);
	assert(COMMAND("back") == -ENOENT); /* Never closes Settings. */
	assert(settings_app_close_request() == 0);
	assert(COMMAND("list") == 0);
	assert(strstr(host_shell_output, "No contacts"));
	assert(COMMAND("add", "Mom", "8613800000000") == 0);
	assert(strstr(host_shell_output, "Added contact 1"));
	assert(COMMAND("add", "Dad Smith", "13900000000") == 0);
	assert(COMMAND("list") == 0);
	assert(strstr(host_shell_output, "1 | Mom | 8613800000000"));
	assert(strstr(host_shell_output, "2 | Dad Smith | 13900000000"));
	assert(COMMAND("show", "1") == 0);
	assert(COMMAND("update", "1", "Mother", "13800000001") == 0);
	assert(COMMAND("show", "1") == 0);
	assert(strstr(host_shell_output, "1 | Mother | 13800000001"));
	assert(COMMAND("update", "1", "Invalid", "13900000000") == -EEXIST);
	assert(COMMAND("add", "Duplicate", "13900000000") == -EEXIST);
	assert(COMMAND("add", "", "123") == -EINVAL);
	assert(COMMAND("add", "Invalid", "abc") == -EINVAL);
	assert(COMMAND("show", "0") == -EINVAL);
	assert(COMMAND("show", "-1") == -EINVAL);
	assert(COMMAND("show", "+1") == -EINVAL);
	assert(COMMAND("show", "1x") == -EINVAL);
	assert(COMMAND("show", "4294967296") == -EINVAL);
	assert(COMMAND("show", "4294967295") == -ENOENT);
	assert(COMMAND("show", "999") == -ENOENT);
	assert(COMMAND("delete", "999") == -ENOENT);
	assert(COMMAND("show", "1") == 0);
	assert(bepio_app_manager_set_ui_available(false) == 0);
	assert(bepio_app_manager_set_ui_available(true) == 0);
	assert(COMMAND("back") == 0 && bepio_app_manager_is_app_running());
	assert(COMMAND("back") == 0 && !bepio_app_manager_is_app_running());
	assert(contacts_controller_show(controller, 1, &entry) == -EACCES);
	assert(contacts_controller_delete(controller, 1) == -EACCES);
	assert(COMMAND("show", "1") == 0); /* Opening does not erase data. */
	assert(strstr(host_shell_output, "Mother"));
	assert(COMMAND("delete", "1") == 0);
	assert(COMMAND("show", "1") == -ENOENT);
	assert(COMMAND("add", "Child", "110") == 0);
	assert(strstr(host_shell_output, "Added contact 3"));
	assert(COMMAND("delete", "2") == 0);
	assert(COMMAND("delete", "3") == 0); /* Deleting selected detail returns to list. */
	assert(COMMAND("back") == 0 && !bepio_app_manager_is_app_running());
	assert(COMMAND("list") == 0);
	assert(strstr(host_shell_output, "No contacts"));
	assert(contacts_controller_get_at(controller, 0, &entry) == -ENOENT);
	assert(contacts_controller_get_at(controller, 0, NULL) == -EINVAL);
	assert(contacts_controller_list(controller, NULL) == -EINVAL);
	assert(bepio_app_manager_delete() == 0);
}

static void test_search_and_editor(void)
{
	contacts_model_t model;
	contacts_view_t view;
	contacts_controller_t c = {0};
	contacts_entry_t entry;
	contacts_model_init(&model);
	for (unsigned i = 0; i < CONFIG_BEPIO_CONTACTS_CAPACITY; i++) {
		char name[24], phone[24];
		uint32_t id;
		snprintf(name, sizeof(name), i == 0 ? "Ava Chen" : "Person %u", i);
		snprintf(phone, sizeof(phone), "120255501%02u", i + 1);
		assert(contacts_model_add(&model, name, phone, &id) == 0);
	}
	assert(model.count == CONFIG_BEPIO_CONTACTS_CAPACITY);
	assert(contacts_model_matches(&model.entries[0], "aVA"));
	assert(contacts_model_matches(&model.entries[0], "550101"));
	assert(contacts_model_matches(&model.entries[0], ""));
	assert(!contacts_model_matches(&model.entries[0], "Missing"));
	assert(!contacts_model_matches(NULL, ""));
	contacts_view_init(&view, NULL, NULL);
	contacts_controller_init(&c, &model, &view);
	assert(contacts_controller_search(&c, "Ava") == 0 && !strcmp(model.flow.search, "Ava"));
	assert(contacts_controller_edit(&c, 1) == 0 && !model.flow.dirty);
	assert(contacts_controller_draft(&c, "Changed", model.entries[1].phone) == 0 && model.flow.dirty);
	assert(contacts_controller_save(&c) == -EEXIST);
	assert(model.flow.page == CONTACTS_PAGE_EDIT && !strcmp(model.entries[0].name, "Ava Chen"));
	assert(contacts_controller_back(&c) && model.flow.page == CONTACTS_PAGE_EDIT && model.flow.discard_pending);
	assert(contacts_controller_cancel_discard(&c) == 0 && !model.flow.discard_pending);
	assert(!strcmp(model.flow.draft.name, "Changed"));
	assert(contacts_controller_back(&c));
	assert(model.flow.discard_pending);
	assert(contacts_controller_discard(&c) == 0 && model.flow.page == CONTACTS_PAGE_DETAIL);
	assert(contacts_controller_edit(&c, 1) == 0);
	assert(contacts_controller_draft(&c, "Ava Updated", "12025550142") == 0);
	assert(contacts_controller_save(&c) == 0 && model.flow.page == CONTACTS_PAGE_DETAIL);
	assert(contacts_model_get(&model, 1, &entry) == 0 && !strcmp(entry.name, "Ava Updated"));
	assert(contacts_controller_confirm_delete(&c) == -EACCES);
	assert(contacts_controller_request_delete(&c) == 0);
	assert(model.flow.page == CONTACTS_PAGE_DETAIL && model.flow.delete_pending);
	assert(contacts_controller_cancel_delete(&c) == 0 && !model.flow.delete_pending);
	assert(contacts_controller_confirm_delete(&c) == -EACCES);
	assert(contacts_model_get(&model, 1, &entry) == 0);
	assert(contacts_controller_request_delete(&c) == 0);
	assert(contacts_controller_back(&c) && model.flow.page == CONTACTS_PAGE_DETAIL);
	assert(!model.flow.delete_pending);
	assert(contacts_controller_request_delete(&c) == 0);
	assert(contacts_controller_confirm_delete(&c) == 0 && model.flow.page == CONTACTS_PAGE_LIST);
	assert(model.count == CONFIG_BEPIO_CONTACTS_CAPACITY - 1);
	assert(contacts_controller_edit(&c, 0) == 0);
	assert(contacts_controller_draft(&c, "New Person", "+") == 0);
	assert(contacts_controller_save(&c) == -EINVAL);
	assert(contacts_controller_draft(&c, "New Person", "12025550199") == 0);
	assert(contacts_controller_save(&c) == 0 && model.count == CONFIG_BEPIO_CONTACTS_CAPACITY);
	assert(model.flow.selected_id > 4);
	assert(contacts_controller_edit(&c, 0) == 0);
	assert(contacts_controller_draft(&c, "Overflow", "999") == 0);
	assert(contacts_controller_save(&c) == -ENOSPC);
	assert(contacts_controller_back(&c));
	assert(model.flow.discard_pending);
	assert(contacts_controller_discard(&c) == 0 && model.flow.page == CONTACTS_PAGE_LIST);
	contacts_controller_deinit(&c);
	assert(contacts_controller_edit(&c, 1) == -EACCES);
	assert(contacts_controller_search(&c, "x") == -EACCES);
}

int main(void)
{
	contacts_model_t pager_model;
	contacts_entry_t pager_entry;
	uint32_t pager_id;
	contacts_model_init(&pager_model);
	assert(contacts_model_add(&pager_model, "PC", "100000000000002", &pager_id) == 0);
	assert(contacts_model_get(&pager_model, pager_id, &pager_entry) == 0);
	assert(!strcmp(pager_entry.phone, "100000000000002"));
	assert(contacts_model_add(&pager_model, "Again", "100000000000002", &pager_id) == -EEXIST);
	assert(contacts_model_add(&pager_model, "Bad", "Pager_", &pager_id) == -EINVAL);
	assert(contacts_model_add(&pager_model, "Bad", "Pager_+/inbox", &pager_id) == -EINVAL);
	test_model_crud();
	test_validation_and_capacity();
	test_app_and_shell();
	test_search_and_editor();
	puts("PASS: contacts CRUD, validation, capacity, stable IDs, lifecycle, shell handlers, search, editor save/discard/delete confirmation");
	return 0;
}

#include "app.h"
#include "bepio_app_manager.h"
#include <errno.h>
#include "bepio_console.h"

static int report_error(const bepio_console_context_t *shell, int ret)
{
	const char *message;

	switch (ret) {
	case -EINVAL: message = "Invalid name, phone or ID"; break;
	case -ENOENT: message = "Contact not found"; break;
	case -EEXIST: message = "Phone number already exists"; break;
	case -ENOSPC: message = "Contact list is full"; break;
	case -ENAMETOOLONG: message = "Name or phone is too long"; break;
	case -EOVERFLOW: message = "Contact ID range exhausted"; break;
	case -EBUSY: message = "Another app is active; use its back command to close it"; break;
	case -EACCES: message = "Contacts is not running"; break;
	default: message = "Contact operation failed"; break;
	}
	bepio_console_error(shell, "%s (%d)", message, ret);
	return ret;
}

static int open_contacts(const bepio_console_context_t *shell)
{
	int ret = bepio_app_manager_show(NULL, NULL, NULL, BEPIO_CONTACTS_APP_NAME);

	return ret < 0 ? report_error(shell, ret) : ret;
}

static int parse_id(const bepio_console_context_t *shell, const char *text, uint32_t *id)
{
	uint32_t value = 0;

	if (!text || !text[0]) {
		return report_error(shell, -EINVAL);
	}
	for (size_t i = 0; text[i]; i++) {
		if (text[i] < '0' || text[i] > '9' ||
		    value > (UINT32_MAX - (uint32_t)(text[i] - '0')) / 10U) {
			return report_error(shell, -EINVAL);
		}
		value = value * 10U + (uint32_t)(text[i] - '0');
	}
	if (value == 0) {
		return report_error(shell, -EINVAL);
	}
	*id = value;
	return 0;
}

static void print_entry(const bepio_console_context_t *shell, const contacts_entry_t *entry)
{
	bepio_console_print(shell, "%u | %s | %s", (unsigned int)entry->id, entry->name, entry->phone);
}

static int list_contacts(const bepio_console_context_t *shell, size_t argc, char **argv)
{
	contacts_controller_t *controller = contacts_app_get_controller();
	contacts_entry_t entry;
	size_t count;
	int ret = open_contacts(shell);

	BEPIO_UNUSED(argc);
	BEPIO_UNUSED(argv);
	if (ret < 0) {
		return ret;
	}
	ret = contacts_controller_list(controller, &count);
	if (ret < 0) {
		return report_error(shell, ret);
	}
	bepio_console_print(shell, "Contacts: %u/%u [RAM-only]", (unsigned int)count,
		    CONFIG_BEPIO_CONTACTS_CAPACITY);
	if (!count) {
		bepio_console_print(shell, "No contacts. Use: contacts add <name> <phone>");
		return 0;
	}
	bepio_console_print(shell, "ID | Name | Phone");
	for (size_t i = 0; i < count; i++) {
		ret = contacts_controller_get_at(controller, i, &entry);
		if (ret < 0) {
			return report_error(shell, ret);
		}
		print_entry(shell, &entry);
	}
	return 0;
}

static int show_contact(const bepio_console_context_t *shell, size_t argc, char **argv)
{
	contacts_entry_t entry;
	uint32_t id;
	int ret = parse_id(shell, argv[1], &id);

	BEPIO_UNUSED(argc);
	if (ret < 0 || (ret = open_contacts(shell)) < 0) {
		return ret;
	}
	ret = contacts_controller_show(contacts_app_get_controller(), id, &entry);
	if (ret < 0) {
		return report_error(shell, ret);
	}
	print_entry(shell, &entry);
	return 0;
}

static int add_contact(const bepio_console_context_t *shell, size_t argc, char **argv)
{
	uint32_t id;
	int ret = open_contacts(shell);

	BEPIO_UNUSED(argc);
	if (ret < 0) {
		return ret;
	}
	ret = contacts_controller_add(contacts_app_get_controller(), argv[1], argv[2], &id);
	if (ret < 0) {
		return report_error(shell, ret);
	}
	bepio_console_print(shell, "Added contact %u [RAM-only]", (unsigned int)id);
	return 0;
}

static int update_contact(const bepio_console_context_t *shell, size_t argc, char **argv)
{
	uint32_t id;
	int ret = parse_id(shell, argv[1], &id);

	BEPIO_UNUSED(argc);
	if (ret < 0 || (ret = open_contacts(shell)) < 0) {
		return ret;
	}
	ret = contacts_controller_update(contacts_app_get_controller(), id, argv[2], argv[3]);
	if (ret < 0) {
		return report_error(shell, ret);
	}
	bepio_console_print(shell, "Updated contact %u [RAM-only]", (unsigned int)id);
	return 0;
}

static int delete_contact(const bepio_console_context_t *shell, size_t argc, char **argv)
{
	uint32_t id;
	int ret = parse_id(shell, argv[1], &id);

	BEPIO_UNUSED(argc);
	if (ret < 0 || (ret = open_contacts(shell)) < 0) {
		return ret;
	}
	ret = contacts_controller_delete(contacts_app_get_controller(), id);
	if (ret < 0) {
		return report_error(shell, ret);
	}
	bepio_console_print(shell, "Deleted contact %u", (unsigned int)id);
	return 0;
}

static int back_contacts(const bepio_console_context_t *shell, size_t argc, char **argv)
{
	int ret = contacts_app_close_request();

	BEPIO_UNUSED(argc);
	BEPIO_UNUSED(argv);
	if (ret < 0) {
		bepio_console_error(shell, "Cannot go back; Contacts is not active (%d)", ret);
		return ret;
	}
	bepio_console_print(shell, "Contacts %s",
		    bepio_app_manager_is_app_running() ? "list" : "closed");
	return 0;
}

BEPIO_COMMANDS(contacts_commands,
	BEPIO_COMMAND(list, NULL, "Open contact list", list_contacts, 1, 0),
	BEPIO_COMMAND(show, NULL, "Show <id>", show_contact, 2, 0),
	BEPIO_COMMAND(add, NULL, "Add <name> <phone>; quote names with spaces", add_contact, 3, 0),
	BEPIO_COMMAND(update, NULL, "Update <id> <name> <phone>", update_contact, 4, 0),
	BEPIO_COMMAND(delete, NULL, "Delete <id>", delete_contact, 2, 0),
	BEPIO_COMMAND(back, NULL, "Back to list, then exit", back_contacts, 1, 0),
	BEPIO_COMMAND_END
);
BEPIO_COMMAND_GROUP(contacts, contacts_commands, "Contacts app", NULL);

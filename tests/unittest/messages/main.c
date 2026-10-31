#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "bepio_console.h"
#include "apps/messages/app.h"
#include "apps/contacts/app.h"
#include "apps/settings/app.h"
#include "bepio_app_manager.h"

int bepio_messaging_init(void);
int bepio_messages_register(void);
int bepio_contacts_register(void);
int bepio_settings_register(void);
extern const bepio_console_command_t *host_commands_messages;
char host_shell_output[4096];
int host_shell_errors;

static bepio_message_payload_t payload = {
	.name = "妈妈",
	.phone = "+8613800000000",
	.body = "我到家了。",
};

static void test_validation(void)
{
	bepio_message_payload_t p = payload;
	uint32_t id;

	assert(bepio_messaging_init() == 0);
	assert(bepio_messaging_validate(&p, true) == 0);
	memset(p.body, 'a', sizeof(p.body) - 1);
	p.body[BEPIO_MESSAGE_BODY_MAX] = 0;
	assert(bepio_messaging_validate(&p, true) == 0);
	memset(p.body, 'a', sizeof(p.body));
	assert(bepio_messaging_validate(&p, true) == -ENAMETOOLONG);
	p = payload;
	strcpy(p.body, "\xc0\xaf"); /* Overlong UTF-8. */
	assert(bepio_messaging_validate(&p, true) == -EINVAL);
	strcpy(p.body, "\xed\xa0\x80"); /* Surrogate. */
	assert(bepio_messaging_validate(&p, true) == -EINVAL);
	strcpy(p.body, "\xf4\x90\x80\x80"); /* Above Unicode range. */
	assert(bepio_messaging_validate(&p, true) == -EINVAL);
	strcpy(p.body, "\xe4\xb8"); /* Truncated character. */
	assert(bepio_messaging_validate(&p, true) == -EINVAL);
	strcpy(p.body, "\xf0\x9f\x98\x80");
	assert(bepio_messaging_validate(&p, true) == 0);
	strcpy(p.body, " \n\t");
	assert(bepio_messaging_validate(&p, true) == -EINVAL);
	strcpy(p.body, "Text\x1b");
	assert(bepio_messaging_validate(&p, true) == -EINVAL);
	p = payload;
	strcpy(p.phone, "+");
	assert(bepio_messaging_submit(0, &p, &id) == -EINVAL);
	strcpy(p.phone, "12x");
	assert(bepio_messaging_validate(&p, true) == -EINVAL);
	memset(&p, 0, sizeof(p));
	assert(bepio_messaging_validate(&p, false) == 0);
	assert(bepio_messaging_validate(&p, true) == -EINVAL);
	assert(bepio_messaging_save(0, &p, &id) == 0);
}

static void test_inbox(void)
{
	bepio_message_t message;
	bepio_messaging_stats_t stats;
	uint32_t first, second, third, unused;

	assert(bepio_messaging_init() == 0);
	assert(bepio_messaging_receive("r1", &payload, 0, false, &first) == 0);
	assert(bepio_messaging_receive("r2", &payload, 123456, false, &second) == 0);
	bepio_messaging_stats(&stats);
	assert(stats.inbox == 2 && stats.unread == 2 && !stats.online);
	assert(bepio_messaging_get_at(BEPIO_MESSAGE_INBOX, 0, &message) == 0);
	assert(message.id == second && message.timestamp_ms == 123456);
	assert(bepio_messaging_get(BEPIO_MESSAGE_INBOX, first, false, &message) == 0 &&
	       !message.read);
	assert(bepio_messaging_get(BEPIO_MESSAGE_INBOX, first, true, &message) == 0 &&
	       message.read);
	bepio_messaging_stats(&stats);
	assert(stats.unread == 1);
	assert(bepio_messaging_receive("r1", &payload, 0, false, &unused) == -EALREADY);
	assert(bepio_messaging_delete(BEPIO_MESSAGE_INBOX, first) == 0);
	assert(bepio_messaging_receive("r1", &payload, 0, false, &unused) == -EALREADY);
	assert(bepio_messaging_receive("r1", &payload, 0, true, &third) ==
	       0); /* Isolated demo IDs. */
	assert(bepio_messaging_receive("r3", &payload, 0, false, &unused) == 0);
	assert(bepio_messaging_receive("r4", &payload, 0, false, &unused) == -ENOSPC);
	assert(bepio_messaging_receive("r2", &payload, 0, false, &unused) == -EALREADY);
	assert(bepio_messaging_delete(BEPIO_MESSAGE_INBOX, third) == 0);
	assert(bepio_messaging_receive("r4", &payload, 0, false, &unused) == 0);
	assert(unused > third); /* Failed insert did not consume ID or poison dedup. */
	for (int i = 0; i < 4; i++) {
		char remote_id[16];
		assert(bepio_messaging_get_at(BEPIO_MESSAGE_INBOX, 0, &message) == 0);
		assert(bepio_messaging_delete(BEPIO_MESSAGE_INBOX, message.id) == 0);
		snprintf(remote_id, sizeof(remote_id), "later-%d", i);
		assert(bepio_messaging_receive(remote_id, &payload, 0, false, &unused) == 0);
	}
	/* Old retained records stay deduplicated even after the recent-ID ring rolls. */
	assert(bepio_messaging_receive("r2", &payload, 0, false, &unused) == -EALREADY);
	assert(bepio_messaging_get((bepio_message_box_t)99, first, false, &message) == -EINVAL);
	assert(bepio_messaging_receive("bad-time", &payload, -1, false, &unused) == -EINVAL);
}

static void test_queue(void)
{
	bepio_message_t first_send, retry_send, message;
	bepio_messaging_stats_t stats;
	bepio_message_payload_t updated = payload;
	uint32_t draft, id, second;

	assert(bepio_messaging_init() == 0);
	assert(bepio_messaging_save(0, &payload, &draft) == 0);
	assert(bepio_messaging_submit(draft, &payload, &id) == 0 && id == draft);
	assert(bepio_messaging_begin(false, &first_send) == -ENETDOWN);
	assert(bepio_messaging_get(BEPIO_MESSAGE_OUTBOX, id, false, &message) == 0);
	assert(message.state == BEPIO_MESSAGE_QUEUED);
	assert(bepio_messaging_save(id, &payload, &second) == -EPERM);
	bepio_messaging_set_online(true);
	assert(bepio_messaging_begin(false, &first_send) == 0);
	assert(first_send.attempt == 1 && !first_send.simulated);
	assert(bepio_messaging_begin(false, &message) == -EBUSY);
	assert(bepio_messaging_delete(BEPIO_MESSAGE_OUTBOX, id) == -EBUSY);
	assert(bepio_messaging_complete(id, 1, true, true) == -ESTALE);
	assert(bepio_messaging_complete(id, 1, false, false) == 0);
	assert(bepio_messaging_retry(id) == 0);
	assert(bepio_messaging_begin(false, &retry_send) == 0);
	assert(retry_send.id == first_send.id && retry_send.attempt == 2);
	assert(!strcmp(retry_send.payload.body, first_send.payload.body));
	assert(bepio_messaging_complete(id, 1, false, true) == -ESTALE);
	assert(bepio_messaging_complete(id, 2, false, true) == 0);
	assert(bepio_messaging_get(BEPIO_MESSAGE_OUTBOX, id, false, &message) == 0);
	assert(!strcmp(bepio_messaging_state_label(&message), "sent(server-ack)"));
	assert(bepio_messaging_retry(id) == -EPERM);
	assert(bepio_messaging_submit(0, &updated, &second) == 0);
	assert(bepio_messaging_begin(true, &message) == 0);
	assert(bepio_messaging_complete(second, message.attempt, true, true) == 0);
	assert(bepio_messaging_get(BEPIO_MESSAGE_OUTBOX, second, false, &message) == 0);
	assert(message.simulated &&
	       !strcmp(bepio_messaging_state_label(&message), "SIMULATED-sent"));
	bepio_messaging_stats(&stats);
	assert(stats.queued == 0);
	assert(bepio_messaging_save(0, &payload, &draft) == 0);
	assert(bepio_messaging_save(0, &payload, &draft) == -ENOSPC);
	assert(bepio_messaging_submit(0, &payload, &draft) == -ENOSPC);
	assert(bepio_messaging_delete(BEPIO_MESSAGE_OUTBOX, id) == 0);
	assert(bepio_messaging_submit(0, &payload, &draft) == 0 && draft > second);
	assert(bepio_messaging_delete(BEPIO_MESSAGE_OUTBOX, draft) == 0); /* Cancel queued item. */
}

static int command(size_t argc, char **argv)
{
	static const bepio_console_context_t shell;

	host_shell_output[0] = 0;
	host_shell_errors = 0;
	for (const bepio_console_command_t *cmd = host_commands_messages; cmd->name; cmd++) {
		if (!strcmp(cmd->name, argv[0])) {
			assert(argc >= (size_t)cmd->required);
			assert(argc <= (size_t)(cmd->required + cmd->optional));
			return cmd->handler(&shell, argc, argv);
		}
	}
	assert(0 && "Missing Messages shell command");
	return -EINVAL;
}
#define COMMAND(...)                                                                               \
	command(sizeof((char *[]){__VA_ARGS__}) / sizeof(char *), (char *[]){__VA_ARGS__})

static void test_app(void)
{
	contacts_controller_t *contacts = contacts_app_get_controller();
	messages_controller_t *c = messages_app_get_controller();
	messages_snapshot_t snapshot;
	bepio_message_t message;
	bepio_messaging_stats_t stats;
	contacts_entry_t contact;
	uint32_t contact_id, draft, incoming;

	assert(bepio_messaging_init() == 0);
	assert(bepio_settings_register() == 0);
	assert(bepio_contacts_register() == 0);
	assert(bepio_messages_register() == 0);
	assert(bepio_app_manager_get_num_apps() == 3);
	assert(bepio_app_manager_show(NULL, NULL, NULL, "Settings") == 0);
	assert(COMMAND("home") == -EBUSY);
	assert(COMMAND("back") == -ENOENT);
	assert(settings_app_close_request() == 0);
	assert(bepio_app_manager_show(NULL, NULL, NULL, "Contacts") == 0);
	assert(contacts_controller_add(contacts, "Mom", "+8613800000000", &contact_id) == 0);
	assert(bepio_app_manager_delete() == 0);
	assert(contacts_controller_lookup(contacts, contact_id, &contact) == 0);
	assert(COMMAND("home") == 0);
	assert(strstr(host_shell_output, "offline"));
	assert(COMMAND("contacts") == 0);
	assert(strstr(host_shell_output, "1 | Mom | +8613800000000"));
	assert(!strcmp(bepio_app_manager_get_current_app_name(), "Messages"));
	assert(COMMAND("to", "1") == 0);
	assert(strstr(host_shell_output, "Mom <+8613800000000>"));
	assert(COMMAND("send") == -EINVAL); /* Empty body cannot be queued. */
	assert(COMMAND("text", "Hello 妈妈") == 0);
	assert(COMMAND("home") == -EBUSY);
	assert(COMMAND("back") == 0);
	assert(strstr(host_shell_output, "save / messages discard / messages cancel"));
	assert(COMMAND("confirm") == -EPERM); /* Leave prompt is not a send/delete confirmation. */
	assert(COMMAND("cancel") == 0);
	assert(COMMAND("recipient", "13900000000", "Dad") == 0);
	assert(COMMAND("to", "1") == 0); /* Contact selection also updates existing editor. */
	assert(COMMAND("save") == 0);
	assert(bepio_messaging_get_at(BEPIO_MESSAGE_OUTBOX, 0, &message) == 0);
	draft = message.id;
	assert(message.state == BEPIO_MESSAGE_DRAFT);
	assert(COMMAND("edit", "1") == 0);
	assert(COMMAND("text", "Unsaved replacement") == 0);
	assert(COMMAND("discard") == 0);
	assert(bepio_messaging_get(BEPIO_MESSAGE_OUTBOX, draft, false, &message) == 0);
	assert(!strcmp(message.payload.body, "Hello 妈妈"));
	assert(COMMAND("edit", "1") == 0);
	assert(COMMAND("send") == 0);
	assert(strstr(host_shell_output, "Confirm queueing"));
	assert(COMMAND("text", "Cannot mutate confirmation") == -EBUSY);
	assert(COMMAND("cancel") == 0);
	assert(COMMAND("send") == 0 && COMMAND("confirm") == 0);
	assert(bepio_messaging_get(BEPIO_MESSAGE_OUTBOX, draft, false, &message) == 0);
	assert(message.state == BEPIO_MESSAGE_QUEUED && !message.simulated);
	assert(COMMAND("edit", "1") == -EPERM);
	assert(COMMAND("demo_send", "fail") == 0);
	assert(COMMAND("retry", "1") == 0);
	assert(COMMAND("demo_send", "ok") == 0);
	assert(COMMAND("show", "out", "1") == 0);
	assert(strstr(host_shell_output, "SIMULATED-sent"));
	assert(COMMAND("demo_receive", "wire-1", "13900000000", "Come home", "Dad") == 0);
	assert(bepio_messaging_get_at(BEPIO_MESSAGE_INBOX, 0, &message) == 0);
	incoming = message.id;
	assert(incoming == 2);
	bepio_messaging_stats(&stats);
	assert(stats.unread == 1);
	assert(COMMAND("inbox") == 0 && strstr(host_shell_output, "UNREAD"));
	bepio_messaging_stats(&stats);
	assert(stats.unread == 1);
	assert(COMMAND("delete", "in", "2") == 0);
	bepio_messaging_stats(&stats);
	assert(stats.inbox == 1 && stats.unread == 1);
	assert(COMMAND("cancel") == 0);
	assert(COMMAND("show", "in", "2") == 0);
	bepio_messaging_stats(&stats);
	assert(stats.unread == 0);
	assert(COMMAND("reply", "2") == 0);
	assert(strstr(host_shell_output, "Dad <13900000000>"));
	assert(COMMAND("text", "On my way") == 0);
	assert(bepio_app_manager_delete() == 0); /* Forced close must retain unsaved editor. */
	assert(COMMAND("text", "Still editing") == 0);
	assert(COMMAND("back") == 0 && COMMAND("save") == 0);
	assert(COMMAND("delete", "in", "2") == 0 && COMMAND("confirm") == 0);
	assert(bepio_messaging_get(BEPIO_MESSAGE_INBOX, incoming, false, &message) == -ENOENT);
	assert(COMMAND("demo_receive", "wire-1", "13900000000", "Come home", "Dad") == -EALREADY);
	assert(COMMAND("show", "in", "-1") == -EINVAL);
	assert(COMMAND("show", "out", "4294967296") == -EINVAL);
	assert(COMMAND("show", "bad", "1") == -EINVAL);
	assert(COMMAND("new", "110") == 0);
	assert(COMMAND("text", "Emergency draft") == 0);
	assert(COMMAND("discard") == 0);
	assert(messages_controller_snapshot(c, &snapshot) == 0 && !snapshot.model.editing);
	assert(COMMAND("home") == 0 && COMMAND("back") == 0);
	assert(!bepio_app_manager_is_app_running());
	assert(COMMAND("outbox") == 0);
	assert(strstr(host_shell_output, "SIMULATED-sent"));
	assert(bepio_app_manager_delete() == 0);
	/* Contact edits/deletion cannot alter historical recipient snapshots. */
	assert(bepio_app_manager_show(NULL, NULL, NULL, "Contacts") == 0);
	assert(contacts_controller_update(contacts, contact_id, "Changed", "119") == 0);
	assert(contacts_controller_delete(contacts, contact_id) == 0);
	assert(bepio_messaging_get(BEPIO_MESSAGE_OUTBOX, draft, false, &message) == 0);
	assert(!strcmp(message.payload.name, "Mom") &&
	       !strcmp(message.payload.phone, "+8613800000000"));
	assert(bepio_app_manager_delete() == 0);
}

static void test_editor_failure_retention(void)
{
	messages_controller_t *c = messages_app_get_controller();
	messages_snapshot_t snapshot;
	uint32_t id;
	char boundary[260] = {0};

	assert(bepio_messaging_init() == 0);
	assert(COMMAND("home") == 0);
	for (int i = 0; i < CONFIG_BEPIO_MESSAGES_OUTBOX_CAPACITY; i++) {
		assert(bepio_messaging_save(0, &payload, &id) == 0);
	}
	assert(COMMAND("new", "110", "Emergency") == 0);
	for (int i = 0; i < 85; i++) {
		memcpy(boundary + i * 3, "\xe4\xb8\xad", 3);
	}
	boundary[255] = 'a'; /* Exactly 256 bytes, no split Unicode character. */
	assert(COMMAND("text", boundary) == 0);
	assert(strstr(host_shell_output, "0 bytes remaining"));
	boundary[256] = 'b';
	assert(COMMAND("text", boundary) == -ENAMETOOLONG);
	assert(messages_controller_snapshot(c, &snapshot) == 0);
	assert(strlen(snapshot.model.editor.body) == 256);
	assert(COMMAND("text", "Retain me") == 0);
	assert(COMMAND("send") == 0);
	assert(COMMAND("confirm") == -ENOSPC);
	assert(messages_controller_snapshot(c, &snapshot) == 0);
	assert(snapshot.model.editing && !strcmp(snapshot.model.editor.body, "Retain me"));
	assert(COMMAND("cancel") == 0);
	assert(COMMAND("save") == -ENOSPC);
	assert(COMMAND("back") == 0 && COMMAND("discard") == 0);
	assert(bepio_app_manager_delete() == 0);
}

static void test_retention(void)
{
	assert(bepio_messaging_init() == 0);
	assert(bepio_messaging_retention_days() == 7);
	assert(bepio_messaging_set_retention_days(5) == -EINVAL);
	assert(bepio_messaging_set_retention_days(3) == 0);
	bepio_message_payload_t payload = {.name = "Alice", .phone = "13800138000", .body = "你好"};
	uint32_t id;
	const int64_t now = (int64_t)time(NULL) * 1000;
	assert(bepio_messaging_receive("old", &payload, now - 4LL * 86400000, false, &id) == 0);
	assert(bepio_messaging_receive("unknown", &payload, 0, false, &id) == 0);
	assert(bepio_messaging_prune(now) == 0);
	bepio_messaging_stats_t stats;
	bepio_messaging_stats(&stats);
	assert(stats.inbox == 1);
	assert(bepio_messaging_set_retention_days(14) == 0);
	assert(bepio_messaging_prune(0) == 0);
	bepio_messaging_stats(&stats);
	assert(stats.inbox == 1);
}

int main(void)
{
	test_validation();
	test_inbox();
	test_queue();
	test_app();
	test_editor_failure_retention();
	test_retention();
	puts("PASS: Messages UTF-8, inbox, queue/ACK/retry, confirmations, drafts, Contacts, "
	     "shell");
	return 0;
}

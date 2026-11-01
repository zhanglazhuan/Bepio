#include "app.h"
#include "bepio_app_manager.h"
#include <errno.h>
#include <inttypes.h>
#include <string.h>
#include "bepio_console.h"
#ifdef CONFIG_BEPIO_APP_CONTACTS
#include "apps/contacts/app.h"
#endif

static int error(const bepio_console_context_t *shell, int ret)
{
	const char *text;

	switch (ret) {
	case -EINVAL:
		text = "Invalid recipient, ID or UTF-8 text; sending needs recipient and body";
		break;
	case -ENAMETOOLONG:
		text = "Name/phone/body exceeds 48/24/256 bytes";
		break;
	case -ENOENT:
		text = "Message/contact/queued item not found";
		break;
	case -ENOSPC:
		text = "Mailbox full; delete a message first (editor retained)";
		break;
	case -EALREADY:
		text = "Duplicate receive ignored";
		break;
	case -EBUSY:
		text = "Finish/cancel the current action or close the other app first";
		break;
	case -EPERM:
		text = "Action not allowed in this state; use save/discard/cancel when editing";
		break;
	case -EACCES:
		text = "Messages is not active";
		break;
	case -ENOTSUP:
		text = "Contacts support is disabled";
		break;
	case -ENETDOWN:
		text = "Offline: message remains queued";
		break;
	case -ESTALE:
		text = "Stale transport result ignored";
		break;
	default:
		text = "Messages operation failed";
		break;
	}
	bepio_console_error(shell, "%s (%d)", text, ret);
	return ret;
}

static int open_messages(const bepio_console_context_t *shell)
{
	int ret = bepio_app_manager_show(NULL, NULL, NULL, BEPIO_MESSAGES_APP_NAME);

	return ret < 0 ? error(shell, ret) : ret;
}

static int recipients(const bepio_console_context_t *shell, size_t argc, char **argv)
{
	int ret = open_messages(shell);

	BEPIO_UNUSED(argc);
	BEPIO_UNUSED(argv);
	if (ret < 0) {
		return ret;
	}
#ifdef CONFIG_BEPIO_APP_CONTACTS
	contacts_entry_t entry;
	bepio_console_print(shell, "Contacts: choose with messages to <id>");
	for (size_t i = 0; i < CONFIG_BEPIO_CONTACTS_CAPACITY; i++) {
		ret = contacts_controller_lookup_at(contacts_app_get_controller(), i, &entry);
		if (ret == -ENOENT) {
			if (!i) {
				bepio_console_print(shell, "No contacts; use messages new <phone> [name]");
			}
			return 0;
		}
		if (ret < 0) {
			return error(shell, ret);
		}
		bepio_console_print(shell, "%u | %s | %s", (unsigned int)entry.id, entry.name, entry.phone);
	}
	return 0;
#else
	return error(shell, -ENOTSUP);
#endif
}

static int parse_id(const bepio_console_context_t *shell, const char *text, uint32_t *id)
{
	uint32_t value = 0;

	if (!text || !text[0]) {
		return error(shell, -EINVAL);
	}
	for (size_t i = 0; text[i]; i++) {
		if (text[i] < '0' || text[i] > '9' ||
		    value > (UINT32_MAX - (uint32_t)(text[i] - '0')) / 10U) {
			return error(shell, -EINVAL);
		}
		value = value * 10U + (uint32_t)(text[i] - '0');
	}
	if (!value) {
		return error(shell, -EINVAL);
	}
	*id = value;
	return 0;
}

static int parse_box(const bepio_console_context_t *shell, const char *text, bepio_message_box_t *box)
{
	if (!strcmp(text, "in")) {
		*box = BEPIO_MESSAGE_INBOX;
		return 0;
	}
	if (!strcmp(text, "out")) {
		*box = BEPIO_MESSAGE_OUTBOX;
		return 0;
	}
	bepio_console_error(shell, "Box must be in or out");
	return -EINVAL;
}

static void print_payload(const bepio_console_context_t *shell, const bepio_message_payload_t *payload)
{
	bepio_console_print(shell, "Peer: %s <%s>\n%s", payload->name, payload->phone, payload->body);
}

static void print_message(const bepio_console_context_t *shell, const bepio_message_t *message)
{
	bepio_console_print(shell, "ID=%u %s %s attempt=%u [RAM-only]", (unsigned int)message->id,
		    bepio_messaging_state_label(message),
		    message->state == BEPIO_MESSAGE_RECEIVED ? (message->read ? "read" : "unread")
							     : "outgoing",
		    (unsigned int)message->attempt);
	if (message->timestamp_ms) {
		bepio_console_print(shell, "Epoch milliseconds: %" PRId64, message->timestamp_ms);
	} else {
		bepio_console_print(shell, "Time unsynchronized; ordered by local arrival/creation");
	}
	print_payload(shell, &message->payload);
}

static int print_box(const bepio_console_context_t *shell, bepio_message_box_t box)
{
	bepio_messaging_stats_t stats;
	bepio_message_t message;
	size_t count;
	int ret;

	bepio_messaging_stats(&stats);
	count = box == BEPIO_MESSAGE_INBOX ? stats.inbox : stats.outbox;
	bepio_console_print(shell, "%s: %u (unread=%u) [RAM-only, newest first]",
		    box == BEPIO_MESSAGE_INBOX ? "Inbox" : "Outbox", (unsigned int)count,
		    (unsigned int)stats.unread);
	if (!count) {
		bepio_console_print(shell, "Empty");
	}
	for (size_t i = 0; i < count; i++) {
		ret = bepio_messaging_get_at(box, i, &message);
		if (ret < 0) {
			return error(shell, ret);
		}
		size_t length = strlen(message.payload.body);
		if (length > 48) {
			length = 48;
			while (((unsigned char)message.payload.body[length] & 0xc0) == 0x80) {
				length--;
			}
		}
		bepio_console_print(shell, "%u | %s <%s> | %s%s | %.*s", (unsigned int)message.id,
			    message.payload.name, message.payload.phone,
			    bepio_messaging_state_label(&message),
			    box == BEPIO_MESSAGE_INBOX && !message.read ? " UNREAD" : "",
			    (int)length, message.payload.body);
	}
	return 0;
}

static int render(const bepio_console_context_t *shell)
{
	messages_snapshot_t snapshot;
	bepio_message_t message;
	bepio_messaging_stats_t stats;
	int ret = messages_controller_snapshot(messages_app_get_controller(), &snapshot);

	if (ret < 0) {
		return error(shell, ret);
	}
	if (snapshot.model.flow.page == MESSAGES_PAGE_BOX) {
		return print_box(shell, snapshot.model.flow.box);
	}
	if (snapshot.model.flow.page == MESSAGES_PAGE_DETAIL) {
		ret = bepio_messaging_get(snapshot.model.flow.box, snapshot.model.flow.selected_id, false,
					  &message);
		if (ret < 0) {
			return error(shell, ret);
		}
		print_message(shell, &message);
	} else if (snapshot.model.flow.page == MESSAGES_PAGE_COMPOSE) {
		print_payload(shell, &snapshot.model.editor);
		bepio_console_print(shell,
			    "Editor: %s; %u bytes remaining. text/recipient/to/save/send/back",
			    snapshot.model.dirty ? "unsaved changes" : "saved draft",
			    (unsigned int)(BEPIO_MESSAGE_BODY_MAX -
					   strlen(snapshot.model.editor.body)));
	} else if (snapshot.model.flow.page == MESSAGES_PAGE_CONFIRM) {
		switch (snapshot.model.pending) {
		case MESSAGES_PENDING_SEND:
			print_payload(shell, &snapshot.model.editor);
			bepio_console_print(shell, "Confirm queueing this message: messages confirm / "
					   "messages cancel");
			break;
		case MESSAGES_PENDING_DELETE:
			ret = bepio_messaging_get(snapshot.model.pending_box,
						  snapshot.model.pending_id, false, &message);
			if (ret < 0) {
				return error(shell, ret);
			}
			print_message(shell, &message);
			bepio_console_print(shell, "Confirm delete (queued messages will be cancelled): "
					   "confirm / cancel");
			break;
		case MESSAGES_PENDING_LEAVE:
			bepio_console_print(shell, "Unsaved changes: messages save / messages discard / "
					   "messages cancel");
			break;
		default:
			break;
		}
	} else {
		bepio_messaging_stats(&stats);
		bepio_console_print(shell, "Messages: inbox=%u unread=%u outbox=%u queued=%u %s [RAM-only]",
			    (unsigned int)stats.inbox, (unsigned int)stats.unread,
			    (unsigned int)stats.outbox, (unsigned int)stats.queued,
			    stats.online ? "transport-online" : "offline (no server adapter)");
		bepio_console_print(shell,
			    "Use messages inbox / outbox / new <phone> [name] / to <contact-id>");
		return print_box(shell, BEPIO_MESSAGE_INBOX);
	}
	return 0;
}

static int finish(const bepio_console_context_t *shell, int ret)
{
	return ret < 0 ? error(shell, ret) : render(shell);
}

static int home(const bepio_console_context_t *shell, size_t argc, char **argv)
{
	BEPIO_UNUSED(argc);
	BEPIO_UNUSED(argv);
	int ret = open_messages(shell);
	return ret < 0 ? ret
		       : finish(shell, messages_controller_home(messages_app_get_controller()));
}

static int mailbox(const bepio_console_context_t *shell, size_t argc, char **argv)
{
	BEPIO_UNUSED(argc);
	int ret = open_messages(shell);
	if (ret < 0) {
		return ret;
	}
	return finish(shell,
		      messages_controller_box(messages_app_get_controller(),
					      !strcmp(argv[0], "inbox") ? BEPIO_MESSAGE_INBOX
									: BEPIO_MESSAGE_OUTBOX));
}

static int show(const bepio_console_context_t *shell, size_t argc, char **argv)
{
	bepio_message_box_t box;
	bepio_message_t message;
	uint32_t id;
	BEPIO_UNUSED(argc);
	int ret = parse_box(shell, argv[1], &box);
	if (ret < 0 || (ret = parse_id(shell, argv[2], &id)) < 0 ||
	    (ret = open_messages(shell)) < 0) {
		return ret;
	}
	return finish(shell,
		      messages_controller_show(messages_app_get_controller(), box, id, &message));
}

static int new_message(const bepio_console_context_t *shell, size_t argc, char **argv)
{
	int ret = open_messages(shell);
	if (ret < 0) {
		return ret;
	}
	return finish(shell, messages_controller_new(messages_app_get_controller(), argv[1],
						     argc > 2 ? argv[2] : ""));
}

static int recipient(const bepio_console_context_t *shell, size_t argc, char **argv)
{
	int ret = open_messages(shell);
	if (ret < 0) {
		return ret;
	}
	return finish(shell, messages_controller_recipient(messages_app_get_controller(), argv[1],
							   argc > 2 ? argv[2] : ""));
}

static int numbered_action(const bepio_console_context_t *shell, size_t argc, char **argv)
{
	uint32_t id;
	messages_controller_t *c = messages_app_get_controller();
	BEPIO_UNUSED(argc);
	int ret = parse_id(shell, argv[1], &id);
	if (ret < 0 || (ret = open_messages(shell)) < 0) {
		return ret;
	}
	if (!strcmp(argv[0], "to")) {
		ret = messages_controller_contact(c, id);
	} else if (!strcmp(argv[0], "reply")) {
		ret = messages_controller_reply(c, id);
	} else if (!strcmp(argv[0], "edit")) {
		ret = messages_controller_edit(c, id);
	} else {
		ret = messages_controller_retry(c, id);
	}
	return finish(shell, ret);
}

static int text(const bepio_console_context_t *shell, size_t argc, char **argv)
{
	BEPIO_UNUSED(argc);
	int ret = open_messages(shell);
	return ret < 0 ? ret
		       : finish(shell,
				messages_controller_text(messages_app_get_controller(), argv[1]));
}

static int delete_message(const bepio_console_context_t *shell, size_t argc, char **argv)
{
	bepio_message_box_t box;
	uint32_t id;
	BEPIO_UNUSED(argc);
	int ret = parse_box(shell, argv[1], &box);
	if (ret < 0 || (ret = parse_id(shell, argv[2], &id)) < 0 ||
	    (ret = open_messages(shell)) < 0) {
		return ret;
	}
	return finish(shell, messages_controller_delete(messages_app_get_controller(), box, id));
}

static int action(const bepio_console_context_t *shell, size_t argc, char **argv)
{
	messages_controller_t *c = messages_app_get_controller();
	uint32_t id;
	BEPIO_UNUSED(argc);
	int ret = open_messages(shell);
	if (ret < 0) {
		return ret;
	}
	if (!strcmp(argv[0], "save")) {
		ret = messages_controller_save(c, &id);
		if (!ret) {
			bepio_console_print(shell, "Saved draft %u", (unsigned int)id);
		}
	} else if (!strcmp(argv[0], "discard")) {
		ret = messages_controller_discard(c);
	} else if (!strcmp(argv[0], "send")) {
		ret = messages_controller_send(c);
	} else if (!strcmp(argv[0], "confirm")) {
		ret = messages_controller_confirm(c, &id);
		if (!ret) {
			bepio_console_print(shell, "Confirmed message %u", (unsigned int)id);
		}
	} else {
		ret = messages_controller_cancel(c);
	}
	return finish(shell, ret);
}

static int back(const bepio_console_context_t *shell, size_t argc, char **argv)
{
	BEPIO_UNUSED(argc);
	BEPIO_UNUSED(argv);
	int ret = messages_app_close_request();
	if (ret < 0) {
		return error(shell, ret);
	}
	const char *current = bepio_app_manager_get_current_app_name();
	if (current && !strcmp(current, BEPIO_MESSAGES_APP_NAME)) {
		return render(shell);
	}
	bepio_console_print(shell, "Messages closed; RAM data retained");
	return 0;
}

#ifdef CONFIG_BEPIO_MESSAGES_DEMO
static int demo_receive(const bepio_console_context_t *shell, size_t argc, char **argv)
{
	bepio_message_payload_t payload = {0};
	uint32_t id;
	const char *name = argc > 4 ? argv[4] : "";

	/* Check lengths before copying external input into fixed-sized fields. */
	if (strlen(argv[2]) > BEPIO_MESSAGE_PHONE_MAX || strlen(argv[3]) > BEPIO_MESSAGE_BODY_MAX ||
	    strlen(name) > BEPIO_MESSAGE_NAME_MAX) {
		return error(shell, -ENAMETOOLONG);
	}
	strcpy(payload.phone, argv[2]);
	strcpy(payload.body, argv[3]);
	strcpy(payload.name, name);
	int ret = bepio_messaging_receive(argv[1], &payload, 0, true, &id);
	if (ret < 0) {
		return error(shell, ret);
	}
	bepio_console_print(shell, "SIMULATED receive id=%u; no network traffic", (unsigned int)id);
	return 0;
}

static int demo_send(const bepio_console_context_t *shell, size_t argc, char **argv)
{
	bepio_message_t message;
	BEPIO_UNUSED(argc);
	if (strcmp(argv[1], "ok") && strcmp(argv[1], "fail")) {
		return error(shell, -EINVAL);
	}
	int ret = bepio_messaging_begin(true, &message);
	if (!ret) {
		ret = bepio_messaging_complete(message.id, message.attempt, true,
					       !strcmp(argv[1], "ok"));
	}
	if (ret < 0) {
		return error(shell, ret);
	}
	bepio_console_print(shell, "SIMULATED %s id=%u; no network traffic", argv[1],
		    (unsigned int)message.id);
	return 0;
}
#endif

BEPIO_COMMANDS(
	messages_commands, BEPIO_COMMAND(home, NULL, "Open Messages", home, 1, 0),
	BEPIO_COMMAND(contacts, NULL, "List contacts without switching apps", recipients, 1, 0),
	BEPIO_COMMAND(inbox, NULL, "Open inbox", mailbox, 1, 0),
	BEPIO_COMMAND(outbox, NULL, "Open outbox (including drafts)", mailbox, 1, 0),
	BEPIO_COMMAND(show, NULL, "Show <in|out> <id>", show, 3, 0),
	BEPIO_COMMAND(new, NULL, "Compose <phone> [name]", new_message, 2, 1),
	BEPIO_COMMAND(recipient, NULL, "Change editor recipient <phone> [name]", recipient, 2, 1),
	BEPIO_COMMAND(to, NULL, "Compose/change recipient from <contact-id>", numbered_action, 2,
		      0),
	BEPIO_COMMAND(reply, NULL, "Reply to <inbox-id>", numbered_action, 2, 0),
	BEPIO_COMMAND(edit, NULL, "Edit saved <draft-id>", numbered_action, 2, 0),
	BEPIO_COMMAND(text, NULL, "Replace body <quoted UTF-8 text> (max 256 bytes)", text, 2, 0),
	BEPIO_COMMAND(save, NULL, "Save draft", action, 1, 0),
	BEPIO_COMMAND(discard, NULL, "Discard unsaved edits; keep stored draft", action, 1, 0),
	BEPIO_COMMAND(send, NULL, "Preview and request send confirmation", action, 1, 0),
	BEPIO_COMMAND(delete, NULL, "Request delete <in|out> <id>", delete_message, 3, 0),
	BEPIO_COMMAND(confirm, NULL, "Confirm send/delete", action, 1, 0),
	BEPIO_COMMAND(cancel, NULL, "Cancel pending action / continue editing", action, 1, 0),
	BEPIO_COMMAND(retry, NULL, "Requeue failed <outbox-id>", numbered_action, 2, 0),
	BEPIO_COMMAND(back, NULL, "Back; prompt on unsaved edits", back, 1, 0),
#ifdef CONFIG_BEPIO_MESSAGES_DEMO
	BEPIO_COMMAND(demo_receive, NULL, "SIMULATED <remote-id> <phone> <body> [name]",
		      demo_receive, 4, 1),
	BEPIO_COMMAND(demo_send, NULL, "SIMULATED next queued send <ok|fail>", demo_send, 2, 0),
#endif
	BEPIO_COMMAND_END);
BEPIO_COMMAND_GROUP(messages, messages_commands, "Messages app (RAM-only)", NULL);

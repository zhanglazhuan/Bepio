#include "model.h"
#include "apps/contacts/app.h"
#include <errno.h>
#include <string.h>

#ifdef CONFIG_BEPIO_MESSAGES_DEMO
typedef struct {
	const char *name;
	const char *phone;
} mock_contact_t;

typedef struct {
	const char *phone;
	const char *body;
	bool outgoing;
} mock_message_t;

static const mock_contact_t mock_contacts[] = {
	{"Alex", "13800138000"},
	{"Home", "13900139000"},
	{"Bepio Lab", "13700137000"},
};

static const mock_message_t mock_messages[] = {
	{"13800138000", "今天下午有空吗？", false},
	{"13800138000", "有空，我们三点见。", true},
	{"13800138000", "好的，到时见！", false},
	{"13900139000", "记得早点回家。", false},
	{"13900139000", "我已经在路上了。", true},
	{"13700137000", "欢迎体验 Chats 演示。", false},
	{"13700137000", "收到了，谢谢！", true},
};
#endif

void bepio_chats_model_load(bepio_chats_model_t *model)
{
	memset(model, 0, sizeof(*model));
	contacts_entry_t entry;
	for (size_t i = 0; model->contact_count < CONFIG_BEPIO_CONTACTS_CAPACITY &&
	     !contacts_controller_lookup_at(contacts_app_get_controller(), i, &entry); i++) {
		bepio_chats_contact_t *contact = &model->contacts[model->contact_count++];
		memcpy(contact->name, entry.name, sizeof(contact->name));
		memcpy(contact->phone, entry.phone, sizeof(contact->phone));
	}
#ifdef CONFIG_BEPIO_MESSAGES_DEMO
	for (size_t i = 0; i < BEPIO_CHATS_MOCK_CONTACTS; i++) {
		bool present = false;
		for (size_t j = 0; j < model->contact_count; j++) {
			if (!strcmp(model->contacts[j].phone, mock_contacts[i].phone)) {
				present = true;
				break;
			}
		}
		if (present) { continue; }
		bepio_chats_contact_t *contact = &model->contacts[model->contact_count++];
		strcpy(contact->name, mock_contacts[i].name);
		strcpy(contact->phone, mock_contacts[i].phone);
		contact->demo = true;
	}
#endif
	if (model->contact_count) {
		size_t initial = 0;
#ifdef CONFIG_BEPIO_MESSAGES_DEMO
		for (size_t i = 0; i < model->contact_count; i++) {
			if (!strcmp(model->contacts[i].phone, mock_contacts[0].phone)) {
				initial = i;
				break;
			}
		}
#endif
		(void)bepio_chats_model_select(model, initial);
	}
}

int bepio_chats_model_select(bepio_chats_model_t *model, size_t index)
{
	if (!model || index >= model->contact_count) { return -ENOENT; }
	model->selected = index;
	model->message_count = 0;
	const char *phone = model->contacts[index].phone;
	for (int box = BEPIO_MESSAGE_INBOX; box <= BEPIO_MESSAGE_OUTBOX; box++) {
		bepio_message_t item;
		for (size_t i = 0; !bepio_messaging_get_at((bepio_message_box_t)box, i, &item); i++) {
			if (strcmp(item.payload.phone, phone) ||
			    model->message_count >= BEPIO_CHATS_MESSAGE_CAPACITY) { continue; }
			size_t at = model->message_count++;
			while (at && model->conversation[at - 1].id > item.id) {
				model->conversation[at] = model->conversation[at - 1];
				at--;
			}
			model->conversation[at] = (bepio_chats_ref_t){
				.id = item.id, .box = (bepio_message_box_t)box, .mock_index = -1
			};
		}
	}
#ifdef CONFIG_BEPIO_MESSAGES_DEMO
	for (size_t i = 0; i < BEPIO_CHATS_MOCK_MESSAGES; i++) {
		if (!strcmp(mock_messages[i].phone, phone) &&
		    model->message_count < BEPIO_CHATS_MESSAGE_CAPACITY) {
			model->conversation[model->message_count++] = (bepio_chats_ref_t){.mock_index = (int8_t)i};
		}
	}
#endif
	return 0;
}

int bepio_chats_model_contact_at(const bepio_chats_model_t *model, size_t index,
				bepio_chats_contact_t *contact)
{
	if (!model || !contact) { return -EINVAL; }
	if (index >= model->contact_count) { return -ENOENT; }
	*contact = model->contacts[index];
	return 0;
}

int bepio_chats_model_message_at(const bepio_chats_model_t *model, size_t index,
				bepio_chats_message_t *message)
{
	if (!model || !message) { return -EINVAL; }
	if (index >= model->message_count) { return -ENOENT; }
	const bepio_chats_ref_t *ref = &model->conversation[index];
	memset(message, 0, sizeof(*message));
#ifdef CONFIG_BEPIO_MESSAGES_DEMO
	if (ref->mock_index >= 0) {
		const mock_message_t *mock = &mock_messages[ref->mock_index];
		strcpy(message->body, mock->body);
		message->outgoing = mock->outgoing;
		message->demo = true;
		return 0;
	}
#endif
	bepio_message_t stored;
	int ret = bepio_messaging_get(ref->box, ref->id, false, &stored);
	if (ret) { return ret; }
	memcpy(message->body, stored.payload.body, sizeof(message->body));
	message->outgoing = ref->box == BEPIO_MESSAGE_OUTBOX;
	message->demo = stored.simulated;
	return 0;
}

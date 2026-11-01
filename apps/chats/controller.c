#include "controller.h"
#include "apps/messages/voice.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>

static void clear_text(char *text, size_t size)
{
	volatile char *byte = text;
	while (size--) { *byte++ = 0; }
}

void bepio_chats_controller_open(bepio_chats_controller_t *controller, bepio_chats_model_t *model)
{
	*controller = (bepio_chats_controller_t){.model = model, .active = true};
	bepio_chats_model_load(model);
}

void bepio_chats_controller_close(bepio_chats_controller_t *controller)
{
	bepio_chats_controller_record_cancel(controller);
	controller->active = false;
}

int bepio_chats_controller_select(bepio_chats_controller_t *controller, size_t index)
{
	if (!controller || !controller->active) { return -EACCES; }
	if (index >= controller->model->contact_count) { return -ENOENT; }
	bepio_chats_controller_record_cancel(controller);
	return bepio_chats_model_select(controller->model, index);
}

int bepio_chats_controller_contact_at(const bepio_chats_controller_t *controller, size_t index,
				bepio_chats_contact_t *contact)
{
	return controller && controller->active ?
		bepio_chats_model_contact_at(controller->model, index, contact) : -EACCES;
}

int bepio_chats_controller_message_at(const bepio_chats_controller_t *controller, size_t index,
				bepio_chats_message_t *message)
{
	return controller && controller->active ?
		bepio_chats_model_message_at(controller->model, index, message) : -EACCES;
}

size_t bepio_chats_controller_selected(const bepio_chats_controller_t *controller)
{
	return controller && controller->active ? controller->model->selected : 0;
}

int bepio_chats_controller_play(bepio_chats_controller_t *controller, size_t index)
{
	bepio_chats_message_t message;
	int ret = bepio_chats_controller_message_at(controller, index, &message);
	return ret ? ret : bepio_messages_voice_play(message.body);
}

int bepio_chats_controller_set_voice_input(bepio_chats_controller_t *controller,
	const bepio_chats_voice_input_t *input, void *context)
{
	if (!controller || (input && (!input->start || !input->stop || !input->cancel))) {
		return -EINVAL;
	}
	bepio_chats_controller_record_cancel(controller);
	controller->voice_input = input;
	controller->voice_context = context;
	return 0;
}

int bepio_chats_controller_record_start(bepio_chats_controller_t *controller)
{
	if (!controller || !controller->active) { return -EACCES; }
	if (controller->recording) { return -EBUSY; }
	if (controller->model->selected >= controller->model->contact_count) { return -ENOENT; }
	if (!controller->voice_input) { return -ENOTSUP; }
	const bepio_chats_contact_t *contact =
		&controller->model->contacts[controller->model->selected];
	if (contact->demo || !contact->phone[0]) { return -ENOENT; }
	int ret = controller->voice_input->start(controller->voice_context);
	if (!ret) {
		controller->recording = true;
		controller->recording_contact = controller->model->selected;
	}
	return ret;
}

void bepio_chats_controller_record_cancel(bepio_chats_controller_t *controller)
{
	if (!controller || !controller->recording) { return; }
	controller->recording = false;
	if (controller->voice_input) {
		controller->voice_input->cancel(controller->voice_context);
	}
}

int bepio_chats_controller_record_stop(bepio_chats_controller_t *controller, uint32_t *id)
{
	if (!controller || !controller->active) { return -EACCES; }
	if (!controller->recording || !id) { return -EINVAL; }
	if (controller->model->selected != controller->recording_contact) {
		bepio_chats_controller_record_cancel(controller);
		return -ECANCELED;
	}
	controller->recording = false;
	char text[BEPIO_MESSAGE_BODY_MAX + 1] = {0};
	int ret = controller->voice_input->stop(controller->voice_context, text, sizeof(text));
	if (!ret && text[sizeof(text) - 1]) { ret = -ENAMETOOLONG; }
	text[sizeof(text) - 1] = 0;
	if (!ret && !text[0]) { ret = -EINVAL; }
	if (!ret) {
		const bepio_chats_contact_t *contact =
			&controller->model->contacts[controller->model->selected];
		bepio_message_payload_t payload = {0};
		snprintf(payload.name, sizeof(payload.name), "%s", contact->name);
		snprintf(payload.phone, sizeof(payload.phone), "%s", contact->phone);
		memcpy(payload.body, text, sizeof(payload.body));
		ret = bepio_messaging_submit(0, &payload, id);
		if (!ret) { (void)bepio_chats_model_select(controller->model, controller->model->selected); }
		clear_text(payload.body, sizeof(payload.body));
	}
	clear_text(text, sizeof(text));
	return ret;
}

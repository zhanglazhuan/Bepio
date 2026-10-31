#include "controller.h"
#include "apps/messages/voice.h"
#include <errno.h>

void bepio_chat_controller_open(bepio_chat_controller_t *controller, bepio_chat_model_t *model)
{
	controller->model = model;
	bepio_chat_model_load(model);
	controller->active = true;
}

void bepio_chat_controller_close(bepio_chat_controller_t *controller)
{
	controller->active = false;
}

int bepio_chat_controller_select(bepio_chat_controller_t *controller, size_t index)
{
	return controller && controller->active ? bepio_chat_model_select(controller->model, index) : -EACCES;
}

int bepio_chat_controller_contact_at(const bepio_chat_controller_t *controller, size_t index,
				bepio_chat_contact_t *contact)
{
	return controller && controller->active ?
		bepio_chat_model_contact_at(controller->model, index, contact) : -EACCES;
}

int bepio_chat_controller_message_at(const bepio_chat_controller_t *controller, size_t index,
				bepio_chat_message_t *message)
{
	return controller && controller->active ?
		bepio_chat_model_message_at(controller->model, index, message) : -EACCES;
}

size_t bepio_chat_controller_selected(const bepio_chat_controller_t *controller)
{
	return controller && controller->active ? controller->model->selected : 0;
}

int bepio_chat_controller_play(bepio_chat_controller_t *controller, size_t index)
{
	bepio_chat_message_t message;
	int ret = bepio_chat_controller_message_at(controller, index, &message);
	return ret ? ret : bepio_messages_voice_play(message.body);
}

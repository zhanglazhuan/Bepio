#include "controller.h"
#include "apps/messages/voice.h"
#include <errno.h>

void bepio_chats_controller_open(bepio_chats_controller_t *controller, bepio_chats_model_t *model)
{
	controller->model = model;
	bepio_chats_model_load(model);
	controller->active = true;
}

void bepio_chats_controller_close(bepio_chats_controller_t *controller)
{
	controller->active = false;
}

int bepio_chats_controller_select(bepio_chats_controller_t *controller, size_t index)
{
	return controller && controller->active ? bepio_chats_model_select(controller->model, index) : -EACCES;
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
